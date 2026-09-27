#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"

// kernel.bin, loaded whole at g_kernel. Section layout per the FF8 Modding Wiki
// (technical-reference/main/kernel/header); offsets match the disc's kernel.bin.

/** @brief Battle command / ability entry (8 bytes). */
typedef struct {
    u16 statParam0;   /**< +0x00: Name text offset. */
    u16 statParam1;   /**< +0x02: Description text offset. */
    u8 cap;           /**< +0x04: Ability cap value (checked by GetAbilityCap). */
    u8 typeField;     /**< +0x05: Command type ID or element/status type. */
    u8 bonusField;    /**< +0x06: Bonus value or GF field B value. */
    u8 extraField;    /**< +0x07: Status immunity byte, party flag, or mode selector. */
} AbilityEntry; /* 8 bytes */

/** @brief Magic entry (60 bytes). */
typedef struct {
    u16 nameParam0;   /**< +0x00: Name text offset (getMagicNamePtr). */
    u16 nameParam1;   /**< +0x02: Description text offset (getSpellDesc). */
    u8 pad04;         /**< +0x04: Unknown. */
    u8 spiritMult;    /**< +0x05: Spirit/magic junction multiplier. */
    u8 magicBase;     /**< +0x06: Magic power base value. */
    u8 pad07;         /**< +0x07: Unknown. */
    u8 statBase;      /**< +0x08: Compatibility/stat modifier base. */
    u8 statMult;      /**< +0x09: Multiplier/junction quantity. */
    u8 flagBits;      /**< +0x0A: Bit-field flags (srav source). */
    u8 juncId;        /**< +0x0B: Junction ID. */
    u8 hitMult;       /**< +0x0C: Hit/accuracy bonus multiplier. */
    u8 juncId2;       /**< +0x0D: Secondary junction ID. */
    u16 flags;        /**< +0x0E: Status/attribute flags. */
    u16 flags2;       /**< +0x10: Secondary flags. */
    u8 pad12[0x0B];   /**< +0x12..+0x1C: Unknown. */
    u8 spiritParam;   /**< +0x1D: Spirit parameter (func_800220E4). */
    u8 magicParam;    /**< +0x1E: Magic parameter (func_80022028). */
    u8 pad1F;         /**< +0x1F: Unknown. */
    u8 statParamA;    /**< +0x20: Stat parameter A (func_8002216C). */
    u8 statParamB;    /**< +0x21: Stat parameter B (func_800221B4). */
    u8 defElemFlag;   /**< +0x22: Element defense flag (checked for bit in getElemResistance). */
    u8 defElemMult;   /**< +0x23: Element defense multiplier (getElemResistance). */
    u8 hitParam;      /**< +0x24: Hit parameter (func_80022404). */
    u8 defStatusBase; /**< +0x25: Status defense base value (getStatusResistance). */
    u16 statusFlags;  /**< +0x26: Status flags bitmask (func_80022328/func_80022370). */
    u16 defStatusFlags;/**< +0x28: Status defense flags (checked for bit in getStatusResistance). */
    u8 pad2A[0x12];   /**< +0x2A..+0x3B: Unknown. */
} MagicEntry; /* 60 bytes */

/** @brief Character entry (36 bytes). */
typedef struct {
    u16 lookupParam;  /**< +0x00: Name text offset (getBattleCharName/getCharName). */
    u8 pad02;         /**< +0x02: Unknown. */
    u8 field03;       /**< +0x03: Bit 0 enables @c BattleSlot.slotFlags @c 0x100 in @c func_800A7518. */
    u8 pad04[2];      /**< +0x04..+0x05: Unknown. */
    u8 linearCoeff;   /**< +0x06: XP curve linear coefficient. */
    u8 quadDivisor;   /**< +0x07: XP curve quadratic divisor. */
    u8 constant;      /**< +0x08: XP curve constant term. */
    u8 pad09[3];      /**< +0x09..+0x0B: Unknown. */
    u8 field0C;       /**< +0x0C: Unknown. */
    u8 field0D;       /**< +0x0D: Unknown. */
    u8 subIdx;        /**< +0x0E: calcHpFromLevel multiplier. */
    u8 divisorField;  /**< +0x0F: calcHpFromLevel divisor. */
    u8 addend;        /**< +0x10: calcHpFromLevel addend. */
    u8 pad11[0x13];   /**< +0x11..+0x23: Unknown. */
} CharacterEntry; /* 36 bytes */

#define GF_ABILITY_SLOT_COUNT 21

/** @brief One of a junctionable GF's learnable-ability slots (4 bytes). */
typedef struct {
    u8 pad00[2];           /**< +0x00..+0x01: Unknown. */
    u8 abilityId;          /**< +0x02: Ability ID (0 = empty). */
    u8 pad03;              /**< +0x03: Unknown. */
} GfAbilitySlot; /* 4 bytes */

/** @brief Junctionable GF entry (132 bytes). */
typedef struct {
    u16 nameOffset;        /**< +0x00: Name text offset. */
    u16 descOffset;        /**< +0x02: Description text offset. */
    u8 pad04[16];          /**< +0x04..+0x13: Attack data. */
    u8 xpLinear;           /**< +0x14: Curve linear coefficient (func_8002172C). */
    u8 xpQuadDiv;          /**< +0x15: Curve quadratic divisor. */
    u8 xpConst;            /**< +0x16: Curve constant term. */
    u8 xpParamA;           /**< +0x17: Unknown. */
    u8 xpParamB;           /**< +0x18: Linear coeff for evalAbilityCurve. */
    u8 xpParamC;           /**< +0x19: Quad divisor for evalAbilityCurve. */
    u8 pad1A[2];           /**< +0x1A..+0x1B: Unknown. */
    GfAbilitySlot abilities[GF_ABILITY_SLOT_COUNT]; /**< +0x1C..+0x6F */
    u8 pad70[20];          /**< +0x70..+0x83: Compatibility and power data. */
} JunctionableGfEntry; /* 132 bytes */

/** @brief Weapon entry (12 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getWeaponName). */
    u8 pad02[5];   /**< +0x02..+0x06: Unknown. */
    u8 field07;    /**< +0x07: Used in func_80022028. */
    u8 pad08;      /**< +0x08: Unknown. */
    u8 field09;    /**< +0x09: Returned directly by @c func_800A7A44 as a per-class lookup byte. */
    u8 pad0A;      /**< +0x0A: Unknown. */
    u8 field0B;    /**< +0x0B: Bit 0 enables @c BattleSlot.slotFlags @c 0x1000 in @c func_800A7518. */
} WeaponEntry; /* 12 bytes */

/** @brief Battle item entry (24 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getItemName). */
    u16 param1;    /**< +0x02: Description text offset (getItemDesc). */
    u8 pad04[20];  /**< +0x04..+0x17: Unknown. */
} BattleItemEntry; /* 24 bytes */

/** @brief Non-battle item entry (4 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getItemName). */
    u16 param1;    /**< +0x02: Description text offset (getItemDesc). */
} NonBattleItemEntry; /* 4 bytes */

/** @brief Renzokuken finisher entry (24 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getRenzokukenFinisherName). */
    u16 param1;    /**< +0x02: Description text offset (getRenzokukenFinisherDesc). */
    u8 pad04[20];  /**< +0x04..+0x17: Unknown. */
} RenzokukenFinisherEntry; /* 24 bytes */

/** @brief Temporary character limit break entry (24 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getTempLimitBreakName). */
    u16 param1;    /**< +0x02: Description text offset (getTempLimitBreakDesc). */
    u8 pad04[20];  /**< +0x04..+0x17: Unknown. */
} TempLimitBreakEntry; /* 24 bytes */

/** @brief Blue magic entry (16 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getBlueMagicName). */
    u16 param1;    /**< +0x02: Description text offset (getBlueMagicDesc). */
    u8 pad04[12];  /**< +0x04..+0x0F: Unknown. */
} BlueMagicEntry; /* 16 bytes */

/** @brief Shot (Irvine limit break) entry (24 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getShotName). */
    u16 param1;    /**< +0x02: Description text offset (getShotDesc). */
    u8 pad04[20];  /**< +0x04..+0x17: Unknown. */
} ShotEntry; /* 24 bytes */

/** @brief Duel (Zell limit break) entry (32 bytes). */
typedef struct {
    u16 param0;        /**< +0x00: Name text offset (getDuelName). */
    u16 param1;        /**< +0x02: Description text offset (getDuelDesc). */
    u8 pad04[6];       /**< +0x04..+0x09: Unknown. */
    u8 abilityFlags;   /**< +0x0A: Ability flags byte (func_8009BA5C). */
    u8 pad0B[0x15];    /**< +0x0B..+0x1F: Unknown. */
} DuelEntry; /* 32 bytes */

/** @brief Rinoa limit break (part 1) entry (8 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getRinoaLimitBreak1Name). */
    u8 pad02[6];   /**< +0x02..+0x07: Unknown. */
} RinoaLimitBreak1Entry; /* 8 bytes */

/** @brief Rinoa limit break (part 2) entry (20 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Name text offset (getRinoaLimitBreak2Name). */
    u8 pad02[18];  /**< +0x02..+0x13: Unknown. */
} RinoaLimitBreak2Entry; /* 20 bytes */

/** @brief Miscellaneous text pointer (2 bytes). */
typedef struct {
    u16 param0;    /**< +0x00: Text offset (getMenuString). */
} MiscTextEntry; /* 2 bytes */

/**
 * @brief kernel.bin: a section count and offset table, then the data sections.
 *
 * The @c ...Text fields hold each section's text offset, which resolveKernelPtr
 * adds to the text-relative offsets stored in the entries.
 */
typedef struct {
    /* 0x0000 */ s32 sectionCount;
    /* 0x0004 */ s32 dataOffsets[31];
    /* 0x0080 */ s32 battleCommandsText;
    /* 0x0084 */ s32 magicText;
    /* 0x0088 */ s32 junctionableGfsText;
    /* 0x008C */ s32 enemyAttacksText;
    /* 0x0090 */ s32 weaponsText;
    /* 0x0094 */ s32 renzokukenFinishersText;
    /* 0x0098 */ s32 charactersText;
    /* 0x009C */ s32 battleItemsText;
    /* 0x00A0 */ s32 nonBattleItemsText;
    /* 0x00A4 */ s32 nonJunctionableGfAttacksText;
    /* 0x00A8 */ s32 junctionAbilitiesText;
    /* 0x00AC */ s32 commandAbilitiesText;
    /* 0x00B0 */ s32 statPercentAbilitiesText;
    /* 0x00B4 */ s32 characterAbilitiesText;
    /* 0x00B8 */ s32 partyAbilitiesText;
    /* 0x00BC */ s32 gfAbilitiesText;
    /* 0x00C0 */ s32 menuAbilitiesText;
    /* 0x00C4 */ s32 tempLimitBreaksText;
    /* 0x00C8 */ s32 blueMagicText;
    /* 0x00CC */ s32 shotText;
    /* 0x00D0 */ s32 duelText;
    /* 0x00D4 */ s32 rinoaLimitBreaks1Text;
    /* 0x00D8 */ s32 rinoaLimitBreaks2Text;
    /* 0x00DC */ s32 devourText;
    /* 0x00E0 */ s32 miscText;
    /* 0x00E4 */ AbilityEntry battleCommands[39];
    /* 0x021C */ MagicEntry magic[57];
    /* 0x0F78 */ JunctionableGfEntry junctionableGfs[16];
    /* 0x17B8 */ u8 enemyAttacks[384][20];
    /* 0x35B8 */ WeaponEntry weapons[33];
    /* 0x3744 */ RenzokukenFinisherEntry renzokukenFinishers[4];
    /* 0x37A4 */ CharacterEntry characters[11];
    /* 0x3930 */ BattleItemEntry battleItems[33];
    /* 0x3C48 */ NonBattleItemEntry nonBattleItems[166];
    /* 0x3EE0 */ u8 nonJunctionableGfAttacks[16][20];
    /* 0x4020 */ u8 commandAbilityData[12][16];
    /* 0x40E0 */ AbilityEntry junctionAbilities[20];
    /* 0x4180 */ AbilityEntry commandAbilities[19];
    /* 0x4218 */ AbilityEntry statPercentAbilities[19];
    /* 0x42B0 */ AbilityEntry characterAbilities[20];
    /* 0x4350 */ AbilityEntry partyAbilities[5];
    /* 0x4378 */ AbilityEntry gfAbilities[9];
    /* 0x43C0 */ AbilityEntry menuAbilities[24];
    /* 0x4480 */ TempLimitBreakEntry tempLimitBreaks[5];
    /* 0x44F8 */ BlueMagicEntry blueMagic[16];
    /* 0x45F8 */ u8 blueMagicParams[0x200];
    /* 0x47F8 */ ShotEntry shot[8];
    /* 0x48B8 */ DuelEntry duel[10];
    /* 0x49F8 */ u8 duelParams[0x64];
    /* 0x4A5C */ RinoaLimitBreak1Entry rinoaLimitBreaks1[2];
    /* 0x4A6C */ RinoaLimitBreak2Entry rinoaLimitBreaks2[5];
    /* 0x4AD0 */ u8 slotArray[0x3C];
    /* 0x4B0C */ u8 slotSets[0x100];
    /* 0x4C0C */ u8 devour[0xC0];
    /* 0x4CCC */ u8 misc[0x3C];
    /* 0x4D08 */ MiscTextEntry miscTextPointers[128];
} Kernel;

extern Kernel g_kernel;

#endif
