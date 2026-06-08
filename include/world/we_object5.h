#ifndef WORLD_WE_OBJECT5_H
#define WORLD_WE_OBJECT5_H

#include "common.h"
#include "world.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"

/**
 * @brief 36-byte prim slot used by @c D_800D4F10[4] — P_TAG header followed
 *        by a 28-byte payload. Seeded with @c len=8 (8-word body) and
 *        @c code=0x38 by @ref func_800ABC98.
 */
typedef struct {
    /* 0x00 */ P_TAG tag;
    /* 0x08 */ u8 pad08[0x1C];
} Prim36;

/**
 * @brief 8-byte short DR_MODE-like used by @c D_800D4FA0[2] — a P_TAG word
 *        (containing @c len) followed by a single GPU command word.
 */
typedef struct {
    /* 0x00 */ u32 tag;   /**< P_TAG-compatible: addr:24 + len:8. */
    /* 0x04 */ u32 cmd;   /**< Raw GP0 command word (e.g. @c 0xE1000220 TPAGE). */
} DrSetMode8;

/**
 * @brief Per-frame world-view transform record: an actor's offset/angles
 *        inputs followed by its composed rotation matrix output.
 *
 * @c func_800AC468 reads @c offset / @c angles and writes @c matrix; its two
 * pointer params normally alias the same actor record. The layout mirrors
 * the leading 0x30 bytes a world actor reserves for its camera transform.
 */
typedef struct {
    /* 0x00 */ SVECTOR offset;
    /* 0x08 */ SVECTOR angles;
    /* 0x10 */ MATRIX  matrix;
} WorldViewXform;  /* 0x30 */

/**
 * @brief 32-byte input passed to @ref func_800ACA70. Contains two
 *        16-byte halves: a 4-word @c a block (opaque, forwarded as-is
 *        to @c func_800423DC) and a @c b block whose first u32 is
 *        kept while the trailing 3 s32s are negated to invert a
 *        position before the call.
 */
typedef struct {
    /* 0x00 */ VECTOR a;       /**< Opaque 16-byte block, passed through. */
    /* 0x10 */ s32    b_pre;   /**< Untouched (likely a tag / mode byte). */
    /* 0x14 */ s32    b_x;     /**< Negated before @c func_800423DC. */
    /* 0x18 */ s32    b_y;     /**< Negated. */
    /* 0x1C */ s32    b_z;     /**< Negated. */
} Input32;

extern POLY_FT4   D_800D8810[2];
extern POLY_FT4   D_800D8860[2];
extern s32        D_800D23D0;
extern Prim36     D_800D4F10[4];
extern DrSetMode8 D_800D4FA0[2];
extern u8         D_800C5448[];
extern u8         D_800DB0D0[];
extern POLY_FT4   D_800D4EC0[2];
extern CVECTOR    D_8009811C;
extern VECTOR     D_800C9858;     /* live camera world position (VECTOR view)     */
extern VECTOR     D_800DD658;     /* source position for func_800BC51C            */

extern void func_800A84D0(void);
extern s32  func_8003F9F4(CVECTOR *input, CVECTOR *cue, s32 w1, s32 w2, CVECTOR *out);
extern s32  func_8009CC3C(void);
extern void func_80048FBC(RECT *r, s32 src_x, s32 src_y);
extern s32  func_80048C50(s32 arg);
extern s32  func_8003F4A4(s32 a);
extern s32  func_80041E84(s32 y, s32 x);
extern s32  worldPosToCell(VECTOR *pos, SVECTOR *out);
extern s32  func_800A4700(s32 a, s32 b);
extern s32  func_800A475C(s32 a, s32 b);
extern s32  func_800A5DC8(s32 x, s32 y);
extern void func_800423DC(VECTOR *a, s32 *b_pos, VECTOR *out);
extern void func_800ACC68(MATRIX *out_mat, SVECTOR *angles,
                          SVECTOR *rotBuf, SVECTOR *offset);
extern void func_800B5C60(s32 ctx, s16 count, MATRIX *outMat, SVECTOR *outAngles,
                          SVECTOR *rotBuf, u8 *xform, ActorRecord *recs);
extern void func_800BC51C(VECTOR *src, VECTOR *dst);
extern void func_800BC544(VECTOR *src, VECTOR *dst);

#endif /* WORLD_WE_OBJECT5_H */
