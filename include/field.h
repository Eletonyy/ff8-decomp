#ifndef FIELD_H
#define FIELD_H

#include "common.h"

/*
 * ============================================================================
 * Field entity overlay (FieldEntity / Eline)
 * ============================================================================
 *
 * One field entity slot is 612 bytes (0x264). Three typedefs describe
 * the SAME 612-byte memory at @ref D_80085224, each capturing a
 * different valid type-interpretation of the bytes:
 *
 *   - @ref Eline       — bytecode VM view (stack[80], resultSlots[8],
 *                        msg* / field_0xNN naming, opcode handlers).
 *   - @ref FieldEntity — movement / animation view (walkSpeed[2],
 *                        unk1A0..unk1B3 sequencer state, used by
 *                        movement helpers in fe_object4/6/9/10).
 *   - @ref FieldActor  — animation slot view (AnimRec rows[4] /
 *                        timers[4] / animOffset / mode at the upper
 *                        half of the stack region; used by
 *                        @c func_800A355C only).
 *
 * Bytes 0x000..0x13F are the stack/animation/movement union — same
 * storage, three valid interpretations depending on which engine
 * subsystem owns the slot at a given moment. Bytes 0x140 onward are
 * named consistently across views and overlap at the same offsets.
 *
 * Cast between views with a plain pointer cast — no union member
 * notation is needed since the layouts agree at every offset that
 * isn't aliased.
 */

/**
 * @brief Field script entity / VM execution context — movement/animation view.
 *
 * See the overlay comment block above for the relationship to @ref Eline.
 *
 * Used by the field engine's stack-based virtual machine. Each entity
 * has a stack of s32 values (slots 0-95), a stack pointer, result
 * registers, and various state fields for animation, movement, and
 * script execution.
 */
typedef struct {
    s32 stack[80];          /**< 0x000: Stack slots (s32 each, indexed by stackIdx). */
    s32 result;             /**< 0x140: Result/output register. */
    s32 result2;            /**< 0x144: Secondary result register. */
    u8 pad148[0x18];        /**< 0x148 */
    s32 unk160;             /**< 0x160 */
    u8 pad164[0x12];        /**< 0x164 */
    u16 unk176;             /**< 0x176 */
    u8 pad178[0x0C];        /**< 0x178 */
    u8 stackIdx;            /**< 0x184: Stack pointer index. */
    u8 pad185[0x03];        /**< 0x185 */
    u8 unk188;              /**< 0x188: Script parameter byte. */
    u8 unk189;              /**< 0x189: Script parameter byte. */
    u16 unk18A;             /**< 0x18A: Step delta added to unk188 each movement tick. */
    u16 unk18C;             /**< 0x18C */
    u16 unk18E;             /**< 0x18E */
    u16 walkSpeed;          /**< 0x190: Walk speed (primary). */
    u16 walkSpeed2;         /**< 0x192: Walk speed (copy). */
    u16 runSpeed;           /**< 0x194: Run speed. */
    u8 pad196[0x06];        /**< 0x196 */
    u16 unk19C;             /**< 0x19C */
    u16 unk19E;             /**< 0x19E */
    u16 unk1A0;             /**< 0x1A0 */
    u16 unk1A2;             /**< 0x1A2 */
    u8 unk1A4;              /**< 0x1A4 */
    u8 unk1A5;              /**< 0x1A5 */
    u8 unk1A6;              /**< 0x1A6 */
    u8 unk1A7;              /**< 0x1A7 */
    u8 unk1A8;              /**< 0x1A8 */
    u8 unk1A9;              /**< 0x1A9 */
    u8 unk1AA;              /**< 0x1AA */
    u8 unk1AB;              /**< 0x1AB */
    u8 unk1AC;              /**< 0x1AC */
    u8 unk1AD;              /**< 0x1AD */
    u8 unk1AE;              /**< 0x1AE */
    u8 unk1AF;              /**< 0x1AF */
    u8 unk1B0;              /**< 0x1B0 */
    u8 unk1B1;              /**< 0x1B1 */
    u8 unk1B2;              /**< 0x1B2 */
    u8 unk1B3;              /**< 0x1B3 */
    u8 pad1B4[0x42];        /**< 0x1B4 */
    u16 unk1F6;             /**< 0x1F6 */
    u16 unk1F8;             /**< 0x1F8 */
    u8 pad1FA[0x1E];        /**< 0x1FA */
    u8 pad218[0x06];        /**< 0x218 */
    u16 unk21E;             /**< 0x21E */
    u8 pad220[0x20];        /**< 0x220 */
    u8 unk240;              /**< 0x240: Animation/display byte. */
    u8 pad241[0x04];        /**< 0x241 */
    u8 unk245;              /**< 0x245 */
    u8 pad246[0x03];        /**< 0x246 */
    u8 unk249;              /**< 0x249 */
    u8 pad24A;              /**< 0x24A */
    u8 unk24B;              /**< 0x24B */
    u8 unk24C;              /**< 0x24C */
} FieldEntity;              /* size >= 0x24D */

/**
 * @brief Field entity overlay used by the line-trigger family.
 *
 * Bytes 0x188-0x195 of an entity are reused by the @c SETLINE / @c LINEON
 * / @c LINEOFF opcodes to describe a 3D line-segment hit volume (despite
 * the wiki's "line" name, it's actually a 3D box bounded by the two
 * end-points). The same storage is interpreted as movement state
 * (@c walkSpeed / @c unk188 step counter) by the MOVE/MSPEED family —
 * use whichever overlay corresponds to the entity's role.
 *
 * @c lineActive byte 0 is the "line collision enabled" flag (set by
 * @c LINEON, cleared by @c LINEOFF) and byte 1 is the script-character
 * marker copied from @c D_800DE4FC by @c SETLINE at init.
 */
typedef struct {
    u8  pad000[0x188];      /**< 0x000 */
    u16 lineX1;             /**< 0x188 */
    u16 lineY1;             /**< 0x18A */
    u16 lineZ1;             /**< 0x18C */
    u16 lineX2;             /**< 0x18E */
    u16 lineY2;             /**< 0x190 */
    u16 lineZ2;             /**< 0x192 */
    u8  lineActive;         /**< 0x194: 1 while LINEON, 0 while LINEOFF. */
    u8  lineCharMarker;     /**< 0x195: D_800DE4FC snapshot, set by SETLINE. */
} FieldLineTrigger;

/**
 * @brief One slot of the @c SystemState mode-slot table at
 *        @c D_800704A8.slots, stride 28 bytes.
 *
 * Each slot encodes a mode-dispatched operation: @c mode selects which
 * code path runs in the field engine's per-frame poller, @c param
 * carries a target-entity byte (e.g. partyId), @c submode is a
 * sub-state byte cleared on init, @c timer is a halfword countdown,
 * and @c p1 / @c p2 are two halfword parameters consumed by the mode
 * body.
 */
typedef struct {
    /* 0x00 */ u8 mode;
    /* 0x01 */ u8 param;
    /* 0x02 */ u8 submode;
    /* 0x03 */ u8 pad03;
    /* 0x04 */ u16 timer;
    /* 0x06 */ u16 unk06;            /**< Cleared on submode=0 entry by @c func_800A10F4. */
    /* 0x08 */ u16 q1;
    /* 0x0A */ u16 q2;
    /* 0x0C */ u16 savedQ1;          /**< Snapshot of @c q1 captured by @c func_800A10F4. */
    /* 0x0E */ u16 savedQ2;          /**< Snapshot of @c q2 captured by @c func_800A10F4. */
    /* 0x10 */ u16 p1;
    /* 0x12 */ u16 p2;
    /* 0x14 */ u16 p3;
    /* 0x16 */ u16 p4;
    /* 0x18 */ u16 p5;
    /* 0x1A */ u16 p6;
} SystemSubMode; /* 0x1C = 28 bytes */

/**
 * @brief One slot of the field-engine's event-queue array at
 *        @c D_8005F0F8->entries, stride 32 bytes.
 *
 * The array is scanned linearly for the entry whose @c field16 equals
 * the sentinel @c 0x7FFF; that entry is then populated with the
 * pushed event parameters and the next entry is re-armed as the new
 * sentinel.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x04];
    /* 0x04 */ u16 field04;
    /* 0x06 */ u16 field06;
    /* 0x08 */ u16 field08;
    /* 0x0A */ u16 field0A;
    /* 0x0C */ u16 position_x;  /**< Snapshot field copied to @c D_800704A8.position_x by @c func_8009AA64. */
    /* 0x0E */ u16 position_y;  /**< Snapshot field copied to @c D_800704A8.position_y. */
    /* 0x10 */ u16 rotation;    /**< Snapshot field copied to @c D_800704A8.rotation. */
    /* 0x12 */ u16 counter;     /**< Snapshot field copied to @c D_800704A8.counter; @c < 72 selects mode 1, else 7. */
    /* 0x14 */ u16 field14;     /**< Set to @c 0xFFFF when the slot is armed. */
    /* 0x16 */ u16 field16;     /**< Slot key / sentinel marker (@c 0x7FFF == free). */
    /* 0x18 */ u8 pad18[0x04];
    /* 0x1C */ u8 anim_state;   /**< Snapshot field copied to @c D_800704A8.anim_state (low byte). */
    /* 0x1D */ u8 pad1D[0x03];
} EventEntry; /* 0x20 = 32 bytes */

/** @brief Container struct at @c D_8005F0F8; first 0x60 bytes are header data, then 16 event entries. */
typedef struct {
    /* 0x00 */ u8 pad00[0x0E];
    /* 0x0E */ u8 unk0E;            /**< When @c == 1, @c func_800A1BB8 issues a StoreImage to VRAM. */
    /* 0x0F */ u8 pad0F[0x51];
    /* 0x60 */ EventEntry entries[16];
} EventQueue;

extern EventQueue *D_8005F0F8;

/** @brief System state block (at @c D_800704A8); also aliased as @c g_fieldEntity. */
typedef struct {
    /* 0x000 */ u8 mode;            /**< Top-level engine mode; @c 4 means exit. */
    /* 0x001 */ u8 pad001;
    /* 0x002 */ s16 counter;
    /* 0x004 */ u16 position_x;     /**< Snapshotted X coordinate for the active party slot. */
    /* 0x006 */ u16 position_y;     /**< Snapshotted Y coordinate for the active party slot. */
    /* 0x008 */ u16 unk008;
    /* 0x00A */ u8 pad00A[0x02];
    /* 0x00C */ u16 rotation;       /**< Snapshotted heading for the active party slot. */
    /* 0x00E */ u16 anim_state;     /**< Snapshotted animation byte for the active party slot. */
    /* 0x010 */ u8 pad010[0x02];
    /* 0x012 */ u8 entityIndex[3];  /**< Per-active-slot field-entity index (mirror of g_fieldVars->memberSlot[]). */
    /* 0x015 */ u8 unk015;          /**< Cleared by @c opHandler_UCON along with the trigger flag. */
    /* 0x016 */ u8 pad016[0x06];
    /* 0x01C */ s32 fieldStepDelta; /**< Step delta passed to @c func_800BD804 each field tick. */
    /* 0x020 */ SystemSubMode slots[8]; /**< 8 mode/param slots, stride 28; slot 0 corresponds to the legacy @c unk020..unk032 fields. */
    /* 0x100 */ u8 pad100[0x02];
    /* 0x102 */ u16 unk102;
    /* 0x104 */ u16 unk104;
    /* 0x106 */ u16 unk106;
    /* 0x108 */ u16 dialogState;    /**< Dialog state word (0=init, 2=run, 4=force-complete). */
    /* 0x10A */ u16 dialogTimer;    /**< Dialog timer target. */
    /* 0x10C */ u16 dialogCount;    /**< Dialog countdown — compared against @c dialogTimer to advance state. */
    /* 0x10E */ u16 field_0x10E;
    /* 0x110 */ u16 field_0x110;
    /* 0x112 */ u16 field_0x112;
    /* 0x114 */ u16 field_0x114;
    /* 0x116 */ u16 field_0x116;
    /* 0x118 */ u16 field_0x118;
    /* 0x11A */ u16 field_0x11A;
    /* 0x11C */ u16 field_0x11C;
    /* 0x11E */ u16 field_0x11E;
    /* 0x120 */ u16 field_0x120;    /**< Snapshotted misc halfword; preserved across SaveSnapshot/RestoreSnapshot. */
    /* 0x122 */ u8 unk122;          /**< Cleared together with @c unk130 by an fe_object6 opcode. */
    /* 0x123 */ u8 pad123[0x03];
    /* 0x126 */ u16 unk126;
    /* 0x128 */ u8 pad128[0x04];
    /* 0x12C */ u16 unk12C;
    /* 0x12E */ u8 pad12E[0x02];
    /* 0x130 */ u8 unk130;
    /* 0x131 */ u8 pad131[0x03];
    /* 0x134 */ u16 unk134;
    /* 0x136 */ u8 pad136[0x04];
    /* 0x13A */ u16 unk13A;
    /* 0x13C */ u8 pad13C[0x14];
    /* 0x150 */ s32 unk150;         /**< Bit-6/7 source for @c func_8009A7E8 's per-entity trigger7 write. */
    /* 0x154 */ s32 unk154;         /**< Bit-6/7 mask gating @c func_8009A7E8 's write (inverse of @c unk150). */
    /* 0x158 */ s32 ambientFlags;   /**< Ambient SFX/state flags; bits 6-7 gate the fade-out path in @c func_800BD9C4. */
    /* 0x15C */ u8 pad15C[0x24];
    /* 0x180 */ u8 unkActive180[16]; /**< 16-byte active-marker region, cleared on @c func_800BF718 mode 1 init. */
    /* 0x190 */ u8 slotActive[16];
    /* 0x1A0 */ u8 unk1A0;          /**< Mode-6 active marker, set with mode = 6 by fe_object6 opcode. */
    /* 0x1A1 */ u8 unk1A1;          /**< Cleared unconditionally by @c func_800A5700 each dialog tick. */
    /* 0x1A2 */ u8 unk1A2;          /**< Mode-7 reentry guard byte. */
    /* 0x1A3 */ u8 unk1A3;          /**< Set to 1 by @c opHandler_UCOFF on every call (re-arm guard). */
    /* 0x1A4 */ u8 pad1A4[0x02];
    /* 0x1A6 */ u8 unk1A6;          /**< Cleared by @c func_800BFBBC on full reset. */
    /* 0x1A7 */ u8 pad1A7[0x04];
    /* 0x1AB */ u8 unk1AB;          /**< Sub-mode byte; written together with @c mode by fe_object6 opcodes. */
    /* 0x1AC */ u8 pad1AC[0x02];
    /* 0x1AE */ u8 unk1AE;          /**< Script-writable byte (set by opcode handler @c opHandler_COUNTERCLOCKWISETURN2, read by @c func_8009FE18). */
    /* 0x1AF */ u8 packedFlagSlot;  /**< Last @c getPackedField2Bit result for the active dispatcher slot; written each tick by @c func_800BD9C4. */
    /* 0x1B0 */ u8 pad1B0[0x04];
    /* 0x1B4 */ s32 field1B4;       /**< Initialised to @c 0xFFFFFF by @c func_800BFBBC on full reset. */
    /* 0x1B8 */ u8 statusBits[0x40]; /**< Packed bit-array (512 bits); set by @c opHandler_IDLOCK, cleared by @c opHandler_IDUNLOCK, zeroed during init. */
} SystemState;

extern SystemState D_800704A8;

/**
 * @brief 256-byte misc3 region of @c GameState — held at @c g_gameState+0xD60
 * and aliased through the @c g_fieldVars pointer.
 *
 * Despite the name, this region tracks general field/world state — step
 * accumulators that drive periodic ticks, SeeD experience and rank
 * bookkeeping, sound channel handles, the packed 2-bit flag table, party
 * ordering, audio channel state, etc. Only partially mapped; fields are
 * added as their usages are identified.
 */
typedef struct {
    /* 0x00 */ u32 pad00;
    /* 0x04 */ s32 stepCounter;         /**< Total step delta accumulator, mirrored to D_80082C14. */
    /* 0x08 */ s32 seedExpStepAcc;      /**< Step accumulator: fires the SeeD level-up tick at @c 0x6000. */
    /* 0x0C */ s32 hpRegenStepAcc;      /**< Step accumulator: fires HP regen ticks at @c 8. */
    /* 0x10 */ u16 seedExp;             /**< SeeD experience (clamped to [100, 3100]; level = exp/100). */
    /* 0x12 */ u16 prevKillSum;         /**< Last frame's total enemy-kill count across all 8 chars. */
    /* 0x14 */ s32 field14;             /**< Copied from @c g_gameState[0xCDC] by @c func_800BFBBC. */
    /* 0x18 */ u16 field18;             /**< Copied from @c g_gameState[0xCE0]. */
    /* 0x1A */ u16 field1A;             /**< Copied from @c g_gameState[0xCE2]. */
    /* 0x1C */ u16 charKills[8];        /**< Snapshot of @c chars[i].kills, refreshed by @c func_800BFBBC. */
    /* 0x2C */ u16 charKos[8];          /**< Snapshot of @c chars[i].kos. */
    /* 0x3C */ u8 pad3C[0x08];
    /* 0x44 */ s32 killSum;             /**< Sum of @c chars[i].kills across all 8 characters. */
    /* 0x48 */ s32 gilMirror;           /**< Mirror of @c g_gameState.mainData.party.gil, kept in sync by fe_object6. */
    /* 0x4C */ s32 dreamGilMirror;      /**< Mirror of @c g_gameState.mainData.party.dreamGil. */
    /* 0x50 */ s32 padInitStatus;       /**< Result of @c func_801E8B58 (pad-init status), updated each field tick. */
    /* 0x54 */ u16 field54;             /**< Mirror of @c D_800704A8.field120, set by @c func_800BFBBC. */
    /* 0x56 */ u8 field56;              /**< Copy of @c D_80082C8D byte, set by @c func_800BFBBC. */
    /* 0x57 */ u8 field57;              /**< Low byte of @c D_8005F14C, set by @c func_800BFBBC. */
    /* 0x58 */ u8 field58;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0x59 */ u8 pad59[0x0F];
    /* 0x68 */ s32 stateFlags;          /**< Field state flags (bits 3-4 checked by getFieldStateFlags). */
    /* 0x6C */ s32 soundHandle0;        /**< Sound channel 0 handle (music; @ref SND_HANDLE_NONE = inactive). */
    /* 0x70 */ s32 soundHandle1;        /**< Sound channel 1 handle (SFX; @ref SND_HANDLE_NONE = inactive). */
    /* 0x74 */ u8 packedFlags[0x40];    /**< Packed 2-bit-per-entry flag table (256 entries, indexed by 8-bit key). */
    /* 0xB4 */ u16 packedFlagsStepAcc;  /**< Step accumulator: fires packed-flags processing at @c 0x2800. */
    /* 0xB6 */ u16 fieldB6;             /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xB8 */ u16 levelUpDisplayTimer; /**< Frames remaining for the SeeD-rank-up notification (set to 150). */
    /* 0xBA */ u16 prevSeedExp;         /**< Snapshot of @c seedExp from the previous tick (for rank-change detection). */
    /* 0xBC */ u8 partyOrderA[3];       /**< Bench list (members not in active party). */
    /* 0xBF */ u8 partyOrderB[3];       /**< Bench list duplicate (initialized identically). */
    /* 0xC2 */ u8 memberSlot[3];        /**< For each active party slot, the Eline index (0xFF = none). */
    /* 0xC5 */ u8 musicVolume;          /**< Music channel volume (0..0x7F). */
    /* 0xC6 */ u8 sfxVolume;            /**< Sound-effects channel volume (0..0x7F). */
    /* 0xC7 */ s8 audioChannel0State;   /**< Audio channel 0 state byte; -1 = reset/inactive. */
    /* 0xC8 */ s8 audioChannel1State;   /**< Audio channel 1 state byte; -1 = reset/inactive. */
    /* 0xC9 */ s8 soundBankSelector;    /**< Sound bank toggle (0 or 1). */
    /* 0xCA */ s8 audioChannel2State;   /**< Audio channel 2 state byte; -1 = reset/inactive. */
    /* 0xCB */ u8 battleMusicId;        /**< Set by @c opHandler_SETBATTLEMUSIC. */
    /* 0xCC */ u8 expectedDiscId;       /**< Currently inserted disc (1..4). The intro/disc-swap screen waits for @c getDiscId() to match. */
    /* 0xCD */ u8 cameraShakeX;         /**< Camera shake X intensity, popped from stack. */
    /* 0xCE */ u8 cameraShakeY;         /**< Camera shake Y intensity, popped from stack. */
    /* 0xCF */ u8 fieldCF;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xD0 */ u8 padD0;
    /* 0xD1 */ u8 fieldD1;              /**< Bit 0 toggled by fe_object6 helper. */
    /* 0xD2 */ u8 sfxActiveMask;        /**< Per-slot SFX active bitmask (set on play, cleared on completion). */
    /* 0xD3 */ u8 sfxStartMask;         /**< Per-slot SFX start bitmask (set on play). */
    /* 0xD4 */ u8 sfxEntryMask;         /**< Per-slot SFX entry-table bitmask (set when slot is registered in @c D_80085300). */
    /* 0xD5 */ u8 nextSoundBank;        /**< Sound bank ID staged by MUSICCHANGE; copied into @c audioChannel0State on swap. */
    /* 0xD6 */ u8 soundLoadComplete;    /**< Set to 1 after sound bank loading finishes. */
    /* 0xD7 */ u8 padD7;
    /* 0xD8 */ u16 dialogStateMirror;   /**< Mirror of @c D_800704A8.dialogState (kept in sync by fe_object9). */
    /* 0xDA */ u16 fieldDA;
    /* 0xDC */ u16 fieldDC;
    /* 0xDE */ u16 fieldDE;
    /* 0xE0 */ u16 fieldE0;
    /* 0xE2 */ u16 fieldE2;
    /* 0xE4 */ u16 fieldE4;
    /* 0xE6 */ u16 fieldE6;
    /* 0xE8 */ u16 fieldE8;
    /* 0xEA */ u16 fieldEA;
    /* 0xEC */ u16 fieldEC;
    /* 0xEE */ u16 fieldEE;
    /* 0xF0 */ u8 fieldF0;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xF1 */ u8 fieldF1;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xF2 */ u8 fieldF2;              /**< Set to popped field index by fe_object7 dispatch handler. */
    /* 0xF3 */ u8 fieldF3;              /**< Mirrored to D_80082C10 when stateFlags bit 0x800 is set. */
    /* 0xF4 */ s32 angeloLearnStepAcc;  /**< Step accumulator: fires the Angelo trick learn tick at @c 0x250. */
    /* 0xF8 */ u8 padF8[0x08];
} FieldVars; /* 0x100 = 256 bytes */

/**
 * @brief Eline (event line) — opcode handler / script-VM view.
 *
 * See the overlay comment block before @ref FieldEntity for the relationship
 * between the two typedefs. Both describe the same 612-byte field entity
 * slot, with this view named for opcode handler usage. The size discrepancy
 * (FieldEntity ≥ 0x24D, Eline ~0x264) is because each typedef only declares
 * up through the highest offset its callers reference; both bound the same
 * 612-byte allocation.
 */
typedef struct {
    /* 0x000 */ s32 stack[80];      /**< Bytecode stack slots (s32 each, indexed by @c stackPtr). */
    /* 0x140 */ s32 resultSlots[8]; /**< Result-slot register file (opcodes 0x08/0x09 read/write). */
    /* 0x160 */ s32 flags;
    /* 0x164 */ u16 groupRanges[8]; /**< Per-script-group saved PC table (0xFFFF = empty); written by @c opHandler_RET. */
    /* 0x174 */ u8 scriptGroup;     /**< Script group index. */
    /* 0x175 */ u8 activeMask;      /**< Entity active bitmask. */
    /* 0x176 */ u16 pc;             /**< Script-VM program counter (advanced by JMP / JPF; pushed by CAL). */
    /* 0x178 */ u16 rangeLo;        /**< Lower bound of script trigger range (tested by @c func_800BE44C). */
    /* 0x17A */ u16 rangeHi;        /**< Upper bound of script trigger range. */
    /* 0x17C */ u8 groupStackBase[8]; /**< Per-script-group saved stackPtr base (used by context-switch in @c opHandler_RET). */
    /* 0x184 */ s8 stackPtr;        /**< Bytecode stack pointer (signed, grows down). */
    /* 0x185 */ u8 pad185[0x03];
    /* 0x188 */ u8 unk188;          /**< Script parameter byte. */
    /* 0x189 */ u8 unk189;          /**< Script parameter byte. */
    /* 0x18A */ u16 unk18A;         /**< Step delta added to unk188 each movement tick. */
    /* 0x18C */ u16 unk18C;         /**< Halfword saved during async msg state. */
    /* 0x18E */ u16 unk18E;         /**< Halfword saved during async msg state. */
    /* 0x190 */ s32 posX;           /**< Entity X position (fixed-point). */
    /* 0x194 */ s32 posY;           /**< Entity Y position (fixed-point). */
    /* 0x198 */ s32 posZ;           /**< Entity Z position (fixed-point). */
    /* 0x19C */ u8 pad19C[0x0C];
    /* 0x1A8 */ s32 unk1A8;
    /* 0x1AC */ s32 unk1AC;
    /* 0x1B0 */ s32 unk1B0;
    /* 0x1B4 */ s32 msgTextPtr;     /**< Message text pointer (fixed-point). */
    /* 0x1B8 */ s32 msgPosX;        /**< Message X position (fixed-point). */
    /* 0x1BC */ s32 msgPosY;        /**< Message Y position (fixed-point). */
    /* 0x1C0 */ s32 field_0x1C0;    /**< Saved message text pointer. */
    /* 0x1C4 */ s32 field_0x1C4;    /**< Saved message X position. */
    /* 0x1C8 */ s32 field_0x1C8;    /**< Saved message Y position. */
    /* 0x1CC */ u8 pad1CC[0x0C];
    /* 0x1D8 */ u16 field_0x1D8;
    /* 0x1DA */ u16 field_0x1DA;
    /* 0x1DC */ s16 field_0x1DC;
    /* 0x1DE */ s16 field_0x1DE;
    /* 0x1E0 */ u16 field_0x1E0;
    /* 0x1E2 */ u16 field_0x1E2;
    /* 0x1E4 */ u16 field_0x1E4;
    /* 0x1E6 */ u16 field_0x1E6;
    /* 0x1E8 */ u16 field_0x1E8;
    /* 0x1EA */ u16 field_0x1EA;
    /* 0x1EC */ u16 field_0x1EC;
    /* 0x1EE */ u16 field_0x1EE;
    /* 0x1F0 */ u16 field_0x1F0;
    /* 0x1F2 */ u16 field_0x1F2;
    /* 0x1F4 */ u16 field_0x1F4;
    /* 0x1F6 */ u16 radius;         /**< Collision radius (used by @c func_8009E468 overlap test). */
    /* 0x1F8 */ u16 talkRadius;     /**< Set by @c opHandler_TALKRADIUS; read alongside @c radius by @c func_8009F74C 's asymmetric overlap test. */
    /* 0x1FA */ u16 field_0x1FA;    /**< Set from path-table entry's @c unk6 by @c func_8009BB18. */
    /* 0x1FC */ u16 field_0x1FC;
    /* 0x1FE */ s16 savedChannel;   /**< Previous message channel. */
    /* 0x200 */ u16 msgChannel;     /**< Current message channel. */
    /* 0x202 */ u16 field_0x202;    /**< Saved channel for async restore. */
    /* 0x204 */ s16 field_0x204;
    /* 0x206 */ u16 field_0x206;
    /* 0x208 */ u16 field_0x208;
    /* 0x20A */ u16 field_0x20A;
    /* 0x20C */ u16 field_0x20C;
    /* 0x20E */ u16 field_0x20E;
    /* 0x210 */ u16 field_0x210;
    /* 0x212 */ u16 field_0x212;
    /* 0x214 */ u16 field_0x214;
    /* 0x216 */ u16 field_0x216;
    /* 0x218 */ s16 unk218;         /**< -1 = inactive (skipped by collision tests in @c func_8009E468). */
    /* 0x21A */ u16 windowId;       /**< Message window ID. */
    /* 0x21C */ u16 field_0x21C;    /**< Saved window ID for async restore. */
    /* 0x21E */ s16 msgState;       /**< Message state (0=init, 2=complete). */
    /* 0x220 */ u16 field_0x220;
    /* 0x222 */ u16 field_0x222;
    /* 0x224 */ u16 field_0x224;
    /* 0x226 */ u16 field_0x226;
    /* 0x228 */ u16 field_0x228;
    /* 0x22A */ u16 field_0x22A;
    /* 0x22C */ u16 field_0x22C;
    /* 0x22E */ u16 field_0x22E;
    /* 0x230 */ u16 field_0x230;
    /* 0x232 */ u16 field_0x232;
    /* 0x234 */ u16 field_0x234;
    /* 0x236 */ u16 field_0x236;
    /* 0x238 */ u8 field_0x238;
    /* 0x239 */ u8 field_0x239;
    /* 0x23A */ u8 field_0x23A;
    /* 0x23B */ u8 field_0x23B;
    /* 0x23C */ u8 msgActive;       /**< Message active flag. */
    /* 0x23D */ u8 pad23D[0x02];
    /* 0x23F */ u8 unk23F;          /**< Anim-sync byte; @c func_8009A7E8 's diff-window check uses @c entity->unk19C - @c eline->unk23F. */
    /* 0x240 */ u8 field_0x240;
    /* 0x241 */ u8 field_0x241;
    /* 0x242 */ u8 field_0x242;
    /* 0x243 */ u8 field_0x243;
    /* 0x244 */ u8 field_0x244;
    /* 0x245 */ u8 unk245;
    /* 0x246 */ u8 pad246[0x02];
    /* 0x248 */ u8 unk248;          /**< Set to 1 by @c func_8009E468 when colliding with self entity; also acts as delayed-SFX trigger 6 in @c func_800BD9C4. */
    /* 0x249 */ u8 unk249;          /**< @c 0 = enable @c unk248 update path in @c func_8009E468. */
    /* 0x24A */ u8 triggerSfx7;     /**< Delayed-SFX trigger 7 (consumed by @c func_800BD9C4); @c 2 also flips @c flags bit @c 0x20. */
    /* 0x24B */ u8 field_0x24B;
    /* 0x24C */ u8 field_0x24C;
    /* 0x24D */ u8 field_0x24D;
    /* 0x24E */ u8 field_0x24E;
    /* 0x24F */ u8 field_0x24F;
    /* 0x250 */ u8 field_0x250;
    /* 0x251 */ u8 field_0x251;
    /* 0x252 */ u8 field_0x252;
    /* 0x253 */ u8 field_0x253;
    /* 0x254 */ u8 field_0x254;
    /* 0x255 */ u8 field_0x255;
    /* 0x256 */ u8 field_0x256;
    /* 0x257 */ u8 field_0x257;
    /* 0x258 */ u8 unk258;          /**< Set from path-table entry's @c unk8 by @c func_8009BB18. */
    /* 0x259 */ u8 field_0x259;
    /* 0x25A */ u8 field_0x25A;
    /* 0x25B */ u8 field_0x25B;
    /* 0x25C */ u8 field_0x25C;
    /* 0x25D */ u8 field_0x25D;
    /* 0x25E */ u8 field_0x25E;
    /* 0x25F */ u8 field_0x25F;
    /* 0x260 */ u8 field_0x260;
    /* 0x261 */ u8 field_0x261;
    /* 0x262 */ u8 field_0x262;
    /* 0x263 */ u8 field_0x263;
} Eline;

/** @brief Push one s32 onto the eline's bytecode stack. */
#define PUSH(eline, val) (((s32 *)(eline))[(s8)(++(eline)->stackPtr)] = (val))

/** @brief Pop one s32 from the eline's bytecode stack. */
#define POP(eline) (((s32 *)(eline))[(s8)(eline)->stackPtr--])

/** @brief Pop one s32 then read low byte only. */
#define POP_BYTE(eline) (*(u8 *)&POP(eline))

/** @brief Pop one s32 then read low signed halfword (sign-extended to s32). */
#define POP_HALF(eline) (*(s16 *)&POP(eline))

/** @brief Read the top s32 from the eline's bytecode stack without popping. */
#define PEEK(eline) (((s32 *)(eline))[(eline)->stackPtr])

/** @brief SeeD salary lookup table indexed by SeeD level (exp / 100). */
extern u16 g_seedSalaryTable[];

/**
 * @brief Field script-VM opcode dispatch table.
 *
 * 392-entry function-pointer table at @c 0x800C6760. Indices 0-17
 * are a 18-entry stack-arithmetic sub-table (ADD, SUB, MUL, DIV, MOD,
 * NEG, EQ, ..., AND, OR, XOR, ...) accessed by @c opHandler_CAL
 * (the "meta-dispatcher") which receives the sub-opcode in @c a1 and
 * tail-calls @c g_fieldOpcodeTable[a1].
 *
 * Indices 18-391 are the main opcode dispatch table. The runtime
 * dispatcher (@c func_800BEBD0 / @c func_800BD9C4) loads
 * @c g_fieldOpcodeTable + 0x48 (i.e. our index 18) as its base, so
 * wiki opcode @c N (0x000-0x175) corresponds to our index @c N + 0x12.
 * See @c src/field/opcodes.c for the full opcode-to-handler mapping.
 */
extern s32 (*g_fieldOpcodeTable[392])();

/** @brief Read the 2-bit packed flag at the given key (256-entry table). */
extern s32 getPackedField2Bit(s32 key);

/** @brief Field-only party-member status predicate (used by dialog opcodes). */
extern s32 func_800211B4(s32 partyMember, s32 code);

/** @brief Field-engine PRNG; consumed by random encounter / step ticks. */
extern s32 fieldRandom(void);

/**
 * @brief Per-entity render slot (rotational/animation state).
 *
 * One slot per field-engine entity, addressed via @ref D_800D9630. The
 * full structure is around 0x64 bytes; only the fields touched by the
 * opcode handlers are named so far.
 */
typedef struct {
    u8 pad00[0x0C];
    u16 unk0C;
    u8 pad0E[0x44];
    u16 unk52;       /**< Current motion halfword (mirror of Eline @c field_0x206). */
    u8 pad54[0x0C];
    u8 unk60;
    u8 unk61;
    u16 unk62;
} EntityRenderSlot;

/** @brief Entity render-slot pointer table; indexed by @c eline->field_0x256. */
extern EntityRenderSlot *D_800D9630[];

/** @brief Entity Eline pointer table; indexed by raw field-entity id. */
extern Eline *D_80085230[];

/** @brief Eline entity array (count @c D_80085388, stride 612). */
extern Eline *D_80085224;

/** @brief Number of entries in the @c D_80085224 entity array. */
extern u8 D_80085388;

/**
 * @brief Block B field entity — stride @c 0x1A0, dispatched by
 *        @c func_800BD9C4 alongside the full-sized @c Eline pool.
 *
 * Shares the script-VM state layout with @c Eline at offsets
 * @c 0x160..0x178, but with a 7-byte SFX trigger region at @c 0x194..0x19B
 * instead of the full @c Eline body. Used by the field engine for compact
 * actors that don't need the full 612-byte slot.
 */
typedef struct {
    /* 0x000 */ u8  pad000[0x160];
    /* 0x160 */ s32 flags;
    /* 0x164 */ u16 groupRanges[8];
    /* 0x174 */ u8  scriptGroup;
    /* 0x175 */ u8  activeMask;
    /* 0x176 */ u16 pc;
    /* 0x178 */ u16 rangeLo;
    /* 0x17A */ u16 rangeHi;
    /* 0x17C */ u8  pad17C[0x08];
    /* 0x184 */ s8  stackPtr;
    /* 0x185 */ u8  pad185[0x0F];
    /* 0x194 */ u8  activeMarker;       /**< Block-active gate; non-zero enables trigger processing. */
    /* 0x195 */ u8  pad195;
    /* 0x196 */ u8  trigger4;       /**< Cleared together with @c unk19D by @c func_8009A8E0. */
    /* 0x197 */ u8  trigger5;
    /* 0x198 */ u8  trigger6;
    /* 0x199 */ u8  trigger7;       /**< Set to 1/2 by @c func_8009A7E8 when the active-marker test + diff window pass. */
    /* 0x19A */ u8  trigger2;
    /* 0x19B */ u8  trigger3;
    /* 0x19C */ u8  unk19C;         /**< Anim-sync byte compared (with diff bias) against @c eline->unk23F by @c func_8009A7E8. */
    /* 0x19D */ u8  unk19D;         /**< Per-entity flag cleared by @c func_8009A8E0 alongside @c trigger4. */
    /* 0x19E */ u8  pad19E[0x02];
} FieldEntityB; /* 0x1A0 */

/**
 * @brief Block C field entity — stride @c 0x18C, dispatched by
 *        @c func_800BD9C4. Same script-VM front-end as @c FieldEntityB
 *        but with only two SFX triggers at @c 0x18A / @c 0x18B.
 */
typedef struct {
    /* 0x000 */ u8  pad000[0x160];
    /* 0x160 */ s32 flags;
    /* 0x164 */ u16 groupRanges[8];
    /* 0x174 */ u8  scriptGroup;
    /* 0x175 */ u8  activeMask;
    /* 0x176 */ u16 pc;
    /* 0x178 */ u16 rangeLo;
    /* 0x17A */ u16 rangeHi;
    /* 0x17C */ u8  pad17C[0x08];
    /* 0x184 */ s8  stackPtr;
    /* 0x185 */ u8  pad185[0x03];
    /* 0x188 */ u8  activeMarker;
    /* 0x189 */ u8  pad189;
    /* 0x18A */ u8  trigger6;
    /* 0x18B */ u8  trigger7;
} FieldEntityC; /* 0x18C */

/**
 * @brief Block D field entity — stride @c 0x1B4, dispatched by
 *        @c func_800BD9C4. Same script-VM front-end; no SFX triggers,
 *        each tick ends with a call to @c func_800B2BA0.
 */
typedef struct {
    /* 0x000 */ u8  pad000[0x160];
    /* 0x160 */ s32 flags;
    /* 0x164 */ u16 groupRanges[8];
    /* 0x174 */ u8  scriptGroup;
    /* 0x175 */ u8  activeMask;
    /* 0x176 */ u16 pc;
    /* 0x178 */ u16 rangeLo;
    /* 0x17A */ u16 rangeHi;
    /* 0x17C */ u8  pad17C[0x08];
    /* 0x184 */ s8  stackPtr;
    /* 0x185 */ u8  pad185[0x03];
    /* 0x188 */ s16 unk188;
    /* 0x18A */ u16 unk18A;
    /* 0x18C */ u8  pad18C[0x1E];
    /* 0x1AA */ u8  unk1AA;
    /* 0x1AB */ u8  unk1AB;
    /* 0x1AC */ u8  unk1AC;
    /* 0x1AD */ u8  pad1AD[0x07];
} FieldEntityD; /* 0x1B4 */

/** @brief Self/anchor pointer used by @c func_8009A8E0 after the
 *         party-member copy in @c opHandler_COPYINFO / @c opHandler_PCOPYINFO; also
 *         the base of the Block B entity array dispatched by @c func_800BD9C4. */
extern FieldEntityB *D_8008538C;

/** @brief Number of entries in the @c D_8008538C entity array. */
extern u8 D_800852F8;

/** @brief Block C entity-array base; count is @c D_80085228. */
extern FieldEntityC *D_80085384;

/** @brief Number of entries in the @c D_80085384 entity array. */
extern u8 D_80085228;

/** @brief Block D entity-array base; count is @c D_80085391. */
extern FieldEntityD *D_800852F4;

/** @brief Number of entries in the @c D_800852F4 entity array. */
extern u8 D_80085391;

/** @brief Per-block iteration counter used by @c func_800BD9C4 inner loops. */
extern u8 D_800DE4FC;

/** @brief Field-engine opcode argument table base; indexed by @c pc. */
extern s32 *D_80085380;

/** @brief Field-engine bit-0 lock byte read by Block C of @c func_800BD9C4. */
extern u8 D_800704BD;

extern void func_80037B7C(s32 *base, s32 *outA, s32 *outB);
extern s32  func_801E8B58(void);

/**
 * @brief Animation slot record (one of four per actor).
 *
 * Used by @c func_800A355C to drive per-actor animation playback. Lives
 * inside the @c FieldActor view (see below) at offset @c 0x80, which
 * shares storage with @c Eline.stack[32..49] — the bytecode VM only
 * uses stack slots @c 0..31 during script execution, so the upper
 * half of the stack region doubles as animation state.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x10];
    /* 0x10 */ s16 id;              /**< Animation ID, -1 = empty slot. */
} AnimRec; /* 0x12 = 18 bytes */

/**
 * @brief Animation-mode view of a field entity (alternate typedef
 *        over the same memory as @ref Eline).
 *
 * The bytecode VM's @c stack[80] region (offsets @c 0x00..0x13F of
 * @c Eline) is repurposed as animation state once the script reaches
 * a movement opcode and surrenders its stack frame: @c rows[4]
 * occupies @c 0x80..0xC7, @c timers[4] occupies @c 0xC8..0xCF, and
 * @c animOffset / @c mode appear at @c 0xF4 / @c 0xFC. The rest of
 * the struct is identical to @c Eline from offset @c 0x140 onward.
 *
 * Use a cast (@c (FieldActor *)&D_80085224[idx]) only inside the
 * animation engine; everywhere else, access via @c Eline.
 */
typedef struct {
    /* 0x000 */ u8 pad000[0x80];
    /* 0x080 */ AnimRec rows[4];    /**< Four animation slots, stride 0x12. */
    /* 0x0C8 */ s16 timers[4];      /**< Per-slot tick counters. */
    /* 0x0D0 */ u8 padD0[0x24];
    /* 0x0F4 */ s16 animOffset;     /**< Byte offset from @c rows[] to source row table. */
    /* 0x0F6 */ u8 padF6[0x06];
    /* 0x0FC */ s16 mode;           /**< Dispatch mode (1/2/3 = different sources). */
    /* 0x0FE */ u8 padFE[0x166];    /**< Rest of the 612-byte slot mirrors @c Eline. */
} FieldActor; /* 0x264 = 612 bytes */


/** @brief Update one packed-flag table slot from a step tick. */
extern void func_800383B8(s32 key, s32 status);

/** @brief Trigger SeeD rank-up notification (palette transition phase 7). */
extern void setTransitionPhase7(void);

/* ======================================================================== */
/* Field-overlay SFX / animation entry tables                                */
/* ======================================================================== */

/**
 * @brief One slot in the field SFX-shadow table @ref D_80085300.
 *
 * Populated by the field-script VM when an SFX instance is registered.
 * @c rect is a 4-halfword on-screen rectangle (used by text/balloon
 * SFX), @c payload typically holds the SFX data pointer cast to s32,
 * and @c volume / @c type mirror the values previously set via
 * @c setSfxEntryVolume / @c setSfxEntityType for the slot. Distinct
 * from the runtime @c SfxEntry in @c battle.h, which is the active
 * playback state — this is just the script-VM's last-set shadow.
 */
typedef struct {
    /* 0x0 */ u16 rect[4];
    /* 0x8 */ s32 payload;
    /* 0xC */ u16 volume;
    /* 0xE */ u16 type;
} FieldSfxSlot;

/** @brief Per-slot SFX shadow table populated by field-VM SFX opcodes. */
extern FieldSfxSlot D_80085300[];

/**
 * @brief One slot in the field anim-shadow table @ref D_80085398.
 *
 * Eight halfwords of opcode-supplied animation parameters. @c flag at
 * @c 0x0 is the slot-active marker (cleared by @c func_800BD1A4),
 * @c field2..fieldE are the per-opcode arg shadow (the same values
 * forwarded to @c setupAnimEntry / @c setupAnimEntryFull / @c updateAnimEntry).
 */
typedef struct {
    /* 0x0 */ s16 flag;
    /* 0x2 */ s16 field2;
    /* 0x4 */ s16 field4;
    /* 0x6 */ s16 field6;
    /* 0x8 */ s16 field8;
    /* 0xA */ s16 fieldA;
    /* 0xC */ u16 fieldC;
    /* 0xE */ u16 fieldE;
} FieldAnimSlot;

/** @brief Per-slot anim shadow table populated by field-VM anim opcodes. */
extern FieldAnimSlot D_80085398[];

/** @brief Small on-screen rectangle in halfword coords (used by SFX balloons). */
typedef struct {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} Rect;

/** @brief Generic SFX/text data pointer used as base for @c func_8003974C. */
extern u8 *D_800704C0;

/** @brief Spatial-entity dispatch context word (passed to @c func_800A8DAC). */
extern u32 D_800C71F8;

/** @brief Field-side dialog companion scalar. */
extern s32 D_800DE4DC;

/** @brief Stashed SFX global flag, saved/restored around dialog SFX. */
extern s32 D_800DE4D8;

/** @brief Global SFX-status flags packed scalar (tested with 0xC0 etc.). */
extern s32 D_80070600;

/** @brief Active field-script entity index (mirrors @c eline->field_0x256). */
extern u8 D_800DE4FC;
extern u8 D_800DE4FD[];
extern u8 D_80085390;
extern u8 D_800704BB;
extern u8 D_800704BC;

/**
 * @brief Field-engine status word array; @c D_800DE8C8[1] aliases
 *        @ref D_800DE8CC. The two are different views of the same
 *        memory — target asm picks one form per call site (the array
 *        form folds the base+4 into a single lhu, the scalar form
 *        uses a separate lui+addiu).
 */
extern s32 D_800DE8C8[];

/** @brief Status word at @c D_800DE8C8[1] (separate symbol for codegen). */
extern s32 D_800DE8CC;

/** @brief Dialog-companion flag byte tested by some opcodes. */
extern u8 D_800DE8D0;

/** @brief SFX/sample-load ready flag — flipped on by the @c func_800B2188
 *         load-complete callback and polled by @c EFFECTLOAD. */
extern u8 D_800DE8D5;

/** @brief Misc 4-byte field-engine slot; stash for @c func_801E8104's return
 *         value in @c MOVIEREADY / @c SPUREADY. */
extern s32 D_800DE4EC;

/** @brief Movie subsystem state pointer (or buffer base) — stored to as
 *         a single u32 by the battle-load opcodes. */
extern u8 D_800DE878[];

/* ======================================================================== */
/* Battle encounter params (populated by field-VM opcode 0x14C)             */
/* ======================================================================== */

/**
 * @brief Battle encounter setup parameters at @ref D_80082C90.
 *
 * Populated by the field-VM @c initiateBattleEncounter opcode just
 * before it kicks off the battle transition. The trailing @c result
 * is written by the battle overlay once the fight finishes and is
 * read back on the @c return-pass of the same field opcode.
 */
typedef struct {
    /* 0x00 */ s32 encounterPtr;
    /* 0x04 */ u8 field_04;
    /* 0x05 */ u8 field_05;
    /* 0x06 */ u8 field_06;
    /* 0x07 */ u8 field_07;
    /* 0x08 */ u8 field_08;
    /* 0x09 */ u8 field_09;
    /* 0x0A */ u8 pad0A[2];
    /* 0x0C */ u8 result;
} EncounterParams;

extern EncounterParams D_80082C90;

/** @brief Mirrored from @ref FieldVars.fieldF3 when @c stateFlags & 0x800 is set. */
extern u8 D_80082C10;

/** @brief Stashed sound-bank selector across the battle transition. */
extern u8 D_80082C11;

/* ======================================================================== */
/* Field-side scalars consumed by fe_object7 / fe_object8 / fe_object9       */
/* ======================================================================== */

/** @brief Sound-load handshake byte for the battle-fade sequence. */
extern u8 D_8007064C;

/** @brief Misc menu/field share scalar. */
extern u16 D_8007737C;

/** @brief Field-side post-battle flag byte. */
extern u8 D_800773C0;

/** @brief Stashed text-id buffer base shared by field-VM string opcodes. */
extern u8 D_8005630C[];

/** @brief Sound-init parameter passed to @c func_80037FB0. */
extern s32 D_8005F13C;

/** @brief Misc sound state halfword used by @c func_800BF718; low byte stashed
 *         into @c FieldVars.field57 on full field reset. */
extern u16 D_8005F14C;

/** @brief Misc field byte; copied into @c FieldVars.field56 on full reset. */
extern u8  D_80082C8D;

/** @brief Bitfield words tested by fe_object4 opcode handlers. */
extern s32 D_800705E8;
extern s32 D_800705F0;
extern s32 D_800705F8;

/** @brief Misc field-VM byte; set by fe_object4 from @c g_fieldVars->fieldF3. */
extern u8  D_80077E5F;

/** @brief @c &g_gameState.fieldVars exposed as a byte array for fe_object4's
 *         script-VM M-memory load/store opcodes (offsets are popped from the
 *         eline stack). */
extern u8  D_800780D8[];

/** @brief Field-side status flag byte; bitfield (0x1, 0x2, 0x4, 0x8, 0x10, 0x20). */
extern u8  D_8007809A;

/** @brief Mirror of @c g_fieldVars->stepCounter (s32). */
extern s32 D_80082C14;

/** @brief Pool sizer for entity/script tables; called from @c fe_object10. */
extern s32 func_80037AEC(u8 *header, u16 *table, s32 **outBase);

/** @brief Field-side rotation/orientation halfword consumed by encounter setup. */
extern s16 D_800704B2;

/** @brief Dialog companion halfword (mirrors @c D_800DE4DC s32 view). */
extern s16 D_800DE4D0;

/** @brief Per-script-group scratch bytes used by fe_object7 dialog opcodes. */
extern s8 D_800DE4D2;
extern s8 D_800DE4D3;
extern s8 D_800DE4D4;

/** @brief Eline-pointer slots used by fe_object7 to remember last-active entities. */
extern Eline *D_800DE4F0;
extern Eline *D_800DE4F4;
extern Eline *D_800DE4F8;

/** @brief Per-text status array consumed by fe_object7 dialog flow. */
extern u8 D_800DE880[];

/**
 * @brief Field-VM event header — script blob preamble parsed by @c func_800BE30C.
 *
 * Each script blob begins with this header. Three byte flags at the
 * start, a 4th flag at offset 3, then two 16-bit offsets that point
 * to sub-tables inside the blob. The actual bytecode starts at
 * @c +0x8.
 */
typedef struct {
    /* 0x0 */ u8 field0;
    /* 0x1 */ u8 field1;
    /* 0x2 */ u8 field2;
    /* 0x3 */ u8 field3;
    /* 0x4 */ u16 offset4;
    /* 0x6 */ u16 offset6;
} EventHeader;

extern u8 D_800DE8D8;       /**< Mirror of @c EventHeader.field2 from the active script. */
extern u8 D_800DE8D9;       /**< Mirror of @c EventHeader.field0. */
extern u8 D_800DE8DA;       /**< Mirror of @c EventHeader.field1. */
extern u8 D_800DE8C0;       /**< Mirror of @c EventHeader.field3. */
extern u16 *D_800DE4E0;     /**< Pointer to script bytecode start (header base + 8); read as packed u16s. */
extern u8 *D_800DE4E4;      /**< Pointer to first sub-table (header base + offset4). */
extern u8 *D_800DE4E8;      /**< Pointer to second sub-table (header base + offset6). */

/* ======================================================================== */
/* Field-side scalars consumed by fe_object6 (SFX / camera / GF opcodes)    */
/* ======================================================================== */

/** @brief Per-channel SFX bank lookup table; indexed by the SFX dispatcher. */
extern u8 *D_800D5EA4;

/** @brief Misc field-side scratch bytes used by fe_object6. */
extern u8 D_8007064A;
extern u8 D_8007064B;
extern u8 D_8007064D;
extern u8 D_8007064E;
extern u8 D_8007064F[];
extern u8 D_8007065C[];
extern u8 D_80070652;
extern u8 D_800704CA;

/** @brief Field-side rotation/orientation halfword consumed by camera opcodes. */
extern u16 D_800704AA;

/** @brief Dialog dispatch mode shared with @ref D_800704A8.dialogState. */
extern u8 D_800DE8D2;

/* ======================================================================== */
/* Spatial / text / SFX helpers (defined in main binary)                    */
/* ======================================================================== */

/**
 * @brief Per-entity spatial dispatch on @ref D_800D9630 render slots.
 *
 * Dispatched on @c cmd; selected cmds:
 *   - @c 0x1E writes 4 grid-cell halfwords to @c out.
 *   - @c 0x20 writes 3 relative-offset halfwords to @c out.
 *   - @c 0x1F returns a per-entity matrix-buffer pointer (returned in @c v0).
 *   - @c 0x2F / @c 0x2E set per-entity flags / queue voice/SFX.
 *
 * Returns a per-entity pointer in @c v0 (used by callers of @c 0x1F);
 * callers that ignore the return value just drop it.
 */
/** @brief Index into a null-terminated entry table by skipping strings. */
extern u8 *func_8003974C(u8 *base, s32 idx);

/** @brief Measure a text string, returning width|height packed as one s32. */
extern s32 func_8002E680(u8 *text);

/** @brief Stash an SFX-slot scalar (paramY/Z/W/V signature). */
extern void func_8002D784(s32 sfxIdx, u8 *data, s32 paramY, s32 paramZ, s32 paramW, s32 paramV);

/** @brief Read the per-slot SFX status word at the table offset 0x?. */
extern s32 func_8002CE84(s32 idx);

/* ======================================================================== */
/* fe_object5 movie-load tables and movie-overlay (0x801E0000) entry points */
/* ======================================================================== */

/** @brief Battle-encounter scratch buffer; fe_object5 hands the pointer to
 *         @c loadBattleCmd as the asset-payload base. */
extern u8 D_800C5FB0[];

/** @brief CD-entry @c {LBA,size} pairs for movie load opcodes (op04F MOVIE). */
extern u32 D_800C2D14[];
/** @brief CD-entry @c {LBA,size,_,_} quadruples for the alt movie loader. */
extern u32 D_800C2E14[];
/** @brief CD-entry @c {LBA,size} pairs for the SPU-stream loader (op056). */
extern u32 D_800C2E1C[];

/** @brief Movie overlay (loaded at @c 0x801E0000) entry points called by
 *         the field-VM movie / SPU-stream opcodes. */
extern void func_801E8000(s32 priority);
extern s32  func_801E8104(s32 a, s32 b, s32 c, s32 d);
extern s32  func_801E82CC(void);
extern void func_801E870C(void);
extern s32  func_801E8B98(void);

#endif /* FIELD_H */
