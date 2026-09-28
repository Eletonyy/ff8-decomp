/**
 * @file menu_tint.h
 * @brief The menu tint pair: the colour everything the menus draw is modulated by,
 * and its blink copy.
 *
 * dialog.c writes it, but it has a header of its own so the menu overlays can use it
 * without dialog.h, whose prototypes clash with the menus' s32-typed display lists.
 */
#ifndef MENU_TINT_H
#define MENU_TINT_H

#include "common.h"

/** @brief Indexes of @ref g_menuTint. */
#define MENU_TINT_NORMAL 0 /**< The tint: a grey sprite colour word from setMenuBrightness. */
#define MENU_TINT_BLINK 1 /**< Its blink copy, used by text colours 8-15. */

/** @brief Menu tints (main exe data at 0x80083848), indexed by @c MENU_TINT_*. */
extern u32 g_menuTint[2];

#endif /* MENU_TINT_H */
