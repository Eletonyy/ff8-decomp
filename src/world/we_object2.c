#include "common.h"
#include "battle.h"
#include "field.h"
#include "gamestate.h"
#include "sound.h"
#include "btl_sfx.h"
#include "world.h"
#include "world/we_object2.h"

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009CCE8);


/**
 * @brief Dispatch SPU cmd 0x1A with (@p a0, 0x78, @p val), stash return as handle.
 *
 * Sister of func_8009CDFC: saves the return value of
 * @c sndCmd1A(a0, 0x78, val) — where @p val is sign-extended from s8 —
 * into the field-engine state's @c soundHandle0 slot.
 *
 * @note @c sndCmd1A is declared here as returning @c s32 for the same reason
 *       as func_8009CDFC's @c sndCmd10 override.
 *
 * @param a0 Parameter for SPU command 0x1A.
 * @param val Signed 8-bit value forwarded as the third arg.
 */
void func_8009CDC4(s32 a0, s8 val) {
    g_fieldVars->soundHandle0 = sndCmd1A(a0, 0x78, val);
}

/* sndCmd10 / sndCmdC1 prototypes come from <sound.h>. */

/**
 * @brief Dispatch SPU cmd 0x10 with @p a0, stash its handle, then issue cmd C1.
 *
 * Saves the return value of @c sndCmd10(a0) into the field-engine state's
 * @c soundHandle0 slot, then calls @c sndCmdC1(0, 0x3C, val) where @p val
 * is sign-extended from s8.
 *
 * @note @c sndCmd10 is declared here as returning @c s32 even though its
 *       definition in snd_init.c is @c void — the return register (v0)
 *       carries the chained return from @c func_8001A1E8 that the caller
 *       uses here. Overlay-local signature override.
 *
 * @param a0 Parameter for SPU command 0x10.
 * @param val Signed 8-bit value forwarded to sndCmdC1 as the third arg.
 */
void func_8009CDFC(s32 a0, s8 val) {
    g_fieldVars->soundHandle0 = sndCmd10(a0);
    sndCmdC1(0, 0x3C, val);
}

/**
 * @brief Send sound command 0x11 then reset audio channel 0's state byte.
 *
 * Issues @c sndCmd11(0) and marks audio channel 0 as inactive by writing
 * -1 to the field-engine state's @c audioChannel0State byte.
 */
void func_8009CE40(void) {
    sndCmd11(0);
    g_fieldVars->audioChannel0State = -1;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009CE70);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009CFB4);


/**
 * @brief Vsync, scroll any partial display window back to origin, then
 *        rebuild the screen-0 DRAWENV/DISPENV and clear the entity model.
 *
 * Steps:
 *  1. @c VSync(0) — wait for vblank.
 *  2. If @c (DISPENV*)D_8005F138->disp.x is non-zero, @c MoveImage
 *     scrolls the live display rect back to @c (0,0) and a @c DrawSync
 *     stalls until the move completes.
 *  3. @c func_800A5F78(0) / @c func_800A5FD4(0) push the screen-0
 *     DRAWENV and DISPENV.
 *  4. @c ClearImage((RECT*)&D_800CA040, 0,0,0) blacks the entity-model
 *     blob (the first 8 bytes of @ref D_800CA040 are interpreted as a
 *     @c RECT here).
 *  5. Final @c DrawSync(0) before returning.
 *
 * The unused @c DRAWENV / @c DISPENV locals match the original stack
 * frame — they live on @c sp but are never written here (a leftover
 * from the helper inlining the per-screen env setup once did).
 */
void func_8009D0F0(void) {
    DRAWENV draw;
    DISPENV disp;
    DISPENV *env;

    VSync(0);
    env = (DISPENV *)D_8005F138;
    if (env->disp.x != 0) {
        MoveImage(&env->disp, 0, 0);
        DrawSync(0);
    }
    func_800A5F78(0);
    func_800A5FD4(0);
    ClearImage(&D_800CA040, 0, 0, 0);
    DrawSync(0);
}

/**
 * @brief If sequence @p idx isn't already running, kick it off via func_80099EDC.
 *
 * Checks @c D_800C4FD8[idx].field12 — if it's 0 (idle), calls
 * @c func_80099EDC(idx) to start the sequence and returns 1. If already
 * running (non-zero field12), returns 0 without doing anything.
 *
 * @param idx Index into the @c D_800C4FD8 sequence table.
 * @return 1 if the sequence was started, 0 if it was already running.
 */
s32 func_8009D16C(s32 idx) {
    if (D_800C4FD8[idx].field12 != 0) return 0;
    func_80099EDC(idx);
    return 1;
}

/**
 * @brief Stop sequence @p idx via sndCmd21 if it's running, and clear its state.
 *
 * If @c D_800C4FD8[idx].field12 is zero (sequence not running), returns 0.
 * Otherwise dispatches @c sndCmd21(entry->field10, entry->field04),
 * clears @c field12, and returns 1.
 *
 * @param idx Index into the @c D_800C4FD8 sequence table.
 * @return 1 if the sequence was stopped, 0 if it wasn't running.
 */
s32 func_8009D1B8(s32 idx) {
    if (D_800C4FD8[idx].field12 == 0) return 0;
    sndCmd21(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04);
    D_800C4FD8[idx].field12 = 0;
    return 1;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009D214);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009D2D8);


/**
 * @brief Reset the scene state block and kick off the scene loader.
 *
 * Chains:
 *  1. @c func_8009D630() to tear down prior state.
 *  2. Resets @c D_80082C8C — mode 5, current dispatch code, markers -1.
 *  3. Calls @c func_800B3FD4 with the scene-data pointer @c D_800D226C
 *     and sub-mode 5.
 */
void func_8009D3F4(void) {
    func_8009D630();
    D_80082C8C.mode = 5;
    D_80082C8C.cmd = D_800C4D38;
    D_80082C8C.unk02 = -1;
    D_80082C8C.unk03 = -1;
    func_800B3FD4(D_800D226C, 5);
}


/**
 * @brief Enter scene mode @c 3 with a packed @c marker, then mass-reset
 *        the world @c SfxSlot table and trigger a render flush.
 *
 * Steps:
 *  1. Snapshot @p marker into @c D_800C8CEA, tear down prior scene
 *     state via @c func_8009D630, and reset @c D_80082C8C to mode 3
 *     with the marker's two bytes in @c unk02 / @c unk03 and the
 *     current dispatch code (@c D_800C4D38) in @c cmd.
 *  2. @c func_800B3FD4(D_800D226C, 3) re-enters the scene driver.
 *  3. Walk the first 13 @c D_800C526C @c SfxSlot entries: fade out each
 *     active slot via @c fadeOutSfxFast and mark it inactive (@c -1).
 *  4. Force a full render: @c renderAndUpdateDisplay(2) then dispatch
 *     the world display list at @c D_800D244C+0x74.
 */
void func_8009D44C(s32 marker) {
    s32 i;
    D_800C8CEA = marker;
    func_8009D630();
    D_80082C8C.mode = 3;
    D_80082C8C.unk02 = (s8)marker;
    D_80082C8C.unk03 = (s8)((u32)marker >> 8);
    D_80082C8C.cmd = D_800C4D38;
    func_800B3FD4(D_800D226C, 3);
    D_80082C0A = 0;
    for (i = 0; i < 13; i++) {
        s32 sfx = D_800C526C[i].field02;
        D_800C526C[i].field00 = -1;
        fadeOutSfxFast(sfx);
    }
    renderAndUpdateDisplay(2);
    renderBattleDisplayList(&D_800D244C->primList[BSC_COLORTAG_IDX]);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009D510);


/**
 * @brief Tear down the active session tracked by D_800C4D84/D_800C4D88.
 *
 * If either @c D_800C4D84 is non-zero (active handle) or
 * @c D_800C4D88 is non-negative (valid slot), calls @c func_800C4450
 * to release the session, clears @c D_800C4D84, and marks
 * @c D_800C4D88 as -1 (inactive).
 *
 * A no-op when both trackers already report inactive.
 */
void func_8009D630(void) {
    if (D_800C4D84 != 0 || D_800C4D88 >= 0) {
        func_800C4450();
        D_800C4D84 = 0;
        D_800C4D88 = -1;
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009D688);

/**
 * @brief 4-byte slot (two halfwords). Default value is two @c -1s.
 */

/**
 * @brief Fill three adjacent @ref HalfwordPair slots from three lookup
 *        helpers, defaulting each slot to @c (-1, @c -1) on lookup miss.
 *
 * Walks @p s through three consecutive slots, calling one of three
 * fetchers per slot:
 *  - slot @c [0]: @c func_800BD380
 *  - slot @c [1]: @c func_800BD2A0
 *  - slot @c [2]: @c func_800BD460
 *
 * Each fetcher writes into @c (&s->a, @c &s->b) and returns non-zero on
 * success. If it returns @c 0 (no entry available), the slot is filled
 * with @c (-1, @c -1) — the "inactive" sentinel.
 *
 * @param s First slot of a three-slot array.
 */
void func_8009D760(HalfwordPair *s) {
    if (func_800BD380(&s->a, &s->b) == 0) {
        s->b = -1;
        s->a = -1;
    }
    s++;
    if (func_800BD2A0(&s->a, &s->b) == 0) {
        s->b = -1;
        s->a = -1;
    }
    s++;
    if (func_800BD460(&s->a, &s->b) == 0) {
        s->b = -1;
        s->a = -1;
    }
}

/** @brief Compare input against two entity IDs, return 0x29, 0x18, or -1. */
s32 func_8009D7D8(s32 a0) {
    s32 result = -1;

    if (D_800C9E68 == a0) {
        result = 0x29;
    } else if (D_800C9E70 == a0) {
        result = 0x18;
    }
    return result;
}

/** Clears bit 0x40 on two related flag bytes. */
void func_8009D814(void) {
    D_800780D8[0x108] &= ~0x40;
    D_800D23D8[0x66] &= ~0x40;
}

/**
 * @brief Clear two flag bytes at entity-relative offsets.
 * @param a0 Entity slot index offset.
 */
void func_8009D840(s32 a0) {
    u8 *base1 = D_800780D8;
    u8 *base2 = D_800D23D8;

    *(u8 *)(a0 + (s32)base1 + 0x10D) = 0;
    *(u8 *)(a0 + (s32)base2 + 0x6B) = 0;
}


/**
 * @brief Dispatch a table-driven SFX slot cleanup if the slot is active.
 *
 * Looks up @c D_800C526C[idx]; if its @c field00 isn't -1 (active),
 * forwards its @c field02 to @c func_8002CE84 (an SFX helper in btl_sfx).
 *
 * @param idx Index into the SFX slot table.
 */
void func_8009D864(s32 idx) {
    if (D_800C526C[idx].field00 != -1) {
        func_8002CE84(D_800C526C[idx].field02);
    }
}


/**
 * @brief Fade out the SFX referenced by slot @p idx and mark the slot inactive.
 *
 * If the slot is active (@c field00 != -1), writes -1 to deactivate it and
 * calls @c fadeOutSfxSlow with the slot's SFX index (@c field02).
 *
 * @param idx Index into the @c D_800C526C SFX slot table.
 */
void func_8009D8A8(s32 idx) {
    s32 field00 = D_800C526C[idx].field00;
    s32 field02 = D_800C526C[idx].field02;
    if (field00 != -1) {
        D_800C526C[idx].field00 = -1;
        fadeOutSfxSlow(field02);
    }
}

/**
 * @brief Fade out every active SFX slot in @c D_800C526C and mark them inactive.
 *
 * Per-slot loop version of @c func_8009D8A8 — walks all 13 entries of
 * @c D_800C526C, and for each slot whose @c field00 is not @c -1, writes
 * @c -1 into @c field00 and calls @c fadeOutSfxSlow with the slot's
 * @c field02 (SFX index). Used at world-map teardown to silence every
 * field-engine-owned SFX channel in one pass.
 */
void func_8009D8F0(void) {
    s32 i;
    for (i = 0; i < 13; i++) {
        s32 field00 = D_800C526C[i].field00;
        s32 field02 = D_800C526C[i].field02;
        if (field00 != -1) {
            D_800C526C[i].field00 = -1;
            fadeOutSfxSlow(field02);
        }
    }
}


/**
 * @brief Hard tear-down: fade every @c D_800C526C SFX slot fast, then push
 *        a 2-frame display flush.
 *
 * Walks all 13 entries of @c D_800C526C and unconditionally marks each
 * @c field00 inactive (@c -1) and calls @c fadeOutSfxFast on its
 * @c field02 — the harder/faster sibling of @c func_8009D8F0, which
 * only acts on already-active slots. Then pushes @c 2 frames via
 * @c renderAndUpdateDisplay and flushes the battle scene's
 * @c colorTag into the display list.
 *
 * Used at world/battle hand-off when an immediate audio cut and frame
 * push are required.
 */
void func_8009D954(void) {
    s32 i;
    for (i = 0; i < 13; i++) {
        s32 sfxIdx = D_800C526C[i].field02;
        D_800C526C[i].field00 = -1;
        fadeOutSfxFast(sfxIdx);
    }
    renderAndUpdateDisplay(2);
    renderBattleDisplayList(&D_800D244C->primList[BSC_COLORTAG_IDX]);
}


/**
 * @brief Return whether SFX slot @p idx is active AND its SFX has pending state.
 *
 * Checks @c D_800C526C[idx].field00 — if the slot is inactive (-1) returns 0.
 * Otherwise queries @c getSfxField28 with the slot's SFX index and returns
 * 1 iff the result is nonzero, else 0.
 *
 * @param idx Index into the @c D_800C526C SFX slot table.
 * @return 1 if slot is active and its SFX is busy, else 0.
 */
s32 func_8009D9C8(s32 idx) {
    s32 field00 = D_800C526C[idx].field00;
    s32 field02 = D_800C526C[idx].field02;
    s32 result = 0;
    if (field00 != -1) {
        result = getSfxField28(field02) != 0;
    }
    return result;
}

/**
 * @brief Find the first active SFX slot in the high range [9, 13).
 *
 * Scans @c D_800C526C[9..12] looking for a slot whose @c field00 is
 * not -1 (active). Returns the matching slot index, or -1 if none.
 *
 * @return First active slot index in [9, 12], or -1 if all inactive.
 */
s32 func_8009DA10(void) {
    s32 i;
    for (i = 9; i < 13; i++) {
        if (D_800C526C[i].field00 != -1) {
            return i;
        }
    }
    return -1;
}


/**
 * @brief Linear-search the D_800C9880 key table for an entry with matching @p key.
 *
 * Walks the 12-byte records between the 4-byte length header and the
 * buffer's end, comparing each entry's @c key against (@p key & 0xFF).
 * Returns the matching Entry12 pointer on hit, or NULL if exhausted.
 *
 * @param key Lookup key (low 8 bits used).
 * @return Matching Entry12*, or NULL if no match.
 */
Entry12 *func_8009DA54(u8 key) {
    KeyBuffer *buf = D_800C9880;
    Entry12 *entry = &buf->entries[0];
    u8 *end = (u8 *)buf + buf->length;
    key &= 0xFF;
    if ((u8 *)entry < end) {
        do {
            if (key == entry->key) return entry;
            entry++;
        } while ((u8 *)entry < end);
    }
    return NULL;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009DAA8);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009DB88);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009E5C8);

/**
 * @brief Smooth-step a 12-bit angle (range @c [0,0x1000) = full revolution)
 *        toward @c target.
 *
 * Normalizes both @c target and @c *current into @c [0,0x1000), wraps
 * the signed delta into roughly @c [-0x800,0x801), and compares the
 * absolute delta against @c limit. If within limit, snaps @c *current
 * to @c target and returns 0 ("arrived"). Otherwise advances @c *current
 * by @c limit (or @c 2*limit if the remaining distance still exceeds
 * @c threshold) toward @c target and returns 1 ("still moving").
 *
 * @param target    desired angle (12-bit-revolution units).
 * @param current   pointer to the angle to step (s16, in/out).
 * @param limit     max single-step magnitude.
 * @param threshold large-step / small-step decision threshold.
 * @return          0 if snapped, 1 if still moving toward target.
 *
 * @note Decompiled at 99.65% in @c permuter/func_8009F594/base.c — the
 *       permuter found two helpful tricks (stash sign-extended limit
 *       through @c t and the @c *current double-store / @c changed
 *       hoist) but plateaued at score 30. Remaining bytes differ in
 *       reg allocation: @c sra targets @c $a2 vs @c $v1, and the
 *       @c lim_s*2 step lands in @c $v0 vs @c $t1.
 *
 * @verbatim
 * s32 func_8009F594(s32 target, s16 *current, s32 limit, s32 threshold) {
 *     s32 lim_s, delta_s, abs_d, cur_now, delta;
 *     s32 t = (s16)target;
 *     s32 c = *current;
 *     s32 changed = 0;
 *
 *     while (t >= 0x1000) t -= 0x1000;
 *     while (t < 0)       t += 0x1000;
 *     while (c >= 0x1000) c -= 0x1000;
 *     while (c < 0)       c += 0x1000;
 *
 *     delta = t - c;
 *     if (delta < -0x7FF) {
 *         while (delta < 0) delta += 0x1000;
 *     } else if (delta >= 0x801) {
 *         while (delta >= 0) delta -= 0x1000;
 *     }
 *
 *     // permuter trick: route lim_s sign-ext through (now-dead) t
 *     t       = (s16)limit;
 *     lim_s   = t;
 *     delta_s = (s16)delta;
 *     abs_d   = delta_s;
 *     if (abs_d < 0) abs_d = -abs_d;
 *
 *     if (lim_s < abs_d) {
 *         if (delta_s > 0) {
 *             cur_now = *current;
 *             if ((s16)threshold < abs_d) {
 *                 *current = cur_now;          // double-store trick
 *                 *current = *current + lim_s * 2;
 *             } else {
 *                 *current = cur_now + lim_s;
 *             }
 *         } else {
 *             cur_now = *current;
 *             if ((s16)threshold < abs_d) {
 *                 changed = lim_s * 2;          // hoist *2 through changed
 *                 *current = cur_now - changed;
 *             } else {
 *                 *current = cur_now - lim_s;
 *             }
 *         }
 *         changed = 1;
 *     } else {
 *         *current = (s16)target;
 *     }
 *     return changed;
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009F594);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009F6EC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_8009FDA4);

/**
 * @brief Build a rotation matrix from angles, multiply by another, and set
 *        as the GTE light matrix.
 *
 * Three-step transform pipeline:
 *  1. @c RotMatrix(angles, &m) — fill @c m with a rotation derived from
 *     the @c SVECTOR pointed to by @p a0.
 *  2. @c MulMatrix(&m, a1) — multiply @c m by the matrix at @p a1.
 *  3. @c SetLightMatrix(&m) — load @c m as the GTE LLM (light matrix).
 *
 * @param a0 Input @c SVECTOR with rotation angles.
 * @param a1 Right-hand multiplier matrix.
 */
void func_8009FE80(SVECTOR *a0, MATRIX *a1) {
    MATRIX m;
    RotMatrix(a0, &m);
    MulMatrix(&m, a1);
    SetLightMatrix(&m);
}


/**
 * @brief Forward @p vec to func_800A017C, ignoring the first arg register.
 *
 * Thin wrapper — takes two register args but only passes the second on.
 *
 * @param unused First arg register, ignored.
 * @param vec Pointer forwarded as the first arg of func_800A017C.
 */
void func_8009FEBC(s32 unused, SVECTOR *vec) {
    func_800A017C(vec);
}

/** @brief Clear a 15-byte animation structure and set type byte at offset 0xA. */
void func_8009FEDC(u8 *a0, u8 a1) {
    a0[0xE] = 0;
    a0[0xD] = 0;
    a0[0xC] = 0;
    a0[0xB] = 0;
    a0[0xA] = 0;
    a0[0x9] = 0;
    a0[0x8] = 0;
    *(u16 *)(a0 + 0x0) = 0;
    *(u16 *)(a0 + 0x2) = 0;
    *(u16 *)(a0 + 0x4) = 0;
    a0[0xA] = a1;
}

/**
 * @brief Pick an audio/parameter constant from a map-ID + mode pair.
 *
 * Special-cased @p mapId values return @c 0x400. The set is the union of
 * the ranges @c [0x20, 0x29), @c [0x10, 0x17), @c [0x40, 0x43) and the
 * singletons @c 0x30, @c 0x32, @c 0x84. For non-special @p mapId values,
 * the result is selected by @p mode:
 *  - @p mode == 0: @c 0x400
 *  - @p mode == 1: @c 0x280
 *  - otherwise:    @c 1
 *
 * The exact semantics are uncertain; the constants look like sound /
 * fade-time parameters keyed off the world-map area.
 *
 * @param mapId World-map area identifier.
 * @param mode  Mode selector consulted only on non-special @p mapId.
 * @return Selected constant — @c 0x400, @c 0x280, or @c 1.
 */
s32 func_8009FF0C(s32 mapId, s32 mode) {
    if ((u32)(mapId - 0x20) < 9 ||
        mapId == 0x84 ||
        mapId == 0x30 ||
        (u32)(mapId - 0x10) < 7 ||
        (u32)(mapId - 0x40) < 3 ||
        mapId == 0x32 ||
        mode == 0) {
        return 0x400;
    }
    return mode == 1 ? 0x280 : 1;
}


/**
 * @brief Copy a per-map 3-halfword parameter triple into @p out, keyed by
 *        @p mapId. Sibling of @c func_8009FF0C — same map-ID classifier,
 *        different output payload.
 *
 * The default source is @c D_800C5354. Two single-map cases pick alternate
 * sources: @c mapId @c == @c 0x30 → @c D_800C5364, @c mapId @c == @c 0x32 →
 * @c D_800C535C. All other map IDs in the "common" set (the ranges
 * @c [0x20, 0x29), @c [0x10, 0x17), @c [0x40, 0x43) and @c mapId @c == @c 0x84)
 * also use the default. The triple is three halfwords (likely RGB or a small
 * fade/colour vector — exact meaning uncertain).
 *
 * @param mapId  World-map area identifier.
 * @param mode   Unused at this call site (kept for ABI; sibling reads it).
 * @param out    Destination halfword triple — receives @c src[0..2].
 */
void func_8009FF70(s32 mapId, s32 mode, u16 *out) {
    u16 *src;
    int new_var;

    if ((u32)(mapId - 0x20) < 9 || mapId == 0x84
        || (u32)(mapId - 0x10) < 7 || (u32)(mapId - 0x40) < 3) {
        src = D_800C5354;
        out[0] = src[0];
    } else if (mapId == 0x30) {
        src = D_800C5364;
        new_var = src[0];
        out[0] = new_var;
    } else if (mapId == 0x32) {
        src = D_800C535C;
        out[0] = src[0];
    } else {
        src = D_800C5354;
        out[0] = src[0];
    }
    new_var = src[1];
    out[1] = new_var;
    out[2] = src[2];
}


/**
 * @brief Pick a per-map screen-scroll offset, with special handling for
 *        @c mapId @c == @c 0x32 and a @p mode-keyed fallback.
 *
 * Returns @c D_800C534C for the "common" map set:
 *  - @c mapId in @c [0x20, @c 0x29),
 *  - @c mapId @c == @c 0x84,
 *  - @c mapId @c == @c 0x30,
 *  - @c mapId in @c [0x10, @c 0x17),
 *  - @c mapId in @c [0x40, @c 0x43).
 *
 * For @c mapId @c == @c 0x32 returns @c -scroll @c / @c 24 @c - @c 0x100
 * — a scroll-based offset computed from @p scroll.
 *
 * Otherwise selects on @p mode:
 *  - @c mode @c == @c 0: @c D_800C534C.
 *  - @c mode @c == @c 1: @c D_800C5344.
 *  - else:               @c 1.
 *
 * @param mapId  World-map area identifier.
 * @param mode   Mode selector consulted only on non-special @p mapId.
 * @param scroll Scroll value, only used for @p mapId @c == @c 0x32.
 * @return Selected scroll offset.
 */
s32 func_800A0000(s32 mapId, s32 mode, s32 scroll) {
    if ((u32)(mapId - 0x20) < 9 ||
        mapId == 0x84 ||
        mapId == 0x30 ||
        (u32)(mapId - 0x10) < 7 ||
        (u32)(mapId - 0x40) < 3) {
        return D_800C534C;
    }
    if (mapId == 0x32) {
        return -scroll / 24 - 0x100;
    }
    if (mode == 0) return D_800C534C;
    if (mode == 1) return D_800C5344;
    return 1;
}

/** Returns a value based on input comparison. */
s32 func_800A009C(s32 val) {
    if (val == 0x32) {
        return 0x1800;
    }
    return 0x1460;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object2", func_800A00B4);

/**
 * @brief Wrap @c vx into @c [0, 0x7FF] and @c vz into @c [-0x7FF, 0] by ±0x800.
 *
 * Used by the world map system to keep coordinates inside the active tile —
 * when a coordinate spills past the tile boundary, it is shifted by one
 * tile (0x800) so subsequent computations stay in-range.
 *
 *  - @c vx (offset 0): clamp to @c [0, 0x7FF]. If @c vx >= 0x800 subtract
 *    0x800; if @c vx < 0 add 0x800.
 *  - @c vz (offset 4): clamp to @c [-0x7FF, 0]. If @c vz > 0 subtract
 *    0x800; if @c vz <= -0x800 add 0x800.
 *
 * @c vy (offset 2) and @c pad (offset 6) are left alone.
 *
 * @param v Target vector.
 * @return @c 1 if either component was shifted, else @c 0.
 */
s32 func_800A017C(SVECTOR *v) {
    s32 changed = 0;
    if (v->vx >= 0x800) {
        v->vx -= 0x800;
        changed = 1;
    } else if (v->vx < 0) {
        v->vx += 0x800;
        changed = 1;
    }
    if (v->vz > 0) {
        v->vz -= 0x800;
        changed = 1;
    } else if (v->vz <= -0x800) {
        v->vz += 0x800;
        changed = 1;
    }
    return changed;
}
