#include "common.h"
#include "battle_results/result.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle_results/display.h"
#include "battle_results/draw.h"
#include "battle_results/update.h"
#include "ability.h"
#include "battle.h"
#include "card.h"
#include "character.h"
#include "dialog.h"
#include "game.h"
#include "gamestate.h"
#include "gf_anim.h"
#include "gf_curve.h"
#include "thread.h"

extern u8 D_80083928;

/** displayStatus bit: HP below a quarter of max (func_800A240C sets it with 0x200). */
#define STATUS_HP_CRITICAL 0x100

/* --- Private functions --- */

static void enableDisplayAndRender(void);
static void func_80035158(void);

/**
 * @brief Enable the display and execute a full rendering pass.
 *
 * Calls SetDispMask(1) to make the framebuffer visible, then calls
 * func_80034DBC (main rendering) followed by func_8003283C (scene submission).
 */
static void enableDisplayAndRender(void) {
    SetDispMask(1);
    func_80034DBC();
    func_8003283C();
}


/**
 * @brief Get the current value of the global flag D_80083928.
 * @return The flag value as an unsigned byte.
 */
u8 getRenderCompleteFlag(void) {
    return D_80083928;
}


/**
 * @brief Step the display shutdown, one step per call.
 *
 * Steps through @c state: blank the display, clear the results display
 * twice (the second time with func_8003283C), render until @c D_80083929
 * reads 1, wait two frames on @c timer, close @c thread if there is one
 * (setting @c D_80083928), then keep the display blanked.
 */
static void func_80035158(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    u16 *state = &ctx->state;
    s32 thread;

    switch (*state) {
    case 0:
        SetDispMask(0);
        *state = 1;
        break;
    case 1:
        clearResultsDisplay();
        *state = 2;
        break;
    case 2:
        clearResultsDisplay();
        func_8003283C();
        *state = 3;
        break;
    case 3:
        enableDisplayAndRender();
        if (D_80083929 == 1) {
            *state = 4;
        }
        break;
    case 4:
        ctx->timer = 2;
        *state = 5;
        break;
    case 5:
        if (--ctx->timer <= 0) {
            *state = 6;
        }
        break;
    case 6:
        thread = ctx->thread;
        if (thread != 0) {
            ctx->thread = 0;
            closeThreadSafe(thread);
            D_80083928 = 1;
            SetDispMask(0);
        }
        *state = 7;
        break;
    case 7:
        SetDispMask(0);
        break;
    }
}


/**
 * @brief Call switchThread with a value loaded from scratchpad memory.
 * @note Reads a 32-bit value from PS1 scratchpad address 0x1F80001C.
 */
void dispatchScratchpadThread(void) {
    switchThread(*(s32 *)0x1F80001C);
}


/**
 * @brief Main game loop — runs forever calling render and VSync handlers.
 *
 * Alternates between func_80035158 (render frame) and switchThread(0)
 * (VSync/update) indefinitely. Never returns.
 */
void mainGameLoop(void) {
    for (;;) {
        func_80035158();
        switchThread(0);
    }
}


/**
 * @brief Clear both VRAM framebuffers to black.
 *
 * Clears two 384x224 (0x180 x 0xE0) regions in VRAM: the first at (0,0)
 * and the second at (0x200,0). Each clear is followed by DrawSync(0) to
 * wait for completion.
 */
void clearFramebuffers(void) {
    RECT rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x180;
    rect.h = 0xE0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    rect.x = 0x200;
    rect.y = 0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}


/**
 * @brief Start the battle results screen.
 *
 * Sets up the results display and the scratchpad state, merges the item and
 * card drops into @c rewards, fills the three EXP rows and starts the results
 * thread (mainGameLoop). Then, for each GF it works out whether it levels up,
 * gives the battle's AP to the ability it is learning and, once that ability
 * is learned, picks the next one to learn.
 */
void func_80035360(void) {
    ColorRenderScratch *ctx = (ColorRenderScratch *)getScratchAddr(0);
    AbilityListEntry list[22];
    u8 *stack;
    s32 i;
    s32 j;
    s32 k;
    s32 m;
    s32 count;
    s32 id;
    s32 level;
    u32 prev;
    u32 next;
    s32 exp;
    s32 learning;
    s32 n;
    s32 found;
    s32 learned;
    s32 ability;
    s32 nextAbility;
    s32 status;
    u8 *card;
    GfLearnData *learnData;
    s32 *bits;
    splitStruct *drop;

    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    g_resultsDisplays[0] = (ResultsDisplay *)getResultsDisplayBase();
    g_resultsDisplays[1] = (ResultsDisplay *)(getResultsDisplayBase() + getResultsDisplaySize());
    clearFramebuffers();
    initResultsDisplays();
    g_resultsDisplay = g_resultsDisplays[0];
    flipResultsDisplay();
    ctx->state = 0;
    D_80083928 = 0;
    ctx->step = 0;
    D_80083929 = 0;
    ctx->unk28 = 0;
    ctx->unk26 = 0;
    ctx->unk2A = 0;
    for (i = 0; i < 32; i++) {
        ctx->rewards[i].id = 0;
        ctx->rewards[i].count = 0;
    }
    drop = g_battleChars.unk5E0;
    count = 0;
    for (i = 0; i < 24; i++, drop++) {
        if (drop->unk0 == 0) {
            break;
        }
        if (drop->unk1 == 0) {
            continue;
        }
        for (j = 0; j < 32; j++) {
            if (ctx->rewards[j].id == 0) {
                ctx->rewards[j].id = drop->unk0;
                ctx->rewards[j].count = drop->unk1;
                count++;
                break;
            }
            if (ctx->rewards[j].id == drop->unk0) {
                ctx->rewards[j].count += drop->unk1;
                break;
            }
        }
    }
    card = g_battleChars.gfEntries[0].unk0;
    for (i = 0; i < 8; i++) {
        id = *card++;
        if (id == 0xFF) {
            break;
        }
        id |= REWARD_CARD;
        for (m = 0; m < 32; m++) {
            if (ctx->rewards[m].id == 0) {
                count++;
                ctx->rewards[m].id = id;
                ctx->rewards[m].count = 1;
                break;
            }
            if (ctx->rewards[m].id == id) {
                ctx->rewards[m].count++;
                break;
            }
        }
    }
    ctx->reward = ctx->rewards;
    ctx->unk40 = 1;
    ctx->unk3E = count;
    ctx->unk3F = count;
    ctx->unk44 = 0;
    ctx->unk48 = NULL;
    ctx->unk4C = 0;
    ctx->unk46 = 0x20;
    ctx->unk2C = 0;
    ctx->unk2E = 0;
    ctx->unk38 = 0;
    for (i = 0; i < 3; i++) {
        ctx->unk275[i] = 0;
    }
    stack = getThreadStackTop();
    for (i = 0; i < 3; i++) {
        ctx->unk27C[i] = 0;
        if (g_battleChars.chars[i].characterId != 0xFF) {
            level = g_battleChars.chars[i].level;
            status = g_battleChars.chars[i].displayStatus;
            ctx->nameColor[i] = 7;
            if (status & STATUS_HP_CRITICAL) {
                ctx->nameColor[i] = 2;
            }
            if (status & STATUS_KO) {
                ctx->nameColor[i] = 1;
            }
            if (level == 100) {
                ctx->unk240[i] = 0;
                ctx->unk228[i] = 0;
                ctx->unk24C[i] = 0;
                ctx->unk234[i] = evalEntityXpCurve(i, level);
            } else {
                prev = evalEntityXpCurve(i, level - 1);
                next = evalEntityXpCurve(i, level);
                ctx->unk240[i] = g_battleChars.unk574[i] + g_battleChars.unk57A[i];
                ctx->unk228[i] = g_battleChars.chars[i].xpToNext;
                if (ctx->unk240[i] == 0) {
                    ctx->unk27C[i] = 0x41;
                    ctx->unk280 |= 1 << i;
                }
                next = (next - prev) >> 7;
                if (next == 0) {
                    next = 1;
                }
                ctx->unk24C[i] = next;
            }
            ctx->names[i] = getBattleCharName(i);
            ctx->unk234[i] = g_battleChars.chars[i].exp;
            ctx->unk272[i] = g_battleChars.chars[i].level;
        } else {
            ctx->names[i] = NULL;
            ctx->unk24C[i] = 0;
            ctx->unk280 |= 1 << i;
        }
    }
    ctx->thread = openThreadSafe(mainGameLoop, stack);
    ctx->timer = 6;
    ctx->title = NULL;
    ctx->prompt = NULL;
    ctx->unk26E = 0;
    setTextBrightness(0x1000);
    ctx->unk27B = getFirstLineWidth(getMenuString(0x30)) + 0x14;
    ctx->unk258 = 0;
    ctx->unk25A = 0;
    ctx->unk42 = 0;
    for (i = 0; i < GF_COUNT; i++) {
        ctx->unk25C[i] = 0xFF;
        if (g_gameState.gfs[i].exists & GF_EXISTS) {
            exp = g_battleChars.unk580[i] + g_battleChars.unk5A0[i];
            if (func_8002274C(i, 0) != func_8002274C(i, exp)) {
                ctx->unk258 |= 1 << i;
                ctx->unk25A |= 1 << i;
            }
            learning = g_gameState.gfs[i].learning;
            n = func_800369CC(i, list, 1);
            found = 0;
            if (g_battleChars.unk5C0[i] != 0) {
                for (m = 0; m < n; m++) {
                    if (learning == list[m].slotIndex && list[m].type == 1) {
                        ctx->unk42 = g_battleChars.unk5C0[i];
                        found = 1;
                    }
                }
            }
            /* Stand-in: the binary walks the list again but only the pointer step
               survives, so this loop's body was removed after loop optimisation.
               A test that no u8 passes is removed at the same point. */
            for (j = 0; j < n; j++) {
                if (list[j].type >> 8) {
                    found = 1;
                }
            }
            if (found) {
                if (AddAbilityExp(i, g_battleChars.unk5C0[i])) {
                    learned = 0;
                    for (j = 0; j < n; j++) {
                        if (learning == list[j].slotIndex && list[j].type == 1) {
                            bits = g_gameState.gfs[i].completeAbilities;
                            bits[learning / 32] |= 1 << (learning & 31);
                            ctx->unk25C[i] = learning;
                            ctx->unk25A |= 1 << i;
                            learned = 1;
                        }
                    }
                    if (learned) {
                        nextAbility = 0;
                        n = func_800369CC(i, list, 1);
                        learnData = &D_80079D78[i];
                        for (k = 0; k < 21; k++) {
                            ability = learnData->abilities[k].slot;
                            for (j = 0; j < n; j++) {
                                if (list[j].slotIndex == ability && list[j].type == 1) {
                                    nextAbility = ability;
                                    break;
                                }
                            }
                            if (nextAbility != 0) {
                                break;
                            }
                        }
                        g_gameState.gfs[i].learning = nextAbility;
                    }
                }
            }
        }
    }
    ctx->unk27F = getFirstLineWidth(getMenuString(0x31));
    ctx->unk26C = 0;
    for (i = 0; i < GF_COUNT; i++) {
        if ((ctx->unk25A >> i) & 1) {
            ctx->unk26C++;
        }
    }
    recalcPartyStats();
}
