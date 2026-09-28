#ifndef LIBGCC_H
#define LIBGCC_H

#include "common.h"
#include "psxsdk/libgpu.h"

/* Misnamed: the symbol at 0x8002BA80 is not libgcc's 64-bit divide but a
   func_8002B8BC wrapper (drawbar.h) with a4 = 0. */
extern DR_AREA *__udivdi3(P_TAG *ot, DR_AREA *prim, RECT *rect, s32 color);

#endif /* LIBGCC_H */
