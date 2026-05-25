#include "common.h"
#include "battle.h"
#include "psxsdk/libgpu.h"
#include "world.h"

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A64DC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A688C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A6A74);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A6BE0);

/**
 * @brief Splice three of @c a0's bone prims into two pairs of OT slots.
 *
 * For each of the three bone ids in @c D_800C53B8[0], @c [3], @c [4],
 * link the entity's prim pointer between the slot-A and slot-B halves
 * of an OT pair, twice — once for the primary OT array at
 * @c D_800D3E98 and once for the secondary at @c D_800D40D8. The cond
 * bit picks the @c [0] or @c [1] row of the array, switching layout
 * for non-canonical entity models.
 *
 * @param a0 Entity model whose @c primList holds the prims to splice.
 */
void func_800A735C(EntityModel *a0) {
    s32 cond = (a0 != &D_800CA040) ? 1 : 0;

    addPrims(&a0->primList[D_800C53B8[0]], &D_800D3E50[cond][0].link, &D_800D3E98[cond][0].link);
    addPrims(&a0->primList[D_800C53B8[3]], &D_800D3E50[cond][1].link, &D_800D3E98[cond][1].link);
    addPrims(&a0->primList[D_800C53B8[4]], &D_800D3E50[cond][2].link, &D_800D3E98[cond][2].link);

    addPrims(&a0->primList[D_800C53B8[0]], &D_800D4090[cond][0].link, &D_800D40D8[cond][0].link);
    addPrims(&a0->primList[D_800C53B8[3]], &D_800D4090[cond][1].link, &D_800D40D8[cond][1].link);
    addPrims(&a0->primList[D_800C53B8[4]], &D_800D4090[cond][2].link, &D_800D40D8[cond][2].link);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7590);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7B38);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7CD0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7E74);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8024);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8270);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8400);

extern DR_MODE D_800D4FB0[2][96];

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

extern u8 D_800D9CB0[];
extern POLY_FT4 D_800D88B0[2][64];
extern TILE D_800DA8D0[2][64];

/**
 * @brief Initialise three world-render pools in one go:
 *
 *  1. Clear bytes @c 0x28 and @c 0x29 of every 0x30-stride entry in
 *     the @c D_800D9CB0 pool (size @c 0xC00 bytes ⇒ 64 entries).
 *  2. Prime each @c POLY_FT4 in @c D_800D88B0[2][64] with
 *     @c len = @c 9 and @c code = @c 0x2C.
 *  3. Prime each @c TILE in @c D_800DA8D0[2][64] with
 *     @c len = @c 3 and @c code = @c 0x60.
 *
 * Used at world setup time to prepare the per-frame prim templates.
 */
void func_800A9254(void) {
    u8 *p = D_800D9CB0;
    s32 j, i;

    while (p < &D_800D9CB0[0xC00]) {
        p[0x28] = 0;
        p[0x29] = 0;
        p += 0x30;
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

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9CC0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9E24);

extern POLY_GT4 D_800D5A00[2][64];
extern POLY_GT3 D_800D7400[2][64];

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

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAD48);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAE04);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAEAC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAED4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAF84);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAFBC);

/**
 * @brief 0x1C-byte prim link — first word is an addPrim-style P_TAG, the
 *        @c prim_len halfword at @c 0x1A is set to @c 0xC by the inserter.
 */
typedef struct {
    /* 0x00 */ s32 link;
    /* 0x04 */ u8 pad04[0x16];
    /* 0x1A */ s16 prim_len;
} PrimLink;

/**
 * @brief Set @p p's length to @c 0xC and splice it onto the
 *        @c D_800D244C->otHead chain using @p maskTake / @p maskKeep
 *        bit-merge masks.
 *
 * The two masks select which bit-groups of the link words come from
 * the old chain vs the new node — calling with
 * @p maskTake = @c 0x00FFFFFF and @p maskKeep = @c 0xFF000000 yields
 * the standard PsyQ @c addPrim semantics (preserve the high tag byte,
 * overwrite the low-24 next pointer).
 *
 * @param p         New node to splice in. Its @c prim_len is reset to
 *                  @c 0xC and its @c link receives the previous
 *                  @c otHead bits selected by @p maskTake.
 * @param unused    Ignored.
 * @param maskTake  Bit-mask selecting the "new value" bits.
 * @param maskKeep  Bit-mask selecting the "old value" bits.
 * @return Updated @c otHead value (also written into the chain).
 */
s32 func_800AB02C(PrimLink *p, s32 unused, s32 maskTake, s32 maskKeep) {
    BattleSceneCtx *ctx;
    s32 result;

    p->prim_len = 0xC;
    ctx = D_800D244C;
    p->link = (p->link & maskKeep) | (ctx->otHead & maskTake);
    result = (ctx->otHead & maskKeep) | ((s32)p & maskTake);
    ctx->otHead = result;
    return result;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB06C);


INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB100);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB2D4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB300);
