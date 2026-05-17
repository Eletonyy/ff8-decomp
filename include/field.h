#ifndef FIELD_H
#define FIELD_H

#include "common.h"

/**
 * @brief Field script entity / VM execution context.
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
    u8 pad18A[0x06];        /**< 0x18A */
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

/** @brief System state block (at @c D_800704A8). */
typedef struct {
    /* 0x000 */ u8 mode;
    /* 0x001 */ u8 pad001;
    /* 0x002 */ s16 counter;
    /* 0x004 */ u8 pad004[0x0E];
    /* 0x012 */ u8 entityIndex[3];  /**< Per-active-slot field-entity index (mirror of g_seedState->memberSlot[]). */
    /* 0x015 */ u8 pad015[0xED];
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
    /* 0x1A0 */ u8 pad1A0[0x0B];
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
    /* 0x14 */ u8 pad14[0x44];
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
    /* 0xC2 */ u8 memberSlot[3];        /**< For each active party slot, the BattleFieldEntity index (0xFF = none). */
    /* 0xC5 */ u8 padC5[0x02];
    /* 0xC7 */ s8 audioChannel0State;   /**< Audio channel 0 state byte; -1 = reset/inactive. */
    /* 0xC8 */ s8 audioChannel1State;   /**< Audio channel 1 state byte; -1 = reset/inactive. */
    /* 0xC9 */ u8 soundBankSelector;    /**< Sound bank toggle (0 or 1). */
    /* 0xCA */ s8 audioChannel2State;   /**< Audio channel 2 state byte; -1 = reset/inactive. */
    /* 0xCB */ u8 padCB;
    /* 0xCC */ u8 expectedDiscId;       /**< Currently inserted disc (1..4). The intro/disc-swap screen waits for @c getDiscId() to match. */
    /* 0xCD */ u8 padCD[0x02];
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
 * @brief Eline (event line) — opcode handler view of the script context.
 *
 * Same memory as FieldEntity, but with fields named for opcode handler usage.
 * Total size: 416 bytes (0x1A0), confirmed by sizeof(eline) debug print.
 */
typedef struct {
    /* 0x000 */ u8 pad000[0x140];
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
    /* 0x1DC */ u8 pad1DC[0x04];
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
    /* 0x206 */ u8 pad206[0x14];
    /* 0x21A */ u16 windowId;       /**< Message window ID. */
    /* 0x21C */ u16 field_0x21C;    /**< Saved window ID for async restore. */
    /* 0x21E */ s16 msgState;       /**< Message state (0=init, 2=complete). */
    /* 0x220 */ u8 pad220[0x1C];
    /* 0x23C */ u8 msgActive;       /**< Message active flag. */
    /* 0x23D */ u8 pad23D[0x03];
    /* 0x240 */ u8 field_0x240;
    /* 0x241 */ u8 pad241[0x04];
    /* 0x245 */ u8 unk245;
    /* 0x246 */ u8 pad246[0x08];
    /* 0x24E */ u8 field_0x24E;
    /* 0x24F */ u8 field_0x24F;
    /* 0x250 */ u8 field_0x250;
    /* 0x251 */ u8 field_0x251;
    /* 0x252 */ u8 pad252[0x03];
    /* 0x255 */ u8 field_0x255;
    /* 0x256 */ u8 field_0x256;
    /* 0x257 */ u8 pad257[0x0B];
    /* 0x262 */ u8 field_0x262;
} Eline;

/** @brief Push one s32 onto the eline's bytecode stack. */
#define PUSH(eline, val) (((s32 *)(eline))[(s8)(++(eline)->stackPtr)] = (val))

/** @brief Pop one s32 from the eline's bytecode stack. */
#define POP(eline) (((s32 *)(eline))[(s8)(eline)->stackPtr--])

/** @brief Pop one s32 then read low byte only. */
#define POP_BYTE(eline) (*(u8 *)&POP(eline))

/** @brief Read the top s32 from the eline's bytecode stack without popping. */
#define PEEK(eline) (((s32 *)(eline))[(eline)->stackPtr])

/** @brief SeeD salary lookup table indexed by SeeD level (exp / 100). */
extern u16 g_seedSalaryTable[];

/** @brief Read the 2-bit packed flag at the given key (256-entry table). */
extern s32 getPackedField2Bit(s32 key);

/** @brief Field-engine PRNG; consumed by random encounter / step ticks. */
extern s32 fieldRandom(void);

/** @brief Update one packed-flag table slot from a step tick. */
extern void func_800383B8(s32 key, s32 status);

/** @brief Trigger SeeD rank-up notification (palette transition phase 7). */
extern void setTransitionPhase7(void);

#endif /* FIELD_H */
