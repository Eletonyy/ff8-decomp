#ifndef WORLD_WE_OBJECT4_H
#define WORLD_WE_OBJECT4_H

#include "common.h"
#include "psxsdk/libgpu.h"

/* we_object4's public surface. Everything else the unit owns is private to
 * src/world/we_object4.c and declared at the top of that file. The callers
 * below are still assembly, so they reach these through the linker rather
 * than this header -- it is here for when they are decompiled. */

extern s32 D_800D2264;              /**< Counter increment per update (read by we_object3, we_object6). */

extern void func_800A7B38(void);    /**< Step the texture-strip animations and upload changed frames. */

/** The camera-follow reference is written as a word but published as its low
 *  half by we_object3, so both views need a name. */
typedef union {
    s32 word;
    u16 half;
} CameraRef;

/* Camera-follow reference read by we_object1, we_object3 and we_object7.
 * volatile is load-bearing: it keeps the read ordered against the particle
 * position stores, matching the original schedule. */
extern volatile CameraRef D_800C9870;

/**
 * Spawn a kind-0xE particle in the D_800D9CB0 pool at @p pos (rotation
 * zeroed, life/limit RNG-jittered). Dead code in the retail build — the
 * only caller is the unreferenced ambient spawner func_800B99A4.
 */
extern void func_800AB2D4(VECTOR *pos);

extern TILE_1 D_800D4FB0[2][96];    /**< Night-sky star pool, also primed by we_object5 (func_800A84D0). */

#endif /* WORLD_WE_OBJECT4_H */
