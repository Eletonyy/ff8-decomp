#include "common.h"

/* The world overlay's entry unit: one ~4.1KB function, called from main.s as
 * the overlay's main loop. It sat inside the leading rodata blob until
 * 2026-08-31, hidden from the split — which is why its callees (the render
 * callback registration, the object-list machinery) once looked like dead
 * islands. */

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object0", func_800987D8);
