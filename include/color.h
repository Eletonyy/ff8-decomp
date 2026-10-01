#ifndef COLOR_H
#define COLOR_H

#include "common.h"
#include "psxsdk/libgpu.h"

/** One glyph of the menu font's metrics table @c D_8008371C. */
typedef struct {
    u8 width; /**< Low nibble: the glyph's advance in pixels. */
    u8 unk1;
    u16 uv; /**< u (low byte) and v (high byte) of the glyph's 12x12 texture cell. */
} FontGlyph;

extern FontGlyph D_8008371C[];
extern u8 D_80083908[];

void *func_800330F4(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color, s32 clut);
void *drawColorDefault(P_TAG *ot, SPRT *sprt, s32 x, u32 value, u32 color);
void func_80033380(s32 t, RECT *src, RECT *dst);
s32 drawColorByMenuPalette(s32 renderCtx, s32 cursorY, s32 packedXY, s32 value, s32 color);

#endif /* COLOR_H */
