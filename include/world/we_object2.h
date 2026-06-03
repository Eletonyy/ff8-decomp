#ifndef WORLD_WE_OBJECT2_H
#define WORLD_WE_OBJECT2_H

#include "common.h"
#include "world.h"
#include "psxsdk/libgte.h"

typedef struct {
    s16 a;
    s16 b;
} HalfwordPair;

extern s32             D_800C9E68;
extern s32             D_800C9E70;
extern s16             D_800C8CEA;
extern BattleSceneCtx *D_800D244C;
extern KeyBuffer      *D_800C9880;
extern u16             D_800C5354[3];
extern u16             D_800C535C[3];
extern u16             D_800C5364[3];
extern s16             D_800C534C;
extern s16             D_800C5344;

/* VSync is declared here with a void-returning view for this file's
 * matching codegen (psxsdk/libetc.h's s32-returning prototype is not used). */
extern void VSync(s32 mode);

extern void func_800A5F78(s32 screen);
extern void func_800A5FD4(s32 screen);
extern void func_80099EDC(s32 idx);
extern void func_800B3FD4(Slot *a0, s32 a1);
extern void func_8009D630(void);
extern void func_800C4450(void);
extern void renderAndUpdateDisplay(s32 mode);
extern s32  renderBattleDisplayList(s32 *colorTag);
extern s32  func_800BD380(s16 *outLow, s16 *outHigh);
extern s32  func_800BD2A0(s16 *outLow, s16 *outHigh);
extern s32  func_800BD460(s16 *outLow, s16 *outHigh);
extern s32  func_8002CE84(s32 idx);
extern s32  func_800A017C(SVECTOR *v);

#endif /* WORLD_WE_OBJECT2_H */
