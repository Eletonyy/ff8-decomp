#ifndef DRAWBAR_H
#define DRAWBAR_H

#include "common.h"
#include "psxsdk/libgpu.h"

/** @brief func_8002B3A0 sides: draw the left corners and edge, the right ones. */
#define WINDOW_FRAME_LEFT 0x1
#define WINDOW_FRAME_RIGHT 0x2

/* Window frames and backgrounds (drawbar.c). func_8002B3A0 draws a frame, func_8002B898 the
   whole frame; func_8002B8BC draws a background, its bgOffset shifting the pattern. */
extern DR_AREA *func_8002B3A0(void *ot, DR_AREA *prim, RECT *rect, s32 color, s32 sides);
extern DR_AREA *func_8002B898(void *ot, DR_AREA *prim, RECT *rect, s32 color);
extern DR_AREA *func_8002B8BC(void *ot, DR_AREA *prim, RECT *rect, s32 color, s32 bgOffset);
extern DR_AREA *drawWindowBackground(void *ot, DR_AREA *prim, RECT *rect, s32 color);

#endif /* DRAWBAR_H */
