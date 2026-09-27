#ifndef GF_H
#define GF_H

#include "common.h"
#include "kernel.h"

/** @brief Total number of GFs. */
#define GF_COUNT 16

/**
 * @brief 8-byte view at &AbilityEntry[0].cap (= D_8007CEE4).
 *
 * Strides through AbilityEntry[] reading the upper 4 bytes (cap | typeField
 * | bonusField | extraField packed as u32) of each entry. The next struct
 * field aligns to the following entry's statParam0/statParam1 (unused).
 */
typedef struct {
    u32 flagWord;
    u32 _next;
} AbilityFlagSlot;

/** @brief Offset view at &AbilityEntry[0].cap; used to read packed flag words. */
extern AbilityFlagSlot D_8007CEE4[];

/** @brief Bitmask of GFs currently available (bit N = GF N unlocked).
 *  @note Defined in @c card.c as @c u16, declared int-width here on purpose:
 *        callers were built against an int-width declaration and narrowing it
 *        breaks @c menugf.ovl / @c menujnc2.ovl. See the note in @c card.h. */
s32 getGfAvailabilityMask(void);

/**
 * @brief Per-magic-spell junction data (stride 60 bytes).
 *
 * Indexed by magic ID (0–56). Each entry holds the stat junction bonus
 * for each of the 9 stats, plus element/status flags and multipliers
 * used to score magic for auto-junction.
 *
 * Lives at D_8007901C (= g_kernel.magic).
 */
typedef struct {
    u8 pad00[0x17];           /**< +0x00: Preamble (name params, base stats, etc.). */
    u8 statJunction[9];       /**< +0x17: Stat junction values [HP,Str,Vit,Mag,Spr,Spd,Eva,Hit,Lck]. */
    u8 atkElemFlags;          /**< +0x20: Attack element flags (popcount → number of elements). */
    u8 atkElemBonus;          /**< +0x21: Attack element bonus multiplier. */
    u8 defElemFlags;          /**< +0x22: Defense element flags (popcount → number of elements). */
    u8 defElemBonus;          /**< +0x23: Defense element bonus multiplier. */
    u8 atkStatusBonus;        /**< +0x24: Attack status bonus multiplier. */
    u8 defStatusBonus;        /**< +0x25: Defense status bonus multiplier. */
    u16 atkStatusFlags;       /**< +0x26: Attack status bitmask (popcount → number of statuses). */
    u16 defStatusFlags;       /**< +0x28: Defense status bitmask (popcount → number of statuses). */
    u8 pad2A[0x12];           /**< +0x2A: Remaining fields (total 60 bytes). */
} MagicJunctionData; /* 60 bytes */

extern MagicJunctionData g_magicJunctionData[];

#endif /* GF_H */