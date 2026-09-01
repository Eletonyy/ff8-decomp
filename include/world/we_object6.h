#ifndef WORLD_WE_OBJECT6_H
#define WORLD_WE_OBJECT6_H

#include "world.h"
#include "world/we_object3.h"   /* GlyphHeader, WorldPrimBank */
#include "world/we_object2.h"   /* getAngleDelta */

/* Defined in this file (via INCLUDE_ASM). */
/* Arg 1 is dereferenced at +0x14, which is MATRIX.t[0]. */
extern void func_800ACC68(MATRIX *outMat, SVECTOR *angles, SVECTOR *rotBuf,
                          SVECTOR *offset);

extern void func_800AD698(SceneState *st, u8 *flags);
extern void func_800AE31C(u8 *flags);
extern s32  func_800AE518(u8 *flags);
extern void func_800AEEB0(u8 *flags, void *a, void *b, void *c);
extern void func_800B04CC(SVECTOR *ang, MATRIX *m, u8 *flags, s32 v);
extern void func_800B164C(SceneState *st, Slot *slot, u8 *flags, VECTOR *pos);
extern void func_800B18B8(u8 *flags, VECTOR *pos, SVECTOR *ang);
extern s32  func_800B1BCC(u8 *p);

extern s32 func_800B0010(u32 kind);

extern void func_800ACD38(MATRIX *outMat);

/* Projects @p pos to screen space in @p screenOut and returns a status of
   -1, 0 or 1. @p byteOut and @p halfOut are optional outputs; both are
   NULL-checked. */
extern s32 func_800B01A0(s16 viewY, s16 viewX, VECTOR *pos, SVECTOR *screenOut,
                         s8 *byteOut, s16 *halfOut);

extern void func_800ACDC4(GlyphHeader *p, BattleSceneCtx *ctx, WorldPrimBank *pools);

#endif /* WORLD_WE_OBJECT6_H */
