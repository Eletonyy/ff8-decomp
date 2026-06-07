#ifndef WORLD_WE_OBJECT3_H
#define WORLD_WE_OBJECT3_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "world.h"

typedef struct {
    s32 val;    /* +0x00 */
    s16 hval;   /* +0x04 */
    s16 pad;    /* +0x06 */
} FeaEntry40C0; /* size 0x08 */

/* Object glyph header returned by func_800A5EC4: a count byte followed by an
   array of @c CmdDesc entries (16-byte stride). */
typedef struct {
    u8      count;        /* 0x00 */
    u8      pad01[3];
    CmdDesc entries[1];   /* 0x04 — [count] entries, stride 0x10 */
} GlyphHeader;

typedef struct {
    u16 x;
    u16 y;
} ImageCoord;

/* func_800A6188 loads a TIM with two fixed-size, back-to-back image blocks
 * (the canonical Tim struct from tim.h, reached via world.h -> battle.h). */

extern FeaEntry40C0  D_800D24A8[12];
extern WorldObject   D_800D3320[16];
extern WorldObject  *D_800D3318;
extern WorldObject  *D_800D34E0;
extern WorldObject  *D_800D34E4;
extern u32           D_800D2284;
extern u32           D_800D34A0[16];
extern u32           D_800D34F0;
extern WorldObject   D_800D33E0[16];
extern WorldObject   D_800C9EF0[16];
extern WorldObject  *D_800CA030;
extern POLY_F4       D_800D3300;
extern ImageCoord    D_800C5388[];
extern ImageCoord    D_800C5378[];
extern RECT          D_800D32F0;

extern s32 func_800A629C(WorldObject *target);

/* Resolve the object glyph header for a cell key (NULL if none). */
extern u32 *func_800A5EC4(s16 id);

/* Point-in-descriptor hit test: returns nonzero and writes a result word to
   @p out when @p point falls inside the region of command descriptor @p cand. */
extern s32 func_800BF024(CmdDesc *cand, VECTOR *point, AngleSlot *out, CmdDesc *end);

/* Project a world position to a grid-cell index; optionally emit its angle triple. */
extern s32 worldPosToCell(VECTOR *pos, SVECTOR *out);

extern void func_800488D4(s32 a);

#endif /* WORLD_WE_OBJECT3_H */
