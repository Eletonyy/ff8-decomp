#ifndef INTRO_H
#define INTRO_H

#include "common.h"
#include "psxsdk/libgpu.h"

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
extern u32     D_800990B8[];   /**< CD asset table: (sector, size) per stage. */
extern DispCtx D_800991D8;     /**< Per-buffer display context. */
extern s32     D_8009928C;     /**< GPU drawing-ready latch (mirrors GetODE). */
extern s32     D_80099290;     /**< Active intro-render mode (0..2). */
extern s32     D_80099294;     /**< When non-zero, runs the per-frame renderer. */
extern s32     D_80099298;     /**< ~previous controller-0 sample (edge mask). */
extern s32     D_8009929C;     /**< ~previous controller-1 sample (edge mask). */
extern s32     D_800992A0;     /**< Latest controller-0 sample. */
extern s32     D_800992A4;     /**< Latest controller-1 sample. */
extern s32     D_800992A8;     /**< Controller-0 rising-edge mask (& 0xF0 = ABXY). */
extern s32     D_800992AC;     /**< Controller-1 rising-edge mask. */
extern s32     D_800992B0;     /**< @c VSync(-1) baseline used to time story-page holds. */

/* --- Intro overlay entry points (defined in src/intro.c) ----------------- */
void func_80098000(void);
void func_8009818C(void);
void func_80098338(s32 stage);
void func_80098378(s32 mode, s32 x, s32 y, s32 width, s32 height);
void func_80098440(s32 brightness, s32 mode, RECT *rect);
void func_800985B4(void);
void func_800985EC(void);
void func_8009869C(void);
void func_80098974(void);
void func_8009879C(void);
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
