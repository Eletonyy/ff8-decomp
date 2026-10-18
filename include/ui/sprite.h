#ifndef UI_SPRITE_H
#define UI_SPRITE_H

#include "common.h"
#include "psxsdk/libgpu.h"

// custom packet for drawing sprites. combines DR_TPAGE and SPRT into one primitive
typedef struct {
    /** 0x00 */ u32 tag;
    /** 0x04 */ u32 drawMode; // GP0(E1h)
    /** 0x08 */ u8 r0, g0, b0, code;
    /** 0x0C */ s16 x0, y0;
    /** 0x10 */ u8 u0, v0;
    /** 0x12 */ u16 clut;
    /** 0x14 */ u16 w, h;
} TSPRT;

// same as setTSprtFull but doesn't set code. Probably a FIXME is needed here to fix the functions that are using this
#define setTSprt(p, dfe, dtd, tpage) \
    (setlen(p, 5), (p)->drawMode = _get_mode(dfe, dtd, tpage))

#define setTSprtFull(p, dfe, dtd, tpage) (setTSprt(p, dfe, dtd, tpage), (p)->code = 0x64)

#endif
