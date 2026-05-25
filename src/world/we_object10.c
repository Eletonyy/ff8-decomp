#include "common.h"
#include "battle.h"
#include "gamestate.h"
#include "world.h"

extern u8 D_800786D8;
extern u8 D_800780D8[];
extern u8 D_800DCE78[];
extern SlotEntry D_800DBFB8[];
extern s32 D_800C5B50;

extern void func_800BAC84(u8 *p);
extern s32 func_800BA870(u8 *p);

extern s32 func_800B0010(void);

/**
 * @brief Search @c D_800DBFB8 for the slot whose @c marker matches the
 *        value returned by @c func_800B0010.
 *
 * Calls @c func_800B0010 to get a marker byte, then linearly scans the
 * first @c D_800C5B50 entries of @c D_800DBFB8 for the first slot whose
 * @c marker matches. Returns the slot index or @c -1 if not found.
 *
 * Twin of @c func_800BD754 — same scan loop but the search key is
 * sourced from @c func_800B0010 instead of an explicit argument.
 *
 * @return Matching slot index, or @c -1 on no match.
 */
s32 func_800BD6EC(void) {
    s32 target = func_800B0010();
    s32 i = 0;
    s32 count = D_800C5B50;
    if (count > 0) {
        s32 bound = count;
        SlotEntry *entry = &D_800DBFB8[0];
        do {
            if (target == entry->marker) return i;
            i++;
            entry++;
        } while (i < bound);
    }
    return -1;
}

/**
 * @brief Linear search D_800DBFB8 for the first SlotEntry whose marker matches @p target.
 *
 * Scans the first @c D_800C5B50 entries of @c D_800DBFB8, comparing each
 * entry's @c marker byte against @p target. Returns the slot index on
 * match or -1 if no match is found (including the empty case when
 * @c D_800C5B50 <= 0).
 *
 * @param target Marker byte value to search for.
 * @return Matching slot index, or -1 on no match.
 */
s32 func_800BD754(s32 target) {
    s32 count = D_800C5B50;
    if (count > 0) {
        s32 i = 0;
        s32 bound = count;
        SlotEntry *entry = &D_800DBFB8[0];
        do {
            if (target == entry->marker) return i;
            i++;
            entry++;
        } while (i < bound);
    }
    return -1;
}

extern CmdDesc *D_800C4D64;

/**
 * @brief Scale @p amount based on the current command's type byte.
 *
 * Peeks the type byte of the current command descriptor (@c D_800C4D64):
 *   - type == 0x1C: halve @p amount.
 *   - type == 0x1B or type == 8: double @p amount.
 *   - anything else: pass through unchanged.
 *
 * @param unused First arg register ignored by the implementation.
 * @param amount Quantity to scale.
 * @return Scaled @p amount.
 */
s32 func_800BD7A4(s32 unused, s32 amount) {
    u8 type = D_800C4D64->type;
    if (type == 0x1C) {
        amount >>= 1;
    } else if (type == 0x1B || type == 8) {
        amount <<= 1;
    }
    return amount;
}

/**
 * @brief Locate the active-party slot whose character matches battleParty[charIdx].
 *
 * Looks up the character id stored in @c g_gameState.battleParty[charIdx]
 * and scans @c g_gameState.mainData.party.party[0..2] for the first slot
 * whose id matches. Returns that slot index (0-2), or 0 if no match.
 *
 * @note A zero return is ambiguous — it's also returned on no-match.
 *
 * @param charIdx Battle-party index (0-3).
 * @return Active-party slot (0-2) whose member matches, else 0.
 */
s32 func_800BD7E4(s32 charIdx) {
    GameState *gs = &g_gameState;
    u8 target = gs->battleParty[charIdx];
    s32 i;
    for (i = 0; i < 3; i++) {
        if (gs->mainData.party.party[i] == target) {
            return i;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BD82C);

/**
 * @brief Copy 10 flag bytes from D_800780D8 to destination struct.
 *
 * Copies bytes at D_800780D8 offsets 0x108-0x10F to dst offsets 0x66-0x6D,
 * and 0x100-0x101 to dst offsets 0x6E-0x6F.
 *
 * @param dst Destination buffer pointer.
 */
void func_800BD918(u8 *dst) {
    u8 *src = D_800780D8;

    dst[0x66] = src[0x108];
    dst[0x67] = src[0x109];
    dst[0x68] = src[0x10A];
    dst[0x69] = src[0x10B];
    dst[0x6A] = src[0x10C];
    dst[0x6B] = src[0x10D];
    dst[0x6C] = src[0x10E];
    dst[0x6D] = src[0x10F];
    dst[0x6E] = src[0x100];
    dst[0x6F] = src[0x101];
}

/**
 * @brief Per-map constant lookup with implicit fall-through return.
 *
 * Dispatches on @p mapId:
 *  - @p mapId @c < @c 0xA or @c == @c 0x80:                   @c 0x10.
 *  - @p mapId @c == @c 0x32:                                  @c 0x78.
 *  - @p mapId in @c [0x20, @c 0x29) or @c == @c 0x84:         @c 0x20.
 *  - @p mapId in @c [0x40, @c 0x43):                          @c 0x78.
 *  - @p mapId @c == @c 0x30:                                  @c 0x20.
 *  - @p mapId @c == @c 0x31:                                  @c 0x78.
 *  - otherwise:                                               @c 0x31.
 *
 * The @c 0x31 "default" is intentionally implicit — there is no
 * trailing @c return statement, and the compiler leaves @c 0x31 in
 * @c v0 from the final @c (mapId @c == @c 0x31) comparison's
 * @c bne-delay-slot @c addiu. Adding @c return @c 0x31; explicitly
 * generates a separate @c addiu and breaks the match.
 *
 * @param mapId Unsigned to make the @c mapId @c - @c 0x20 @c < @c 9 and
 *              @c mapId @c - @c 0x40 @c < @c 3 range tests compile as
 *              @c sltiu (one instruction) rather than a signed two-step
 *              subtract+compare.
 * @return Per-map constant — @c 0x10, @c 0x20, @c 0x78, or @c 0x31.
 */
s32 func_800BD998(u32 mapId) {
    if (mapId < 0xA) return 0x10;
    if (mapId == 0x80) return 0x10;
    if (mapId == 0x32) return 0x78;
    if (mapId - 0x20 < 9) return 0x20;
    if (mapId == 0x84) return 0x20;
    if (mapId - 0x40 < 3) return 0x78;
    if (mapId == 0x30) return 0x20;
    if (mapId == 0x31) return 0x78;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BDA08);

/**
 * @brief Edge-trigger each of two threshold flags at @p flags[0..1] from
 *        the position @p pos, returning the index that crossed.
 *
 *  - Slot @c 0 (threshold @c 0x110): when @p pos is below the threshold,
 *    the flag is reset to @c 0; otherwise, if the flag was @c 0, the
 *    crossing is registered (@c result = @c 0, flag becomes @c 1).
 *  - Slot @c 1 (threshold @c 0x70): the inverse — when @p pos is below
 *    the threshold the flag is reset and no crossing is reported;
 *    otherwise the same edge-trigger as slot 0.
 *
 * If both slots crossed in the same call the return value is the
 * higher-index one (slot 1 overwrites slot 0).
 *
 * @param pos   Position to test against the two thresholds.
 * @param flags Two-byte array; @c flags[0] is slot 0, @c flags[1] is slot 1.
 * @return The index of the (last) slot that crossed this call, or @c -1.
 */
s32 func_800BDA78(s32 pos, u8 *flags) {
    s32 result = -1;
    s32 i;

    for (i = 0; i < 2; i++, flags++) {
        if (i == 0) {
            if (pos < 0x110) {
                *flags = 0;
                continue;
            }
        } else if (pos < 0x70) {
            goto clear;
        }
        if (*flags == 0) {
            result = i;
            *flags = 1;
        }
        continue;
    clear:
        *flags = 0;
    }
    return result;
}

/**
 * @brief Classify @p pos against two band centres (@c 0x90 and @c 0x130)
 *        within tolerance @p radius.
 *
 * Tests @p pos for proximity to each band centre:
 *  - @c |pos @c - @c 0x130| @c < @c radius: returns @c 0.
 *  - @c |pos @c - @c 0x90|  @c < @c radius: returns @c 1.
 *  - otherwise:                              returns @c -1.
 *
 * @param pos     Position to classify.
 * @param radius  Half-width tolerance for each band.
 * @return Band index @c 0 or @c 1, or @c -1 if @p pos is outside both bands.
 */
s32 func_800BDAE4(s32 pos, s32 radius) {
    s32 result = -1;
    if (abs(pos - 0x130) < radius) {
        result = 0;
    } else if (abs(pos - 0x90) < radius) {
        result = 1;
    }
    return result;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BDB5C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BDBE0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BDC94);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BDDC4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BDEF4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE040);

extern LookupTarget *D_800DDB00[];

/**
 * @brief Fetch the @c field52 halfword of the lookup target for slot @p idx.
 *
 * Uses the slot's @c lookupIdx to index into @c D_800DDB00, following the
 * resulting pointer to the target struct and reading its @c field52.
 * Returns -1 if @p idx is negative or the slot isn't mapped (lookupIdx < 0).
 *
 * @param idx Slot index into @c D_800DBFB8.
 * @return @c field52 of the target, or -1 when the slot has no lookup.
 */
s32 func_800BE158(s32 idx) {
    s32 result = -1;
    if (idx >= 0) {
        s8 lookup = D_800DBFB8[idx].lookupIdx;
        if (lookup >= 0) {
            result = D_800DDB00[lookup]->field52;
        }
    }
    return result;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE1A8);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE284);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE3D0);

/**
 * @brief Linearly interpolate between two 3D vectors.
 *
 * Computes out[i] = b[i] + (a[i] - b[i]) * t / 4096 for each of 3
 * components, using fixed-point arithmetic with rounding toward zero.
 *
 * @param a Source vector (3 x s32).
 * @param b Base vector (3 x s32).
 * @param t Interpolation factor (s16, fixed-point 12-bit).
 * @param out Output vector (3 x s32). If NULL, no operation.
 */
void func_800BE4D8(s32 *a, s32 *b, s32 t, s32 *out) {
    s32 diff;
    s32 val;
    s16 frac;

    if (out == 0) {
        return;
    }

    diff = a[0] - b[0];
    frac = t;
    val = diff * frac;
    if (val < 0) {
        val += 0xFFF;
    }
    out[0] = b[0] + (val >> 12);

    val = (a[1] - b[1]) * frac;
    if (val < 0) {
        val += 0xFFF;
    }
    out[1] = b[1] + (val >> 12);

    val = (a[2] - b[2]) * frac;
    if (val < 0) {
        val += 0xFFF;
    }
    out[2] = b[2] + (val >> 12);
}

extern s32 ratan2(s32 y, s32 x);

/**
 * @brief Convert a direction vector to (pitch, yaw, 0) Euler angles.
 *
 * Treats @p dir as a 3D direction (PSX y-down convention) and writes
 * the equivalent rotation into @p out:
 *  - @c out->vx (pitch) = @c ratan2(-vy, @c sqrt(vz*vz + vx*vx))
 *  - @c out->vy (yaw)   = @c ratan2(vx, vz)
 *  - @c out->vz         = @c 0
 *
 * Uses the PsyQ GTE helpers @c SquareRoot0 (integer sqrt) and
 * @c ratan2 (returns an angle in the standard PSX 0x1000 = full-circle
 * unit).
 *
 * @param dir Input direction vector.
 * @param out Destination Euler angles (vz is cleared).
 */
void func_800BE578(SVECTOR *dir, SVECTOR *out) {
    s32 horizDist = SquareRoot0(dir->vz * dir->vz + dir->vx * dir->vx);
    out->vx = ratan2(-dir->vy, horizDist);
    out->vy = ratan2(dir->vx, dir->vz);
    out->vz = 0;
}

/** @brief 32-byte record used by func_800BE5F8's table walker. */
typedef struct {
    s16 marker;         /**< 0x00: Scan terminator (-1 = end, -2 = continuation). */
    u8 pad02[0x1E];     /**< 0x02: Rest of the record — payload not yet mapped. */
} Record;

/**
 * @brief Walk a table of Records to the first -1/-2 marker and return context.
 *
 * Advances through a stride-0x20 record table while the current
 * @c marker is neither -1 nor -2. When a marker is found:
 *   - If @c marker == -2 (continuation), returns the *next* record's
 *     @c marker minus 1 — likely a link/count follow.
 *   - Otherwise (marker == -1, end-of-list), returns -2 as a sentinel.
 *
 * @param rec Pointer into the Record table.
 * @return Adjusted continuation value, or -2 if terminator was hit.
 */
s32 func_800BE5F8(Record *rec) {
    s16 val;
    while (rec->marker != -1 && rec->marker != -2) {
        rec++;
    }
    val = rec->marker;
    if (val == -2) {
        rec++;
        return rec->marker - 1;
    }
    return -2;
}

/**
 * @brief Switch the @c D_800DCE78 subsystem between save and load modes.
 *
 * Dispatches based on @p reset:
 *  - Non-zero @c reset: rebuild the @c D_800DCE78 state from scratch by
 *    calling @c func_800BAC84 (init pass) followed by @c func_800BEFEC
 *    (ramp/table fill). Returns @c 0.
 *  - Zero @c reset: query @c func_800BA870 against the current state and
 *    forward its return value.
 *
 * @param reset Non-zero rebuilds @c D_800DCE78; zero queries it instead.
 * @return @c 0 in the rebuild path; @c func_800BA870's return value
 *         otherwise.
 */
s32 func_800BE63C(s32 reset) {
    s32 result = 0;
    if (reset != 0) {
        func_800BAC84(D_800DCE78);
        func_800BEFEC(D_800DCE78);
    } else {
        result = func_800BA870(D_800DCE78);
    }
    return result;
}

/**
 * @brief 0x34-byte transform entry — only the snapshot pair at @c 0x20
 *        and @c 0x28 is mapped here. The trailing fields are consumed
 *        by the per-entry handler @c func_800AEB58.
 */
typedef struct {
    /* 0x00 */ u8 pad0[0x20];
    /* 0x20 */ s32 prevWord;    /**< Snapshot of @c currWord, refreshed each tick. */
    /* 0x24 */ s32 currWord;
    /* 0x28 */ u16 prevHalf;    /**< Snapshot of @c currHalf. */
    /* 0x2A */ u16 currHalf;
    /* 0x2C */ u8 pad2C[0x8];
} XformEntry; /* 0x34 */

/**
 * @brief Header for the transform-entry table walked by @c func_800BE69C.
 *
 * Only the @c count byte at @c 0x02 and the @c entries pointer at
 * @c 0x18 are mapped — the surrounding padding holds fields used by
 * other handlers in the same overlay.
 */
typedef struct {
    /* 0x00 */ u8 pad0[2];
    /* 0x02 */ s8 count;
    /* 0x03 */ u8 pad3[0x15];
    /* 0x18 */ XformEntry *entries;
} XformGroup;

extern void func_800AEB58(XformEntry *entry, XformGroup *group);

/**
 * @brief Snapshot each entry's @c curr fields into @c prev and invoke
 *        the per-entry handler.
 *
 * Walks @p group->entries for @c group->count iterations. Before
 * dispatching each entry to @c func_800AEB58:
 *  - @c entry->prevHalf is loaded with @c entry->currHalf.
 *  - @c entry->prevWord is loaded with @c entry->currWord.
 *
 * @c group->count is re-read each iteration since @c func_800AEB58 may
 * mutate it (the cached @c i is compared against the fresh count).
 *
 * @param group Transform-entry group container.
 */
void func_800BE69C(XformGroup *group) {
    XformEntry *entry = group->entries;
    s32 i = 0;
    if (group->count > 0) {
        do {
            i++;
            entry->prevHalf = entry->currHalf;
            entry->prevWord = entry->currWord;
            func_800AEB58(entry, group);
            entry++;
        } while (i < group->count);
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE720);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE7FC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BE8B0);

extern u8 *D_800C9E34;
extern ScriptOp *func_800AF004(u8 *base, s32 flag);

/**
 * @brief Walk the @c D_800C9E34 script and return the last
 *        @c 0xFF15 opcode's @c param (when @c D_800C4D64->flag has
 *        bit 3 set), else @c -1.
 *
 * Steps:
 *  1. If @c D_800C4D64->flag bit 3 isn't set, returns @c -1.
 *  2. Calls @c func_800AF004(@c D_800C9E34, 0) — if no script, returns
 *     @c -1.
 *  3. Walks the script, treating opcodes as:
 *     - @c 0xFF05 (END): stop walking.
 *     - @c 0xFF0E (JUMP): follow the @c param-relative jump.
 *     - @c 0xFF15: snapshot the @c param into @c result.
 *     - other: skip to next op.
 *
 * Returns the last @c 0xFF15 param seen, or @c -1 if none.
 */
s32 func_800BE958(void) {
    s32 result = -1;
    ScriptOp *p;

    if (D_800C4D64->flag & 0x8) {
        p = func_800AF004(D_800C9E34, 0);
        if (p != 0) {
            while (1) {
                u16 op = p->op;
                if (op == 0xFF05) break;
                if (op == 0xFF0E) {
                    p = (ScriptOp *)(D_800C9E34 + p->param);
                    continue;
                }
                if (op == 0xFF15) {
                    result = p->param;
                }
                p++;
            }
        }
    }
    return result;
}

extern void func_8009B358(s32 a0, s32 a1, s32 a2);
extern void func_8009D8A8(s32 a0);

/**
 * @brief Dispatch to one of two kind-2 handlers based on sign of @p arg.
 *
 * If @p arg is non-negative, calls @c func_8009B358(2, arg, 0).
 * Otherwise calls @c func_8009D8A8(2) with no argument beyond the kind.
 *
 * @note Purpose uncertain — appears to route a slot index / handle to a
 *       "present" handler when valid, and a "missing" handler when -1.
 *
 * @param arg Slot index (>=0) or sentinel (<0) indicating absence.
 */
void func_800BE9F8(s32 arg) {
    if (arg >= 0) {
        func_8009B358(2, arg, 0);
    } else {
        func_8009D8A8(2);
    }
}

extern s32 D_800C5D54;
extern void func_80048C50(s32 a0);
extern void fadeOutSfxFast(s32 idx);
extern void renderAndUpdateDisplay(s32 frameCount);
extern s32 renderBattleDisplayList(s32 *colorTag);

/**
 * @brief Tear down the battle scene: clear state flag, fade SFX, render two frames.
 *
 * Typical end-of-battle / scene-transition cleanup sequence:
 *  1. @c func_80048C50(0) — system/display reset.
 *  2. Clear the @c D_800C5D54 flag.
 *  3. @c fadeOutSfxFast(0) — stop channel 0 SFX.
 *  4. @c renderAndUpdateDisplay(2) — push 2 frames.
 *  5. Flush the battle scene's colorTag into the display list.
 */
void func_800BEA34(void) {
    func_80048C50(0);
    D_800C5D54 = 0;
    fadeOutSfxFast(0);
    renderAndUpdateDisplay(2);
    renderBattleDisplayList(&D_800D244C->colorTag);
}

extern s32 D_800C4D38;
extern s32 D_800C5C1C;

/**
 * @brief Pick a per-map screen/mode constant keyed off @c D_800C4D38.
 *
 * Dispatches based on the current map id @c D_800C4D38:
 *  - @c mapId @c < @c 0xA or @c == @c 0x80:
 *      - @c D_800C5C1C @c > @c 0: returns @c 0x31.
 *      - otherwise:               returns @c 0x20.
 *  - @c mapId @c == @c 0x31:                          returns @c 0x20.
 *  - @c mapId in @c [0x20, @c 0x29) or @c == @c 0x84: returns @c 0x32.
 *  - @c mapId @c == @c 0x32:                          returns @c 0.
 *  - otherwise:                                       returns @c -1.
 *
 * Constants look like screen-format / camera-mode selectors; the exact
 * semantics aren't mapped yet.
 *
 * @return Per-map constant — @c 0x31, @c 0x32, @c 0x20, @c 0, or @c -1.
 */
s32 func_800BEA7C(void) {
    s32 mapId = D_800C4D38;

    if ((u32)mapId < 0xA || mapId == 0x80) {
        if (D_800C5C1C > 0) return 0x31;
        return 0x20;
    }
    if (mapId == 0x31) return 0x20;
    if ((u32)(mapId - 0x20) < 9 || mapId == 0x84) return 0x32;
    if (mapId == 0x32) return 0;
    return -1;
}

extern s32 D_800C5C18;
extern s32 D_800C5C20;
extern s32 D_800C5C24;
extern s32 D_800C5C28;
extern s32 D_800C5C2C;
extern s32 D_800C5C30;

/**
 * @brief Check if @p val matches any of the 7 entries in the
 *        @c D_800C5C18..D_800C5C30 slot table.
 *
 * Returns @c 0 immediately on negative @p val. Otherwise compares
 * against each of the 7 named slot globals (@c D_800C5C18,
 * @c D_800C5C1C, @c D_800C5C20, @c D_800C5C24, @c D_800C5C28,
 * @c D_800C5C2C, @c D_800C5C30) and returns @c 1 on first match,
 * @c 0 if none.
 *
 * @param val Slot identifier to test.
 * @return @c 1 if @p val is in the slot table, @c 0 otherwise.
 */
s32 func_800BEAF4(s32 val) {
    s32 result;
    if (val < 0) return 0;
    result = 0;
    if (D_800C5C18 == val) result = 1;
    else if (D_800C5C1C == val) result = 1;
    else if (D_800C5C20 == val) result = 1;
    else if (D_800C5C24 == val) result = 1;
    else if (D_800C5C28 == val) result = 1;
    else if (D_800C5C2C == val) result = 1;
    else if (D_800C5C30 == val) result = 1;
    return result;
}

/**
 * @brief Compute a per-actor action code from the marker at
 *        @c D_800D23D8[idx+2] crossed with the actor's @c flag1E.
 *
 * Looks up two pieces of per-actor state and combines them:
 *  - marker = @c D_800D23D8[idx @c + @c 2] (byte from the world-state
 *    flag buffer).
 *  - flag = @c D_800DD6A8[idx].flag1E (s8 in the actor record).
 *
 * Returns:
 *  - marker @c == @c 0 and flag @c == @c 1: @c 0x35.
 *  - marker @c == @c 1 and flag @c == @c 1: @c 0x34.
 *  - marker @c == @c 1 and flag @c == @c -1: @c 0x37.
 *  - any other combination: @c -1.
 *
 * @param idx Actor index.
 * @return Action code, or @c -1 if no rule matches.
 */
s32 func_800BEB84(s32 idx) {
    s32 result = -1;
    u8 marker = D_800D23D8[idx + 2];

    if (marker == 0) {
        if (D_800DD6A8[idx].flag1E == 1) {
            result = 0x35;
        }
    } else if (marker == 1) {
        s8 v = D_800DD6A8[idx].flag1E;
        if (v == 1) {
            result = 0x34;
        } else if (v == -1) {
            result = 0x37;
        }
    }
    return result;
}

/**
 * @brief Map a command-kind code to a duration/timeout in frames.
 *
 * Lookup table flattened into a small branch chain:
 *   - kind 50:              528 frames
 *   - kind 32..40:          256 frames
 *   - kind 132:             256 frames
 *   - kind 48:              700 frames
 *   - anything else:        0
 *
 * @param kind Command type byte.
 * @return Frame count for the command, or 0 if unrecognised.
 */
s32 func_800BEC1C(s32 kind) {
    if (kind == 50) return 528;
    if ((kind >= 32 && kind <= 40) || kind == 132) return 256;
    if (kind == 48) return 700;
    return 0;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BEC60);

/**
 * @brief Search g_gameState array for an active entry with type 0xA2.
 */
s32 func_800BED90(s32 *outIndex, s32 *outCount) {
    s32 found = 0;
    s32 i = found;
    s32 target = 0xA2;
    u8 *ptr = (u8 *)&g_gameState;
    s32 type;
    s32 count;

    top:
        type = *(u8 *)(ptr + 0xB44);
        count = *(u8 *)(ptr + 0xB45);
        if (type != target) goto skip;
        if (count <= 0) goto skip;
        found = 1;
        if (outIndex != 0) {
            *outIndex = i;
        }
        if (outCount != 0) {
            *outCount = count;
            goto end;
        }
        goto end;
    skip:
        i += 1;
        if (i < 0xC6) {
            ptr += 2;
            goto top;
        }
    end:
    return found;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BEDF0);

void func_800BEECC(void) {
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BEED4);

/**
 * @brief Return 1 if any active slot's marker is 0x4F, else 0.
 *
 * Scans the first @c D_800C5B50 entries of @c D_800DBFB8 for a slot
 * whose @c marker byte equals 0x4F. Early-exits on first hit.
 *
 * @return 1 if a 0x4F-marker slot exists, 0 otherwise.
 */
s32 func_800BEF6C(void) {
    SlotEntry *entry = &D_800DBFB8[0];
    SlotEntry *end = entry + D_800C5B50;
    if (entry < end) {
        do {
            if (entry->marker == 0x4F) return 1;
            entry++;
        } while (entry < end);
    }
    return 0;
}

/** Checks two flag bits and returns status. */
s32 func_800BEFC4(void) {
    u8 val = D_800786D8;
    s32 result = 0;
    if (val & 1) {
        s32 bit = val & 2;
        result = (u32)bit < 1;
    }
    return result;
}

/** Initializes nested array with ramp values. */
void func_800BEFEC(u8 *base) {
    s32 outer = 7;
    s32 inner;
    s32 val;
    s32 *ptr;
    do {
        inner = 4;
        val = -0x1000;
        ptr = (s32 *)(base + 0x10);
        do {
            *(s32 *)((u8 *)ptr + 0x38) = val;
            val += 0x400;
            inner--;
            ptr = (s32 *)((u8 *)ptr - 4);
        } while (inner >= 0);
        outer--;
        base += 0x4C;
    } while (outer >= 0);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BF024);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BF20C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BF2E8);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object10", func_800BF3D8);

