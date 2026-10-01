#include "common.h"
#include "battle_results/result.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle_results/display.h"
#include "battle_results/draw.h"
#include "battle_results/update.h"

extern u8 D_80083928;
extern u8 D_80083929;

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


INCLUDE_ASM("asm/nonmatchings/battle_results/result", func_80035360);
