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

/* --- Private functions --- */

static void stepExpRows(void);
static void finishExpRows(void);
static s32 addRewardCard(s32 id, s32 count);

/**
 * @brief Write a reward's name into @p buf.
 *
 * An item's name, or for a card (@p id has REWARD_CARD set) menu string 0x6E
 * with its 0x0A code replaced by the card's name (func_80023A54).
 *
 * @param id Reward id: an item, or a card when REWARD_CARD is set.
 * @param buf Buffer that receives the name.
 */
void formatRewardName(s32 id, u8 *buf)
{
    u8 *src;
    u8 *dst;
    s32 cond;
    u8 c;

    dst = buf;
    cond = id & REWARD_CARD;

    if (cond) {
        src = getMenuString(0x6E);

        while (1) {
            c = *src;
            src++;

            if (c == 0xA) {
                src++;
                {
                    u8 *src2 = func_80023A54(id & 0xFF);
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
        copyString(dst, getItemName(id));
    }
}


/**
 * @brief Copy a message into @p dst, expanding its 0x0A codes.
 *
 * Each 0x0A byte is followed by a code byte naming what to insert: 0x25 the
 * name of ability @p ability, 0x26 the name of magic @p magic, 0x23 the name
 * of reward @p reward (formatRewardName), 0x20 @p number in decimal without
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
void formatMessage(u8 *msg, u8 *dst, s32 reward, s32 arg3, s32 ability, s32 number, s32 magic) {
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
            formatRewardName(reward, p);
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
 * @brief Count each EXP row up by one step.
 *
 * For each row with a name, moves @c expStep of @c expAcquired into
 * @c currentExp; when no more than a step is left, moves all of it and clears
 * the step. A row whose current EXP reaches evalEntityXpCurve for its
 * @c level levels up, with the "LEVEL UP!" popup (@c levelUpTimer = 0x20), up
 * to level 100. @c nextLevelExp is then the EXP still needed for the next
 * level; at level 100 the current EXP is held at level 99's curve value and
 * nothing is left to add. Plays sound 9 if any row levelled up.
 */
static void stepExpRows(void) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 levelUp;
    s32 i;
    u32 exp;
    u32 next;

    levelUp = 0;
    for (i = 0; i < 3; i++) {
        if (ctx->names[i] != NULL) {
            exp = ctx->expAcquired[i];
            if (ctx->expStep[i] < exp) {
                exp = ctx->expStep[i];
            } else {
                ctx->expStep[i] = 0;
            }
            ctx->currentExp[i] += exp;
            ctx->expAcquired[i] -= exp;
            next = evalEntityXpCurve(i, ctx->level[i]);
            if (ctx->currentExp[i] >= next) {
                if (ctx->level[i] < 100) {
                    levelUp = 1;
                    ctx->level[i]++;
                    ctx->levelUpTimer[i] = 0x20;
                    next = evalEntityXpCurve(i, ctx->level[i]);
                }
            }
            if (ctx->level[i] < 100) {
                ctx->nextLevelExp[i] = next - ctx->currentExp[i];
            } else {
                ctx->currentExp[i] = evalEntityXpCurve(i, 99);
                ctx->nextLevelExp[i] = 0;
                ctx->expAcquired[i] = 0;
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
 * Sets each named row's step (@c expStep) to all the EXP still to add and runs
 * one count-up step (stepExpRows), so level-ups and their popups come out as
 * if counted. Then adds the row's battle EXP (@c unk574 + @c unk57A of
 * g_battleChars) to the party member (func_8002257C) and takes the row's level,
 * current EXP and EXP to the next level from the result, capped at level 100
 * as in stepExpRows.
 * @note @c ch starts as NULL although every use sets it first: with a value
 *       from before the loop, &g_battleChars stays in its own register when
 *       the loop sets up its pointers, as in the binary.
 */
static void finishExpRows(void) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
    s32 i;
    s32 level;
    u32 next;
    u32 exp;
    BattleCharData *ch = NULL;

    for (i = 0; i < 3; i++) {
        if (ctx->names[i] != NULL) {
            ctx->expStep[i] = ctx->expAcquired[i];
        }
    }
    stepExpRows();
    for (i = 0; i < 3; i++) {
        if (ctx->names[i] != NULL) {
            ch = &g_battleChars.chars[i];
            level = func_8002257C(i, g_battleChars.unk574[i] + g_battleChars.unk57A[i]);
            next = evalEntityXpCurve(i, level);
            ctx->level[i] = level;
            exp = ch->exp;
            ctx->expAcquired[i] = 0;
            ctx->expStep[i] = 0;
            ctx->nextLevelExp[i] = next - exp;
            ctx->currentExp[i] = exp;
            if (level == 100) {
                ctx->currentExp[i] = evalEntityXpCurve(i, 99);
                ctx->nextLevelExp[i] = 0;
                ctx->expAcquired[i] = 0;
                ctx->expStep[i] = 0;
            }
        }
    }
}


/**
 * @brief Add @p count copies of a card reward to the player's cards.
 *
 * Adds them one at a time (markItemPresent) and stops at the first that does
 * not fit.
 *
 * @param id Reward id of the card; only the low byte, the card id, is used.
 * @param count Copies to add.
 * @return 0 when all were added, else markItemPresent's nonzero result
 *         (updateResults then shows "over 100 was discarded").
 */
static s32 addRewardCard(s32 id, s32 count) {
    s32 result = 0;
    id &= 0xFF;
    while (count > 0) {
        result = markItemPresent(id);
        if (result != 0) {
            break;
        }
        count--;
    }
    return result;
}


/**
 * @brief Return a reward's description: the item's (getItemDesc), or NULL for
 * a card (@p id has REWARD_CARD set).
 *
 * @param id Reward id: an item, or a card when REWARD_CARD is set.
 * @return The item's description, or NULL for a card.
 */
u8 *getRewardDesc(s32 id) {
    if (id & REWARD_CARD) return NULL;
    return getItemDesc(id);
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
 * page and count the EXP up (stepExpRows), or finish it on the confirm button
 * (finishExpRows); close it and open the item page, then hand out each reward
 * in turn, items into the inventory and cards into the card list, with
 * "Couldn't find any items!" when there are none; show "GF received N AP!";
 * show each GF's level-up and learned-ability windows in turn; finally set
 * D_80083929.
 */
void updateResults(void) {
    ResultsScreen *ctx = (ResultsScreen *)getScratchAddr(0);
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
    if (ctx->messageProgress != 0) {
        ctx->messageProgress += 0x200;
        if (ctx->messageProgress > 0x1000) {
            ctx->messageProgress = 0x1000;
        }
    }
    for (i = 0; i < 3; i++) {
        if (ctx->levelUpTimer[i] != 0) {
            ctx->levelUpTimer[i]--;
        }
    }
    step = &ctx->step;
    reward = ctx->reward;
    count = ctx->rewardsLeft;
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
        ctx->page = 0;
        *step = 2;
        break;
    case 2:
        ctx->pageProgress += 0x100;
        ctx->titleProgress += 0x100;
        if (ctx->titleProgress >= 0x1000) {
            ctx->titleProgress = 0x1000;
        }
        if (ctx->pageProgress >= 0x1000) {
            ctx->pageProgress = 0x1000;
            *step = 3;
        }
        break;
    case 3:
        if (pressed & PADRdown) {
            found = 0;
            for (j = 0; j < 3; j++) {
                if (ctx->noExpPopup[j] != 0) {
                    ctx->noExpPopup[j] = 0x40;
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
            sum += ctx->expStep[k];
        }
        stepExpRows();
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
        finishExpRows();
        *step = 6;
        break;
    case 6:
        if (ctx->noExpRows != 7) {
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
        ctx->pageProgress -= 0x100;
        for (m = 0; m < 3; m++) {
            if (ctx->noExpPopup[m] != 0) {
                ctx->noExpPopup[m] -= 8;
            }
        }
        if (ctx->pageProgress <= 0) {
            ctx->pageProgress = 0;
            *step = 9;
        }
        break;
    case 9:
        ctx->page = 1;
        ctx->unk39 = 0;
        ctx->descProgress += 0x100;
        if (ctx->rewardCount != 0 || ctx->gfWindowCount == 0) {
            ctx->title = getMenuString(0x15);
            ctx->prompt = getMenuString(0x16);
        } else {
            ctx->title = NULL;
            ctx->prompt = getMenuString(0x16);
        }
        if (ctx->descProgress >= 0x1000) {
            ctx->descProgress = 0x1000;
            state = *step = 0xA;
            goto dispatch;
        }
        ctx->pageProgress = 0x1000;
        break;
    case 0xA:
        ctx->pageProgress += 0x100;
        if (ctx->pageProgress > 0x800) {
            if (ctx->unk39 == 0 && ctx->rewardCount == 0) {
                ctx->messageProgress = 0x200;
                ctx->unk44 = -1;
                ctx->message = getMenuString(0x1C);
                ctx->unk4C = 0;
            }
            ctx->unk39 = 1;
        }
        if (ctx->pageProgress >= 0x1000) {
            ctx->pageProgress = 0x1000;
            if (ctx->rewardCount != 0) {
                *step = 0xB;
                state = 0xB;
                goto dispatch;
            }
            ctx->messageProgress = 0x1000;
            *step = 0x10;
        }
        break;
    case 0xB:
        if (pressed & PADRdown) {
            if (reward->id >= REWARD_CARD) {
                full = addRewardCard(reward->id, reward->count);
            } else {
                full = addItemToInventory(reward->id, reward->count);
                func_800370AC(reward->id);
            }
            ctx->message = getMenuString(full != 0 ? 0x18 : 6);
            ctx->unk44 = 0x258;
            ctx->messageReward = reward->id;
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
            ctx->messageProgress = 0x200;
            state = 0x11;
            goto dispatch;
        }
        reward++;
        count--;
        state = 0xE;
        goto dispatch;
    case 0xE:
        ctx->wipeProgress = 0x100;
        ctx->messageProgress = 0x200;
        *step = 0xF;
        break;
    case 0xF:
        slide = ctx->wipeProgress;
        slide += 0x100;
        if (slide >= 0x1000) {
            slide = 0;
            *step = 0xB;
        }
        if (pressed & PADRdown) {
            ctx->wipeProgress = 0;
            state = 0xB;
            goto dispatch;
        }
        ctx->wipeProgress = slide;
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
        ctx->closeDelay = 0x20;
        *step = 0x12;
        /* fall through */
    case 0x12:
        ctx->closeDelay--;
        if (ctx->closeDelay > 0) {
            break;
        }
        state = 0x13;
        goto dispatch;
    case 0x13:
        ctx->pageProgress = -0x1000;
        *step = 0x14;
        break;
    case 0x14:
        ctx->pageProgress += 0x100;
        if (ctx->pageProgress >= 0) {
            ctx->pageProgress = 0;
            *step = 0x15;
        }
        break;
    case 0x15:
        ctx->descProgress = -0x1000;
        *step = 0x16;
        break;
    case 0x16:
        ctx->descProgress += 0x100;
        if (ctx->descProgress >= 0) {
            ctx->descProgress = 0;
            *step = 0x17;
        }
        if (ctx->gfWindowCount == 0 && ctx->ap == 0) {
            ctx->titleProgress = ctx->descProgress;
        }
        break;
    case 0x17:
        *step = 0x18;
        break;
    case 0x18:
        if (ctx->gfWindowCount == 0 && ctx->ap == 0) {
            D_80083929 = 1;
            break;
        }
        state = 0x19;
        goto dispatch;
    case 0x19:
        sendSpuCommand(8);
        ctx->title = getMenuString(0x6F);
        ctx->page = 2;
        if (ctx->ap == 0) {
            state = 0x1C;
            goto dispatch;
        }
        ctx->pageProgress = -0x1000;
        *step = 0x1A;
        /* fall through */
    case 0x1A:
        if (pressed & PADRdown) {
            sendSpuCommand(2);
            *step = 0x1B;
        }
        break;
    case 0x1B:
        ctx->pageProgress += 0x100;
        if (ctx->gfWindowCount == 0) {
            ctx->titleProgress = ctx->pageProgress;
            if (ctx->titleProgress == 0) {
                D_80083929 = 1;
            }
        }
        if (ctx->pageProgress < 0) {
            break;
        }
        state = 0x1C;
        goto dispatch;
    case 0x1C:
        ctx->prompt = getMenuString(0x16);
        ctx->page = 3;
        ctx->gfWindowsShown = 0;
        *step = 0x1D;
        break;
    case 0x1D:
        ctx->pageProgress = -0x1000;
        gf = findSetBit(ctx->gfWindows, ctx->gfWindowsShown);
        ctx->gfWindowFlags = 0;
        if ((ctx->gfLevelUp >> gf) & 1) {
            ctx->gfWindowFlags = GF_WINDOW_LEVEL_UP;
        }
        if (ctx->gfLearned[gf] != 0xFF) {
            ctx->learnedAbility = ctx->gfLearned[gf];
            ctx->gfWindowFlags |= GF_WINDOW_LEARNED;
        }
        sendSpuCommand(9);
        ctx->windowGf = gf;
        ctx->gfWindowsShown++;
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
        ctx->pageProgress += 0x100;
        if (ctx->pageProgress >= 0) {
            ctx->pageProgress = 0;
            *step = 0x1D;
        }
        if (ctx->gfWindowsShown >= ctx->gfWindowCount) {
            ctx->titleProgress = ctx->pageProgress;
            if (ctx->titleProgress == 0) {
                D_80083929 = 1;
            }
        }
        break;
    case 0x20:
    case 0x21:
        /* Unused steps; they give the jump table its 34 entries, as in the binary. */
        break;
    }
    ctx->reward = reward;
    ctx->rewardsLeft = count;
}


