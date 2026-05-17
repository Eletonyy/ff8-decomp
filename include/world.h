#ifndef WORLD_H
#define WORLD_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"

/**
 * @brief World-space 3-vector used for translation/position state.
 *
 * Stores a world-space integer position. Some callers use the y/z axes
 * with a sign flip to account for the PS1 y-down screen convention
 * (see @c we_object9 for the source-side mapping).
 */
typedef struct {
    s32 x;             /* 0x00 */
    s32 y;             /* 0x04 */
    s32 z;             /* 0x08 */
} WorldPos;

/**
 * @brief World-engine object node — singly-linked, keyed by s16 id.
 *
 * Callers use the @c sectionIdx at +0x0A to index into the world data
 * region base @c D_800C4D5C (stride 0x9000 bytes).
 */
typedef struct WorldObject {
    struct WorldObject *next;   /**< 0x00: Next node. */
    s16 id;                     /**< 0x04: s16 id used by search. */
    u8 pad06[4];
    u8 sectionIdx;              /**< 0x0A: Section index into the D_800C4D5C region table. */
} WorldObject;

/**
 * @brief One 0x9000-byte section of the world data region table.
 *
 * Pointed-to by @c D_800C4D5C as an array indexed by @c sectionIdx.
 * The section begins with a 1-byte @c key (used by lookup helpers to
 * match sections), followed by a u32 offset/handle array starting at
 * offset 4. The rest of the section contains the data that the offsets
 * index into.
 *
 * Known fields:
 *   - @c key at offset 0x00 — section type/key byte (0xFF when empty).
 *   - @c offsets[] at offset 0x04 — u32 array; low 2 bits may carry flags.
 */
typedef struct {
    /* 0x00 */ u8 key;               /**< Section type/key byte. */
    /* 0x01 */ u8 pad01[3];
    /* 0x04 */ u32 offsets[9215];    /**< Fills section up to 0x9000 total. */
} WorldSection;

/**
 * @brief World-engine command descriptor (16 bytes).
 *
 * Used by dispatch functions that peek the current command's
 * @c type / @c flag / @c param. The current active descriptor is pointed
 * to by D_800C4D64, D_800C4D68, D_800C4D6C (three slots).
 */
typedef struct {
    u8 unk_00[0xD];
    u8 type;        /**< 0x0D: Command type byte. */
    u8 flag;        /**< 0x0E: Command flag byte. */
    u8 param;       /**< 0x0F: Command parameter byte. */
} CmdDesc;

/**
 * @brief Slot entry in the D_800DBFB8 table (stride 40 bytes).
 *
 * Each entry describes one battle/world slot. Fields are added as
 * usages are discovered.
 */
typedef struct {
    /* 0x00 */ s32 x;               /**< Slot world X coordinate. */
    /* 0x04 */ s32 y;               /**< Slot world Y (compared to world camera angle). */
    /* 0x08 */ s32 z;               /**< Slot world Z coordinate. */
    /* 0x0C */ s32 pad0C;
    /* 0x10 */ s8 marker;           /**< Scan terminator / type byte. */
    /* 0x11 */ u8 pad11;
    /* 0x12 */ s8 lookupIdx;        /**< Index into D_800DDB00 when >= 0. */
    /* 0x13 */ u8 pad13;
    /* 0x14 */ u8 data14[0x14];     /**< Tail buffer passed to dispatcher. */
} SlotEntry; /* 0x28 = 40 bytes */

/**
 * @brief Lookup target reached via D_800DDB00[SlotEntry.lookupIdx].
 *
 * Only the @c field52 u16 is known so far.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x52];
    /* 0x52 */ u16 field52;
} LookupTarget;

/**
 * @brief 4-byte scene state block at D_80082C8C.
 *
 * Accessed by several world and field_engine routines; stores
 * the current scene mode, dispatch code, and two marker bytes.
 */
typedef struct {
    /* 0x00 */ s8 mode;         /**< Scene mode / kind. */
    /* 0x01 */ s8 cmd;          /**< Current dispatch code (copy of D_800C4D38). */
    /* 0x02 */ s8 unk02;        /**< Marker — set to -1 on reset. */
    /* 0x03 */ s8 unk03;        /**< Marker — set to -1 on reset. */
} SceneState;

/**
 * @brief 12-byte keyed record used by D_800C9880's packed lookup buffer.
 *
 * Only @c key at +0x02 is known; the surrounding bytes are likely a
 * (key, value) descriptor but remain unmapped.
 */
typedef struct {
    u8 pad00[2];
    u8 key;
    u8 pad03[9];
} Entry12;

/**
 * @brief Packed length-prefixed buffer of Entry12 records.
 *
 * @c length is the total buffer length in bytes including the 4-byte
 * header. Records start at @c entries[0] and run until @c length bytes.
 */
typedef struct {
    u32 length;
    Entry12 entries[1];
} KeyBuffer;

/* world globals (we_object7) */
extern s32 D_800C4D2C;
extern s32 D_800C4D40;
extern s32 D_800C4D44;
extern s32 D_800C4D58;
extern s32 D_800C4D70;
extern s32 D_800C4D98;
extern s32 D_800C4D9C;
extern s32 D_800C4DC0;
extern s32 D_800C4DC4;
extern s32 D_800C4DC8;
extern s32 D_800C5924;
extern s32 D_800C5B50;
extern s32 D_800C5B54;
extern s32 D_800C5B58;
extern s32 D_800C5BFC;
extern s32 D_800C5C04;
extern s32 D_800C5C18;
extern s32 D_800C5C1C;
extern s32 D_800C5C20;
extern s32 D_800C5C24;
extern s32 D_800C5C28;
extern s32 D_800C5C2C;
extern s32 D_800C5C30;
extern s32 D_800C5C38;
extern s32 D_800C5C3C;
extern s32 D_800C5C40;
extern s32 D_800C5D54;
/**
 * @brief Pointer to the dialogue/action table buffer.
 *
 * The buffer begins with a u32 byte-offset table indexed by subject id;
 * each offset points (within the same buffer) to a 0-terminated list of
 * u32 byte-offsets to the actual entry payloads.
 */
extern u8 *D_800C976C;
extern u8  D_800C5984[];
extern u16 D_800C5C44[];
extern s32 D_800C97A4;
extern s32 D_800C9878;
extern s16 D_800C987C;
extern u8  D_800D23D8[];
extern s32 D_800DCB48;

/**
 * @brief Struct view over @c D_800D23D8 (world-state flag buffer).
 *
 * Documents the per-byte layout observed in callers: an opcode-param
 * byte at offset 0, a per-actor mode byte array starting at offset 2,
 * and a bit-flag byte at offset 0x66.  Use via
 * @c ((WorldFlags *)D_800D23D8)->field — the global itself stays
 * declared as @c u8[] so older callers that still index it as a byte
 * array keep working.
 */
typedef struct {
    /* 0x00 */ u8 opParam;            /**< Set from script opcode @c param. */
    /* 0x01 */ u8 pad01;
    /* 0x02 */ u8 actorMode[0x64];    /**< Per-actor mode byte. */
    /* 0x66 */ u8 flags;              /**< Bit flags (bits 0x20, 0x40 used). */
    /* 0x67 */ u8 pad67;
} WorldFlags;

/**
 * @brief Two-halfword script opcode entry (4 bytes).
 *
 * The world-engine script consists of a u32 array of segment offsets
 * (terminated by a zero offset), followed by ScriptOp entries pointed-to
 * by those offsets. Markers in the 0xFFxx range gate sub-states; non-FF
 * codes are dispatched to handlers.
 */
typedef struct {
    u16 op;
    u16 param;
} ScriptOp;

/**
 * @brief Per-actor 36-byte state record indexed by actor id.
 *
 * Used by the dialogue/animation orchestrator.  Currently mapped fields:
 * @c unk02 / @c unk03 (counters), @c flag1E (-1 disables actor), and
 * @c unk1F (signed phase byte read by @c func_800B674C).
 */
typedef struct {
    /* 0x00 */ u8 pad00[2];
    /* 0x02 */ s8 unk02;            /**< Signed counter / clamp value. */
    /* 0x03 */ u8 unk03;            /**< Byte counter (range-checked against 1..2). */
    /* 0x04 */ u8 pad04[0x1A];
    /* 0x1E */ s8 flag1E;           /**< -1 disables actor; otherwise active. */
    /* 0x1F */ s8 unk1F;            /**< Signed phase byte (-1, 0, 1). */
    /* 0x20 */ u8 pad20[4];
} ActorRecord;                       /* 0x24 bytes */

/**
 * @brief 32-byte node in a sorted keyframe list traversed by @c func_800B674C.
 *
 * The first halfword is a time/key value with sentinels @c -1 (end of
 * list) and @c -2 (skip / continuation).  Remaining 30 bytes hold the
 * keyframe payload, format-dependent on caller.
 */
typedef struct {
    /* 0x00 */ s16 time;
    /* 0x02 */ u8 pad02[0x1E];
} KeyframeNode;                      /* 0x20 bytes */

extern ActorRecord D_800DD6A8[];

/**
 * @brief 12-byte transform track (translation + y-rotation).
 *
 * Two of these live inside @c Slot at offsets 0x18 (track A) and 0x24
 * (track B). The translation z is negated when copied into a VECTOR
 * (PSX y-down convention).
 */
typedef struct {
    /* 0x00 */ s32 trans_x;
    /* 0x04 */ s32 trans_z;
    /* 0x08 */ s16 trans_y;
    /* 0x0A */ u16 rot_y;
} Track;

/**
 * @brief Slot-state record at @c *D_800D226C (used by 0xFF13/0xFF14
 * placement opcodes and 0xFF28 / 0xFF2E flag handlers).
 *
 * Holds two transform tracks and a 64-bit flag set; the trailing
 * @c bytes pair selects per-action data via the 0xFF2E handler.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x18];
    /* 0x18 */ Track tracks[2];   /**< Track A at 0x18, Track B at 0x24. */
    /* 0x30 */ u8 pad30[0x44];
    /* 0x74 */ u32 flags[2];      /**< 64-bit flag set (low/high). */
    /* 0x7C */ u8 bytes[2];       /**< Action bytes selectable by 0xFF2E. */
    /* 0x7E */ u8 pad7E[2];
} Slot;

/**
 * @brief 16-byte transform entry from @c *D_800D2128 — VECTOR translation
 * plus a partial (y, z) rotation pair.
 *
 * The first 12 bytes alias a @c VECTOR's vx/vy/vz translation; the trailing
 * 4 bytes hold a (y-rotation, z-rotation) pair that callers read separately.
 */
typedef struct {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ u16 rot_y;
    /* 0x0E */ s16 rot_z;
} TransformEntry;

extern u8 *D_800D2288;
extern Slot *D_800D226C;
extern TransformEntry *D_800D2128;

/** @brief One slot of a worldmap OT (ordering table), stride 0x60. */
typedef struct {
    P_TAG link;          /**< 0x00: P_TAG with low-24 next-prim addr. */
    u8    pad[0x58];     /**< 0x08..0x5F: rest of the slot (stride 0x60). */
} OTSlot;

/** @brief Entity model with a per-bone prim pointer table at @c +0x70. */
typedef struct {
    u8  pad00[0x70];
    u32 primList[1];     /**< +0x70: per-bone prim addr, indexed by bone id. */
} EntityModel;

extern EntityModel D_800CA040;          /**< Canonical worldmap entity model. */
extern s16         D_800C53B8[];        /**< Bone-id table (used by we_object4). */
extern OTSlot      D_800D3E98[2][3];    /**< Primary worldmap OT slot-B[cond][bone]. */
extern OTSlot      D_800D40D8[2][3];    /**< Secondary worldmap OT slot-B[cond][bone]. */

/* Slot-A arrays: each slot-B is paired with a slot-A 0x48 bytes earlier
 * in memory. The macros keep the toolchain emitting one named %hi/%lo
 * pair per primary symbol while call sites still read as struct array
 * accesses (e.g. @c &D_800D3E50[cond][i].link). */
#define D_800D3E50  (*(OTSlot (*)[2][3])((u8 *)D_800D3E98 - 0x48))
#define D_800D4090  (*(OTSlot (*)[2][3])((u8 *)D_800D40D8 - 0x48))

extern ScriptOp *func_800AF004(u8 *base, s32 flag);
extern s32 func_800AF28C(ScriptOp *p);
extern s32 func_800BEFC4(void);
extern void func_800BD82C(u8 *actor, SlotEntry *slot, s32 marker, s32 flag, SVECTOR *rot, VECTOR *trans);

#endif /* WORLD_H */
