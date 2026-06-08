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

/* Program the GTE translation vector for world-map rendering from two packed coords. */
extern void setWorldMapTransVector(s16 coord0, s16 coord1);

/* Register master-list objects not yet present in any tracking list onto the active list. */
extern void registerNewWorldObjects(void);

/**
 * @brief One placed world-map sprite produced by @c placeWorldSpriteFan (0x2C stride).
 *
 * @c pos is the final world position; @c cell receives the @c worldPosToCell
 * projection; @c cellId/flag are the projected grid-cell id and a fixed marker.
 * @note Field purpose partly uncertain — named from the access pattern.
 */
typedef struct {
    VECTOR  pos;        /* 0x00 */
    SVECTOR cell;       /* 0x10 — worldPosToCell output */
    u8      pad18[0x8]; /* 0x18 */
    s16     cellId;     /* 0x20 — worldPosToCell return */
    s16     flag;       /* 0x22 */
    u8      pad24[0x8]; /* 0x24 */
} WorldSprite;          /* 0x2C */

/* Generate up to 5 spread-positioned SVECTOR offsets for the scene @p ctx. */
extern void func_800B5ADC(s32 ctx, SVECTOR *out, s32 c, s32 d);
/* Returns a per-scene angle bias used to orient the placed sprites. */
extern s32  func_800BC5E0(s32 ctx);

extern void func_800488D4(s32 a);

#endif /* WORLD_WE_OBJECT3_H */
