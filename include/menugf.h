/**
 * @file menugf.h
 * @brief Public symbols owned by the menugf overlay (@c src/menu/menugf).
 *
 * The menugf overlay renders the GF status/detail panels: stat rows read
 * out of the overlay's own lookup table, the GF name/level header, and the
 * compatibility and ability readouts.
 *
 * @note @c func_801E6B3C is an overlay-local name — @c menumgc defines a
 *       different function at the same overlay address with a different
 *       signature. Prototypes that collide across overlays must stay in
 *       each overlay's own header, never in a shared one.
 *
 * @note @c menugf.c still declares @c g_menuDisplayCfg and @c g_menuColor at
 *       file scope instead of including @c menu.h. @c g_menuColor alone would
 *       be a clean swap, but the same header types @c g_menuDisplayCfg as a
 *       @c MenuDisplayConfig struct while this unit walks it as raw bytes
 *       (@c *(s16 *)&g_menuDisplayCfg[0]); including @c menu.h therefore
 *       requires converting those accesses to struct fields first, which is a
 *       codegen-affecting decomp change rather than a header cleanup.
 */
#ifndef MENUGF_H
#define MENUGF_H

#include "common.h"

/* ======================================================================== */
/* Data symbols                                                             */
/* ======================================================================== */

/** @brief GF stat lookup table (8 halfwords), read by @ref func_801E58C8. */
extern u8 D_801E7DD0[];

/** @brief Selected-GF index used by the panel renderers. */
extern u8 D_801E7E88;

/* ======================================================================== */
/* Functions                                                                */
/* ======================================================================== */

/** @brief Display a GF stat value from the lookup table at index @p a1. */
void func_801E58C8(u8 *a0, s32 a1);

/** @brief Render a GF name/label pair. */
void func_801E5988(u8 *a0, u8 *a1);

/** @brief Render a GF detail row (menugf's own; see the file note). */
void func_801E6B3C(u8 *a0, s32 a1, s32 a2, s32 a3, s32 arg4);

/** @brief Render the GF panel body for the selected GF. */
void func_801E6C84(s32 a0, s32 a1);

/** @brief Render a GF ability/compatibility entry. */
void func_801E6D20(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg4,
                   volatile unsigned int arg5, s32 arg6, u16 arg7, s32 arg8);

/** @brief Refresh the cached GF availability list. */
void func_801E7C20(s32 unused);

/** @brief GF panel draw entry point. */
void func_801E7CB8(u8 *a0);

/** @brief GF panel draw entry point (secondary variant). */
void func_801E7CF4(u8 *a0);

/* Bodies still in assembly; used both as callbacks (cast to s32) and called
 * directly, so their argument lists are declared K&R-style until known. */
extern void func_801E5A60();
extern void func_801E6A8C();
extern void func_801E7988();

#endif /* MENUGF_H */
