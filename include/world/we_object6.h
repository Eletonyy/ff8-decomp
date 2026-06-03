#ifndef WORLD_WE_OBJECT6_H
#define WORLD_WE_OBJECT6_H

#include "world.h"

/* func_800A00B4 is defined in we_object2; this caller-local prototype mirrors
 * the (s32, s32) -> s32 view we_object6 needs. */
extern s32 func_800A00B4(s32 a, s32 b);

/* Defined in this file (via INCLUDE_ASM). */
extern void func_800ACC68(s32 a, WorldXformBlock *buf, u8 *p1, WorldXform *xform);

#endif /* WORLD_WE_OBJECT6_H */
