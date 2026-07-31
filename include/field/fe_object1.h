#ifndef FE_OBJECT1_H
#define FE_OBJECT1_H

#include "common.h"
#include "cd.h"
#include "field.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"

/*
 * Field asset-bundle streaming table and section pointers, all consumed by
 * func_800983F0 when a map is (re)loaded.
 *
 * The streaming table is one 24-byte (six word) entry per map, indexed through
 * D_800C2568. The three symbols below are three views into that same table:
 * D_800C0900 is the primary {sector,size} pair, D_800C0908 the secondary pair,
 * and D_800C0910 the third pair, which is handed to func_800AA8A0 by address.
 */
extern u32 D_800C0900[];         /**< Streaming table, primary {sector,size} pair. */
extern u32 D_800C0908[];         /**< Streaming table, secondary {sector,size} pair. */
extern CdFileDesc D_800C0910[];  /**< Streaming table, third descriptor (passed by address). */

/** @brief Field id currently being streamed in (compared against @c D_8005F100). */
extern s16 D_8005F14E;
/** @brief Field render/present state halfword; the loader spins while it reads 4. */
extern volatile s16 D_8005F146;

/** @brief Section-pointer table bases published by the loaded field bundle. */
extern u8 **D_800C7208;          /**< Event-queue block; assigned to @c D_8005F0F8. */
extern u8 **D_800C71EC;          /**< Walkmesh/section block; assigned to @c D_800D5EA4. */
extern s32 *D_800D5EAC;          /**< Word copied into @c D_800704A8.unk018. */
extern u8 **D_800D5E8C;          /**< End of the script region (used to size the copy). */
extern u8 **D_800D5ED4;          /**< Start of the script region; advanced past the header. */
extern u8 **D_800D5E94;          /**< Field-file header pointer. */
/** @brief Field-file header; @c NULL when the header carries the empty @c 0x2020 tag. */
extern u8 *D_800C7200;

/** @brief Double-buffered prim-chain heads laid out after the field bundle. */
extern u8 *D_800C6D98[2];
extern u8 *D_800D5EC8[2];
extern u8 *D_800D5EB8[2];

/** @brief Field-VM command tables selected by @c EventQueue::unk0D. */
extern u8 D_800C30DC[];
extern u8 D_800C311C[];
extern u8 D_800C315C[];
/** @brief Shared scratch block handed to @c func_800AA8A0. */
extern u8 D_800C06A0[];

/** @brief 12-byte signed integer 3D position (x, y, z). */
typedef struct {
    s32 x;
    s32 y;
    s32 z;
} Vec3i;

/**
 * 3D line-segment coordinate view: six consecutive s16 coordinates
 * (start XYZ, end XYZ). This is the layout shared by the head of
 * @ref FieldLineTrigger and the per-entity trigger segment embedded in
 * @ref Eline at 0x188 — @c func_8009A2BC accepts either through this view.
 */
typedef struct {
    /* 0x00 */ s16 x0, y0, z0;
    /* 0x06 */ s16 x1, y1, z1;
} LineSeg;

/** @brief 4-byte signed 16-bit 2D position (x, y). */
typedef struct {
    s16 x;
    s16 y;
} Vec2s;

/** @brief 6-byte signed 16-bit 3D position (x, y, z). */
typedef struct {
    s16 x;
    s16 y;
    s16 z;
} Vec3s;

/** @brief 8-byte navmesh vertex; the packed (sx, sy) word doubles as a GTE SXY operand. */
typedef struct {
    s16 sx;
    s16 sy;
    s16 sz;
    s16 pad;
} SVert;

/** @brief 24-byte navmesh triangle — three @ref SVert corners. */
typedef struct {
    SVert v[3];
} Triangle;

/** @brief Counted inline triangle list scanned by @ref func_8009AC9C. */
typedef struct {
    /* 0x0 */ s32 count;
    /* 0x4 */ Triangle tris[1];        /* variable length */
} TriangleList;

/** @brief 6-byte per-triangle adjacency record — neighbor triangle index per edge (0xFFFF = none). */
typedef struct {
    u16 neighbor[3];
} AdjRec;

/**
 * @brief Field navmesh vertices (loaded per field), three consecutive @ref SVert
 *        per triangle — triangle @c t owns @c D_800C71F0[t*3 .. t*3+2].
 */
extern SVert *D_800C71F0;
extern AdjRec *D_800D5E98;    /**< Per-triangle edge adjacency table, one entry per triangle. */


/** @brief 32-byte slot stride for indexing into a particle system buffer. */
typedef struct {
    u8 b[32];
} ParticleBlock;

/**
 * @brief Interpolating oscillator / wobble driver (14 bytes).
 *
 * Steps a value from @c start toward @c end over @c total ticks, sampling a
 * waveform from @c D_800C3520 (scaled by @c amplitude) and interpolating each
 * tick via @c func_800A0EB8. @c mode / @c phase select the behaviour
 * (@c mode==1 continuously oscillates by flipping @c end 's sign each cycle;
 * otherwise it runs once and stops). Advanced by @ref func_800A17B8.
 */
typedef struct {
    /* 0x00 */ u8  mode;       /**< Dispatch mode (1 = continuous oscillation). */
    /* 0x01 */ u8  phase;      /**< Sub-state within the current mode. */
    /* 0x02 */ u8  tableIdx;   /**< Cursor into the @c D_800C3520 waveform table. */
    /* 0x03 */ u8  output;     /**< Latest interpolated value from @c func_800A0EB8. */
    /* 0x04 */ s16 amplitude;  /**< Scale applied to the sampled waveform byte. */
    /* 0x06 */ s16 start;      /**< Interpolation start value. */
    /* 0x08 */ s16 end;        /**< Interpolation end (target) value. */
    /* 0x0A */ s16 total;      /**< Interpolation duration in ticks. */
    /* 0x0C */ s16 angle;      /**< Current tick (0..total). */
} Oscillator;  /* 0x0E = 14 bytes */

/**
 * @brief Particle system buffer.
 *
 * Modeled as a flat array of 32-byte slots: the first ~313 slots hold the
 * emitter table and other buffer metadata; particle records overlay the
 * remaining slots starting at slot index 313 (byte offset 0x2720).
 * Casting a slot's address to @c Particle* gives access to that slot's
 * particle data via the absolute-offset view above.
 */
typedef struct {
    ParticleBlock slots[1];
} ParticleSystem;

/**
 * @brief Particle emitter record (one of an array within ParticleSystem).
 *
 * Stride 0x174 bytes. Indexed by emitter id from the start of @c sys.
 * Holds the emitter's spawn-rate counters and the velocity/position
 * jitter ranges used to seed each particle.
 */
typedef struct {
    /* 0x000 */ u8 pad000[0x14E];
    /* 0x14E */ u8 unk14E;          /**< Reset to 0 on each call. */
    /* 0x14F */ u8 pad14F[0x0B];
    /* 0x15A */ s16 maxCount;       /**< Cap on simultaneously-active particles. */
    /* 0x15C */ s16 curCount;       /**< Currently active particle count. */
    /* 0x15E */ s16 unk15E;         /**< Cleared together with @c curCount by @c func_800A3018. */
    /* 0x160 */ s16 unk160;         /**< Velocity-Z bias (added * 32). */
    /* 0x162 */ s16 unk162;         /**< Velocity-Z jitter half-range. */
    /* 0x164 */ s16 unk164;         /**< unk16 jitter half-range. */
    /* 0x166 */ s16 unk166;         /**< Position-X jitter (* 256). */
    /* 0x168 */ s16 unk168;         /**< Position-Y jitter (* 256). */
    /* 0x16A */ u16 unk16A;         /**< Position-Z jitter (low 7 bits). */
    /* 0x16C */ s16 unk16C;         /**< Velocity-X jitter half-range. */
    /* 0x16E */ s16 unk16E;         /**< Velocity-Y jitter half-range. */
    /* 0x170 */ s16 unk170;         /**< Velocity-Z jitter half-range. */
    /* 0x172 */ u8 pad172[0x02];
} Emitter; /* 0x174 = 372 bytes */

/**
 * @brief Particle "view" — overlay struct positioned at @c &sys->slots[slot].
 *
 * The view's fields are at the absolute byte offsets (0x2720..0x273B) where
 * each particle's data actually lives. Indexing @c sys->slots[slot] gives a
 * 32-byte slot stride; casting that address to @c Particle* lets field
 * accesses (e.g. @c p->posX) compile to @c sw v0,0x2720(s0) — the original
 * "keep @c sys+slot*32 in a register, full immediate offsets" pattern.
 */
typedef struct {
    /* 0x0000 */ u8 pad0000[0x2720];
    /* 0x2720 */ s32 posX;
    /* 0x2724 */ s32 posY;
    /* 0x2728 */ s32 posZ;
    /* 0x272C */ s16 velX;
    /* 0x272E */ s16 velY;
    /* 0x2730 */ s16 velZ;
    /* 0x2732 */ s16 unk12;
    /* 0x2734 */ u8 pad2734[0x02];
    /* 0x2736 */ s16 unk16;
    /* 0x2738 */ u8 emitterIdx;
    /* 0x2739 */ u8 unk19;
    /* 0x273A */ u8 unk1A;
    /* 0x273B */ u8 active;
} Particle;

/**
 * @brief 16-byte script entry consumed by @c func_800A0640 to populate
 *        a TILE primitive.
 *
 * The list is terminated by @c terminator == @c 0x7FFF.
 */
typedef struct ScriptEntry {
    /* 0x00 */ s16 terminator;  /**< @c 0x7FFF marks end of list. */
    /* 0x02 */ u8  pad02[6];
    /* 0x08 */ u16 h;           /**< Copied to @c TILE::h. */
    /* 0x0A */ u8  wLo;         /**< Low byte of @c TILE::w. */
    /* 0x0B */ u8  wHi;         /**< High byte of @c TILE::w. */
    /* 0x0C */ u8  padC;
    /* 0x0D */ u8  kind;        /**< @c 4 = opaque, else semi-translucent. */
    /* 0x0E */ u8  padE[2];
} ScriptEntry;

/** @brief Container for the entry list at @c D_800D5E90. */
typedef struct ScriptList {
    ScriptEntry *entries;
} ScriptList;

/* FieldLineTrigger (field line-trigger table entry) is defined in field.h. */

extern ScriptList *D_800D5E90;

extern void func_80098934(void);
extern void func_80099124(void);
extern void func_8009912C(void);
/** @brief 12-byte path waypoint (64 entries per table, indexed by angle/64). */
typedef struct {
    /* 0x00 */ s16 x;       /**< Position X (fixed-point, << 12 when written). */
    /* 0x02 */ s16 y;       /**< Position Y. */
    /* 0x04 */ s16 z;       /**< Position Z. */
    /* 0x06 */ u16 unk6;    /**< Stored to entity offset 0x1FA. */
    /* 0x08 */ u8  unk8;    /**< Stored to entity offset 0x258. */
    /* 0x09 */ s8  field_09; /**< Signed step magnitude; scaled by the caller's multiplier. */
    /* 0x0A */ u8  field_0A; /**< Movement mode selector (4 = the walk step written here). */
    /* 0x0B */ u8  field_0B; /**< Heading byte copied into the entity's @c field_0x241. */
} PathEntry;

extern void func_8009B74C(s16 slotIdx, u16 paramIdx, PathEntry *params, s16 multiplier);
extern void func_8009BB18(void);
extern void func_8009BD50(Eline *e, s16 mode, s8 b9, u8 b8);
extern s16  func_8009D234(s32 a0);
extern s16  func_8009D254(s32 a0);
extern void func_8009DED8(Vec3i *out, SVert *a, SVert *b);
extern s32  func_8009E468(s16 selfIdx, Vec3i *pos);
extern s32  func_8009E604(Eline *a, Eline *b);
extern void func_800A17A4(u8 *a0);
extern void func_800A1C64(void);
extern void func_800A1CC0(void);
extern void func_800A2EE0(u8 *a0);
extern void func_800A2F28(s32 a0, u8 *a1);
extern void func_800A303C(s16 emIdx, ParticleSystem *sys, s16 *pos, s16 count);
extern void func_800A355C(FieldActor *actor, s32 slot, s32 a2);
extern void func_800A44D8(void);
extern void func_800A4550(s16 a0);
extern s32  func_800A4910(s32 a0, s32 a1, s32 a2, s32 a3);
extern void func_800A59D0();  /* K&R: a0 declared but ignored in body; callers vary 0/1-arg */
extern void func_800A5A14(s16 a0);
extern s32  func_800A5CF8(void);

/* INCLUDE_ASM stubs — bodies still in assembly, signatures unknown.
 * Declared K&R-style; refine when these get decomped to C. */
extern void func_80098314(void);
/** @brief Load/refresh the active field map's asset bundle from CD. */
extern s32 *func_800983F0(void);
extern int  func_8009895C();
extern void func_80099180(void);
extern int  func_80099348();
extern s32  func_8009A0E8(s32 *p0, s32 *p1, s32 *outDist);
extern s32  func_8009A2BC(LineSeg *seg, Vec3i *p, Vec3i *out);
extern s32  func_8009A4C0(Eline *self, FieldEntityB *records, VECTOR *pt);
extern void func_8009A7E8(Eline *e, FieldEntityB *pool);
extern void func_8009A8E0(FieldEntityB *e);
extern void func_8009A920(Eline *eline, FieldEntityB *entities);
extern void func_8009AA64(EventEntry *e);
extern void func_8009AAC8(Eline *eline, EventEntry *segs, Vec3i *pt);
extern s16  func_8009AC9C(s16 px, s16 py, s16 pz, TriangleList *list);
/** @brief Place every field entity on the navmesh when a field is entered. */
extern void func_8009AEC0(void);
extern int  func_8009BEC8();
extern void func_8009CEE8(void);
extern s32  func_8009D274(Eline *self, s16 pad);
extern s32  func_8009D500();  /* arg2 is a file-private scratchpad view in fe_object1.c */
extern int  func_8009D598();
extern s32  func_8009DF18(u16 *pTriIdx, Vec3i *out, s32 *dxy, s32 *aux);
extern s32 func_8009E338(Vec3i *a0, Vec3i *a1, Vec3i *a2, Vec3s *a3);  /* plane-cross intersection */
extern int  func_8009E660();
extern int  func_8009ECA4();
extern s32  func_8009F74C(Eline *a, Eline *b);
extern void func_8009F7F4(s16 idx, s8 sign, u8 b, s16 mode);
extern void func_8009B4A8(s16 a, u8 b, s32 c, s32 d);
extern void func_8009F8D0(s16 idx);
extern void func_8009F990(s16 idx, s32 flags);
extern int  func_8009FE18();
extern TILE *func_800A0640(TILE *prim);
extern int  func_800A06F0();
extern void func_800A0D6C(u8 *buf);
extern s32  func_800A0E54(s32 start, s32 end, s32 total, s32 progress);
extern s32  func_800A0EB8(s32 start, s32 end, s32 total, s32 angle);
extern s32  func_800A0F34(SVECTOR *v, s32 *sxy);
extern void func_800A0FB8(Vec2s *out, s16 a, s16 b);
extern int  func_800A10F4();
extern void func_800A11E0(Vec2s *arg0);
extern int  func_800A1318();
extern int  func_800A15C0();
void func_800A17B8(Oscillator *osc);
extern int  func_800A19B8();
extern void func_800A1BB8(void);
extern void func_800A1CFC(Eline *ents, u8 *arg1);
extern void func_800A2128();  /* arg is a file-private buffer view in fe_object1.c */
extern int  func_800A222C();
/**
 * @brief Shape @c func_800A29C0 sees: array of 20-byte items with five
 *        leading bytes that get initialized per item.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x3];
    /* 0x03 */ u8 b3;     /**< Set to @c 4 each iter. */
    /* 0x04 */ u8 b4;     /**< Cleared. */
    /* 0x05 */ u8 b5;     /**< Cleared. */
    /* 0x06 */ u8 b6;     /**< Cleared. */
    /* 0x07 */ u8 b7;     /**< Set to @c 0x22. */
    /* 0x08 */ u8 pad08[0xC];
} func_800A29C0_arg0;  /* 0x14 = 20 bytes */

extern func_800A29C0_arg0 *func_800A29C0(func_800A29C0_arg0 *p);
/**
 * @brief Shape of the prim records @c func_800A2A30 writes — @c 8 bytes
 *        with a @c tag byte and a @c cmd word (GPU command + color).
 */
typedef struct {
    /* 0x00 */ u8  pad00[0x03];
    /* 0x03 */ u8  tag;     /**< Always written as @c 1. */
    /* 0x04 */ s32 cmd;     /**< @c 0xE1000200 | (color & 0x9FF). */
} func_800A2A30_item;  /* 8 bytes */

extern func_800A2A30_item *func_800A2A30(func_800A2A30_item *p);
extern int  func_800A2AF8();
extern void func_800A2D2C(s16 *buf, s32 slot);
extern s16  func_800A2EA4(s16 range);
extern void func_800A2F48();  /* arg is a file-private buffer view in fe_object1.c */
extern void func_800A2F70();  /* arg is a file-private buffer view in fe_object1.c */
extern s16  func_800A2FE0();  /* arg is a file-private buffer view in fe_object1.c */
extern void func_800A327C();  /* arg0 is a file-private Eline-stack view in fe_object1.c */
extern void func_800A3488();  /* arg0 is a file-private Eline-stack view in fe_object1.c */
extern void func_800A3534();  /* arg is a file-private buffer view in fe_object1.c */
extern void func_800A37A8(void *arg0, s32 arg1, FieldSubsceneBuffer *buf);

extern void func_800A38B4(MoveAccum *out, MoveStep *in, MoveStep *target);
extern int  func_800A39D8();
extern void func_800A3FE0(FieldSubsceneBuffer *buf);
void func_800A42EC(POLY_G4 *polys, DR_TPAGE *tpages);
extern void func_800A4500(s32 x, s32 y, s32 z);
void func_800A455C(s16 entityIdx);
extern void func_800A4758(void);
extern s32  func_800A48CC(void);
extern void func_800A4934();  /* args are file-private ObjSlot/DrawPoint in fe_object1.c */
extern int  func_800A4C14();

typedef struct { u8 pad[0xB4]; } func_800A5224_arg2; /* 0xB4 = 180 bytes */
typedef struct { u8 pad[0x20]; } func_800A5224_arg3; /* 0x20 = 32 bytes */
extern void func_800A5224(MATRIX *m, void *arg1, func_800A5224_arg2 *arg2,
                          func_800A5224_arg3 *arg3);
extern void func_800A5360(u32 *ot, s16 r, s16 g, s16 b);
extern volatile u16 g_bufferIndex;       /**< Active double-buffer index. */
extern u32 g_orderingTablePtrs[];        /**< Per-buffer ordering-table heads. */
extern TILE g_clearTiles[];              /**< Per-buffer screen-clear TILEs. */

extern void func_800A553C(u32 *ot, s16 r, s16 g, s16 b);
extern void func_800A5698(void);
extern void func_800A5700(void);
extern s16  func_800A5748(s16 start, s16 end, s16 progress, s16 total);
extern void func_800A5788(s32 a0);
extern int  func_800A5898();
extern void func_800A5A20(Eline *self, EventEntry *entries);
extern s32  func_800A5C9C(void);
extern void func_800A5D28(void);
extern void func_800A5FA4();  /* arg 0 = entry pointer (16-byte stride); arg 1 = flag */
extern void func_800A6100(Eline *eline, FieldLineTrigger *segs);
extern void func_800A62EC();  /* arg 0 = array of 12 16-byte entries */
extern int  func_800A63AC();
extern int  func_800A6A80();

/**
 * @brief Element of a @ref FieldObject part's sub-range (8-byte stride).
 * @note Purpose uncertain — @c func_800A8058 clears @c field06 for every
 *       element in each part's @c [subStart, @c subStart+subCount) range.
 */
typedef struct {
    /* 0x00 */ u8 pad00[6];
    /* 0x06 */ s16 field06;
} FieldObjectSub;  /* 0x08 */

/**
 * @brief One part of a @ref FieldObject (0x20-byte stride).
 *
 * Each part owns a contiguous @c [subStart, @c subStart+subCount) slice of the
 * object's @ref FieldObjectSub array.
 * @note Field naming reflects only observed usage.
 */
typedef struct {
    /* 0x00 */ s16 subStart;   /**< first sub-element index owned by this part. */
    /* 0x02 */ s16 subCount;   /**< number of sub-elements in the part. */
    /* 0x04 */ u8 pad04[0x0A];
    /* 0x0E */ s16 field0E;    /**< cleared per part by @c func_800A8058. */
    /* 0x10 */ u8 pad10[0x10];
} FieldObjectPart;  /* 0x20 */

/**
 * @brief A field-engine object instance (element of the @c D_800D6620 table).
 *
 * @note Purpose partially understood — holds a part list (@c parts /
 *       @c partCount) indexing a shared sub-element array (@c subs), plus a
 *       @c 0x12345678 signature word and a couple of init fields.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x1C];
    /* 0x1C */ FieldObjectPart *parts;  /**< part array (@c partCount entries). */
    /* 0x20 */ FieldObjectSub *subs;    /**< sub-element array indexed by parts. */
    /* 0x24 */ s32 partCount;
    /* 0x28 */ u8 pad28[0x24];
    /* 0x4C */ u32 signature;           /**< set to @c 0x12345678 on init. */
    /* 0x50 */ u16 field50;             /**< init'd from @c D_800D60E8. */
    /* 0x52 */ u8 pad52[0x0D];
    /* 0x5F */ u8 field5F;              /**< cleared on init. */
} FieldObject;

/** @brief 64-slot field-engine object table (indexed by object id). */
extern FieldObject *D_800D6620[];
/** @brief u16 seed value written into @c FieldObject::field50 at init. */
extern u16 D_800D60E8;

extern void func_800A7194(void);
extern void func_800A7224(s32 idx, u16 *vals, s32 mode);
extern void func_800A736C(s32 idx, u16 *vals, s32 mode);
extern void func_800A74B4(s32 idx, EntityRenderXform *vals, s32 mode);
extern int  func_800A7564();
extern s32  func_800A8058(s32 idx, s32 arg1, FieldObject *newObj, u8 count);
extern int  func_800A81AC();
/** @brief Scratch sprite rectangle built by @c func_800AA5F8 before a @c MoveImage upload. */
extern RECT D_800D5ED8;

extern s32 *func_800A8CDC(s32 idx, s32 firstWord, EntityRenderSlot *slot);
/** @brief Per-entity animation tick: advances the frame and rebuilds the sprite rect. */
extern s32  func_800AA5F8(s32 idx);
extern u8  *func_800A8DAC(s32 spatialIdx, s32 cmd, u32 arg, void *out);
extern int  func_800A91C8();
extern int  func_800A9434();
extern void func_800A97E4(s32 spatialIdx, s32 cmd, s32 arg2, s32 arg3);
extern void func_800AA46C(u8 spatialIdx, s32 cmd, s32 arg, s32 arg4);
extern int  func_800AA8A0();

#endif
