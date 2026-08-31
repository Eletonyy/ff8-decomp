#ifndef WORLD_WE_OBJECT3_H
#define WORLD_WE_OBJECT3_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "world.h"

/** One entry of the @c D_800D24A8 descriptor cache: a command descriptor and
    the cell key it was resolved for, kept in most-recently-used order. */
typedef struct {
    CmdDesc *val;   /* +0x00 */
    s16 hval;       /* +0x04 — cell key the descriptor was resolved for */
    s16 pad;        /* +0x06 */
} FeaEntry40C0; /* size 0x08 */

/* Object glyph header returned by func_800A5EC4: a count byte followed by an
   array of @c CmdDesc entries (16-byte stride). */
typedef struct {
    u8      count;        /* 0x00 */
    u8      id;           /* 0x01 — glyph kind, dispatched on by func_800BF20C/func_800BFBFC */
    u8      pad02[2];
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
extern WorldObject  *D_800D2284;     /**< Head of the active world-object list. */
extern WorldObject   D_800C9888[];   /**< Pool the visible-cell list is built in. */
extern u32           D_800D34A0[16];
extern u32           D_800D34F0[];  /**< Streamed-record staging buffer. */
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

/* Generate up to 5 spread-positioned SVECTOR offsets for the scene @p ctx. */
extern void func_800B5ADC(s32 ctx, SVECTOR *out, s32 c, s32 d);


/* Compute a linear tile index from 2D world coordinates (32x24 grid). */
extern s32 func_800A5E40(s32 x, s32 y);

/* Visibility gate keyed on a packed command code; sibling of func_800A45D8. */
extern s32 func_800A4670(u32 a, s32 b);

/* Linear tile index from 2D world coordinates; sibling of func_800A5E40. */
extern s32 func_800A5DC8(s32 x, s32 y);

/* Rebuild the world-map sprite pool and draw every pending world object. */
extern void renderWorldMapFrame(void);

/* World render callback: advances the streamer and the object lists once per
   frame. Registered with func_800C3DB0; returns 2 busy / 1 armed / 0 idle. */
extern s32 func_800A47A4(void);

/* Probe up to 8 rotations for a free 5-sprite fan placement around the camera;
   returns 1 and writes sprite 0's position to hitPos when one is accepted. */
extern s32 func_800A2D50(s32 code, s32 angZ, SVECTOR *angles, VECTOR *hitPos, s32 arg4, s32 *hitCell);

/* Place a five-sprite fan around a slot and emit it; returns func_800B21EC's
   result (negative when the fan was rejected). */
extern s32 func_800A358C(s32 kind, SlotEntry *slot, SVECTOR *angles, s32 flag);

/* Visibility gate: picks a CMDPAR_VIS_* bit of packed code @p a by dispatch
   code @p b (only b's low 16 bits are examined; D_800C4D20 == 0 force-passes). */
extern s32 func_800A45D8(u32 a, s32 b);

#endif /* WORLD_WE_OBJECT3_H */
