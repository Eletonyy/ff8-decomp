#ifndef TIM_H
#define TIM_H

#include "common.h"
#include "psxsdk/libgpu.h"

/** @brief Tim.flags bit: the file has a CLUT section. */
#define TIM_HAS_CLUT 0x08

/**
 * @brief One section of a PS1 TIM image file (CLUT block or pixel block).
 *
 * Both the CLUT and the pixel data use this identical layout, so a TIM is
 * walked by stepping @c len bytes from one section to the next.
 */
typedef struct {
    s32  len;       /**< 0x00: section length in bytes, including this header. */
    RECT rect;      /**< 0x04: destination rectangle in VRAM. */
    u32 data[1]; /**< 0x0C: section data, as the words LoadImage takes. */
} TimSection;

/**
 * @brief PS1 standard TIM image file header.
 *
 * @c clut is the first section; the pixel section follows immediately in
 * memory at @c (u8 *)&clut + clut.len.
 */
typedef struct {
    u32        id;      /**< 0x00: magic id (0x10). */
    u8 flags; /**< 0x04: bits 0-2 pixel mode, and TIM_HAS_CLUT. */
    u8 reserved[3]; /**< 0x05: rest of the flag word, zero. */
    TimSection clut;    /**< 0x08: CLUT section; pixel section follows. */
} Tim;

/** @brief Variable-width font file: width table + TIM glyph sheet. */
typedef struct {
    s32 widthTableOffset; /**< 0x00: offset of the width table, one nibble per character. */
    s32 timOffset; /**< 0x04: offset of the glyph sheet's TIM, 0 for none. */
} NameFont;

#endif /* TIM_H */
