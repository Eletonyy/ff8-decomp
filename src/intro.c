#include "common.h"
#include "battle.h"
#include "field.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "psxsdk/libcd.h"

extern u32 D_800990B8[];
extern s32 D_8009928C;
extern s32 D_80099290;
extern s32 D_80099294;
extern s32 D_80099298;
extern s32 D_8009929C;
extern s32 D_800992A0;
extern s32 D_800992A4;
extern s32 D_800992A8;
extern s32 D_800992AC;
extern s32 D_800992B0;
extern SeedState *g_seedState;

/** @brief Display-init double-buffer context (184 bytes at 0x800991D8). */
typedef struct {
    u32      ot[2];        /**< 0x00: double-buffered single-entry OT. */
    DRAWENV  draw;         /**< 0x08: 92-byte drawing environment. */
    DISPENV  disp;         /**< 0x64: 20-byte display environment. */
    TILE     tiles[2];     /**< 0x78: per-buffer fade overlay rectangles. */
    DR_TPAGE drTPages[2];  /**< 0x98: per-buffer SetDrawTPage commands. */
    s32      currBuf;      /**< 0xA8: active buffer index (0 or 1). */
    s32      unkAC;
    u8      unkB0;         /**< 0xB0: flag byte. */
    u8      unkB1;
    u8      pad[6];
} DispCtx;

extern DispCtx D_800991D8;

extern void sndCmdF0(void);
extern void sndCmdF1(void);
extern s16  sndCmd10(u32 bank);
extern void sndCmdC0(s32 a, s32 b);
extern void sndCmdC1(s32 id, s32 b, s32 c);
extern s32  sndGetStatus(void);
extern u32  toggleSoundBank(void);
extern s32  func_8004D174(void);
extern s32  func_8004D208(s32 a);
extern void func_8009818C(void);
extern void func_80098000(void);
extern void func_80098378(s32 a, s32 b, s32 c, s32 d, s32 e);
extern void func_800985EC(void);
extern void func_800275D4();
extern void func_80037FB0(s32 a, s32 b, s32 c);
extern void func_800393C8(void);
extern s32  getAnimFrameParam(s32 slot, s32 sub);

/**
 * @brief Display-init overlay entry point — set up display + run 60-frame intro.
 *
 * Boot-time setup for the intro sequence (Squaresoft logo / opening movie
 * preface) before the main menu. Steps:
 *  - Ring the sound side (@c sndCmdF0/F1) and wait for the sound CPU to
 *    drain its pending command queue (@c func_8004D174 / @c func_8004D208).
 *  - Disable the display, sync the CD module to a known idle state, and
 *    issue @c CdControlB(0xE, ...) — CD command 0xE is @c CdlSetmode with
 *    param @c 0x80 (auto-pause + double-speed reporting).
 *  - Set up a 640x480 interlaced display: clear VRAM, build the
 *    @c DRAWENV / @c DISPENV in @c D_800991D8, mark dfe on the draw env
 *    and @c isinter on the disp env, install both with @c PutDrawEnv /
 *    @c PutDispEnv. Clear the active OT entry.
 *  - Run the per-frame intro tick @c func_8009818C 60 times (one second
 *    at 60 Hz).
 */
void func_80098000(void) {
    RECT clearRect;
    s32 status;
    s32 i;

    sndCmdF0();
    sndCmdF1();

    while ((status = func_8004D174()) == -1) {
        VSync(0);
    }
    if (status != 0) {
        while (func_8004D208(1) != 0) {}
    }

    SetDispMask(0);
    CdSync(0, 0);
    CdControlB(0xE, (u8 *)0x80, 0);
    VSync(4);

    clearRect.x = 0;
    clearRect.y = 0;
    clearRect.w = 0x280;
    clearRect.h = 0x1E0;
    ClearImage(&clearRect, 0, 0, 0);
    DrawSync(0);

    SetDefDispEnv(&D_800991D8.disp, 0, 0, 0x280, 0x1E0);
    SetDefDrawEnv(&D_800991D8.draw, 0, 0, 0x280, 0x1E0);

    D_800991D8.disp.isinter = 1;
    D_800991D8.draw.dfe = 1;
    PutDispEnv(&D_800991D8.disp);
    PutDrawEnv(&D_800991D8.draw);
    D_800991D8.currBuf = 0;
    D_800991D8.unkB0 = 0;
    D_800991D8.unkB1 = 0;
    D_80099294 = 0;
    VSync(0);
    D_8009928C = ((u32)GetODE() < 1);
    ClearOTagR(&D_800991D8.ot[D_800991D8.currBuf], 1);
    for (i = 0; i < 60; i++) {
        func_8009818C();
    }
}

/**
 * @brief One frame of the intro display loop.
 *
 * Called 60 times in a row from @c func_80098000. Per-frame steps:
 *  - @c DrawSync(0) + @c VSync(0): wait for GPU idle and vblank.
 *  - Spin until @c GetODE flips: the busy-wait re-arms the previous
 *    drawing pass.
 *  - If @c D_80099294 is set, dispatch on @c D_80099290 to one of the
 *    intro-stage renderers:
 *      - state 0: @c func_80098378 with a full-width 640x400 mode.
 *      - state 1: @c func_80098378 with the cropped 580x406 layout.
 *      - state 2: @c func_800985EC (VRAM scanline copy).
 *  - Submit the current buffer's OT via @c DrawOTag, flip @c currBuf,
 *    install the next frame's @c DRAWENV / @c DISPENV pair, and clear
 *    the newly-active OT for the next pass.
 *  - Sample the controllers (@c getAnimFrameParam slots 0 and 1) and
 *    update the edge-detect mirrors at @c D_800992A8 (slot 0 rising
 *    edges) and @c D_800992AC (slot 1 rising edges) using the
 *    @c D_80099298 / @c D_8009929C complements latched last frame.
 */
void func_8009818C(void) {
    DrawSync(0);
    VSync(0);

    while (D_8009928C == ((u32)GetODE() < 1)) {}

    D_8009928C = ((u32)GetODE() < 1);

    if (D_80099294 != 0) {
        switch (D_80099290) {
        case 0:
            func_80098378(0, 0, 0x28, 0x280, 0x190);
            break;
        case 1:
            func_80098378(1, 0x1E, 0x26, 0x244, 0x196);
            break;
        case 2:
            func_800985EC();
            break;
        }
    }

    DrawOTag(&D_800991D8.ot[D_800991D8.currBuf]);
    D_800991D8.currBuf = D_800991D8.currBuf ^ 1;
    PutDispEnv(&D_800991D8.disp);
    PutDrawEnv(&D_800991D8.draw);
    ClearOTagR(&D_800991D8.ot[D_800991D8.currBuf], 1);

    func_800275D4();
    D_800992A0 = getAnimFrameParam(0, 0);
    D_800992A4 = getAnimFrameParam(1, 0);

    D_800992A8 = D_80099298 & D_800992A0;
    D_800992AC = D_8009929C & D_800992A4;
    D_8009929C = ~D_800992A4;
    D_80099298 = ~D_800992A0;
}

/**
 * @brief Kick off an asynchronous CD load for an intro asset.
 *
 * @c D_800990B8 is a table of (sector, size) pairs (8 bytes per entry,
 * laid out as @c u32 sector then @c u32 size). For each story-stage
 * index used by the intro sequence (e.g. @c 0x23 Squaresoft logo,
 * @c 4 FF8 logo, @c 5..0x1F SeeD application text pages, @c 0x20 "The
 * End", @c 0x21 final fade), the entry is fetched and queued via
 * @c cdReadAsyncSync into VRAM-adjacent main RAM at @c 0x80100000.
 *
 * @param stage Asset table index — selects which (sector, size) pair
 *              from @c D_800990B8.
 */
void func_80098338(s32 stage) {
    /* Each table entry is 2 u32s (sector, size); index by stage * 2. */
    cdReadAsyncSync(D_800990B8[stage * 2],
                    D_800990B8[stage * 2 + 1],
                    (s32)0x80100000, 0);
}

/**
 * @brief Stream a sub-rectangle of decoded video into VRAM scanline-by-scanline.
 *
 * Generalized version of @c func_800985EC. Pulls raw 16-bpp pixel data from
 * the staged buffer at @c 0x80100008 (the @c +8 skips a small header in the
 * decoded asset). Each call uploads @p height rows of @p width pixels into
 * VRAM starting at @c (x, (u16)D_8009928C + y), advancing by 2 lines per
 * @c LoadImage call (interlaced — odd lines belong to the other field and
 * are filled in by the alternating frame buffer; @c D_8009928C provides
 * the parity offset).
 *
 * Mode 0 (called by @c func_8009818C): full 640x400 area at (0, 0x28).
 * Mode 1: cropped 580x406 layout at (0x1E, 0x26).
 *
 * @param mode    Stage layout selector (passed by caller; not read here).
 * @param x       Destination VRAM x.
 * @param y       Vertical offset added to @c D_8009928C for the first line.
 * @param width   Pixels per row (also row stride / 4 in the source buffer).
 * @param height  Total vertical extent; loop runs while @c y < y + height.
 */
void func_80098378(s32 mode, s32 x, s32 y, s32 width, s32 height) {
    s16 rect[4];
    s32 val;
    s16 curY;
    u8 *src;

    val = D_8009928C * 2;
    rect[0] = x;
    rect[2] = width;
    rect[3] = 1;
    curY = (u16)D_8009928C + y;
    rect[1] = curY;
    src = (u8 *)(((u32)(width * val) >> 2 << 2) + (s32)0x80100008);

    if ((s16)curY < y + height) {
        do {
            LoadImage((RECT *)rect, (u32 *)src);
            curY = rect[1] + 2;
            rect[1] = curY;
            src += width * 4;
        } while ((s16)curY < y + height);
    }
}

/**
 * @brief Queue a semi-transparent fade overlay onto the active buffer's OT.
 *
 * Builds two primitives per call into @c D_800991D8's per-buffer slots and
 * adds them to the active buffer's OT:
 *  - A @c TILE primitive at @c tiles[currBuf]: a flat-shaded rectangle
 *    covering @p rect, filled with grey @p brightness on all channels
 *    (r0=g0=b0=brightness), with semi-transparency enabled. This is what
 *    draws the fade — the GPU's semi-trans mode (set via @c mode) blends
 *    it with the underlying scene.
 *  - A @c DR_TPAGE primitive at @c drTPages[currBuf]: switches the GPU
 *    semi-trans operator to @p mode (1 = add, 2 = subtract, etc.).
 *
 * @param brightness Grey level (0..255) applied to all three colour channels.
 * @param mode       Semi-trans blend mode forwarded to @c GetTPage.
 * @param rect       Destination rectangle covered by the fade tile.
 */
void func_80098440(s32 brightness, s32 mode, RECT *rect) {
    DispCtx  *dc = &D_800991D8;
    TILE     *tiles  = dc->tiles;
    DR_TPAGE *tpages;

    SetTile(&tiles[dc->currBuf]);
    SetSemiTrans(&tiles[dc->currBuf], 1);
    tiles[dc->currBuf].r0 = brightness;
    tiles[dc->currBuf].g0 = brightness;
    tiles[dc->currBuf].b0 = brightness;
    tiles[dc->currBuf].x0 = rect->x;
    tiles[dc->currBuf].y0 = rect->y;
    tiles[dc->currBuf].w  = rect->w;
    tiles[dc->currBuf].h  = rect->h;
    AddPrim(&dc->ot[dc->currBuf], &tiles[dc->currBuf]);

    tpages = dc->drTPages;
    SetDrawTPage(&tpages[dc->currBuf], 0, 0, GetTPage(0, mode, 0, 0));
    AddPrim(&dc->ot[dc->currBuf], &tpages[dc->currBuf]);
}

/**
 * @brief Load and play a specific music track for the display init overlay.
 *
 * Reads CD file entry at offset 0x110 (entry 34) from D_800990B8 and
 * calls cdReadAsyncSync to load it to address 0x8017D000.
 */
void func_800985B4(void) {
    u32 *base = D_800990B8;

    cdReadAsyncSync(*(u32 *)((u8 *)base + 0x110),
                  *(u32 *)((u8 *)base + 0x114),
                  (s32)0x8017D000, 0);
}

/**
 * @brief Transfer display rows from VRAM buffer at 0x8017D008.
 *
 * Computes a VRAM source offset using D_8009928C and copies scanlines
 * starting at y = (u16)D_8009928C + 0x26, incrementing by 2 each iteration,
 * until y >= 0x1BC.
 */
void func_800985EC(void) {
    s16 rect[4];
    s32 val;
    s16 y;
    u8 *ptr;

    val = D_8009928C;
    rect[0] = 0x1E;
    rect[2] = 0x244;
    rect[3] = 1;
    y = (u16)D_8009928C + 0x26;
    rect[1] = y;
    ptr = (u8 *)((s32)0x8017D008 + val * 0x488);

    if ((s16)y < 0x1BC) {
        do {
            LoadImage(&rect, ptr);
            y = rect[1] + 2;
            rect[1] = y;
            ptr += 0x910;
        } while ((s16)y < 0x1BC);
    }
}

/**
 * @brief Perform a display fade-out and fade-in transition.
 *
 * Fades display brightness from 255 down to 0 (by 4 each step),
 * waits 300 frames, then fades back up from 0 to 255. Updates
 * D_80099290 state flag and D_8009928C/D_80099294 during the transition.
 */
void func_8009869C(void) {
    s16 rect[4];
    s32 brightness;
    s32 i;

    D_80099290 = 2;
    VSync(0);

    i = GetODE();
    brightness = 0xFF;
    D_8009928C = (u32)i < 1;

    rect[0] = 0x1E;
    rect[1] = 0x26;
    rect[2] = 0x244;
    rect[3] = 0x196;

    /* Fade out: brightness 255 -> 0 by 4 */
    do {
        func_80098440(brightness, 2, &rect);
        brightness -= 4;
        func_8009818C();
        D_80099294 = 1;
        SetDispMask(1);
    } while (brightness >= 0);

    /* Wait 300 frames */
    i = 0;
    do {
        i++;
        func_8009818C();
    } while (i < 0x12C);

    /* Fade in: brightness 0 -> 255 by 4 */
    i = 0;
    do {
        func_80098440(i, 2, &rect);
        i += 4;
        func_8009818C();
    } while (i < 0xFF);

    SetDispMask(0);
    D_80099290 = 1;
}

INCLUDE_ASM("asm/ovl/intro/nonmatchings/intro", func_8009879C);

/**
 * @brief Squaresoft / FF8 startup intro sequence.
 *
 * Plays the boot-time intro shown when no save is loaded — Squaresoft logo,
 * FF8 logo, then the SeeD application story text crawl (stages 5-0x1F, 27
 * pages of text rendered by @c func_80098338), followed by "The End"
 * (stage 0x20) and a final fade. Any button press on controller 1
 * (@c D_800992A8 high nibble) skips to the cleanup tail.
 *
 * Each segment fades in / holds / fades out via @c func_80098440 (the
 * fade primitive — modes 1 and 2 control how brightness is applied) over
 * a 640x400 (rectY=0x28, rectW=0x280, rectH=0x190) draw area. The per-frame
 * tick @c func_8009818C drives display flipping and input sampling.
 *
 * SeeD sound loading: waits for @c g_seedState->soundLoadComplete before
 * starting music (@c sndCmd10(toggleSoundBank()), @c sndCmdC0(0, 0x7F)).
 * On exit, @c sndCmdC1(D_8005F11C, 0x60, 0) fades the music out and a
 * final brightness ramp restores the display to full before returning.
 */
void func_80098974(void) {
    RECT rect;
    s32 outerCount;
    s32 stage;
    s32 brightness;
    s32 holdFrames;
    s32 rectY, rectW, rectH;
    s32 frame;
    s32 one;

    func_80098000();
    rectY = 0x28;
    rectW = 0x280;
    rectH = 0x190;

    while (1) {
        func_80098338(0x23);
        rect.x = 0; rect.y = rectY; rect.w = rectW; rect.h = rectH;

        for (brightness = 0xFF, one = 1; brightness >= 0; brightness -= 4) {
            func_80098440(brightness, 2, &rect);
            func_8009818C();
            D_80099294 = one;
            SetDispMask(1);
        }

        for (frame = 0; frame < 0x78; frame++) {
            func_8009818C();
        }

        for (frame = 0; frame < 0x78; frame++) {
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        for (brightness = 0; brightness < 0xFF; brightness += 4) {
            func_80098440(brightness, 2, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        func_80098338(4);
        rect.x = 0; rect.y = rectY; rect.w = rectW; rect.h = rectH;

        for (brightness = 0xFF, one = 1; brightness >= 0; brightness -= 4) {
            func_80098440(brightness, 2, &rect);
            func_8009818C();
            D_80099294 = one;
            SetDispMask(1);
        }

        for (frame = 0; frame < 0x78; frame++) {
            func_8009818C();
        }

        for (frame = 0; frame < 0x78; frame++) {
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        for (brightness = 0; brightness < 0xFF; brightness += 4) {
            func_80098440(brightness, 2, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        func_80037FB0(0, 0x4F, 0x80110000);
        if (g_seedState->soundLoadComplete == 0) {
            do {
                func_800393C8();
            } while (g_seedState->soundLoadComplete == 0);
        }

        D_8005F11C = sndCmd10(toggleSoundBank());
        sndCmdC0(0, 0x7F);

        for (frame = 0; frame < 0xB4; frame++) {
            func_80098440(0xFF, 2, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        outerCount = 0;
        D_800992B0 = VSync(-1);

        for (stage = 5; stage < 0x20; stage++, outerCount++) {
            func_80098338(stage);
            rect.x = 0; rect.y = rectY; rect.w = rectW; rect.h = rectH;

            for (brightness = 0xFF; brightness >= 0; brightness -= 8) {
                func_80098440(brightness, 2, &rect);
                func_8009818C();
                if (D_800992A8 & 0xF0) goto exit;
            }

            holdFrames = (stage - 4) * 0x1B3;
            while ((u32)(VSync(-1) - D_800992B0) < (u32)holdFrames) {
                func_8009818C();
                if (D_800992A8 & 0xF0) goto exit;
            }

            for (brightness = 0; brightness < 0xFF; brightness += 8) {
                func_80098440(brightness, 2, &rect);
                func_8009818C();
                if (D_800992A8 & 0xF0) goto exit;
            }
        }

        func_80098338(0x20);
        rect.x = 0; rect.y = rectY; rect.w = rectW; rect.h = rectH;

        for (brightness = 0xFF; brightness >= 0; brightness -= 8) {
            func_80098440(brightness, 2, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        holdFrames = (outerCount + 1) * 0x1B3;
        while ((u32)(VSync(-1) - D_800992B0) < (u32)holdFrames) {
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        for (brightness = 0; brightness < 0xFF; brightness += 4) {
            func_80098440(brightness, 1, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        for (frame = 0; frame < 0x78; frame++) {
            func_80098440(0xFF, 1, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        func_80098338(0x21);
        rect.x = 0; rect.y = rectY; rect.w = rectW; rect.h = rectH;

        for (brightness = 0xFF; brightness >= 0; brightness -= 2) {
            func_80098440(brightness, 1, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        while (sndGetStatus() != 0) {
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        for (frame = 0; frame < 0xE10; frame++) {
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        for (brightness = 0; brightness < 0xFF; brightness++) {
            func_80098440(brightness, 2, &rect);
            func_8009818C();
            if (D_800992A8 & 0xF0) goto exit;
        }

        SetDispMask(0);
    }

exit:
    sndCmdC1(D_8005F11C, 0x60, 0);

    if (brightness >= 0x100) {
        brightness = 0xFF;
    }
    if (brightness < 0) {
        brightness = 0;
    }

    for (; brightness < 0xFF; brightness += 2) {
        func_80098440(brightness, 2, &rect);
        func_8009818C();
    }
}

/**
 * @brief Display initialization entry point with mode selection.
 *
 * Configures display buffers, selects a display mode (0 or 1), runs
 * the corresponding intro sequence, then resets the display back.
 *
 * @param mode 0 for the standard intro sequence (func_80098974),
 *             1 for the alternate intro sequence (func_8009879C).
 */
void func_80098FD4(s32 mode) {
    s16 rect[4];

    ResetGraph(1);

    rect[0] = 0x100;
    rect[1] = 0xE0;
    rect[2] = 0x40;
    rect[3] = 0x20;
    MoveImage(&rect, 0, 0x1E0);
    DrawSync(0);
    VSync(0);

    switch (mode) {
    case 0:
        D_80099290 = 0;
        func_80098974();
        break;
    case 1:
        D_80099290 = mode;
        func_8009879C();
        break;
    }

    SetDispMask(0);

    rect[1] = 0x1E0;
    rect[2] = 0x40;
    rect[0] = 0;
    rect[3] = 0x20;
    MoveImage(&rect, 0x100, 0xE0);
    DrawSync(0);
    VSync(0);
}
