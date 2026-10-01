#include "common.h"
#include "battle_results/update.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "battle_results/result.h"
#include "game.h"
#include "gf_curve.h"
#include "numstr.h"
#include "snd_init.h"
#include "snd_sfx.h"

/**
 * @brief Write a reward's name into @p arg1.
 *
 * An item's name, or for a card (@p arg0 has REWARD_CARD set) menu string 0x6E with
 * its 0x0A code replaced by the card's name (func_80023A54).
 *
 * @param arg0 Reward id: an item, or a card when REWARD_CARD is set.
 * @param arg1 Buffer that receives the name.
 */
void func_8003228C(s32 arg0, u8 *arg1)
{
    u8 *src;
    u8 *dst;
    s32 cond;
    u8 c;

    dst = arg1;
    cond = arg0 & REWARD_CARD;

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
 * @param reward Reward id for code 0x23: an item, or a card when REWARD_CARD is set.
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
 * a card (@p a0 has REWARD_CARD set).
 *
 * @param a0 Reward id: an item, or a card when REWARD_CARD is set.
 * @return The item's description, or NULL for a card.
 */
u8 *guardedEntityLookup(s32 a0) {
    if (a0 & REWARD_CARD) return NULL;
    return getItemDesc(a0);
}


/**
 * @brief Return the bit number of the (@p n + 1)th set bit of @p mask, or 0 if
 * @p mask has fewer set bits.
 * @note Inline, like the binary: the result is set separately on each exit.
 */
static inline s32 findSetBit(s32 mask, s32 n) {
    s32 i = 0;
    s32 bit = 1;

    do {
        if (mask & (bit << i)) {
            if (n == 0) {
                return i;
            }
            n--;
        }
        i++;
    } while (i < 32);
    return 0;
}


/**
 * @brief Run one frame of the results screen: read the pad and advance @c step.
 *
 * Counts the LEVEL UP! popups down, then works through the steps: open the EXP
 * page and count the EXP up (func_80032534), or finish it on the confirm button
 * (func_80032688); close it and open the item page, then hand out each reward
 * in turn, items into the inventory and cards into the card list, with
 * "Couldn't find any items!" when there are none; show "GF received N AP!";
 * show each GF's level-up and learned-ability windows in turn; finally set
 * D_80083929.
 */
void func_8003283C(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    u16 *step;
    ItemReward *reward;
    s32 count;
    s32 pressed;
    s32 found;
    s32 sum;
    s32 full;
    s32 gf;
    s32 slide;
    s32 i;
    s32 j;
    s32 k;
    s32 m;
    u16 state;

    func_800275D4();
    pressed = applyButtonRemapTranslation(getPadReadPressed(0, 0));
    applyButtonRemapTranslation(getPadReadRepeat(0, 0));
    if (ctx->unk2C != 0) {
        ctx->unk2C += 0x200;
        if (ctx->unk2C > 0x1000) {
            ctx->unk2C = 0x1000;
        }
    }
    for (i = 0; i < 3; i++) {
        if (ctx->unk275[i] != 0) {
            ctx->unk275[i]--;
        }
    }
    step = &ctx->step;
    reward = ctx->reward;
    count = ctx->unk3E;
    state = *step;
    /* Some steps go straight on to another step in the same frame. They jump back to
       the switch: a loop around it (92.50%) lets gcc keep the constants the cases share
       in saved registers, where the binary loads them in each case. */
dispatch:
    switch (state) {
    case 0:
        *step = 1;
        break;
    case 1:
        ctx->title = getMenuString(0x17);
        ctx->prompt = getMenuString(0x16);
        ctx->unk38 = 0;
        *step = 2;
        break;
    case 2:
        ctx->unk2A += 0x100;
        ctx->unk2E += 0x100;
        if (ctx->unk2E >= 0x1000) {
            ctx->unk2E = 0x1000;
        }
        if (ctx->unk2A >= 0x1000) {
            ctx->unk2A = 0x1000;
            *step = 3;
        }
        break;
    case 3:
        if (pressed & PADRdown) {
            found = 0;
            for (j = 0; j < 3; j++) {
                if (ctx->unk27C[j] != 0) {
                    ctx->unk27C[j] = 0x40;
                    found = 1;
                }
            }
            sndPlaySfx(0x23, 0, 0x80, 0x7F);
            if (found) {
                sendSpuCommand(0x10);
            }
            *step = 4;
        }
        break;
    case 4:
        sum = 0;
        for (k = 0; k < 3; k++) {
            sum += ctx->unk24C[k];
        }
        func_80032534();
        if (pressed & PADRdown) {
            sendSpuCommand(2);
            *step = 5;
        }
        if (sum == 0) {
            *step = 5;
        }
        break;
    case 5:
        sndCmd21(0x23, 0);
        func_80032688();
        *step = 6;
        break;
    case 6:
        if (ctx->unk280 != 7) {
            sndCmdF1();
        }
        if (pressed & PADRdown) {
            sendSpuCommand(2);
            *step = 7;
        }
        break;
    case 7:
        *step = 8;
        break;
    case 8:
        ctx->unk2A -= 0x100;
        for (m = 0; m < 3; m++) {
            if (ctx->unk27C[m] != 0) {
                ctx->unk27C[m] -= 8;
            }
        }
        if (ctx->unk2A <= 0) {
            ctx->unk2A = 0;
            *step = 9;
        }
        break;
    case 9:
        ctx->unk38 = 1;
        ctx->unk39 = 0;
        ctx->unk26 += 0x100;
        if (ctx->unk3F != 0 || ctx->unk26C == 0) {
            ctx->title = getMenuString(0x15);
            ctx->prompt = getMenuString(0x16);
        } else {
            ctx->title = NULL;
            ctx->prompt = getMenuString(0x16);
        }
        if (ctx->unk26 >= 0x1000) {
            ctx->unk26 = 0x1000;
            state = *step = 0xA;
            goto dispatch;
        }
        ctx->unk2A = 0x1000;
        break;
    case 0xA:
        ctx->unk2A += 0x100;
        if (ctx->unk2A > 0x800) {
            if (ctx->unk39 == 0 && ctx->unk3F == 0) {
                ctx->unk2C = 0x200;
                ctx->unk44 = -1;
                ctx->unk48 = getMenuString(0x1C);
                ctx->unk4C = 0;
            }
            ctx->unk39 = 1;
        }
        if (ctx->unk2A >= 0x1000) {
            ctx->unk2A = 0x1000;
            if (ctx->unk3F != 0) {
                *step = 0xB;
                state = 0xB;
                goto dispatch;
            }
            ctx->unk2C = 0x1000;
            *step = 0x10;
        }
        break;
    case 0xB:
        if (pressed & PADRdown) {
            if (reward->id >= 0x100) {
                full = pollItemPresent(reward->id, reward->count);
            } else {
                full = addItemToInventory(reward->id, reward->count);
                func_800370AC(reward->id);
            }
            ctx->unk48 = getMenuString(full != 0 ? 0x18 : 6);
            ctx->unk44 = 0x258;
            ctx->unk3C = reward->id;
            sendSpuCommand(8);
            state = 0xC;
            goto dispatch;
        }
        break;
    case 0xC:
        state = 0xD;
        goto dispatch;
    case 0xD:
        if (count < 2) {
            ctx->unk2C = 0x200;
            state = 0x11;
            goto dispatch;
        }
        reward++;
        count--;
        state = 0xE;
        goto dispatch;
    case 0xE:
        ctx->unk28 = 0x100;
        ctx->unk2C = 0x200;
        *step = 0xF;
        break;
    case 0xF:
        slide = ctx->unk28;
        slide += 0x100;
        if (slide >= 0x1000) {
            slide = 0;
            *step = 0xB;
        }
        if (pressed & PADRdown) {
            ctx->unk28 = 0;
            state = 0xB;
            goto dispatch;
        }
        ctx->unk28 = slide;
        break;
    case 0x10:
        if (pressed & PADRdown) {
            ctx->unk44 = 0;
            sendSpuCommand(2);
            state = 0x11;
            goto dispatch;
        }
        break;
    case 0x11:
        ctx->unk46 = 0x20;
        *step = 0x12;
    case 0x12:
        ctx->unk46--;
        if (ctx->unk46 > 0) {
            break;
        }
        state = 0x13;
        goto dispatch;
    case 0x13:
        ctx->unk2A = -0x1000;
        *step = 0x14;
        break;
    case 0x14:
        ctx->unk2A += 0x100;
        if (ctx->unk2A >= 0) {
            ctx->unk2A = 0;
            *step = 0x15;
        }
        break;
    case 0x15:
        ctx->unk26 = -0x1000;
        *step = 0x16;
        break;
    case 0x16:
        ctx->unk26 += 0x100;
        if (ctx->unk26 >= 0) {
            ctx->unk26 = 0;
            *step = 0x17;
        }
        if (ctx->unk26C == 0 && ctx->unk42 == 0) {
            ctx->unk2E = ctx->unk26;
        }
        break;
    case 0x17:
        *step = 0x18;
        break;
    case 0x18:
        if (ctx->unk26C == 0 && ctx->unk42 == 0) {
            D_80083929 = 1;
            break;
        }
        state = 0x19;
        goto dispatch;
    case 0x19:
        sendSpuCommand(8);
        ctx->title = getMenuString(0x6F);
        ctx->unk38 = 2;
        if (ctx->unk42 == 0) {
            state = 0x1C;
            goto dispatch;
        }
        ctx->unk2A = -0x1000;
        *step = 0x1A;
    case 0x1A:
        if (pressed & PADRdown) {
            sendSpuCommand(2);
            *step = 0x1B;
        }
        break;
    case 0x1B:
        ctx->unk2A += 0x100;
        if (ctx->unk26C == 0) {
            ctx->unk2E = ctx->unk2A;
            if (ctx->unk2E == 0) {
                D_80083929 = 1;
            }
        }
        if (ctx->unk2A < 0) {
            break;
        }
        state = 0x1C;
        goto dispatch;
    case 0x1C:
        ctx->prompt = getMenuString(0x16);
        ctx->unk38 = 3;
        ctx->unk26D = 0;
        *step = 0x1D;
        break;
    case 0x1D:
        ctx->unk2A = -0x1000;
        gf = findSetBit(ctx->unk25A, ctx->unk26D);
        ctx->unk271 = 0;
        if ((ctx->unk258 >> gf) & 1) {
            ctx->unk271 = GF_WINDOW_LEVEL_UP;
        }
        if (ctx->unk25C[gf] != 0xFF) {
            ctx->unk26F = ctx->unk25C[gf];
            ctx->unk271 |= GF_WINDOW_LEARNED;
        }
        sendSpuCommand(9);
        ctx->unk270 = gf;
        ctx->unk26D++;
        *step = 0x1E;
        playSoundEffect(0x10);
        break;
    case 0x1E:
        if (pressed & PADRdown) {
            sendSpuCommand(2);
            *step = 0x1F;
        }
        break;
    case 0x1F:
        ctx->unk2A += 0x100;
        if (ctx->unk2A >= 0) {
            ctx->unk2A = 0;
            *step = 0x1D;
        }
        if (ctx->unk26D >= ctx->unk26C) {
            ctx->unk2E = ctx->unk2A;
            if (ctx->unk2E == 0) {
                D_80083929 = 1;
            }
        }
        break;
    case 0x20:
    case 0x21:
        break;
    }
    ctx->reward = reward;
    ctx->unk3E = count;
}


