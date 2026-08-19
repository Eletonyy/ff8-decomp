#ifndef WORLD_WE_OBJECT4_H
#define WORLD_WE_OBJECT4_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "battle.h"

/**
 * @brief World-map VRAM row animation slot (12 bytes).
 *
 * Describes one animated strip: the source frames are stacked one VRAM
 * row per frame starting at (@c u, @c v), and the current frame's row is
 * blitted to the fixed destination (@c x, @c y) — CLUT/texture-row
 * cycling driven by the shared counter @c D_800C547C.
 */
typedef struct {
    /* 0x00 */ u8 phase;     /**< Phase offset added to the shared counter. */
    /* 0x01 */ u8 divisor;   /**< Counter divisor (animation speed). */
    /* 0x02 */ u8 frames;    /**< Number of frames (0 = slot disabled). */
    /* 0x03 */ u8 pingpong;  /**< Nonzero = reflect the sequence back and forth. */
    /* 0x04 */ s16 x;        /**< Destination VRAM x. */
    /* 0x06 */ s16 y;        /**< Destination VRAM y. */
    /* 0x08 */ u16 u;        /**< Source strip VRAM x. */
    /* 0x0A */ u16 v;        /**< Source strip VRAM y of frame 0 (one row per frame). */
} WorldTexAnim; /* 0xC bytes */

extern s32 D_800C547C;              /**< Shared animation counter, wraps mod 384. */
extern s32 D_800C5478;              /**< Texture-strip animation counter, wraps mod 384 (func_800A7B38). */
extern s32 D_800D2264;              /**< Counter increment per update. */
extern s32 D_800C5458[];            /**< Last uploaded frame per texture strip (parallel to the offset table). */
extern WorldTexAnim D_800D4E80[4];  /**< The four VRAM row animation slots. */
extern s32 D_800D4EB0[4];           /**< Last blitted frame per slot. */

extern u16 D_800D2452;              /**< Map-view HUD slide-in coordinate fed to the func_800A84xx drawers. */
extern s32 D_800C5454;              /**< Likely nonzero while the map pointer highlights a named location — enables the name banner (func_800A8524). */

extern void func_800A7B38(void);    /**< Step the texture-strip animations and upload changed frames. */

/* Map-view HUD drawers of this unit (see func_800A8400 for the driving values). */
extern void func_800A8524(s32 phase, s16 pos, s32 x);

/**
 * @brief World-map effect particle (covers offsets 0x00-0x2D; full stride unknown).
 *
 * func_800A9CC0 steps one of these against a second instance that supplies
 * the per-step deltas (velocity increments and the scale multiplier).
 */
typedef struct {
    /* 0x00 */ s32 x;          /**< Position x (fixed point). */
    /* 0x04 */ s32 y;          /**< Position y (fixed point). */
    /* 0x08 */ s32 z;          /**< Position z (fixed point). */
    /* 0x0C */ u8 pad0C[4];
    /* 0x10 */ u16 scaleRate;  /**< Per-step scale multiplier, 4.12 fixed (read from the delta-source instance). */
    /* 0x12 */ u8 pad12[0xE];
    /* 0x20 */ s16 vx;         /**< Velocity x. */
    /* 0x22 */ s16 vy;         /**< Velocity y. */
    /* 0x24 */ s16 vz;         /**< Velocity z. */
    /* 0x26 */ u8 pad26[3];
    /* 0x29 */ u8 age;         /**< Step counter, incremented every update. */
    /* 0x2A */ u8 kind;        /**< Particle kind; 12 and 13 get the camera-follow drift. */
    /* 0x2B */ u8 pad2B;
    /* 0x2C */ u16 scale;      /**< Current scale, 4.12 fixed. */
} WorldParticle;

/* Camera-follow references for the kind-12/13 drift in func_800A9CC0.
 * volatile is load-bearing: it keeps these reads ordered against the
 * particle position stores, matching the original schedule. */
extern volatile s32 D_800C9870;
extern volatile s32 D_800C974C;

/* Main-binary helper: applies matrix @p m to @p in, writing @p out. */
extern void func_800404D4(MATRIX *m, SVECTOR *in, SVECTOR *out);

extern void func_800A9CC0(WorldParticle *p, WorldParticle *q);

/**
 * @brief POLY_GT3 view with packed 32-bit vertex words (0x28 bytes).
 *
 * Same layout as the SDK @c POLY_GT3, but the screen coordinates are
 * held as one @c u32 word per vertex — func_800AAD48 copies them from
 * @c WorldVtx::xy with single word moves.
 */
typedef struct {
    /* 0x00 */ u32 tag;              /**< OT link word (see setaddr/getaddr). */
    /* 0x04 */ u8 r0, g0, b0, code;  /**< Vertex 0 colour + primitive code. */
    /* 0x08 */ u32 xy0;              /**< Vertex 0 packed screen x,y. */
    /* 0x0C */ u8 u0, v0;            /**< Vertex 0 texture coords. */
    /* 0x0E */ u16 clut;             /**< Palette (CLUT) id. */
    /* 0x10 */ u8 r1, g1, b1, p1;    /**< Vertex 1 colour. */
    /* 0x14 */ u32 xy1;              /**< Vertex 1 packed screen x,y. */
    /* 0x18 */ u8 u1, v1;            /**< Vertex 1 texture coords. */
    /* 0x1A */ u16 tpage;            /**< Texture page. */
    /* 0x1C */ u8 r2, g2, b2, p2;    /**< Vertex 2 colour. */
    /* 0x20 */ u32 xy2;              /**< Vertex 2 packed screen x,y. */
    /* 0x24 */ u8 u2, v2;            /**< Vertex 2 texture coords. */
    /* 0x26 */ u16 pad26;
} WorldPolyGT3;

/** @brief Transformed world vertex: packed screen word + texture coords. */
typedef struct {
    /* 0x00 */ u32 xy;               /**< Packed screen x,y. */
    /* 0x04 */ u8 u;                 /**< Texture u. */
    /* 0x05 */ u8 pad5;
    /* 0x06 */ u8 v;                 /**< Texture v. */
    /* 0x07 */ u8 pad7;
} WorldVtx;

/** @brief Per-vertex grayscale shade triple for one triangle. */
typedef struct {
    u8 c0, c1, c2;
} TriShade;

extern WorldPolyGT3 *D_800D8804;    /**< Current POLY_GT3 slot being filled. */

extern void func_800AAD48(WorldVtx *vtx, TriShade *shade);

/**
 * @brief POLY_GT4 view with packed 32-bit vertex words (0x34 bytes).
 *
 * Same layout as the SDK @c POLY_GT4, but the screen coordinates are
 * held as one @c u32 word per vertex — func_800AAEAC copies them from
 * @c WorldVtx::xy with single word moves.
 */
typedef struct {
    /* 0x00 */ u32 tag;              /**< OT link word (see setaddr/getaddr). */
    /* 0x04 */ u8 r0, g0, b0, code;  /**< Vertex 0 colour + primitive code. */
    /* 0x08 */ u32 xy0;              /**< Vertex 0 packed screen x,y. */
    /* 0x0C */ u8 u0, v0;            /**< Vertex 0 texture coords. */
    /* 0x0E */ u16 clut;             /**< Palette (CLUT) id. */
    /* 0x10 */ u8 r1, g1, b1, p1;    /**< Vertex 1 colour. */
    /* 0x14 */ u32 xy1;              /**< Vertex 1 packed screen x,y. */
    /* 0x18 */ u8 u1, v1;            /**< Vertex 1 texture coords. */
    /* 0x1A */ u16 tpage;            /**< Texture page. */
    /* 0x1C */ u8 r2, g2, b2, p2;    /**< Vertex 2 colour. */
    /* 0x20 */ u32 xy2;              /**< Vertex 2 packed screen x,y. */
    /* 0x24 */ u8 u2, v2;            /**< Vertex 2 texture coords. */
    /* 0x26 */ u16 pad26;
    /* 0x28 */ u8 r3, g3, b3, p3;    /**< Vertex 3 colour. */
    /* 0x2C */ u32 xy3;              /**< Vertex 3 packed screen x,y. */
    /* 0x30 */ u8 u3, v3;            /**< Vertex 3 texture coords. */
    /* 0x32 */ u16 pad32;
} WorldPolyGT4;

/** @brief Per-vertex grayscale shade quad for one quad. */
typedef struct {
    u8 c0, c1, c2, c3;
} QuadShade;

extern WorldPolyGT4 *D_800D8800;    /**< Current POLY_GT4 slot being filled. */

extern void func_800AAEAC(WorldVtx *vtx, QuadShade *shade);
extern void func_800A6A74(BattleSceneCtx *ctx);

/**
 * Spawn a kind-0xE particle in the D_800D9CB0 pool at @p pos (rotation
 * zeroed, life/limit RNG-jittered). Dead code in the retail build — the
 * only caller is the unreferenced ambient spawner func_800B99A4.
 */
extern void func_800AB2D4(VECTOR *pos);

extern s16 D_800C53B4[];            /**< Per-plane depth-cue weight fed to func_800ABDD8. */

/* Per-pool GPU primitive template arrays (file-private to we_object4). */
extern POLY_GT4 D_800D58C0[3];      /**< Map-panel quads for the active scene. */
extern POLY_GT4 D_800D595C[3];      /**< Map-panel quads for the D_800CA040 sentinel scene. */
extern DR_MODE  D_800D4FB0[2][96];
extern POLY_FT4 D_800D88B0[2][64];
extern TILE     D_800DA8D0[2][64];
extern WorldPolyGT4 D_800D5A00[2][64];
extern WorldPolyGT3 D_800D7400[2][64];

/* func_800491E8 is main-binary. */
extern void func_800491E8(void *p);

#endif /* WORLD_WE_OBJECT4_H */
