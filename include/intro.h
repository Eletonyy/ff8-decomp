#ifndef INTRO_H
#define INTRO_H

#include "common.h"
#include "intro_assets.h"
#include "psxsdk/libgpu.h"

/**
 * @brief LZSS-decoded intro frame in staging RAM.
 *
 * Every entry in @c g_introAssetTable is an LZSS-compressed bitmap. After
 * @c cdReadAsyncSync (mode 7) decompresses one into staging RAM, the
 * layout starts with this 8-byte header followed by row-major 16bpp
 * BGR555 pixel data.
 */
typedef struct {
    u16 hdr0;
    u16 hdr1;
    u16 width;
    u16 height;
    u16 pixels[1];          /**< Flexible array — actually @c width × @c height entries. */
} IntroFrame;

/** @brief Pre-loaded 580x406 "CAUTION WRONG DISC" frame at @c 0x8017D000. */
#define g_wrongDiscFrame      ((IntroFrame *)0x8017D000)
#define WRONG_DISC_FRAME_W    580
#define WRONG_DISC_FRAME_H    406

/** @brief Per-stage scratch buffer where @c loadIntroSlide decodes a slide. */
#define g_introStagedFrame    ((IntroFrame *)0x80100000)

/**
 * @brief Display-init double-buffer context (180 bytes at 0x800991D8).
 *
 * Per-buffer display state for the intro overlay's double-buffered
 * rendering. The PSX GPU draws to one buffer while the other is
 * displayed; @c currBuf flips each frame.
 */
typedef struct {
    u32      ot[2];        /**< 0x00: double-buffered single-entry OT. */
    DRAWENV  draw;         /**< 0x08: 92-byte drawing environment. */
    DISPENV  disp;         /**< 0x64: 20-byte display environment. */
    TILE     tiles[2];     /**< 0x78: per-buffer fade overlay rectangles. */
    DR_TPAGE drTPages[2];  /**< 0x98: per-buffer SetDrawTPage commands. */
    s32      currBuf;      /**< 0xA8: active buffer index (0 or 1). */
    s32      unkAC;
    u8       unkB0;        /**< 0xB0: flag byte. */
    u8       unkB1;
    u8       pad[2];
} DispCtx;

/* --- Data globals defined in the intro overlay's .data section ----------- */
extern DispCtx g_introDispCtx;       /**< 0x800991D8 — double-buffer display context. */
extern s32     g_introOdeLatch;      /**< 0x8009928C — last-seen @c GetODE() < 1 parity. */
extern s32     g_introRenderMode;    /**< 0x80099290 — sub-renderer dispatch (0..2). */
extern s32     g_introRenderEnable;  /**< 0x80099294 — non-zero runs the per-frame renderer. */
extern s32     g_introCtrl0Inv;      /**< 0x80099298 — ~previous controller-0 sample. */
extern s32     g_introCtrl1Inv;      /**< 0x8009929C — ~previous controller-1 sample. */
extern s32     g_introCtrl0;         /**< 0x800992A0 — latest controller-0 sample. */
extern s32     g_introCtrl1;         /**< 0x800992A4 — latest controller-1 sample. */
extern s32     g_introCtrl0Edge;     /**< 0x800992A8 — controller-0 rising-edge mask (& 0xF0 = ABXY). */
extern s32     g_introCtrl1Edge;     /**< 0x800992AC — controller-1 rising-edge mask. */
extern s32     g_introVSyncBase;     /**< 0x800992B0 — VSync(-1) baseline for story-page hold timing. */

/* --- Intro overlay entry points (defined in src/intro.c) ----------------- */
void initIntroOverlay(void);
void func_8009818C(void);
void loadIntroSlide(s32 stage);
void func_80098378(s32 mode, s32 x, s32 y, s32 width, s32 height);
void func_80098440(s32 brightness, s32 mode, RECT *rect);
void loadWrongDiscWarning(void);
void func_800985EC(void);
void func_8009869C(void);
void playBootIntro(void);
void waitForCorrectDisc(void);
void func_80098FD4(s32 mode);

/* --- External helpers called by the intro overlay ------------------------ */
extern void sndCmdF0(void);
extern void sndCmdF1(void);
extern s16  sndCmd10(u32 bank);
extern void sndCmdC0(s32 a, s32 b);
extern void sndCmdC1(s32 id, s32 b, s32 c);
extern s32  sndGetStatus(void);
extern u32  toggleSoundBank(void);
extern s32  func_8004D174(void);
extern s32  func_8004D208(s32 a);
extern void func_800275D4();          /* K&R prototype, intentional. */
extern void func_80037FB0(s32 a, s32 b, s32 c);
extern s32  func_800393C8(void);
extern void resetCdDriveMode(void);
extern s32  getDiscId(void);
extern s32  func_80038A60(void);
extern s32  getAnimFrameParam(s32 slot, s32 sub);

#endif /* INTRO_H */
