#include "common.h"
#include "world.h"
#include "battle.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"
#include "psxsdk/libc.h"

/**
 * @brief Spawn a pair of POLY_FT4 quads into one of two world-scene icon
 *        pools, with screen-space (x,y) projected through a per-iteration
 *        rotation matrix.
 *
 * Picks the output pool based on @c D_800D244C: when it points at the
 * @c &D_800CA040 "no battle scene" sentinel the @c D_800D8860 pool is
 * used; otherwise @c D_800D8810. The function then iterates 1-2 times
 * (mode == 0 → one iteration of kind 1; mode != 0 → kind 0 then kind 1):
 *
 *  -# Build an SVECTOR rotation with @c vz = D_800C977A+0x400 (kind 0)
 *     or @c D_800D239A-0x600 (kind 1), call @c RotMatrix, push to GTE.
 *  -# Project 4 corner offsets through the matrix via @c gte_mvmva and
 *     add @p screenX / @p screenY to get screen-space XYs.
 *  -# Seed the slot's @c P_TAG (len=9, code=0x2C/0x2E), RGB=(0x80x3),
 *     and either dim-modulated RGB + kind 0 UVs/clut/tpage (when kind 0)
 *     or fixed kind 1 UVs/clut/tpage. The kind-0 brightness is driven by
 *     a triangle wave on @c D_800D23D0.
 *  -# Copy the 4 (x,y) corners into the slot and @c addPrim into
 *     @c D_800D244C->otHead.
 *
 * @param screenX Pixel X offset added to every projected corner X.
 * @param screenY Pixel Y offset added to every projected corner Y.
 * @param mode    Zero → only emit kind 1 (skips kind 0); non-zero → emit
 *                both kinds (kind 0 first, then kind 1).
 *
 */
extern POLY_FT4     D_800D8810[2];
extern POLY_FT4     D_800D8860[2];
extern s16          D_800C977A;
extern s16          D_800D239A;
extern s32          D_800D23D0;

void func_800AB540(s32 screenX, s32 screenY, s32 mode) {
    MATRIX    m;
    SVECTOR   rot;
    struct { s16 x, y; s16 pad[2]; } corners[4];
    SVECTOR   corner_in;
    VECTOR    corner_out;
    POLY_FT4 *slot;
    s32       i, c;
    s32       dim;

    slot = (D_800D244C == &D_800CA040) ? D_800D8860 : D_800D8810;

    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;

    for (i = (mode == 0); i < 2; i++) {

        rot.vx = 0;
        rot.vy = 0;
        if (i == 0) {
            rot.vz = D_800C977A;
        } else {
            rot.vz = D_800D239A;
        }
        {
            s32 v = rot.vz;
            if (i == 0) v += 0x400;
            else        v -= 0x600;
            rot.vz = (s16)v;
        }

        RotMatrix(&rot, &m);
        gte_SetRotMatrix(&m);
        gte_SetTransMatrix(&m);

        for (c = 0; c < 4; c++) {
            if (i == 0) {
                corner_in.vx = (c & 1) ? 2    : -0xD;
                corner_in.vy = (c < 2)  ? -8   : 7;
            } else {
                corner_in.vx = (c & 1) ? 0xE  : -1;
                corner_in.vy = (c < 2)  ? -1   : 0xE;
            }
            corner_in.vz = 0;
            gte_ldv0(&corner_in);
            gte_mvmva(1, 0, 0, 0, 0);
            gte_stlvnl(&corner_out);
            corners[c].x = (s16)(corner_out.vx + screenX);
            corners[c].y = (s16)(corner_out.vy + screenY);
        }

        setPolyFT4(slot);
        setSemiTrans(slot, i);

        setRGB0(slot, 0x80, 0x80, 0x80);

        if (i == 0) {
            /* Yaw-0 (primary) icon: brightness modulated by D_800D23D0 phase. */
            s32 phase = (D_800D23D0 << 3) & 0xFF;
            dim = phase - 0x80;
            if (dim > 0) {
                dim >>= 1;
            } else {
                dim = (0x80 - phase) >> 1;
            }
            dim += 0x40;
            setRGB0(slot, dim, dim, dim);
        }

        if (i == 0) {
            slot->u2 = 0xC0; slot->u0 = 0xC0;
            slot->u3 = 0xCF; slot->u1 = 0xCF;
            slot->v1 = 0x90; slot->v0 = 0x90;
            slot->v3 = 0x9F; slot->v2 = 0x9F;
            slot->tpage = 0x0D;
            slot->clut  = 0x383C;
        } else {
            /* Yaw-1 (alternate) icon: fixed bright frame. */
            slot->u2 = 0xF0; slot->u0 = 0xF0;
            slot->u3 = 0xFF; slot->u1 = 0xFF;
            slot->v1 = 0x70; slot->v0 = 0x70;
            slot->v3 = 0x7F; slot->v2 = 0x7F;
            slot->tpage = 0x2D;
            slot->clut  = 0x383B;
        }

        *(s32 *)&slot->x0 = *(s32 *)&corners[0];
        *(s32 *)&slot->x1 = *(s32 *)&corners[1];
        *(s32 *)&slot->x2 = *(s32 *)&corners[2];
        *(s32 *)&slot->x3 = *(s32 *)&corners[3];

        addPrim(&D_800D244C->primList[BSC_OTHEAD_IDX], slot);
        slot++;
    }
}

/**
 * @brief Spawn one of the 64 in-world icon slots (kind 0x10 / 0x11).
 *
 * Copies the per-call SVECTOR rotation from @c D_800C9770[+8..+0xF] into
 * a local buffer, gets a world position via @ref func_800BC544, then —
 * based on @p kind — selects a per-kind parameter block at
 * @c D_800C5480 + 0x280 (kind < 10 || == 0x80) or @c +0x2A8 (== 0x31)
 * and searches the 64-entry pool at @c D_800D9CB0 for the first slot
 * whose @c count is below its @c limit. The found slot is seeded with
 * its kind tag (0x10 / 0x11), the per-kind frame cap, life=0x1000, the
 * world position, the (yaw-adjusted) rotation, and a projected screen
 * XYZ obtained by running the per-kind SVECTOR offset through @c RotMatrix
 * + the GTE @c mvmva. Always restores @c &D_800C9838 to the rot/trans
 * matrices on exit.
 *
 * @param kind Discriminator: 0..9 / 0x80 spawn kind 0x10, 0x31 spawns
 *             kind 0x11, anything else is a no-op for the spawn path.
 * @param dir  Yaw flip — non-zero adds +0x80 to @c rot.vy, zero adds -0x80.
 * @param a2   Passed through to @ref func_800BC544 (camera distance / mode).
 *
 * @see https://decomp.me/scratch/KwpzK — plateau at 85.49%, blocked on
 *      gcc-2.7.2 picking s1 for the slot pointer where the original picked
 *      s0 (reusing kind's register). Reordering declarations to free s0
 *      first breaks the prologue stack layout.
 */
INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object5", func_800AB8C0);

/**
 * @brief 36-byte prim slot used by @c D_800D4F10[4] — P_TAG header followed
 *        by a 28-byte payload. Seeded with @c len=8 (8-word body) and
 *        @c code=0x38 by @ref func_800ABC98.
 */
typedef struct {
    /* 0x00 */ P_TAG tag;
    /* 0x08 */ u8 pad08[0x1C];
} Prim36;

/**
 * @brief 8-byte short DR_MODE-like used by @c D_800D4FA0[2] — a P_TAG word
 *        (containing @c len) followed by a single GPU command word.
 */
typedef struct {
    /* 0x00 */ u32 tag;   /**< P_TAG-compatible: addr:24 + len:8. */
    /* 0x04 */ u32 cmd;   /**< Raw GP0 command word (e.g. @c 0xE1000220 TPAGE). */
} DrSetMode8;

extern Prim36     D_800D4F10[4];
extern DrSetMode8 D_800D4FA0[2];
extern u8         D_800C5448[];
extern u8         D_800DB0D0[];

extern void func_800A84D0(void);

/**
 * @brief Seed three world-render prim pools and kick off the DR_MODE
 *        primer in @ref func_800A84D0.
 *
 *  1. Primes the four 36-byte slots at @c D_800D4F10 with @c len=8 and
 *     @c code=0x38 (likely a custom 8-word gouraud-textured prim).
 *  2. Primes the two 8-byte slots at @c D_800D4FA0 with @c len=1 and the
 *     GPU draw-mode command word @c 0xE1000220 (TPAGE with 50%% blend).
 *  3. Copies twelve bytes (three u32 entries) from @c D_800C5448 to
 *     @c D_800DB0D0 — preserves the source layout via @c memcpy so the
 *     copy emits @c lwl/@c lwr/@c swl/@c swr pairs (unknown alignment).
 *  4. Calls @ref func_800A84D0 to initialise the larger @c DR_MODE[2][96]
 *     pool that begins at @c D_800D4FB0.
 *
 * @note Matching the original requires a @c do/@c while loop with the
 *       counter increment placed *between* the prim and DR_MODE blocks,
 *       and the second loop's @c i=0 reset hoisted above the @c dst /
 *       @c src setup. Other ordering produces equivalent C but slightly
 *       different gcc 2.8.0 scheduling.
 */
void func_800ABC98(void) {
    s32 i;
    Prim36     *p = D_800D4F10;
    DrSetMode8 *m = D_800D4FA0;
    u8         *src;
    u8         *dst;

    i = 0;
    do {
        setlen(p, 8);
        setcode(p, 0x38);
        p++;
        setlen(p, 8);
        setcode(p, 0x38);
        p++;
        i++;
        setlen(m, 1);
        m->cmd = 0xE1000220;
        m++;
    } while (i < 2);

    i = 0;
    dst = D_800DB0D0;
    src = D_800C5448;
    do {
        memcpy(dst, src, 4);
        dst += 4;
        i++;
        src += 4;
    } while (i < 3);

    func_800A84D0();
}

extern POLY_FT4 D_800D4EC0[2];

/**
 * @brief Initialise the two-element @c POLY_FT4 prim pool at
 *        @c D_800D4EC0 with a fixed semi-transparent textured quad
 *        template.
 *
 * Each of the two slots is seeded with:
 *  - @c len = 9, @c code = 0x2E (POLY_FT4, semi-transparent),
 *  - RGB tint @c (0xB4, 0xB4, 0xB4) (light gray),
 *  - UV rectangle @c (0, 0x40) — @c (0x5F, 0x9F) (95 x 95 patch),
 *  - @c tpage = @c 0x000D, @c clut = @c 0x3835.
 *
 * The XY coordinates are left untouched — the per-frame renderer fills
 * those in. Used to prime the template before per-frame GPU command
 * generation.
 *
 * @note Matching requires (a) all @c u0..u3 stores before @c v0..v3
 *       (with the @c 0x5F values written first so gcc allocates @c t0
 *       to that constant) and (b) the loop counter @c i++ placed at
 *       the top of the body via a @c do/@c while form (gcc otherwise
 *       schedules the increment to the end and the addiu lands in a
 *       different slot).
 */
void func_800ABD54(void) {
    s32 i;
    POLY_FT4 *p = D_800D4EC0;
    i = 0;
    do {
        i++;
        setlen(p, 9);
        setcode(p, 0x2E);
        setRGB0(p, 0xB4, 0xB4, 0xB4);
        p->u2 = 0;
        p->u0 = 0;
        p->u3 = 0x5F;
        p->u1 = 0x5F;
        p->v1 = 0x40;
        p->v0 = 0x40;
        p->v3 = 0x9F;
        p->v2 = 0x9F;
        p->tpage = 0x000D;
        p->clut  = 0x3835;
        p++;
    } while (i < 2);
}

extern CVECTOR D_8009811C;
extern s32 func_8003F9F4(CVECTOR *input, CVECTOR *cue, s32 w1, s32 w2, CVECTOR *out);

/**
 * @brief Per-pixel depth-cued shade-and-pack — feed each @c CVECTOR
 *        through @c func_8003F9F4, then pack the resulting RGB into
 *        PSX 15-bit @c BGR555 (with @c STP from the input's @c cd bit)
 *        at @p output.
 *
 * For each of @p count entries:
 *  - If @c input[i].cd has the @c 0x80 bit set, store @c 0 to
 *    @c output[i] (skipped / transparent).
 *  - Else, call
 *    @c func_8003F9F4(@c &input[i], @c &cue, @c 0x1000 - z, @c z,
 *    @c &rgb) — a depth-cued blend between the per-pixel color and
 *    the cue color copied from @c D_8009811C, weighted by
 *    @c z / (@c 0x1000 - @p z). The output @c CVECTOR (rgb) lands in
 *    a stack scratch.
 *  - If the call set @c cd's high bit on the way out (signalling
 *    "drop this pixel"), clear @c input[i].cd and leave @c output[i]
 *    untouched. Otherwise pack
 *    @c (R>>3) | (cd<<15) | ((G&0xF8)<<2) | ((B&0xF8)<<7) into
 *    @c output[i] — PSX BGR555 layout with the high bit reused as STP.
 *
 * @param input   Per-pixel source @c CVECTOR array (4-byte stride).
 * @param output  Destination halfword array (BGR555 + STP).
 * @param z       Depth weight in 1.12 fixed-point — paired with
 *                @c (0x1000 - z) inside the blend.
 * @param count   Number of pixels to convert. @c <= @c 0 skips.
 *
 * @note Matching needs (a) @c input[i] / @c output[i] array indexing
 *       (not a @c p++ pointer walk — gcc 2.8.0 otherwise splits
 *       @c input into @c s0 @c = @c input+3 plus @c s3 @c = @c input,
 *       costing one extra s-reg) and (b) the final BGR555 store
 *       written as a single inline OR chain — @c output[i] @c = @c
 *       (R>>3) @c | @c (cd<<15) @c | @c ((G&0xF8)<<2) @c | @c
 *       ((B&0xF8)<<7) — so gcc interleaves the R and @c cd loads
 *       first, matching the target instruction order.
 */
void func_800ABDD8(CVECTOR *input, u16 *output, s32 z, s16 count) {
    CVECTOR rgb;
    CVECTOR cue;
    s32 i;

    memcpy(&cue, &D_8009811C, sizeof(cue));

    for (i = 0; i < count; i++) {
        if (input[i].cd & 0x80) {
            output[i] = 0;
        } else {
            func_8003F9F4(&input[i], &cue, 0x1000 - z, z, &rgb);
            if (input[i].cd & 0x80) {
                input[i].cd = 0;
            } else {
                output[i] = (rgb.r >> 3)
                          | (input[i].cd << 15)
                          | ((rgb.g & 0xF8) << 2)
                          | ((rgb.b & 0xF8) << 7);
            }
        }
    }
}

extern MATRIX D_800C9838;

/**
 * @brief Project 4 SVECTORs through (custom rot+trans) × (camera) to screen.
 *
 * Builds an inner matrix from @p rot via @c RotMatrix and sign-extends
 * @p trans into its translation row, then transforms each of the 4
 * @p src vertices through it (mvmva). Loads the camera matrix
 * @c D_800C9838 and projects the 4 transformed vertices to screen XY
 * via @c RTPT (vertices 0–2) + @c RTPS (vertex 3), writing the four
 * packed SXY values to @p outSXY. Finally @c AVSZ4 averages the
 * resulting Z values and writes the result to @p outOTZ.
 *
 * @param src     4 input SVECTORs (stride 8 — packed array).
 * @param rot     Rotation angles for the inner matrix (passed to @c RotMatrix).
 * @param trans   Translation (s16 components sign-extended into @c matrix.t).
 * @param outSXY  4 packed s32 SXY outputs (16 bytes total).
 * @param outOTZ  s32 average-Z output.
 */
void func_800ABEF0(SVECTOR *src, SVECTOR *rot, SVECTOR *trans,
                   s32 *outSXY, s32 *outOTZ) {
    MATRIX  m;
    SVECTOR transformed[4];
    s32     i;

    RotMatrix(rot, &m);
    gte_SetRotMatrix(&m);
    m.t[0] = trans->vx;
    m.t[1] = trans->vy;
    m.t[2] = trans->vz;
    gte_SetTransMatrix(&m);

    for (i = 0; i < 4; i++) {
        gte_ldv0(&src[i]);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stsv(&transformed[i]);
    }

    gte_SetRotMatrix(&D_800C9838);
    gte_SetTransMatrix(&D_800C9838);

    gte_ldv3(&transformed[0]);
    gte_RTPT();
    gte_stsxy3(outSXY);

    gte_ldv0(&transformed[3]);
    gte_RTPS();
    gte_stsxy(outSXY + 3);

    gte_AVSZ4();
    gte_stotz(outOTZ);
}

extern s32 func_8009CC3C(void);

/**
 * @brief Spawn an inactive slot in the @c D_800D9CB0 particle pool.
 *
 * Finds the first slot in @c D_800D9CB0[64] where @c count @>= @c limit
 * (= inactive/reusable), initializes it from (@p type, @p pos, @p vec),
 * optionally jitters @c life and @c limit via the RNG @c func_8009CC3C,
 * builds a rotation matrix from @p vec via @c RotMatrix, pushes it to
 * the GTE (zero translation), then projects the kind's per-axis
 * @c offset through the matrix into the slot's @c proj_xyz.
 *
 * @note The two sentinel checks (pre-loop and post-loop) use the same
 *       expression @c &D_800D9CB0[64] so gcc CSEs both branches into a
 *       single target. Using the @c D_800DA8B0 alias for the post-check
 *       would prevent the fold and leave the pre-loop branch pointing at
 *       the post-check instead of the epilogue (matches target's encoded
 *       branch destination once both checks share an expression).
 *
 * @param type   Slot kind — also index into the @c D_800C5480 KindParams table.
 * @param pos    Source world position (12 bytes — only low s16 of each coord read).
 * @param vec    Source rotation vector (8 bytes, unaligned-tolerant copy).
 * @param flags  Bit 1 → RNG-jitter @c life by [-0x80, +0x7F].
 *               Bit 0 → RNG-jitter @c limit by [-4, +3].
 */
s32 func_800AC0A0(s32 type, VECTOR *pos, SVECTOR *vec, u16 flags) {
    Slot30     *slot;
    KindParams *kp;
    SVECTOR    *vcp;
    VECTOR      projected;
    SVECTOR    *vcp2;
    SVECTOR     vec_copy;
    MATRIX      m;

    slot = D_800D9CB0;
    kp = &D_800C5480[type];

    while (slot < &D_800D9CB0[64]) {
        if (slot->count >= slot->limit) break;
        slot++;
    }
    if (slot >= &D_800D9CB0[64]) return;

    slot->kind  = (u8)type;
    slot->count = 0;
    slot->limit = kp->limit;
    slot->life  = 0x1000;
    slot->pos.vx = pos->vx;
    slot->pos.vy = pos->vy;
    slot->pos.vz = pos->vz;

    vcp = &vec_copy;
    vcp2 = vcp;
    memcpy(vcp2, vec, 8);

    if (flags & 2) {
        slot->life += func_8009CC3C() - 0x80;
    }
    if (flags & 1) {
        slot->limit += (func_8009CC3C() & 7) - 4;
    }

    memcpy(&slot->rot, vcp, 8);

    RotMatrix(vcp, &m);
    gte_SetRotMatrix(&m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    gte_SetTransMatrix(&m);

    gte_ldv0(&kp->offset);
    gte_mvmva(1, 0, 0, 0, 0);
    gte_stlvnl(&projected);

    slot->proj_x = (s16)projected.vx;
    slot->proj_y = (s16)projected.vy;
    slot->proj_z = (s16)projected.vz;
}

extern s32 D_800D23D0;
extern void func_80048FBC(RECT *r, s32 src_x, s32 src_y);
extern s32  func_80048C50(s32 arg);

/**
 * @brief Stream two consecutive 8x32 sprite slots into VRAM, advancing
 *        through a 4-slot rotation keyed off @c D_800D23D0.
 *
 * Picks a starting slot from @c (D_800D23D0 >> 2) & 3, then issues two
 * back-to-back @c func_80048FBC blits (the LoadImage-style transfer) at
 * destination VRAM coords @c (0x358, 0x60) and @c (0x360, 0x60), each
 * sandwiched by a @c func_80048C50(0) sync. Source RECTs are
 * @c (slot * 8 + 0x340, 0xA0, 8, 32) and @c (((slot + 1) & 3) * 8 +
 * 0x340, 0xA0, 8, 32) — adjacent 8-wide tiles wrapping every 4 slots.
 *
 * @note Matching requires the second iter's @c r.x to read @c slot
 *       through @c (*&slot) instead of @c slot directly — the explicit
 *       address-of shifts gcc 2.8.0's s-register allocation so @c slot
 *       lands in @c s0 (lowest priority slot) and the three RECT
 *       constants (@c 0xA0 / @c 8 / @c 0x20) take @c s3 / @c s2 / @c s1
 *       in descending order, matching the target. The "natural" second
 *       @c r.x @c = @c slot @c * @c 8 @c + @c 0x340 leaves @c slot in
 *       @c s3 and rotates the rest, dropping the match to 87.89%.
 */
void func_800AC2B8(void) {
    s32 slot;
    RECT r;

    slot = D_800D23D0;
    slot = (slot >> 2) & 3;
    r.x = slot * 8 + 0x340;
    r.y = 0xA0;
    r.w = 0x8;
    r.h = 0x20;
    func_80048FBC(&r, 0x358, 0x60);
    func_80048C50(0);

    slot = (slot + 1) & 3;
    r.x = (*&slot) * 8 + 0x340;
    r.y = 0xA0;
    r.w = 0x8;
    r.h = 0x20;
    func_80048FBC(&r, 0x360, 0x60);
    func_80048C50(0);
}

extern s16 D_800C53B8[];
extern s16 D_800C53C4[];
extern s16 D_800C53D0[];

/**
 * @brief Sibling of @ref func_800AC3EC for a different table triple —
 *        per-axis tile delta between two reference tables, scaled by
 *        @p divisor.
 *
 * Picks one of two reference tables — @c D_800C53C4 when @p use_alt is
 * non-zero, @c D_800C53D0 otherwise — subtracts the matching entry of
 * @c D_800C53B8, and divides the result by @p divisor.
 *
 * @param idx       Halfword index shared across the three tables.
 * @param divisor   Signed divisor for the delta.
 * @param use_alt   Non-zero picks @c D_800C53C4; zero picks
 *                  @c D_800C53D0.
 * @return          @c (table[idx] - D_800C53B8[idx]) / divisor.
 */
s32 func_800AC370(s32 idx, s32 divisor, s32 use_alt) {
    if (use_alt != 0) {
        return (D_800C53C4[idx] - D_800C53B8[idx]) / divisor;
    }
    return (D_800C53D0[idx] - D_800C53B8[idx]) / divisor;
}

extern s16 D_800C53DC[];
extern s16 D_800C53E4[];
extern s16 D_800C53EC[];

/**
 * @brief Per-axis tile delta between two map-layout tables, scaled by
 *        @p divisor.
 *
 * Picks one of two reference tables — @c D_800C53DC when @p use_alt is
 * non-zero, @c D_800C53E4 otherwise — subtracts the matching entry of
 * @c D_800C53EC, and divides the result by @p divisor. The tables are
 * indexed as halfword arrays of @p idx, so callers pass the same index
 * to look up paired values across the three tables.
 *
 * @param idx       Halfword index shared across the three tables.
 * @param divisor   Signed divisor for the delta.
 * @param use_alt   Non-zero picks @c D_800C53DC; zero picks
 *                  @c D_800C53E4.
 * @return          @c (table[idx] - D_800C53EC[idx]) / divisor.
 */
s32 func_800AC3EC(s32 idx, s32 divisor, s32 use_alt) {
    if (use_alt != 0) {
        return (D_800C53DC[idx] - D_800C53EC[idx]) / divisor;
    }
    return (D_800C53E4[idx] - D_800C53EC[idx]) / divisor;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object5", func_800AC468);

extern s32  func_8003F4A4(s32 a);
extern s32  func_80041E84(s32 y, s32 x);
extern s32  func_800A40F8(VECTOR *pos, SVECTOR *out_buf);
extern s32  func_800A4700(s32 a, s32 b);
extern s32  func_800A475C(s32 a, s32 b);
extern void func_800ACC68(MATRIX *out_mat, SVECTOR *angles,
                          SVECTOR *rotBuf, SVECTOR *offset);

/**
 * @brief Compute a 3D pitch/yaw frame between two world positions and build
 *        the camera-relative rotation matrix for the chained target.
 *
 * Treating each @c VECTOR as low-s16 world coords, this:
 *  -# Computes the s16 component-wise delta and its squared distance via
 *     the GTE @c SQR instruction.
 *  -# Takes the 3D and horizontal sqrts (@c func_8003F4A4) for the slant
 *     and ground-plane distances.
 *  -# Resolves @c pitch and @c yaw via @c func_80041E84 (atan2-like).
 *  -# Calls @c func_800A40F8 (with @p posB) to seed @c rotBuf[0] with the
 *     view-yaw projection and returns the view-yaw.
 *  -# Wraps the view-yaw and @p angleArg into per-axis tile deltas
 *     (@c func_800A4700 / @c func_800A475C), scales them by
 *     @c WORLD_TILE_SIZE into a @c (tileX,0,-tileZ) 3D delta vector,
 *     writes that delta into @c rotBuf[1] and adds it to @c rotBuf[0].
 *  -# Calls @c func_800ACC68 to compose the final rotation matrix.
 *  -# Each non-null output pointer receives a copy of the corresponding
 *     piece: @p outMat ← matrix, @p outAngles ← (pitch,yaw,0),
 *     @p outOffset ← (0,0,-distFull), @p outRotBuf ← @c rotBuf[0].
 *
 * @param posA       World position A (delta source).
 * @param posB       World position B (delta target; also fed to @c func_800A40F8).
 * @param angleArg   Camera yaw wrapped together with view-yaw into tile deltas.
 * @param outMat     Optional 32-byte rotation matrix output.
 * @param outAngles  Optional 8-byte SVECTOR output ← (pitch, yaw, 0).
 * @param outOffset  Optional 8-byte SVECTOR output ← (0, 0, -distFull).
 * @param outRotBuf  Optional 8-byte SVECTOR output ← biased @c rotBuf[0].
 */
void func_800AC778(VECTOR *posA, VECTOR *posB, s16 angleArg,
                   MATRIX *outMat, SVECTOR *outAngles, SVECTOR *outOffset,
                   SVECTOR *outRotBuf) {
    SVECTOR delta;
    SVECTOR offset;
    SVECTOR angles;
    SVECTOR rotBuf[2];
    MATRIX  m;
    VECTOR  sq;
    s32     distFull;
    s32     distXZ;
    s16     viewYaw;
    s16     tileX, tileZ;
    s32     tileX_scaled, tileY_scaled, tileZ_scaled;

    delta.vx = (s16)posB->vx - (s16)posA->vx;
    delta.vy = (s16)posB->vy - (s16)posA->vy;
    delta.vz = (s16)posB->vz - (s16)posA->vz;

    gte_ldsv(&delta);
    gte_SQR(0);
    gte_stlvnl(&sq);

    distFull = func_8003F4A4(sq.vx + sq.vy + sq.vz);
    distXZ   = func_8003F4A4(sq.vx + sq.vz);

    angles.vx = func_80041E84(-delta.vy, distXZ);
    angles.vy = func_80041E84(delta.vx, delta.vz);
    angles.vz = 0;

    offset.vx = 0;
    offset.vy = 0;
    offset.vz = -distFull;

    viewYaw = func_800A40F8(posB, &rotBuf[0]);
    tileX = func_800A4700(viewYaw, angleArg);
    tileZ = func_800A475C(viewYaw, angleArg);

    tileX_scaled =   tileX * WORLD_TILE_SIZE;
    tileY_scaled = 0;
    tileZ_scaled = -(tileZ * WORLD_TILE_SIZE);

    rotBuf[1].vx = tileX_scaled;
    rotBuf[1].vy = tileY_scaled;
    rotBuf[1].vz = tileZ_scaled;

    rotBuf[0].vx += tileX_scaled;
    rotBuf[0].vy += tileY_scaled;
    rotBuf[0].vz += tileZ_scaled;

    func_800ACC68(&m, &angles, &rotBuf[0], &offset);

    if (outMat   ) *outMat    = m;
    if (outAngles) *outAngles = angles;
    if (outOffset) *outOffset = offset;
    if (outRotBuf) *outRotBuf = rotBuf[0];
}

/**
 * @brief Compute an @c SVECTOR offset from two angle-like inputs and write
 *        it to @p out, scaled to tile-units.
 *
 * Calls @c func_800A4700 (signed delta wrapped to ~[-64, 64]) and
 * @c func_800A475C (signed delta wrapped to ~[-48, 48]) on (@p z, @p x)
 * to get the per-axis tile deltas, then multiplies each by
 * @c WORLD_TILE_SIZE and stores the result in @p out. Vertical axis
 * (vy) is zeroed; vz is negated for the PS1 y-down screen convention.
 *
 * @param out  Destination @c SVECTOR — receives
 *             @c (delta_x * WORLD_TILE_SIZE, 0, -delta_z * WORLD_TILE_SIZE).
 * @param x    Tile-space X coordinate (sign-extended s16).
 * @param z    Tile-space Z coordinate (sign-extended s16).
 *
 * @note Matching requires hoisting the products into @c s32 @c vx and
 *       @c vz locals before the @c sh store. Assigning
 *       @c out->vx @c = @c (s16)first @c * @c WORLD_TILE_SIZE directly
 *       lets gcc 2.8.0 elide the sign-extend (the sll/sra pair
 *       collapses to a single sll), dropping the match from 100% to
 *       93.23%.
 */
void func_800AC9F4(SVECTOR *out, s16 x, s16 z) {
    s32 first  = func_800A4700(z, x);
    s32 second = func_800A475C(z, x);
    s32 vx     = (s16)first  * WORLD_TILE_SIZE;
    s32 vz     = -((s16)second * WORLD_TILE_SIZE);
    out->vx = vx;
    out->vy = 0;
    out->vz = vz;
}

extern void     func_800423DC(VECTOR *a, s32 *b_pos, VECTOR *out);
extern WorldPos D_800C9868;
extern s32      func_800A5DC8(s32 x, s32 y);
void            func_800ACB70(s16 angle, VECTOR *out);   /* forward — defined below func_800ACA70. */

/**
 * @brief 32-byte input passed to @ref func_800ACA70. Contains two
 *        16-byte halves: a 4-word @c a block (opaque, forwarded as-is
 *        to @c func_800423DC) and a @c b block whose first u32 is
 *        kept while the trailing 3 s32s are negated to invert a
 *        position before the call.
 */
typedef struct {
    /* 0x00 */ VECTOR a;       /**< Opaque 16-byte block, passed through. */
    /* 0x10 */ s32    b_pre;   /**< Untouched (likely a tag / mode byte). */
    /* 0x14 */ s32    b_x;     /**< Negated before @c func_800423DC. */
    /* 0x18 */ s32    b_y;     /**< Negated. */
    /* 0x1C */ s32    b_z;     /**< Negated. */
} Input32;

/**
 * @brief Combine a transformed @ref Input32 with the camera-angle world
 *        offset and write the sum to @p out.
 *
 * Copies the 32-byte @p input into a local @ref Input32, negates the
 * three trailing fields of the @c b half (@c b_x / @c b_y / @c b_z) to
 * invert that position, and hands @c &a / @c &b_x off to
 * @c func_800423DC together with a stack scratch @c result.
 *
 * Then reads the camera world position from @c D_800C9868, derives a
 * 16-bit angle via @c func_800A5DC8, expands it into a world-axis
 * @c VECTOR via @c func_800ACB70, and — if @p out is non-NULL — writes
 * @c result @c + @c angle_vec into @p out per axis.
 *
 * @param out    Destination @c VECTOR; @c NULL skips the final add.
 * @param input  32-byte input passed by reference.
 */
void func_800ACA70(VECTOR *out, Input32 *input) {
    Input32 local;
    VECTOR  result;
    VECTOR  vec;
    s16     angle;

    local = *input;

    local.b_x = -local.b_x;
    local.b_y = -local.b_y;
    local.b_z = -local.b_z;

    func_800423DC(&local.a, &local.b_x, &result);

    angle = func_800A5DC8(D_800C9868.x, D_800C9868.y);
    func_800ACB70(angle, &vec);

    if (out != 0) {
        out->vx = result.vx + vec.vx;
        out->vy = result.vy + vec.vy;
        out->vz = result.vz + vec.vz;
    }
}

/**
 * @brief Expand a 16-bit @p angle into a world-axis @c VECTOR via
 *        @c angle / 128 and @c angle % 128 decomposition.
 *
 * Splits @p angle into:
 *  - @c mod @c = @c angle @c % @c 128 — low-7-bit residue, used to drive
 *    the X axis: @c out->vx @c = @c mod @c * @c 0x800 @c - @c 0x20000.
 *  - @c div @c = @c angle @c / @c 128 — high bits, used for Z:
 *    @c out->vz @c = @c 0x18000 @c - @c div @c * @c 0x800.
 *  - @c out->vy stays @c 0.
 *
 * The @c / and @c % are signed (round-toward-zero), matching the
 * @c bgez @c +0x7F @c sra fix-up pattern in the asm.
 *
 * @param angle Input angle (s16).
 * @param out   Destination @c VECTOR; @c NULL skips the writes.
 */
void func_800ACB70(s16 angle, VECTOR *out) {
    s32 mod = angle % 128;
    s32 div = angle / 128;
    if (out != NULL) {
        out->vx = mod * 0x800 - 0x20000;
        out->vy = 0;
        out->vz = 0x18000 - div * 0x800;
    }
}

/**
 * @brief Bias an @c SVECTOR by the camera-angle-projected world offset
 *        produced by @c func_800ACB70 and write the result to @p out.
 *
 * Reads the current camera world position from @c D_800C9868, derives
 * an @c s16 angle via @c func_800A5DC8(@c x, @c y), expands that angle
 * into a per-axis world @c VECTOR via @c func_800ACB70, then — if
 * @p out is non-NULL — adds the components of @p in (an @c SVECTOR
 * with @c s16 fields) to that vector and stores the sum into @p out.
 *
 * @param out  Destination @c VECTOR. @c NULL skips the add and
 *             effectively makes this a pure "advance the camera angle
 *             projection" call (the @c VECTOR on the stack is dropped).
 * @param in   Per-axis bias to add (only consulted when @p out is
 *             non-NULL).
 */
void func_800ACBD0(VECTOR *out, SVECTOR *in) {
    VECTOR vec;
    s16 angle;

    angle = func_800A5DC8(D_800C9868.x, D_800C9868.y);
    func_800ACB70(angle, &vec);

    if (out != 0) {
        out->vx = in->vx + vec.vx;
        out->vy = in->vy + vec.vy;
        out->vz = in->vz + vec.vz;
    }
}
