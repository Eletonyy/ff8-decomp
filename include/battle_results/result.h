#ifndef BATTLE_RESULTS_RESULT_H
#define BATTLE_RESULTS_RESULT_H

#include "common.h"
#include "psxsdk/libgpu.h"

/** Bits of @c ResultsScreen::gfWindowFlags: the GF windows to show. */
#define GF_WINDOW_LEVEL_UP 1 /**< "GF <name> LEVEL UP!" (drawGfLevelUpWindow). */
#define GF_WINDOW_LEARNED 2 /**< "GF <name> learned <ability>!" (drawGfLearnedWindow). */

/** One item reward: an item, or a card when @c id has REWARD_CARD set (see formatRewardName). */
typedef struct {
    u16 id;
    u8 count;
    u8 pad;
} ItemReward;

#define REWARD_CARD 0x100 /**< ItemReward::id bit: a card; the low byte is the card id. */

/**
 * @brief The battle results screen's state, kept in the scratchpad.
 *
 * startBattleResults sets it up, updateResults advances it a frame at a time
 * and the drawers read it. drawResultsFrame also stores the frame's draw
 * origin and full-screen draw area here for resetDrawArea and setDrawAreaRect.
 */
typedef struct {
    RECT rect; /**< 0x00: window rect the drawers take. */
    RECT animRect; /**< 0x08: @c rect scaled by the open/close animation. */
    u32 drawAreaTL; /**< 0x10: GP0(0xE3) "Drawing Area Top Left" command. */
    u32 drawAreaBR; /**< 0x14: GP0(0xE4) "Drawing Area Bottom Right" command. */
    s16 originX; /**< 0x18: VRAM x of the buffer being drawn. */
    s16 originY; /**< 0x1A: VRAM y of the buffer being drawn. */
    s32 thread; /**< 0x1C: the results thread, or 0 once it is closed. */
    u16 state; /**< 0x20: step of the results thread (stepResultsThread). */
    u16 step; /**< 0x22: step of the results screen's update (updateResults). */
    u8 pad24[0x2];
    s16 descProgress; /**< 0x26: progress of the item page's description and message windows; negative fades. */
    s16 wipeProgress; /**< 0x28: progress of the wipe from one reward to the next (drawTextWipe). */
    s16 pageProgress; /**< 0x2A: progress of the current page's main window; negative fades. */
    s16 messageProgress; /**< 0x2C: progress of the message window (drawMessageWindow). */
    s16 titleProgress; /**< 0x2E: progress of the title and button prompt windows; negative fades. */
    u8 *title; /**< 0x30: message for the title window, or NULL. */
    u8 *prompt; /**< 0x34: message for the button prompt window, or NULL. */
    u8 page; /**< 0x38: page drawResultsFrame draws: 0 EXP, 1 items, 2 GF AP, 3 GF windows. */
    u8 unk39; /**< 0x39: set once the item page is half open, where the no-items message starts. */
    u8 pad3A[0x2];
    u16 messageReward; /**< 0x3C: reward named in @c message. */
    u8 rewardsLeft; /**< 0x3E: rewards still to show. */
    u8 rewardCount; /**< 0x3F: number of rewards; 0 shows "Couldn't find any items!". */
    u8 unk40;
    s8 timer; /**< 0x41: frames left in the results thread's wait step. */
    u8 ap; /**< 0x42: AP the GFs received; 0 skips the "GF received N AP!" window. */
    u8 pad43;
    s16 unk44; /**< 0x44: nonzero while the message window is shown. */
    s16 closeDelay; /**< 0x46: frames before the item page closes. */
    u8 *message; /**< 0x48: message for the message window. */
    s32 unk4C; /**< 0x4C: cleared whenever @c message is set. */
    ItemReward *reward; /**< 0x50: the reward on display; [-1] is the previous one. */
    u8 *names[3]; /**< 0x54: name on each EXP row, or NULL for an empty row. */
    ItemReward rewards[32]; /**< 0x60: the battle's item and card drops, one entry per id. */
    u8 text[4][0x18]; /**< 0xE0: string buffers for the drawers. */
    u8 pad140[0xE8];
    u32 nextLevelExp[3]; /**< 0x228: each row's "Next LEVEL", the EXP still needed. */
    u32 currentExp[3]; /**< 0x234: each row's "Current EXP". */
    u32 expAcquired[3]; /**< 0x240: each row's "EXP Acquired" still to count into @c currentExp. */
    u32 expStep[3]; /**< 0x24C: EXP each row's count-up moves per step (stepExpRows). */
    u16 gfLevelUp; /**< 0x258: one bit per GF that levelled up. */
    u16 gfWindows; /**< 0x25A: one bit per GF with a window to show. */
    u8 gfLearned[16]; /**< 0x25C: ability each GF learned, or 0xFF. */
    u8 gfWindowCount; /**< 0x26C: number of GFs with windows. */
    u8 gfWindowsShown; /**< 0x26D: GFs whose windows have been shown. */
    u8 unk26E;
    u8 learnedAbility; /**< 0x26F: ability named in the learned window. */
    u8 windowGf; /**< 0x270: GF whose windows are shown. */
    u8 gfWindowFlags; /**< 0x271: GF_WINDOW_* bits, the windows to draw. */
    u8 level[3]; /**< 0x272: each row's level. */
    u8 levelUpTimer[3]; /**< 0x275: frames left of each row's "LEVEL UP!" popup; 0 hides it. */
    u8 nameColor[3]; /**< 0x278: text colour of each row's name. */
    u8 levelUpWidth; /**< 0x27B: width of the "LEVEL UP!" popup. */
    u8 noExpPopup[3]; /**< 0x27C: each row's "Didn't receive EXP" popup: 0x41 waiting, 1..0x40 open, 0 none. */
    u8 noExpWidth; /**< 0x27F: text width of the "Didn't receive EXP" popup. */
    u8 noExpRows; /**< 0x280: one bit per row that gets no EXP. */
} ResultsScreen;

extern u8 D_80083929; /**< Set by the update when the results screen is finished. */

void switchToResultsThread(void);
s32 isResultsThreadDone(void);
void startBattleResults(void);

#endif /* BATTLE_RESULTS_RESULT_H */
