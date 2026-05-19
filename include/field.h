#ifndef FIELD_H
#define FIELD_H

#include "common.h"

/*
 * ============================================================================
 * Field entity overlay (FieldEntity / Eline)
 * ============================================================================
 *
 * One field entity slot is 612 bytes (0x264). Both @ref FieldEntity and
 * @ref Eline below describe the SAME memory — they are two struct typedefs
 * over identical addresses, used by different parts of the engine to express
 * different valid type-interpretations of the bytes.
 *
 * TODO: Unify the structs
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
    /* 0x06 */ u8 pad06[0x02];
    /* 0x08 */ u16 q1;
    /* 0x0A */ u16 q2;
    /* 0x0C */ u8 pad0C[0x04];
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
    /* 0x0A */ u8 pad0A[0x0A];
    /* 0x14 */ u16 field14;     /**< Set to @c 0xFFFF when the slot is armed. */
    /* 0x16 */ u16 field16;     /**< Slot key / sentinel marker (@c 0x7FFF == free). */
    /* 0x18 */ u8 pad18[0x08];
} EventEntry; /* 0x20 = 32 bytes */

/** @brief Container struct at @c D_8005F0F8; first 0x60 bytes are header data, then 16 event entries. */
typedef struct {
    /* 0x00 */ u8 pad00[0x60];
    /* 0x60 */ EventEntry entries[16];
} EventQueue;

extern EventQueue *D_8005F0F8;

/** @brief System state block (at @c D_800704A8). */
typedef struct {
    /* 0x000 */ u8 mode;
    /* 0x001 */ u8 pad001;
    /* 0x002 */ s16 counter;
    /* 0x004 */ u16 unk004;
    /* 0x006 */ u16 unk006;
    /* 0x008 */ u16 unk008;
    /* 0x00A */ u8 pad00A[0x02];
    /* 0x00C */ u16 unk00C;
    /* 0x00E */ u16 unk00E;
    /* 0x010 */ u8 pad010[0x02];
    /* 0x012 */ u8 entityIndex[3];  /**< Per-active-slot field-entity index (mirror of g_seedState->memberSlot[]). */
    /* 0x015 */ u8 pad015[0x0B];
    /* 0x020 */ SystemSubMode slots[8]; /**< 8 mode/param slots, stride 28; slot 0 corresponds to the legacy @c unk020..unk032 fields. */
    /* 0x100 */ u8 pad100[0x02];
    /* 0x102 */ u16 unk102;
    /* 0x104 */ u16 unk104;
    /* 0x106 */ u16 unk106;
    /* 0x108 */ u8 pad108[0x1A];
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
    /* 0x13C */ u8 pad13C[0x54];
    /* 0x190 */ u8 slotActive[16];
    /* 0x1A0 */ u8 unk1A0;          /**< Mode-6 active marker, set with mode = 6 by fe_object6 opcode. */
    /* 0x1A1 */ u8 pad1A1;
    /* 0x1A2 */ u8 unk1A2;          /**< Mode-7 reentry guard byte. */
    /* 0x1A3 */ u8 pad1A3[0x08];
    /* 0x1AB */ u8 unk1AB;          /**< Sub-mode byte; written together with @c mode by fe_object6 opcodes. */
    /* 0x1AC */ u8 pad1AC[0x02];
    /* 0x1AE */ u8 unk1AE;          /**< Script-writable byte (set by opcode handler @c func_800B85C8, read by @c func_8009FE18). */
} SystemState;

extern SystemState D_800704A8;

/**
 * @brief 256-byte misc3 region of @c GameState — held at @c g_gameState+0xD60
 * and aliased through the @c g_seedState pointer.
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
    /* 0x14 */ u8 pad14[0x34];
    /* 0x48 */ s32 gilMirror;           /**< Mirror of @c g_gameState.mainData.party.gil, kept in sync by fe_object6. */
    /* 0x4C */ s32 dreamGilMirror;      /**< Mirror of @c g_gameState.mainData.party.dreamGil. */
    /* 0x50 */ u8 pad50[0x08];
    /* 0x58 */ u8 field58;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0x59 */ u8 pad59[0x0F];
    /* 0x68 */ s32 stateFlags;          /**< Field state flags (bits 3-4 checked by getFieldStateFlags). */
    /* 0x6C */ s32 soundHandle0;        /**< Sound channel handle 0. */
    /* 0x70 */ s32 soundHandle1;        /**< Sound channel handle 1 (-1 = inactive). */
    /* 0x74 */ u8 packedFlags[0x40];    /**< Packed 2-bit-per-entry flag table (256 entries, indexed by 8-bit key). */
    /* 0xB4 */ u16 packedFlagsStepAcc;  /**< Step accumulator: fires packed-flags processing at @c 0x2800. */
    /* 0xB6 */ u16 fieldB6;             /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xB8 */ u16 levelUpDisplayTimer; /**< Frames remaining for the SeeD-rank-up notification (set to 150). */
    /* 0xBA */ u16 prevSeedExp;         /**< Snapshot of @c seedExp from the previous tick (for rank-change detection). */
    /* 0xBC */ u8 partyOrderA[3];       /**< Bench list (members not in active party). */
    /* 0xBF */ u8 partyOrderB[3];       /**< Bench list duplicate (initialized identically). */
    /* 0xC2 */ u8 memberSlot[3];        /**< For each active party slot, the Eline index (0xFF = none). */
    /* 0xC5 */ u8 padC5[0x02];
    /* 0xC7 */ s8 audioChannel0State;   /**< Audio channel 0 state byte; -1 = reset/inactive. */
    /* 0xC8 */ s8 audioChannel1State;   /**< Audio channel 1 state byte; -1 = reset/inactive. */
    /* 0xC9 */ u8 soundBankSelector;    /**< Sound bank toggle (0 or 1). */
    /* 0xCA */ s8 audioChannel2State;   /**< Audio channel 2 state byte; -1 = reset/inactive. */
    /* 0xCB */ u8 padCB;
    /* 0xCC */ u8 expectedDiscId;       /**< Currently inserted disc (1..4). The intro/disc-swap screen waits for @c getDiscId() to match. */
    /* 0xCD */ u8 cameraShakeX;         /**< Camera shake X intensity, popped from stack. */
    /* 0xCE */ u8 cameraShakeY;         /**< Camera shake Y intensity, popped from stack. */
    /* 0xCF */ u8 fieldCF;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xD0 */ u8 padD0;
    /* 0xD1 */ u8 fieldD1;              /**< Bit 0 toggled by fe_object6 helper. */
    /* 0xD2 */ u8 sfxActiveMask;        /**< Per-slot SFX active bitmask (set on play, cleared on completion). */
    /* 0xD3 */ u8 sfxStartMask;         /**< Per-slot SFX start bitmask (set on play). */
    /* 0xD4 */ u8 padD4[0x02];
    /* 0xD6 */ u8 soundLoadComplete;    /**< Set to 1 after sound bank loading finishes. */
    /* 0xD7 */ u8 padD7;
    /* 0xD8 */ u16 fieldD8;             /**< Mirrored from D_800704A8+0x108 by fe_object9 dialog helpers. */
    /* 0xDA */ u8 padDA[0x16];          /**< 0xDA..0xEF */
    /* 0xF0 */ u8 fieldF0;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xF1 */ u8 fieldF1;              /**< Used by fe_object7 dispatch (purpose TBD). */
    /* 0xF2 */ u8 fieldF2;              /**< Set to popped field index by fe_object7 dispatch handler. */
    /* 0xF3 */ u8 fieldF3;              /**< Mirrored to D_80082C10 when stateFlags bit 0x800 is set. */
    /* 0xF4 */ s32 angeloLearnStepAcc;  /**< Step accumulator: fires the Angelo trick learn tick at @c 0x250. */
    /* 0xF8 */ u8 padF8[0x08];
} SeedState; /* 0x100 = 256 bytes */

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
    /* 0x164 */ u8 pad164[0x10];
    /* 0x174 */ u8 scriptGroup;     /**< Script group index. */
    /* 0x175 */ u8 activeMask;      /**< Entity active bitmask. */
    /* 0x176 */ u8 pad176[0x0E];
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
    /* 0x1F6 */ u8 pad1F6[0x04];
    /* 0x1FA */ u16 field_0x1FA;
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
    /* 0x218 */ u8 pad218[0x02];
    /* 0x21A */ u16 windowId;       /**< Message window ID. */
    /* 0x21C */ u16 field_0x21C;    /**< Saved window ID for async restore. */
    /* 0x21E */ s16 msgState;       /**< Message state (0=init, 2=complete). */
    /* 0x220 */ u16 field_0x220;
    /* 0x222 */ u16 field_0x222;
    /* 0x224 */ u16 field_0x224;
    /* 0x226 */ u16 field_0x226;
    /* 0x228 */ u8 pad228[0x02];
    /* 0x22A */ u16 field_0x22A;
    /* 0x22C */ u8 pad22C[0x02];
    /* 0x22E */ u16 field_0x22E;
    /* 0x230 */ u8 pad230[0x02];
    /* 0x232 */ u16 field_0x232;
    /* 0x234 */ u16 field_0x234;
    /* 0x236 */ u16 field_0x236;
    /* 0x238 */ u8 pad238[0x03];
    /* 0x23B */ u8 field_0x23B;
    /* 0x23C */ u8 msgActive;       /**< Message active flag. */
    /* 0x23D */ u8 pad23D[0x03];
    /* 0x240 */ u8 field_0x240;
    /* 0x241 */ u8 field_0x241;
    /* 0x242 */ u8 field_0x242;
    /* 0x243 */ u8 field_0x243;
    /* 0x244 */ u8 field_0x244;
    /* 0x245 */ u8 unk245;
    /* 0x246 */ u8 pad246[0x07];
    /* 0x24D */ u8 field_0x24D;
    /* 0x24E */ u8 field_0x24E;
    /* 0x24F */ u8 field_0x24F;
    /* 0x250 */ u8 field_0x250;
    /* 0x251 */ u8 field_0x251;
    /* 0x252 */ u8 pad252[0x03];
    /* 0x255 */ u8 field_0x255;
    /* 0x256 */ u8 field_0x256;
    /* 0x257 */ u8 field_0x257;
    /* 0x258 */ u8 pad258;
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
 * NEG, EQ, ..., AND, OR, XOR, ...) accessed by @c func_800AE048
 * (the "meta-dispatcher") which receives the sub-opcode in @c a1 and
 * tail-calls @c g_fieldOpcodeTable[a1].
 *
 * Indices 18-391 are the main opcode dispatch table. The runtime
 * dispatcher (@c func_800BEBD0 / @c func_800BD9C4) loads
 * @c g_fieldOpcodeTable + 0x48 (i.e. our index 18) as its base, so
 * wiki opcode @c N (0x000-0x175) corresponds to our index @c N + 0x12.
 * See @c src/field/opcodes.c for the full opcode-to-handler mapping.
 */
extern s32 (*g_fieldOpcodeTable[392])(Eline *eline);

/** @brief Read the 2-bit packed flag at the given key (256-entry table). */
extern s32 getPackedField2Bit(s32 key);

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

/** @brief Issue a dispatch command via the active spatial-entity table. */
extern void func_800AA46C(u8 spatialIdx, s32 cmd, s32 arg, s32 arg4);

/** @brief Motion-command setup: cmd 0xD with byte arg. */
extern void func_800B912C(Eline *eline, s16 a1);

/** @brief Update one packed-flag table slot from a step tick. */
extern void func_800383B8(s32 key, s32 status);

/** @brief Trigger SeeD rank-up notification (palette transition phase 7). */
extern void setTransitionPhase7(void);

#endif /* FIELD_H */
