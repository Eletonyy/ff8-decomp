#ifndef ABILITY_LIST_H
#define ABILITY_LIST_H

/**
 * @file ability_list.h
 * @brief Ability-list buffer used by func_800369CC to enumerate a GF's
 *        available abilities for the junction/ability menus.
 */

#include "common.h"

/**
 * @brief Output entry for GF ability list (8 bytes).
 *
 * One per available ability, populated by func_800369CC for the
 * junction/ability menu.
 */
typedef struct {
    u8 slotIndex;      /**< Ability slot index (0-127). */
    u8 abilityIndex;   /**< Ability index within the GF's learn table. */
    u8 type;           /**< Slot type (learned/eligible/chained). */
    u8 category;       /**< Ability category (0-6, from getAbilityCategory). */
    u8 gfDataValue;    /**< Data value from g_gfData ability table. */
    u8 gsValue;        /**< Current ability level from GF save data. */
    u8 pad[2];         /**< Padding to 8-byte stride. */
} AbilityListEntry; /* 8 bytes */

/**
 * @brief Build the ability list for a GF.
 *
 * Populates @p output with the GF's available abilities and returns the count.
 *
 * @param gfIndex          GF index (0–15).
 * @param output           Output array of AbilityListEntry structs (or NULL to just count).
 * @param includeJunction  Non-zero to include junction-style entries.
 * @return Number of entries written.
 */


#endif /* ABILITY_LIST_H */
