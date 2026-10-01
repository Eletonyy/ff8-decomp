#ifndef BATTLE_RESULTS_RESULT_H
#define BATTLE_RESULTS_RESULT_H

#include "common.h"
#include "psxsdk/libgpu.h"

/** One item reward: an item, or a card when @c id has 0x100 set (see func_8003228C). */
typedef struct {
    u16 id;
    u8 count;
    u8 pad;
} ItemReward;

/**
 * @brief Scene render context staged in PS1 scratchpad each frame.
 *
 * Populated by @c func_80034DBC during scene setup. Holds packed GPU
 * GP0 command words that @c func_8003334C emits as a DR_AREA primitive
 * once per frame, and the VRAM origin @c func_8003346C offsets its draw
 * areas by. The window drawers take their rects from here.
 */
typedef struct {
    RECT rect; /**< 0x00: window rect the drawers take. */
    RECT animRect; /**< 0x08: @c rect scaled by the open/close animation. */
    u32 drawAreaTL;  /**< 0x10: GP0(0xE3) "Drawing Area Top Left" command. */
    u32 drawAreaBR;  /**< 0x14: GP0(0xE4) "Drawing Area Bottom Right" command. */
    s16 originX; /**< 0x18: VRAM x of the buffer being drawn. */
    s16 originY; /**< 0x1A: VRAM y of the buffer being drawn. */
    s32 thread; /**< 0x1C: thread to close when the display shuts down, or 0. */
    u16 state; /**< 0x20: step of the display shutdown (func_80035158). */
    u8 pad22[0x4];
    s16 unk26; /**< 0x26: progress of the popup and description windows; negative fades. */
    s16 unk28; /**< 0x28: slide progress of the two-pane divider (see func_800337FC). */
    s16 unk2A; /**< 0x2A: progress of the item reward window; negative fades. */
    s16 unk2C; /**< 0x2C: progress of the message window (see func_8003406C). */
    s16 unk2E; /**< 0x2E: progress of the title and button prompt windows; negative fades. */
    u8 *title; /**< 0x30: message for the title window, or NULL. */
    u8 *prompt; /**< 0x34: message for the button prompt window, or NULL. */
    u8 unk38; /**< 0x38: which window group func_80034DBC adds, 0..3. */
    u8 pad39[0x3];
    u16 unk3C; /**< 0x3C: argument formatted into @c unk48. */
    u8 pad3E;
    u8 unk3F; /**< 0x3F: nonzero while the item reward line is shown. */
    u8 pad40;
    s8 timer; /**< 0x41: frames left in the shutdown's wait step. */
    u8 unk42; /**< 0x42: value formatted into the message box (see func_80034C74). */
    u8 pad43;
    u16 unk44; /**< 0x44: nonzero while the message window is shown. */
    u8 pad46[0x2];
    u8 *unk48; /**< 0x48: message for the message window. */
    u8 pad4C[0x4];
    ItemReward *reward; /**< 0x50: the item reward on display; [-1] is the previous one. */
    u8 *names[3]; /**< 0x54: name on each results row, or NULL for an empty row. */
    u8 pad60[0x80];
    u8 text[4][0x18]; /**< 0xE0: string buffers for the drawers. */
    u8 pad140[0xE8];
    u32 unk228[3]; /**< 0x228: third number on each results row. */
    u32 unk234[3]; /**< 0x234: second number on each results row. */
    u32 unk240[3]; /**< 0x240: first number on each results row. */
    u32 unk24C[3]; /**< 0x24C: EXP each row's count-up moves per step (func_80032534). */
    u8 pad258[0x17];
    u8 unk26F; /**< 0x26F: ability id for func_800349F4's window. */
    u8 unk270; /**< 0x270: magic id for the magic windows. */
    u8 unk271; /**< 0x271: MAGIC_WINDOW_* bits, the windows to draw. */
    u8 unk272[3]; /**< 0x272: number after icon 0xE on each results row. */
    u8 unk275[3]; /**< 0x275: progress of each row's menu string 0x30 popup; 0 hides it. */
    u8 nameColor[3]; /**< 0x278: text colour of each row's name. */
    u8 unk27B; /**< 0x27B: width of the menu string 0x30 popup. */
    u8 unk27C[3]; /**< 0x27C: progress of each row's centred menu string 0x31 popup, 1..0x40. */
    u8 unk27F; /**< 0x27F: text width of the menu string 0x31 popup. */
} ColorRenderScratch;

void dispatchScratchpadThread(void);
u8 getRenderCompleteFlag(void);
void func_80035360(void);

#endif /* BATTLE_RESULTS_RESULT_H */
