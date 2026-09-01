/**
 * @file menucfg.h
 * @brief Symbols and types owned by the menucfg overlay unit.
 *
 * The menucfg overlay draws the config screen: the list of options, their
 * availability flags and the cursor that moves between them. Availability is
 * decided per entry — the memory-card option is hidden when no card responds,
 * and the battle-animation option when no animation is active.
 *
 * The overlay is a single translation unit, so nothing it defines is public and
 * this header declares no prototypes. Note that other menu overlays contain
 * their own functions at these same addresses, and therefore the same
 * auto-generated names: a @c func_801Exxxx seen in menugf or menupty is a
 * different function, not a call into this unit.
 */
#ifndef MENUCFG_H
#define MENUCFG_H

#include "common.h"

#endif /* MENUCFG_H */
