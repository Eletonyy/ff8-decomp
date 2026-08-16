/**
 * @file menusts.h
 * @brief Symbols and types owned by the menusts overlay unit.
 *
 * The menusts overlay draws the party status screen: the per-character
 * status/condition rows, the stat readouts and the junction summary.
 *
 * Cross-overlay shared types live in @c include/menu.h. Calls into
 * menumain at fixed addresses keep their own file-local prototypes
 * (overlay-conflict rule), as in the other menu sub-overlays.
 */
#ifndef MENUSTS_H
#define MENUSTS_H

#include "common.h"
#include "battle.h"

/* ======================================================================== */
/* Public typedefs/structs                                                  */
/* ======================================================================== */

/** @brief Status display table entry (used by func_801E72D8). */
typedef struct {
    u16 statusId;     /**< Status text ID (terminator: 0xFF). */
    u16 padOrType;    /**< Type/position byte. */
} StatusEntry;

/** @brief Status table entry (8-byte stride) used by func_801E582C/48/6C. */
typedef struct {
    u16 xOff; /**< Offset into D_801E99AC for x-position. */
    u16 yOff; /**< Offset into D_801E99AC for y-position. */
    u8  type; /**< Status display type byte. */
    u8  pad;
    u16 unk6;
} MenustsStatusEntry;

/* ======================================================================== */
/* Data owned by this unit                                                  */
/* ======================================================================== */

extern s32 D_801E961C[];
extern StatusEntry D_801E95CC[];
extern BattleCharData D_801E9EE4;
extern MenustsStatusEntry D_801E9964[]; /**< Status table (8-byte entries). */
extern u8 D_801E99AC[];                 /**< Coordinate/string base data. */

#endif /* MENUSTS_H */
