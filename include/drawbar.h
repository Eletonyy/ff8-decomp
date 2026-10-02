#ifndef DRAWBAR_H
#define DRAWBAR_H

#include "common.h"
#include "psxsdk/libgpu.h"

/* Clipped colour-bar GPU primitive builders (drawbar.c). func_8002B3A0 is the core,
   mode-parameterised routine — it unpacks its RECT argument and emits GP0 packets (e.g. the
   0xE100041E draw-mode word); func_8002B898 wraps it with fill mode 3. func_8002B8BC draws a
   window's background; bgOffset shifts its texture along x, in texels. */
extern DR_AREA *func_8002B3A0(void *ot, DR_AREA *prim, RECT *rect, s32 color, s32 mode);
extern DR_AREA *func_8002B898(void *ot, DR_AREA *prim, RECT *rect, s32 color);
extern DR_AREA *func_8002B8BC(void *ot, DR_AREA *prim, RECT *rect, s32 color, s32 bgOffset);
extern DR_AREA *drawWindowBackground(void *ot, DR_AREA *prim, RECT *rect, s32 color);

#endif /* DRAWBAR_H */
