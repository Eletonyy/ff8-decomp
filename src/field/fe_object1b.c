/**
 * @file fe_object1b.c
 *
 * The PsyQ 4.3 (gcc 2.8.0) island that sits between @c func_800A63AC and
 * the end of the original fe_object1 translation unit. The functions in
 * this file are originally compiled with the PsyQ 4.3 toolchain — their
 * FILLED branch-delay-slot epilogues (`jr ra; addiu sp` instead of PsyQ
 * 4.1's `addiu sp; jr ra; nop`) are the dead giveaway. The rest of
 * fe_object1 is PsyQ 4.1, so these had to live in a separate translation
 * unit (added to @c PSYQ43_SRCS in the Makefile).
 *
 * Everything below was originally INCLUDE_ASM in fe_object1.c with notes
 * to "split the file" — this is that split.
 */

#include "common.h"
#include "field.h"
#include "psxsdk/libgte.h"
#include "psxsdk/libgpu.h"
#include "field/fe_object1.h"

extern u8 D_800DD6D0[];
extern s32 D_800D6620[];
extern void *func_80047CE4(void *dst, s32 val, s32 n);

/**
 * @brief Per-frame entity render pipeline orchestrator.
 *
 * Three-phase walk over the @c D_800D9630[] entity slot table:
 *
 *  1. **Prepare**: for each active slot (@c slot->base @c != -1), call
 *     @c func_800AA5F8 (per-entity preparation).
 *
 *  2. **Cache update / vertex backup**: for each active slot, either
 *     copy its @c POLY_FT4 / @c POLY_GT4 vertex blocks from the main
 *     buffer to the alt buffer (when @c flags @c & @c 0x10 = DIRTY), or
 *     update the entity's cached @c ctxId / @c frameKey (when not).
 *
 *  3. **Transform** (call @c func_800ACBD4 with the OT base + 1) — runs
 *     the GTE transform pass for all queued vertices.
 *
 *  4. **OT linkage**: for each active slot with @c READY bit set, walk
 *     the slot's quads and link each transformed primitive (via the
 *     classic @c setaddr(prim,getaddr(ot)) + @c setaddr(ot,prim) pair)
 *     into the ordering table at the slot's per-vertex OT index.
 *
 * @note Permuter base at @c permuter/func_800A63AC/base.c sits at 77.02%
 *       — function structure and the three loops match; remaining diff
 *       is register-allocation cascade (reg names s1/s2/s3 shuffled,
 *       t-regs in different slots) and a 0x30-byte stack-frame size
 *       gap. Needs permuter to close the last 23%.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A63AC);

/**
 * @brief Per-quad vertex transform + GTE scratch dispatch.
 *
 * For each quad in the entity's @c ref->quads array, iterates over its
 * vertices (from @c quad->h0 up to @c quad->h0 @c + @c quad->h2) and
 * dispatches on @c quad->hE flags:
 *
 *  - @c bit 0 set (path A): full keyframe transform — calls
 *    @c func_800406A4 to set GTE registers, @c func_80040734 to push,
 *    interpolates the @c (h18-h8)/(h1A-hA)/(h1C-hC) position triplet by
 *    @c h4/h1E, runs the GTE transform via @c func_80040E74, copies an
 *    8-word vertex block from the source buffer, applies the delta, runs
 *    a second @c func_80040E74 + @c func_800406A4 pair, increments
 *    @c h4 and either flips the @c hE bit-0 / sets bit-2 (if exhausted)
 *    or sets @c slot->unk78 = -1 (otherwise).
 *
 *  - @c bit 2 set (path B): same single-step transform without the
 *    second round.
 *
 *  - @c neither set (path C): just @c func_800406A4 + @c func_80040734
 *    on the vertex buffer.
 *
 * After the per-vertex dispatch, calls @c func_800AD048 (or zeroes the
 * D_800D96B0/D_800D6720 entries if the @c slot->flags @c & @c 0x20
 * cached-keyframe path applies) and advances the counters.
 *
 * After the inner loop completes per-quad, calls @c func_800AD0E8 to
 * link @c POLY_FT4 primitives and @c func_800AD300 for @c POLY_GT4.
 *
 * @note Permuter base at @c permuter/func_800A6A80/base.c sits at 56.85%
 *       — three-path branch + shared tail + final calls are in place,
 *       but path A's vertex copy + double-transform sequence is partial
 *       and the stack-frame size is short (0x68 vs target's 0x88).
 *       Needs permuter to close the gap.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A6A80);

/**
 * @brief Reset three field-engine tables.
 *
 * 1. Calls @c func_80047CE4 (@c memset) on each of 64 @c D_800DD6D0
 *    entries (stride 0x30 = 48 bytes), zeroing each in turn.
 * 2. Clears 64 @c s32 entries at @c D_800D6620.
 * 3. Clears 32 @c EntityRenderSlot* pointer entries at @c D_800D9630.
 *
 * The forward for-loops in the scratch source compile to backward
 * @c bgez loops — gcc strength-reduces the index since each iteration
 * is independent.
 */
void func_800A7194(void) {
    s32 i = 0;
    u8 *p = D_800DD6D0;

    do {
        func_80047CE4(p, 0, 0x30);
        i++;
        p += 0x30;
    } while (i < 64);
    for (i = 0; i < 64; i++) {
        D_800D6620[i] = 0;
    }
    for (i = 0; i < 32; i++) {
        D_800D9630[i] = NULL;
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A7224);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A736C);

/**
 * @brief Per-mode write/accumulate of three or four @c s32 values into
 *        an @ref EntityRenderSlot.
 *
 * @param idx   Render-slot index into @ref D_800D9630.
 * @param vals  Pointer to four @c s32 values (only first three used in mode 1).
 * @param mode  @c 0 → overwrite @c field20..field2C; @c 1 → add to
 *              @c field20..field28 (skipping @c field2C); other → no-op.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A74B4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A7564);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A8058);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A81AC);

/**
 * @brief Claim a render-slot pool entry and initialize its state.
 *
 * Acquires @c D_800D9630[idx] if currently @c NULL by installing @p slot
 * and writing @p firstWord to the slot's leading @c s32. The slot's state
 * fields (@c unk10/12/14/18/1A/1C/50/52) are zeroed except @c unk12 which
 * gets the default @c 0x190; the three @c field20/24/28 scale factors are
 * set to @c 0x1000 (unit scale); @c unk60 (active flag) is cleared.
 *
 * @return @p slot @c + @c 0x98 on successful claim, or @c NULL if the
 *         render-slot was already occupied.
 */
s32 *func_800A8CDC(s32 idx, s32 firstWord, EntityRenderSlot *slot) {
    if (D_800D9630[idx] != NULL) {
        return NULL;
    }
    D_800D9630[idx] = slot;
    *(s32 *)slot = firstWord;
    D_800D9630[idx]->unk60 = 0;
    D_800D9630[idx]->unk10 = 0;
    D_800D9630[idx]->unk12 = 0x190;
    D_800D9630[idx]->unk14 = 0;
    D_800D9630[idx]->unk18 = 0;
    D_800D9630[idx]->unk1A = 0;
    D_800D9630[idx]->unk1C = 0;
    D_800D9630[idx]->field20 = 0x1000;
    D_800D9630[idx]->field24 = 0x1000;
    D_800D9630[idx]->field28 = 0x1000;
    D_800D9630[idx]->unk50 = 0;
    D_800D9630[idx]->unk52 = 0;
    D_800D9630[idx]->unk60 = 0;
    return &slot->subBuffer;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A8DAC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A91C8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A9434);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800A97E4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800AA46C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800AA5F8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800AA870);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1b", func_800AA8A0);
