#ifndef TIM_H
#define TIM_H

#include "common.h"
#include "psxsdk/libgpu.h"   /* RECT */

/**
 * @brief One section of a PS1 TIM image file (CLUT block or pixel block).
 *
 * Both the CLUT and the pixel data use this identical layout, so a TIM is
 * walked by stepping @c len bytes from one section to the next.
 */
typedef struct {
    s32  len;       /**< 0x00: section length in bytes, including this header. */
    RECT rect;      /**< 0x04: destination rectangle in VRAM. */
    u16  data[1];   /**< 0x0C: section data — 16-bit CLUT entries / pixels. */
} TimSection;

/**
 * @brief PS1 standard TIM image file header.
 *
 * @c clut is the first section; the pixel section follows immediately in
 * memory at @c (u8 *)&clut + clut.len.
 */
typedef struct {
    u32        id;      /**< 0x00: magic id (0x10). */
    u32        flags;   /**< 0x04: format flags (bpp + CLUT-present). */
    TimSection clut;    /**< 0x08: CLUT section; pixel section follows. */
} Tim;

#endif /* TIM_H */
