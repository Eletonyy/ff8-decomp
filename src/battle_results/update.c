#include "common.h"
#include "battle_results/update.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "battle_results/result.h"
#include "game.h"
#include "gf_curve.h"
#include "numstr.h"
#include "snd_sfx.h"

/**
 * @brief Write a reward's name into @p arg1.
 *
 * An item's name, or for a card (@p arg0 has 0x100 set) menu string 0x6E with
 * its 0x0A code replaced by the card's name (func_80023A54).
 *
 * @param arg0 Reward id: an item, or a card when 0x100 is set.
 * @param arg1 Buffer that receives the name.
 */
void func_8003228C(s32 arg0, u8 *arg1)
{
    u8 *src;
    u8 *dst;
    s32 cond;
    u8 c;

    dst = arg1;
    cond = arg0 & 0x100;

    if (cond) {
        src = getMenuString(0x6E);

        while (1) {
            c = *src;
            src++;

            if (c == 0xA) {
                src++;
                {
                    u8 *src2 = func_80023A54(arg0 & 0xFF);
                    u8 c2;

                    while (1) {
                        c2 = *src2;
                        src2++;
                        if (c2 == 0) break;
                        *dst = c2;
                        dst++;
                    }
                }
            } else {
                *dst = c;
                dst++;
            }

            if (c == 0) break;
        }
    } else {
        copyString(dst, getItemName(arg0));
    }
}


/**
 * @brief Copy a message into @p dst, expanding its 0x0A codes.
 *
 * Each 0x0A byte is followed by a code byte naming what to insert: 0x25 the
 * name of ability @p ability, 0x26 the name of magic @p magic, 0x23 the name
 * of reward @p reward (func_8003228C), 0x20 @p number in decimal without
 * leading zeros. Other codes insert nothing.
 *
 * @param msg Message with 0x0A codes.
 * @param dst Buffer that receives the expanded message.
 * @param reward Reward id for code 0x23: an item, or a card when 0x100 is set.
 * @param arg3 Not used.
 * @param ability Ability id for code 0x25.
 * @param number Number for code 0x20.
 * @param magic Magic id for code 0x26; from 0x40 on, a GF's name.
 */
void func_80032350(u8 *msg, u8 *dst, s32 reward, s32 arg3, s32 ability, s32 number, s32 magic) {
    u8 buf[0x40];
    u8 *p;
    u8 *q;
    s32 c;

    while (1) {
        c = *msg++;
        if (c == 0) {
            break;
        }
        if (c != 0xA) {
            *dst++ = c;
            continue;
        }
        p = buf;
        c = *msg++;
        buf[0] = 0;
        switch (c) {
        case 0x25:
            copyString(p, getAbilityName(ability));
            break;
        case 0x26:
            copyString(p, getMagicNamePtr(magic));
            break;
        case 0x23:
            func_8003228C(reward, p);
            break;
        case 0x20:
            intToDecStringShort(number, p, 1);
            replaceLeadingZeros(p, 4, 1, 0x10);
            while (*p == 0x10) {
                p++;
            }
            for (q = p; *q != 0; q++) {
                *q = *q - 1 + getMenuString(0xB)[1];
            }
            break;
        }
        while (*p != 0) {
            *dst++ = *p++;
        }
    }
    *dst = 0;
}


/**
 * @brief Count each results row's EXP up by one step.
 *
 * For each row with a name, moves @c unk24C of the EXP still to add
 * (@c unk240) into the current EXP (@c unk234); when no more than a step is
 * left, moves all of it and clears the step. A row whose current EXP reaches
 * evalEntityXpCurve for its level (@c unk272) levels up, with the LEVEL UP!
 * popup (@c unk275 = 0x20), up to level 100. @c unk228 is then the EXP still
 * needed for the next level; at level 100 the current EXP is held at level 99's
 * curve value and nothing is left to add. Plays sound 9 if any row levelled up.
 */
void func_80032534(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 levelUp;
    s32 i;
    u32 exp;
    u32 next;

    levelUp = 0;
    for (i = 0; i < 3; i++) {
        if (ctx->names[i] != NULL) {
            exp = ctx->unk240[i];
            if (ctx->unk24C[i] < exp) {
                exp = ctx->unk24C[i];
            } else {
                ctx->unk24C[i] = 0;
            }
            ctx->unk234[i] += exp;
            ctx->unk240[i] -= exp;
            next = evalEntityXpCurve(i, ctx->unk272[i]);
            if (ctx->unk234[i] >= next) {
                if (ctx->unk272[i] < 100) {
                    levelUp = 1;
                    ctx->unk272[i]++;
                    ctx->unk275[i] = 0x20;
                    next = evalEntityXpCurve(i, ctx->unk272[i]);
                }
            }
            if (ctx->unk272[i] < 100) {
                ctx->unk228[i] = next - ctx->unk234[i];
            } else {
                ctx->unk234[i] = evalEntityXpCurve(i, 99);
                ctx->unk228[i] = 0;
                ctx->unk240[i] = 0;
            }
        }
    }
    if (levelUp) {
        sendSpuCommand(9);
    }
}


/**
 * @brief Finish the EXP count-up at once and give the party the battle's EXP.
 *
 * Sets each named row's step (@c unk24C) to all the EXP still to add and runs
 * one count-up step (func_80032534), so level-ups and their popups come out as
 * if counted. Then adds the row's battle EXP (@c unk574 + @c unk57A of
 * g_battleChars) to the party member (func_8002257C) and takes the row's level,
 * current EXP and EXP to the next level from the result, capped at level 100
 * as in func_80032534.
 * @note @c ch starts as NULL although every use sets it first: with a value
 *       from before the loop, &g_battleChars stays in its own register when
 *       the loop sets up its pointers, as in the binary.
 */
void func_80032688(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    s32 i;
    s32 level;
    u32 next;
    u32 exp;
    BattleCharData *ch = NULL;

    for (i = 0; i < 3; i++) {
        if (ctx->names[i] != NULL) {
            ctx->unk24C[i] = ctx->unk240[i];
        }
    }
    func_80032534();
    for (i = 0; i < 3; i++) {
        if (ctx->names[i] != NULL) {
            ch = &g_battleChars.chars[i];
            level = func_8002257C(i, g_battleChars.unk574[i] + g_battleChars.unk57A[i]);
            next = evalEntityXpCurve(i, level);
            ctx->unk272[i] = level;
            exp = ch->exp;
            ctx->unk240[i] = 0;
            ctx->unk24C[i] = 0;
            ctx->unk228[i] = next - exp;
            ctx->unk234[i] = exp;
            if (level == 100) {
                ctx->unk234[i] = evalEntityXpCurve(i, 99);
                ctx->unk228[i] = 0;
                ctx->unk240[i] = 0;
                ctx->unk24C[i] = 0;
            }
        }
    }
}


/**
 * @brief Poll markItemPresent with retries until it returns a non-zero result.
 *
 * Masks the input to 8 bits and repeatedly calls markItemPresent up to the
 * specified number of attempts. Returns the first non-zero result, or 0
 * if all attempts fail.
 *
 * @param a0 Entity or resource identifier (low 8 bits used).
 * @param retries Maximum number of poll attempts.
 * @return Non-zero result from markItemPresent, or 0 on timeout.
 */
s32 pollItemPresent(s32 a0, s32 retries) {
    s32 result = 0;
    a0 &= 0xFF;
    while (retries > 0) {
        result = markItemPresent(a0);
        if (result != 0) {
            break;
        }
        retries--;
    }
    return result;
}


/**
 * @brief Return a reward's description: the item's (getItemDesc), or NULL for
 * a card (@p a0 has 0x100 set).
 *
 * @param a0 Reward id: an item, or a card when 0x100 is set.
 * @return The item's description, or NULL for a card.
 */
u8 *guardedEntityLookup(s32 a0) {
    if (a0 & 0x100) return NULL;
    return getItemDesc(a0);
}


INCLUDE_ASM("asm/nonmatchings/battle_results/update", func_8003283C);


