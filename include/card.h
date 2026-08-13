/**
 * @file card.h
 * @brief Public symbols and types owned by @c src/card.c.
 *
 * Despite the name, this unit is the GF ability/junction bookkeeping layer:
 * it builds the 128-entry ability working buffer from a GF's learn table,
 * answers category queries about individual slots, and maintains the
 * party/GF availability masks and HP mirrors in @c g_gameState.
 *
 * @note @c getGfAvailabilityMask is defined by this unit but declared in
 *       @c gf.h, and the widths deliberately differ. The definition must be
 *       @c u16 (its epilogue is @c "andi v0, a1, 0xffff"; an @c s32 definition
 *       breaks @c SLUS_008.92), while callers must see an int-width
 *       declaration (declaring @c u16 to them breaks @c menugf.ovl and
 *       @c menujnc2.ovl). That is faithful to the original build, in which
 *       @c card.c did not include the header its callers used — which is also
 *       why @c menugf.c masks the result by hand with @c "& 0xFFFF".
 *       Do not "reconcile" the two.
 */
#ifndef CARD_H
#define CARD_H

#include "common.h"
#include "ability_list.h"

/* ======================================================================== */
/* Types                                                                    */
/* ======================================================================== */

/** @brief GF ability learn requirement (4 bytes). */
typedef struct {
    u8 levelReq;        /**< Level required, or index for chained abilities (101+). */
    u8 prereq;          /**< Prerequisite ability index (0xFF = none). */
    u8 slot;            /**< Ability slot index. */
    u8 pad03;
} GfAbilityEntry;

/** @brief GF learnable ability table (stride 0x84). */
typedef struct {
    u8 pad00[0x1C];
    GfAbilityEntry abilities[21];
    u8 pad70[0x14];
} GfLearnData;

/** @brief Ability slot entry in the 128-slot working buffer (2 bytes). */
typedef struct {
    u8 type;           /**< Slot state: 0=empty, 1=chained, 2=learned, 3=level-eligible. */
    u8 abilityIndex;   /**< Ability index (0xFF = unused). */
} AbilitySlot;

/**
 * @brief Ability category lookup info (4 bytes, indexed by category 0-6).
 *
 * Maps ability categories to offsets within @c g_gfData for looking up
 * ability-specific data (e.g. AP cost, stat modifiers).
 */
typedef struct {
    u16 dataOffset;    /**< Byte offset into @c g_gfData for this category's table. */
    u8 startIndex;     /**< First slot index in this category range. */
    u8 stride;         /**< Byte stride between entries in the data table. */
} AbilityCategoryInfo;

/* ======================================================================== */
/* Data symbols                                                             */
/* ======================================================================== */

/** @brief GF learn tables (@c g_gfData + 0xF78). */
extern GfLearnData D_80079D78[];

/** @brief Ability category lookup table, indexed by category 0-6. */
extern AbilityCategoryInfo D_80053C3C[];

/* ======================================================================== */
/* Functions                                                                */
/* ======================================================================== */

/** @brief Initialize 128 ability slots to empty (state 0, index 0xFF). */
void initAbilitySlots(u8 *ptr);

/** @brief Return the ability category (0-6) that owns @p slotIndex. */
s32 getAbilityCategory(s32 slotIndex);

/** @brief Set the party leader to @p charId. */
void setPartyLeader(s32 charId);

/** @brief Bitmask of characters currently available to the party. */
u16 func_80036EC0(void);

/** @brief Copy GF @p gfIdx's runtime HP into its save-data entry. */
void copyGfHpToSave(s32 gfIdx);

/** @brief Card / character refresh hook: invoked when a party member
 *         changes; refreshes derived character data. */
void func_80036B90(s32 charIndex);

/** @brief Companion to @ref func_80036B90 — applies a bitmask of flags
 *         to the active-party char records. */
void func_80036D44(s32 mask);

s32  func_80036710(s32 index, u8 *dest, s32 count);
s32  func_8003678C(s32 gfIndex, u8 *dest, s32 count);
s32  func_8003685C(s32 gfIndex, u8 *dest, s32 count);
void func_80036C74(void);
void func_80036FE0(s32 charIdx);

#endif /* CARD_H */
