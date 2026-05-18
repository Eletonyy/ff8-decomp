#include "common.h"

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
} CmdEntry; /* 12 bytes */

typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ u8  pad4[4];
    /* 0x08 */ CmdEntry entries[1];
} CmdQueue;

extern CmdQueue D_800DE7B0;

INCLUDE_ASM("asm/field/nonmatchings/fe_object3", func_800AD7AC);

/**
 * Append one entry to the @c D_800DE7B0 command queue. Stores @p a
 * verbatim, rounds @p size up to the next @c 0x800-byte boundary, and
 * scales @p c by @c >>2 (likely a byte-to-stride or fixed-point
 * narrowing). The slot is at queue index @c count and @c count is
 * incremented after the writes.
 *
 * @param a    Identifier/tag stored at @c entries[count].unk0.
 * @param size Byte size; rounded up to @c 0x800 alignment before store.
 * @param c    Misc parameter; scaled by @c >>2 before store.
 */
void func_800ADAF4(s32 a, u32 size, u32 c) {
    size = ((size + 0x7FF) >> 11) << 11;
    c >>= 2;
    D_800DE7B0.entries[D_800DE7B0.count].unk0 = a;
    D_800DE7B0.entries[D_800DE7B0.count].unk4 = size;
    D_800DE7B0.entries[D_800DE7B0.count].unk8 = c;
    D_800DE7B0.count = D_800DE7B0.count + 1;
}
