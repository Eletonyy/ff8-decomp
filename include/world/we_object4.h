#ifndef WORLD_WE_OBJECT4_H
#define WORLD_WE_OBJECT4_H

#include "common.h"
#include "psxsdk/libgpu.h"

/**
 * @brief 0x1C-byte prim link — first word is an addPrim-style P_TAG, the
 *        @c prim_len halfword at @c 0x1A is set to @c 0xC by the inserter.
 */
typedef struct {
    /* 0x00 */ s32 link;
    /* 0x04 */ u8 pad04[0x16];
    /* 0x1A */ s16 prim_len;
} PrimLink;

/* Per-pool GPU primitive template arrays (file-private to we_object4). */
extern DR_MODE  D_800D4FB0[2][96];
extern POLY_FT4 D_800D88B0[2][64];
extern TILE     D_800DA8D0[2][64];
extern POLY_GT4 D_800D5A00[2][64];
extern POLY_GT3 D_800D7400[2][64];

/* func_800491E8 is main-binary; func_80048C50 is main-binary and needs the
 * void-returning view here for matching codegen. */
extern void func_800491E8(void *p);
extern void func_80048C50(s32 arg);

#endif /* WORLD_WE_OBJECT4_H */
