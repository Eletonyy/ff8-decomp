#ifndef WORLD_WE_OBJECT0_H
#define WORLD_WE_OBJECT0_H

#include "common.h"
#include "overlay.h"   /* func_800987D8 -- the entry point this unit defines */

/* we_object0 has exactly one public symbol, the overlay entry point, and that
 * is declared with the other overlay entry points in overlay.h rather than
 * here. Everything else the unit touches is private to
 * src/world/we_object0.c. This header exists so the unit has an owner for any
 * public surface it grows, and so its users get the entry point by including
 * it. */

#endif /* WORLD_WE_OBJECT0_H */
