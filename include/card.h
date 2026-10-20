/**
 * @file card.h
 * @brief Public symbols and types owned by @c src/card.c.
 *
 * Despite the name, this unit is the GF ability/junction bookkeeping layer:
 * it builds the 128-entry ability working buffer from a GF's learn table
 * and answers category queries about individual slots.
 */
#ifndef CARD_H
#define CARD_H

#include "common.h"
#include "ability_list.h"

/* ======================================================================== */
/* Types                                                                    */
/* ======================================================================== */

/** @brief Ability slot entry in the 128-slot working buffer (2 bytes). */
typedef struct {
    u8 type;           /**< Slot state: 0=empty, 1=chained, 2=learned, 3=level-eligible. */
    u8 abilityIndex;   /**< Ability index (0xFF = unused). */
} AbilitySlot;

/**
 * @brief Ability category lookup info (4 bytes, indexed by category 0-6).
 *
 * Maps ability categories to offsets within @c g_kernel for looking up
 * ability-specific data (e.g. AP cost, stat modifiers).
 */
typedef struct {
    u16 dataOffset;    /**< Byte offset into @c g_kernel for this category's table. */
    u8 startIndex;     /**< First slot index in this category range. */
    u8 stride;         /**< Byte stride between entries in the data table. */
} AbilityCategoryInfo;

/* ======================================================================== */
/* Data symbols                                                             */
/* ======================================================================== */

/** @brief Ability category lookup table, indexed by category 0-6. */
extern AbilityCategoryInfo D_80053C3C[];

/* ======================================================================== */
/* Functions                                                                */
/* ======================================================================== */

/** @brief Return the ability category (0-6) that owns @p slotIndex. */
s32 getAbilityCategory(s32 slotIndex);

#endif /* CARD_H */
