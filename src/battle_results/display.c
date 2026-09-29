#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle_results/display.h"

/* --- Local type definitions --- */

/** @brief The size of the stack a second thread runs on. */
#define THREAD_STACK_SIZE 0x1800

/**
 * @brief One buffer of the battle results screen's display: its display and draw
 * environments, a 2-entry ordering table and its packet space.
 */
typedef struct {
    DISPENV disp; /* 0x00 */
    DRAWENV draw; /* 0x14 */
    u32 ot[2]; /* 0x70: 2-entry ordering table */
    void *pktAlloc; /* 0x78: current packet allocation pointer */
    u32 pktBase; /* 0x7C: packet buffer start */
} ResultsDisplay;

/* --- Externs (sorted by address) --- */

extern ResultsDisplay *g_resultsDisplay; /* 0x80083918: the buffer being drawn */
extern ResultsDisplay *g_resultsDisplays[]; /* 0x80083920: the pair */
extern u8 g_threadStack[THREAD_STACK_SIZE]; /* 0x80083938 */

/* --- Private functions --- */

static u8 *getThreadStackBase(void);

/**
 * @brief Return the stack pointer a second thread starts with: the last word of
 * g_threadStack. The battle's thread and the battle results screen's render
 * thread both start on it.
 */
u8 *getThreadStackTop(void) {
    return &g_threadStack[THREAD_STACK_SIZE - 4];
}

/** @brief Return the base of the second thread's stack. */
static u8 *getThreadStackBase(void) {
    return g_threadStack;
}

/**
 * @brief Spin until the GPU is idle.
 *
 * Polls IsIdleGPU with a count of 1 until it stops returning -1, i.e. until
 * the GPU is ready for commands.
 */
void waitGpuIdle(void) {
    while (IsIdleGPU(1) == -1) {
    }
}


/**
 * @brief Return where the results display's first buffer lives; the second
 * follows one getResultsDisplaySize() later.
 * @return 0x801F4000.
 */
u32 getResultsDisplayBase(void) {
    return 0x801F4000;
}


/**
 * @brief Return the size of one results display buffer, packet space included.
 * @return 0x4000 (16 KB).
 */
s32 getResultsDisplaySize(void) {
    return 0x4000;
}


/**
 * @brief Switch to the other results display buffer.
 *
 * Makes the buffer not being drawn the active one, clears its ordering table
 * and resets its packet cursor.
 */
void flipResultsDisplay(void) {
    ResultsDisplay *buf;
    ResultsDisplay *active;

    buf = g_resultsDisplays[0];
    if (g_resultsDisplay == buf) {
        buf = g_resultsDisplays[1];
    }
    g_resultsDisplay = buf;
    ClearOTag(buf->ot, 2);
    active = g_resultsDisplay;
    active->pktAlloc = &active->pktBase;
}


/**
 * @brief Put the results display's draw environment at the head of its ordering
 * table, then splice the table into the GPU's current drawing.
 *
 * BreakDraw stops the GPU's linked-list DMA and returns the address it stopped at,
 * or -1 while it cannot; it is tried up to 500 times. ContinueDraw then inserts the
 * table and continues from that address.
 */
void submitResultsDisplay(void) {
    ResultsDisplay *buf;
    DR_ENV *env;
    u32 *ot;
    u32 *next;
    s32 i;

    buf = g_resultsDisplay;
    env = buf->pktAlloc;
    ot = buf->ot;
    SetDrawEnv(env, &buf->draw);
    addPrimFast(ot, env, s2);
    env++;
    g_resultsDisplay->pktAlloc = env;
    for (i = 0; i < 500; i++) {
        next = BreakDraw();
        if (next != (u32 *)-1) {
            break;
        }
    }
    if (next != (u32 *)-1) {
        ContinueDraw(ot, next);
    }
}


/**
 * @brief Set up the results display's two buffers for 384x224 double buffering
 * at VRAM x=0 and x=512, each buffer drawing where the other displays, clearing
 * to black.
 *
 * The screen is moved down 8 lines, and 24 more when GetVideoMode reports PAL.
 */
void initResultsDisplays(void) {
    s32 i;

    SetDefDispEnv(&g_resultsDisplays[0]->disp, 0, 0, 384, 224);
    SetDefDispEnv(&g_resultsDisplays[1]->disp, 512, 0, 384, 224);
    SetDefDrawEnv(&g_resultsDisplays[1]->draw, 0, 0, 384, 224);
    SetDefDrawEnv(&g_resultsDisplays[0]->draw, 512, 0, 384, 224);
    for (i = 0; i < 2; i++) {
        g_resultsDisplays[i]->draw.isbg = 1;
        setRGB0(&g_resultsDisplays[i]->draw, 0, 0, 0);
        g_resultsDisplays[i]->disp.screen.y += 8;
        g_resultsDisplays[i]->disp.screen.h = 224;
        if (GetVideoMode() == MODE_PAL) {
            g_resultsDisplays[i]->disp.screen.y += 24;
        }
    }
}


/**
 * @brief Show a black frame on the results display: flip to the other buffer,
 * fill it with a black 384x224 tile and submit it.
 */
void clearResultsDisplay(void) {
    ResultsDisplay *buf;
    TILE *tile;
    u32 *ot;

    flipResultsDisplay();
    buf = g_resultsDisplay;
    tile = buf->pktAlloc;
    ot = buf->ot; /* read before the tile is filled, the retail load order */
    setTile(tile);
    setRGB0(tile, 0, 0, 0);
    setXY0(tile, 0, 0);
    setWH(tile, 384, 224);
    addPrimFast(ot, tile, s0);
    g_resultsDisplay->pktAlloc = tile + 1;
    submitResultsDisplay();
}
