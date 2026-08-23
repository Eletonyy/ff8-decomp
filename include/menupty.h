/**
 * @file menupty.h
 * @brief Symbols and types owned by the menupty overlay unit.
 *
 * The menupty overlay draws the party-formation screen: the character
 * roster, the three active-party slots and the swap cursor.
 *
 * Cross-overlay shared types live in @c include/menu.h. The menu overlays
 * all load at the same address and are never resident together, so nothing
 * here is reachable from another overlay; this header exists to give the
 * unit's public types a home, and currently declares none. Its rodata
 * coordinate tables are file-local and stay in menupty.c.
 */
#ifndef MENUPTY_H
#define MENUPTY_H

#include "common.h"

#endif /* MENUPTY_H */
