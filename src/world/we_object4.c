#include "common.h"
#include "battle.h"
#include "psxsdk/libgpu.h"
#include "world.h"
#include "world/we_object4.h"

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A64DC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A688C);

/**
 * @brief Splice two of @p ctx's bone prims into the sub-OT records of
 *        the @c D_800D3510 and @c D_800D3690 pools.
 *
 * Sibling of @ref func_800A735C: for the bone ids in @c D_800C53B8[1]
 * and @c [2], point the record's last sub-slot ([3]) at the bone's prim
 * (@c ctx->primList entry) and the bone's prim back at the record's
 * first sub-slot ([0]), using the PsyQ @c setaddr / @c getaddr tag
 * operations. The pool row is picked by whether @p ctx is the
 * @c D_800CA040 sentinel.
 *
 * @param ctx Scene context whose @c primList holds the bone prims.
 */
void func_800A6A74(BattleSceneCtx *ctx) {
    s32 cond;
    s32 i;

    cond = ctx == &D_800CA040;
    for (i = 0; i < 2; i++) {
        setaddr(&D_800D3510[cond][i][3], getaddr(&ctx->primList[D_800C53B8[i + 1]]));
        setaddr(&ctx->primList[D_800C53B8[i + 1]], &D_800D3510[cond][i][0]);
    }
    for (i = 0; i < 2; i++) {
        setaddr(&D_800D3690[cond][i][3], getaddr(&ctx->primList[D_800C53B8[i + 1]]));
        setaddr(&ctx->primList[D_800C53B8[i + 1]], &D_800D3690[cond][i][0]);
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A6BE0);

/**
 * @brief Splice three of @c a0's bone prims into the sub-OT records of
 *        the @c D_800D3E50 and @c D_800D4090 pools.
 *
 * For each of the three bone ids in @c D_800C53B8[0], @c [3], @c [4],
 * link the entity's prim pointer between a record's first ([0]) and
 * last ([3]) sub-slots, once per pool. The cond bit picks the @c [0]
 * or @c [1] pool row, switching layout for non-canonical entity models.
 *
 * @param a0 Entity model whose @c primList holds the prims to splice.
 */
void func_800A735C(BattleSceneCtx *a0) {
    s32 cond = (a0 != &D_800CA040) ? 1 : 0;

    addPrims(&a0->primList[D_800C53B8[0]], &D_800D3E50[cond][0][0].link, &D_800D3E50[cond][0][3].link);
    addPrims(&a0->primList[D_800C53B8[3]], &D_800D3E50[cond][1][0].link, &D_800D3E50[cond][1][3].link);
    addPrims(&a0->primList[D_800C53B8[4]], &D_800D3E50[cond][2][0].link, &D_800D3E50[cond][2][3].link);

    addPrims(&a0->primList[D_800C53B8[0]], &D_800D4090[cond][0][0].link, &D_800D4090[cond][0][3].link);
    addPrims(&a0->primList[D_800C53B8[3]], &D_800D4090[cond][1][0].link, &D_800D4090[cond][1][3].link);
    addPrims(&a0->primList[D_800C53B8[4]], &D_800D4090[cond][2][0].link, &D_800D4090[cond][2][3].link);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7590);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7B38);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7CD0);

/**
 * @brief Advance the four world-map VRAM row animations.
 *
 * Steps the shared animation counter @c D_800C547C by @c D_800D2264,
 * wrapping modulo 384, then walks the four @c WorldTexAnim slots in
 * @c D_800D4E80. Each enabled slot (frames != 0) derives its current
 * frame from the counter: cyclic (`(counter + phase) / divisor % frames`),
 * or reflected over the frame range with period `frames * 2 - 2` when
 * @c pingpong is set (0,1,..,frames-1,frames-2,..,1,..). When the frame
 * differs from the slot's last blitted one (@c D_800D4EB0), the 256x1
 * source row at (u, v + frame) is copied to the slot's destination
 * (x, y) via @c func_80048FBC and the new frame is cached.
 */
void func_800A7E74(void) {
    WorldTexAnim *anim;
    s32 i;
    u32 n;
    u32 over;
    s32 frame;
    RECT rect;

    D_800C547C = (D_800C547C + D_800D2264) % 384;

    i = 0;
    for (anim = D_800D4E80; anim < &D_800D4E80[4]; anim++, i++) {
        if (anim->frames == 0) {
            continue;
        }
        if (anim->pingpong != 0) {
            n = (u8)(((D_800C547C + anim->phase) / anim->divisor)
                     % (u8)(anim->frames * 2 - 1));
            if (n >= anim->frames) {
                over = n - anim->frames + 2;
                frame = anim->frames - over;
            } else {
                frame = n;
            }
        } else {
            frame = ((D_800C547C + anim->phase) / anim->divisor) % anim->frames;
        }
        if (D_800D4EB0[i] != frame) {
            rect.x = anim->u;
            rect.y = anim->v + frame;
            rect.w = 0x100;
            rect.h = 1;
            func_80048FBC(&rect, anim->x, anim->y);
            D_800D4EB0[i] = frame;
        }
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8024);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8270);

/**
 * @brief Lay out and draw the world-map HUD trio at slide coordinate
 *        @c D_800D2452.
 *
 * Derives a wrap phase `0x100 - (u8)D_800D239A` from the low byte of the
 * camera heading and passes it to @c func_800A8868 (drawn at
 * `D_800D2452 - 0x30`). When @c D_800C5454 is set, also draws the centred
 * element via @c func_800A8524 at `D_800D2452 - 0xA0`, with x offset
 * `(0x300 - (b0 + b1 + b2 * 2) * 2) >> 1` clamped to [0, 0x200], where
 * b0..b2 are the width bytes of @c D_800DB0D0. Always finishes with
 * @c func_800A8A28 at the raw coordinate.
 *
 * @note Purpose uncertain — appears to drive the world-map map-view HUD:
 *       a map panel that wraps with the camera heading (@c func_800A8868),
 *       a location-name banner shown only while the map pointer highlights
 *       a named point (@c func_800A8524, centred to its text width), and
 *       the pointer/marker sprite (@c func_800A8A28).
 */
void func_800A8400(void) {
    s32 phase;
    s32 pos;
    s32 x;

    phase = 0x100 - (u8)D_800D239A;
    func_800A8868(phase, D_800D2452 - 0x30);
    if (D_800C5454 != 0) {
        pos = (0x300 - (D_800DB0D0[0] + D_800DB0D0[1] + D_800DB0D0[2] * 2) * 2) >> 1;
        if (pos >= 0) {
            x = 0x200;
            if (pos <= 0x200) {
                x = pos;
            }
        } else {
            x = 0;
        }
        func_800A8524(phase, D_800D2452 - 0xA0, x);
    }
    func_800A8A28(D_800D2452);
}


/**
 * @brief Initialise two 96-slot DR_MODE pools with fixed tag fields.
 *
 * For each of the two pools at @c D_800D4FB0[0..1], walks 96 DR_MODE
 * slots and sets @c len = 2 (2-word payload) and @c code = 0x6A via
 * the PsyQ @c setlen/@c setcode macros. Used to prime the primitive
 * templates before per-frame GPU command generation fills in the rest.
 */
void func_800A84D0(void) {
    s32 s, i;
    for (s = 0; s < 2; s++) {
        for (i = 0; i < 96; i++) {
            setlen(&D_800D4FB0[s][i], 2);
            setcode(&D_800D4FB0[s][i], 0x6A);
        }
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8524);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8868);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8A28);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8C1C);


/**
 * @brief Initialise three world-render pools in one go:
 *
 *  1. Reset @c limit and @c count to zero on every entry of the 64-slot
 *     @c D_800D9CB0 particle pool (mark all slots inactive/reusable).
 *  2. Prime each @c POLY_FT4 in @c D_800D88B0[2][64] with
 *     @c len = @c 9 and @c code = @c 0x2C.
 *  3. Prime each @c TILE in @c D_800DA8D0[2][64] with
 *     @c len = @c 3 and @c code = @c 0x60.
 *
 * Used at world setup time to prepare the per-frame prim templates.
 */
void func_800A9254(void) {
    Slot30 *p = D_800D9CB0;
    s32 j, i;

    while (p < &D_800D9CB0[64]) {
        p->limit = 0;
        p->count = 0;
        p++;
    }

    for (j = 0; j < 2; j++) {
        for (i = 0; i < 64; i++) {
            setlen(&D_800D88B0[j][i], 9);
            setcode(&D_800D88B0[j][i], 0x2C);
            setlen(&D_800DA8D0[j][i], 3);
            setcode(&D_800DA8D0[j][i], 0x60);
        }
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9300);

/**
 * @brief Step one world-map effect particle.
 *
 * Advances @p p by one tick: multiplies its 4.12 scale by @p q's
 * @c scaleRate, integrates position from the 16-bit velocity, and then
 * accelerates the velocity by @p q's velocity deltas and bumps @c age.
 * From the second tick on, particles of kind 12 or 13 additionally drift
 * with the camera: 5/6 of the per-frame camera deltas (@c D_800C9E38[0],
 * @c D_800C9870 - @c D_800C974C, @c D_800C9E38[2]) is added to x/y/z so
 * the effect roughly follows the view (weather-style particles).
 *
 * @param p Particle to update.
 * @param q Delta source: velocity increments and scale multiplier.
 *
 * @note The position words are updated through the walking pointer @c w on
 *       purpose: the multi-set pointer defeats gcc's alias base tracking,
 *       which keeps the volatile camera reads ordered after each position
 *       store, matching the original instruction schedule. The offsets
 *       still fold to plain 0x0/0x4/0x8 accesses.
 */
void func_800A9CC0(WorldParticle *p, WorldParticle *q) {
    s32 *w;

    p->scale = p->scale * q->scaleRate / 4096;
    p->x += p->vx;
    p->y += p->vy;
    p->z += p->vz;
    if (p->age != 0) {
        if (p->kind == 12 || p->kind == 13) {
            w = &p->x;
            *w += D_800C9E38[0] * 5 / 6;
            w++;
            *w += (D_800C9870 - D_800C974C) * 5 / 6;
            w++;
            *w += D_800C9E38[2] * 5 / 6;
        }
    }
    p->vx += q->vx;
    p->vy += q->vy;
    p->vz += q->vz;
    p->age++;
}

/**
 * @brief Initialise the first record's 4 sub-OT slots in each of two
 *        pools for the entity model in @p a0.
 *
 * Runs @c func_800491E8 on all four sub-slots of record 0 in the
 * @c D_800D3E50 and @c D_800D4090 pools, then dispatches
 * @c func_80048C50(0). @c cond is the canonical entity bit — @c 0 for
 * @c &D_800CA040, @c 1 otherwise — selecting the pool row.
 */
void func_800A9E24(BattleSceneCtx *a0) {
    s32 cond = (a0 != &D_800CA040) ? 1 : 0;

    func_800491E8(&D_800D3E50[cond][0][0]);
    func_800491E8(&D_800D3E50[cond][0][1]);
    func_800491E8(&D_800D3E50[cond][0][2]);
    func_800491E8(&D_800D3E50[cond][0][3]);
    func_800491E8(&D_800D4090[cond][0][0]);
    func_800491E8(&D_800D4090[cond][0][1]);
    func_800491E8(&D_800D4090[cond][0][2]);
    func_800491E8(&D_800D4090[cond][0][3]);
    func_80048C50(0);
}


/**
 * @brief Initialise two double-buffered prim pools with their tag headers.
 *
 * For each of the two pools at @c D_800D5A00[0..1] (POLY_GT4) and
 * @c D_800D7400[0..1] (POLY_GT3), walks all 64 slots and primes each
 * primitive's header:
 *  - @c POLY_GT4: @c len = @c 0xC (12 words), @c code = @c 0x3C.
 *  - @c POLY_GT3: @c len = @c 0x9 (9 words),  @c code = @c 0x34.
 *
 * Used to prime the prim templates before per-frame GPU command
 * generation fills in the colour/uv/xy fields.
 *
 * The @c p4 / @c p3 pointer locals shadow the array-index expressions
 * to force the right per-iteration register layout (a leftover idiom
 * from the original — without the locals the compiler hoists the
 * @c 0xC constant outside the inner loop, which doesn't match).
 */
void func_800A9ED4(void) {
    POLY_GT4 *p4;
    POLY_GT3 *p3;
    s32 j, i;
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 64; i++) {
            p4 = &D_800D5A00[j][i];
            p3 = &D_800D7400[j][i];
            setlen(&D_800D5A00[j][i], 0xC);
            setcode(&D_800D5A00[j][i], 0x3C);
            setlen(&D_800D7400[j][i], 0x9);
            setcode(&D_800D7400[j][i], 0x34);
        }
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9F54);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AA210);

/**
 * @brief Fill the current POLY_GT3 (@c D_800D8804) from a transformed
 *        triangle and splice it onto the main OT chain.
 *
 * Writes a grayscale gouraud colour per vertex (@p shade byte copied to
 * r=g=b), the three packed screen words and texture coordinates from
 * @p vtx[0..2], a fixed CLUT id of @c 0x3A74 and texture page @c 0xC,
 * then links the primitive to @c D_800D244C's main chain head with the
 * PsyQ @c setaddr / @c getaddr tag operations (standard @c addPrim
 * semantics).
 *
 * The prim pointer is re-read from @c D_800D8804 around every byte-wide
 * store because gcc must assume those stores may alias the pointer
 * global — the word/half stores between them reuse the cached value.
 *
 * @param vtx   Three transformed vertices (packed screen word + u/v).
 * @param shade Per-vertex grayscale shade triple.
 */
void func_800AAD48(WorldVtx *vtx, TriShade *shade) {
    D_800D8804->r0 = shade->c0;
    D_800D8804->g0 = shade->c0;
    D_800D8804->b0 = shade->c0;
    D_800D8804->r1 = shade->c1;
    D_800D8804->g1 = shade->c1;
    D_800D8804->b1 = shade->c1;
    D_800D8804->r2 = shade->c2;
    D_800D8804->g2 = shade->c2;
    D_800D8804->b2 = shade->c2;
    D_800D8804->xy0 = vtx[0].xy;
    D_800D8804->xy1 = vtx[1].xy;
    D_800D8804->xy2 = vtx[2].xy;
    D_800D8804->u0 = vtx[0].u;
    D_800D8804->v0 = vtx[0].v;
    D_800D8804->u1 = vtx[1].u;
    D_800D8804->v1 = vtx[1].v;
    D_800D8804->u2 = vtx[2].u;
    D_800D8804->v2 = vtx[2].v;
    D_800D8804->clut = 0x3A74;
    D_800D8804->tpage = 0xC;
    setaddr(D_800D8804, getaddr(&D_800D244C->primList[BSC_OTHEAD_IDX]));
    setaddr(&D_800D244C->primList[BSC_OTHEAD_IDX], D_800D8804);
}


/**
 * @brief Fill the current POLY_GT4 (@c D_800D8800) from a transformed
 *        quad and splice it onto the main OT chain.
 *
 * Quad twin of @ref func_800AAD48: writes a grayscale gouraud colour per
 * vertex (@p shade byte copied to r=g=b), the four packed screen words
 * and texture coordinates from @p vtx[0..3], a fixed CLUT id of
 * @c 0x3A74 and texture page @c 0xC, then links the primitive to
 * @c D_800D244C's main chain head with the PsyQ @c setaddr / @c getaddr
 * tag operations (standard @c addPrim semantics).
 *
 * @param vtx   Four transformed vertices (packed screen word + u/v).
 * @param shade Per-vertex grayscale shade quad.
 */
void func_800AAEAC(WorldVtx *vtx, QuadShade *shade) {
    D_800D8800->r0 = shade->c0;
    D_800D8800->g0 = shade->c0;
    D_800D8800->b0 = shade->c0;
    D_800D8800->r1 = shade->c1;
    D_800D8800->g1 = shade->c1;
    D_800D8800->b1 = shade->c1;
    D_800D8800->r2 = shade->c2;
    D_800D8800->g2 = shade->c2;
    D_800D8800->b2 = shade->c2;
    D_800D8800->r3 = shade->c3;
    D_800D8800->g3 = shade->c3;
    D_800D8800->b3 = shade->c3;
    D_800D8800->xy0 = vtx[0].xy;
    D_800D8800->xy1 = vtx[1].xy;
    D_800D8800->xy2 = vtx[2].xy;
    D_800D8800->xy3 = vtx[3].xy;
    D_800D8800->u0 = vtx[0].u;
    D_800D8800->v0 = vtx[0].v;
    D_800D8800->u1 = vtx[1].u;
    D_800D8800->v1 = vtx[1].v;
    D_800D8800->u2 = vtx[2].u;
    D_800D8800->v2 = vtx[2].v;
    D_800D8800->u3 = vtx[3].u;
    D_800D8800->v3 = vtx[3].v;
    D_800D8800->clut = 0x3A74;
    D_800D8800->tpage = 0xC;
    setaddr(D_800D8800, getaddr(&D_800D244C->primList[BSC_OTHEAD_IDX]));
    setaddr(&D_800D244C->primList[BSC_OTHEAD_IDX], D_800D8800);
}




INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB06C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB2D4);
