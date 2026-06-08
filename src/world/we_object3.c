#include "common.h"
#include "psxsdk/libgpu.h"
#include "world.h"
#include "world/we_object1.h"
#include "world/we_object3.h"

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A01DC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A0388);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A05E8);

/**
 * @brief Program the GTE translation vector (TR, control regs 5/6/7) for world-map rendering.
 *
 * Each coordinate packs a coarse tile index (@c /128) and a fine sub-tile offset
 * (@c %128). The view offset is the delta from @p coord0 to @p coord1, taken
 * separately on the sub-tile axis (X) and the tile axis (Z), each wrapped into a
 * half-range so the world map scrolls seamlessly across the wrap seam:
 *  - @c off.vx = wrap(coord1%128 - coord0%128) into [-64,64] (via ±128), scaled *2048
 *  - @c off.vy = 0
 *  - @c off.vz = -wrap(coord1/128 - coord0/128) into [-48,48] (via ±96), scaled *2048
 * The offset is added to the camera base @c D_800DB0E8 and loaded via @c gte_ldtr.
 *
 * @note Handwritten GTE routine. The block-2 conditions re-evaluate the tile delta
 *       @c (coord1/128 - coord0/128) (rather than reusing @c d) to match the original
 *       codegen, which keeps both dividends live through the signed-division rounding.
 *
 * @param coord0 Reference world-map coordinate.
 * @param coord1 Current world-map coordinate; the view tracks the delta to @p coord0.
 */
void setWorldMapTransVector(s16 coord0, s16 coord1) {
    SVECTOR off;
    s32 d;

    d = (coord1 % 128) - (coord0 % 128);
    if (d < 65) {
        if (d < -64) {
            off.vx = (d + 128) * 2048;
        } else {
            off.vx = d * 2048;
        }
    } else {
        off.vx = (d - 128) * 2048;
    }
    off.vy = 0;

    d = (coord1 / 128) - (coord0 / 128);
    if (((coord1 / 128) - (coord0 / 128)) < 49) {
        if (((coord1 / 128) - (coord0 / 128)) < -48) {
            off.vz = -(d + 96) * 2048;
        } else {
            off.vz = -d * 2048;
        }
    } else {
        s32 t = d - 96;
        off.vz = -t * 2048;
    }

    gte_ldtr(D_800DB0E8.vx + off.vx, D_800DB0E8.vy + off.vy, D_800DB0E8.vz + off.vz);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A1678);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A1F10);

/**
 * @brief Initialize the world's two double-buffered graphics contexts.
 *
 * Sets up the draw/display environments for both @c BattleSceneCtx buffers
 * (the sentinel @c D_800CA040 and its successor at @c +0x4070) at VRAM x=0 /
 * x=384, sized by @c D_800C97EA x @c D_800C97E8. Then, for each buffer, clears
 * the 0x1000-entry ordering table, patches the display screen region to the
 * NTSC active area (@c y=8, @c h=224), and enables dithering. Finally installs
 * the first buffer as the active scene context (@c D_800D244C).
 */
void initWorldDoubleBuffer(void) {
    s32 i;

    SetDefDrawEnv(&(&D_800CA040)[1].drawEnv, 0, 0, D_800C97EA, D_800C97E8);
    SetDefDrawEnv(&(&D_800CA040)[0].drawEnv, 384, 0, D_800C97EA, D_800C97E8);
    SetDefDispEnv(&(&D_800CA040)[1].disp, 384, 0, D_800C97EA, D_800C97E8);
    SetDefDispEnv(&(&D_800CA040)[0].disp, 0, 0, D_800C97EA, D_800C97E8);

    for (i = 0; i < 2; i++) {
        ClearOTagR((u32 *)(&D_800CA040)[i].primList, 0x1000);
        (&D_800CA040)[i].disp.screen.y = 8;
        (&D_800CA040)[i].disp.screen.h = 224;
        (&D_800CA040)[i].drawEnv.dtd = 1;
    }

    D_800D244C = &D_800CA040;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A246C);

/**
 * @brief Program the GTE for world-map rendering: screen offset, back color, color matrix.
 *
 * Sets the GTE projection screen offset to half the worldmap screen dimensions
 * (@c D_800C97EA, @c D_800C97E8), copies the background (ambient) color
 * @c D_800C53F8 into the active back-color cache @c D_800DB0E0 and programs it
 * via @c SetBackColor, then copies the lighting color matrix @c D_800C5428 into
 * @c D_800DA8B0 and loads it via @c SetColorMatrix.
 */
void setupWorldRenderParams(void) {
    func_800488D4(3);
    SetGeomOffset((s16)D_800C97EA / 2, (s16)D_800C97E8 / 2);

    D_800DB0E0.r = D_800C53F8.r;
    D_800DB0E0.g = D_800C53F8.g;
    D_800DB0E0.b = D_800C53F8.b;
    SetBackColor(D_800DB0E0.r, D_800DB0E0.g, D_800DB0E0.b);

    D_800DA8B0 = D_800C5428;
    SetColorMatrix(&D_800DA8B0);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A26E8);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A2920);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A2D50);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A358C);

/**
 * @brief Resolve the command descriptor (glyph) for the projected world query @p v.
 *
 * Fetches the object glyph header for the query's cell key (@c v->buf.angle)
 * via @c func_800A5EC4, then searches two tables for the descriptor whose
 * region contains the projected point @c v->buf.proj (tested by
 * @c func_800BF024): first the @c D_800D24A8 cell cache, then the header's own
 * @c entries[]. On a hit the test's result word is copied to @p out (when
 * non-NULL) and the matching @c CmdDesc is returned; otherwise NULL.
 *
 * @param v   Projected world query (position + projection + cell key).
 * @param out Optional slot to receive the hit's result word.
 * @return The matching command descriptor, or NULL.
 */
CmdDesc *glyphAt(GlyphQuery *v, AngleSlot *out) {
    GlyphHeader *hdr;
    FeaEntry40C0 *e = &D_800D24A8[0];
    CmdDesc *g;
    AngleSlot res1, res2;

    hdr = (GlyphHeader *)func_800A5EC4(v->buf.angle);
    if (hdr == NULL) {
        return NULL;
    }

    for (; e < &D_800D24A8[12]; e++) {
        if (e->val != 0 && v->buf.angle == e->hval) {
            if (func_800BF024((CmdDesc *)e->val, &v->buf.proj, &res1, &hdr->entries[hdr->count])) {
                if (out != NULL) {
                    *out = res1;
                }
                return (CmdDesc *)e->val;
            }
        }
    }

    for (g = &hdr->entries[0]; g < &hdr->entries[hdr->count]; g++) {
        if (func_800BF024(g, &v->buf.proj, &res2, &hdr->entries[hdr->count])) {
            if (out != NULL) {
                *out = res2;
            }
            return g;
        }
    }

    return NULL;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A39BC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A3C9C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A3EE4);


/** Clears an array of 12 entries. */
void func_800A40C0(void) {
    FeaEntry40C0 *ptr = D_800D24A8;
    FeaEntry40C0 *end = ptr + 12;
    while (ptr < end) {
        ptr->val = 0;
        ptr->hval = 0;
        ptr++;
    }
}

/* Dual view of a world coordinate: full position (@c word) and its
 * low-16-bit angle component (@c half). */
typedef union {
    s32 word;
    u16 half;
} WorldCoord;

typedef struct {
    WorldCoord x, y, z;
} WorldVec;

/**
 * @brief Project a world position to a grid-cell index, optionally emitting its angle triple.
 *
 * When @p out is non-NULL, copies @p pos's three low-16-bit angle components into
 * @p out — the X/Y components masked to 11 bits, the Z component additionally
 * wrapped into the @c [-0x800, 0] half-revolution range.
 *
 * Always returns a grid-cell index derived from @p pos's X/Z world coordinates:
 * the X coordinate (biased by @c 0x60000) folds modulo @c 0x40000 into one of
 * @c 0x80 columns, and the Z coordinate (taken as @c 0x48000 - z) folds modulo
 * @c 0x30000 into one of @c 0x60 rows, combined as @c col + row * 0x80.
 *
 * @param pos World position; each coordinate is read both as a full word and as
 *            a low-u16 angle component.
 * @param out Optional destination for the angle triple (s16 x/y/z); may be NULL.
 * @return    Grid-cell index @c col + row * 0x80.
 */
s32 worldPosToCell(VECTOR *pos, SVECTOR *out) {
    WorldVec *p = (WorldVec *)pos;

    if (out != NULL) {
        out->vx = p->x.half & 0x7FF;
        out->vy = p->y.half;
        {
            s32 t = p->z.half & 0x7FF;
            out->vz = t;
            if (t != 0) out->vz = t - 0x800;
        }
        if (out->vz < -0x7FF) out->vz = (u16)out->vz + 0x800;
    }

    return ((p->x.word + 0x60000) % 0x40000) / 0x800
         + ((0x48000 - p->z.word) % 0x30000) / 0x800 * 0x80;
}

/**
 * @brief Place a fan of 5 world-map sprites around @p origin, oriented by @p angles.
 *
 * Builds a rotation matrix from @p angles and transforms @p v through it to get a
 * base offset; then asks @c func_800B5ADC for up to 4 spread vectors and
 * @c func_800BC5E0 for a per-scene angle bias, builds a second (Y-axis) rotation
 * from @c arg4 + that bias whose translation is the base offset, and emits one
 * @ref WorldSprite per source vector at @c origin + transformedOffset. Each sprite
 * is projected with @c worldPosToCell (filling @c cell / @c cellId) and tagged
 * @c flag = 2. The first sprite uses @p v's offset directly; the remaining four
 * use the spread vectors transformed by the second matrix.
 *
 * @param out    Output array of 5 @ref WorldSprite entries.
 * @param v      Base offset vector, transformed by the @p angles matrix.
 * @param angles Rotation angles for the primary matrix.
 * @param arg3   Scene context id passed to @c func_800B5ADC / @c func_800BC5E0.
 * @param arg4   Base Y-axis angle for the secondary (spread) rotation.
 * @param origin World-space origin the sprites are placed relative to.
 */
void placeWorldSpriteFan(WorldSprite *out, VECTOR *v, SVECTOR *angles, s32 arg3, s32 arg4,
                   VECTOR *origin) {
    MATRIX m;
    VECTOR xf;
    SVECTOR pts[4];
    SVECTOR yang;
    WorldSprite *e;
    s32 i;
    s32 ret;

    RotMatrix(angles, &m);
    gte_SetRotMatrix(&m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    gte_SetTransMatrix(&m);
    e = out;
    gte_ldv0(v);
    gte_mvmva(1, 0, 0, 0, 0);
    gte_stlvnl(&xf);

    func_800B5ADC(arg3, pts, 0, 0);
    ret = func_800BC5E0(arg3);

    yang.vx = 0;
    yang.vz = 0;
    yang.vy = arg4 + ret;
    RotMatrix(&yang, &m);
    gte_SetRotMatrix(&m);
    gte_SetTransVector(&xf);

    e->pos.vx = origin->vx + xf.vx;
    e->pos.vy = origin->vy + xf.vy;
    e->pos.vz = origin->vz + xf.vz;
    e->cellId = worldPosToCell(&e->pos, &e->cell);
    e->flag = 2;
    e++;

    for (i = 1; i < 5; i++) {
        gte_ldv0(&pts[i - 1]);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&xf);
        e->pos.vx = origin->vx + xf.vx;
        e->pos.vy = origin->vy + xf.vy;
        e->pos.vz = origin->vz + xf.vz;
        e->cellId = worldPosToCell(&e->pos, &e->cell);
        e->flag = 2;
        e++;
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A4420);

/**
 * @brief Tag-based flag lookup — larger sibling of @c func_800A4670.
 *
 * Returns 1 when @c D_800C4D20 is zero. Otherwise picks a bit from the
 * upper bytes of @p a based on @p b's low 16 bits:
 *   - 0x00..0x09, or 0x80: bit 7 of @c (a >> 16)
 *   - 0x20..0x28, or 0x84: bit 6 of @c (a >> 16)
 *   - 0x30: bit 5 of @c (a >> 16)
 *   - 0x31: bit 4 of @c (a >> 16)
 *   - otherwise: 1
 *
 * @note Purpose uncertain — likely command/slot flag dispatch.
 *
 * @param a Source word containing the flag bits in its upper bytes.
 * @param b Tag selector (low 16 bits examined).
 * @return The extracted bit (0 or the mask value), or 1 as a default.
 */
s32 func_800A45D8(u32 a, s32 b) {
    u16 op = (u16)b;
    if (D_800C4D20 == 0) return 1;
    if (op < 0xA || (s16)b == 0x80) return (a >> 16) & 0x80;
    if ((u16)(b - 0x20) < 9 || (s16)b == 0x84) return (a >> 16) & 0x40;
    if ((s16)b == 0x30) return (a >> 16) & 0x20;
    if ((s16)b == 0x31) return (a >> 16) & 0x10;
    return 1;
}

/**
 * @brief Extract one of several bit flags from @p a based on @p b's tag.
 *
 * Uses @p b as a selector to pick which bit of @p a's upper bytes to return.
 * When @c D_800C4D20 is zero the upper byte of @p a is forced to 0xFF before
 * extraction. The selector ranges are:
 *   - 0x20..0x28, or 0x84: bit 4 of @c (a >> 16)
 *   - 0x30: bit 2 of @c (a >> 16)
 *   - 0x31: bit 1 of @c (a >> 16)
 *   - 0x32: bit 7 of @c (a >> 8)
 *   - otherwise: 1
 *
 * @note Purpose uncertain — appears to be a tag-based flag lookup on
 *       command/slot data.
 *
 * @param a Source word containing the flag bits.
 * @param b Tag selector (low 16 bits examined).
 * @return The extracted bit (0 or the masked value), or 1 as a default.
 */
s32 func_800A4670(u32 a, s32 b) {
    if (D_800C4D20 == 0) {
        a |= 0xFFFFFF00;
    }
    if ((u16)(b - 0x20) < 9 || (s16)b == 0x84) return (a >> 16) & 4;
    if ((s16)b == 0x30) return (a >> 16) & 2;
    if ((s16)b == 0x31) return (a >> 16) & 1;
    if ((s16)b == 0x32) return (a >> 8) & 0x80;
    return 1;
}

/**
 * @brief Signed delta of (a mod 128) and (b mod 128), wrapped into [-64, 64].
 *
 * Computes the difference of @p a and @p b after each is reduced modulo 128
 * (signed truncation toward zero), then wraps the difference into a
 * ~[-64, 64] band by adding or subtracting 128. Looks like 256-step angle
 * arithmetic — 128 maps to a half-turn — used to find the shortest signed
 * distance between two angle-like values.
 *
 * @param a First value.
 * @param b Second value.
 * @return Wrapped signed delta in approx [-64, 64].
 */
s32 func_800A4700(s32 a, s32 b) {
    s32 d = (a % 128) - (b % 128);
    if (d < 65) {
        if (d < -64) {
            d += 128;
        }
    } else {
        d -= 128;
    }
    return d;
}

/**
 * @brief Signed delta between two /128-scaled inputs, wrapped into (-48, 48].
 *
 * Divides @p a and @p b by 128 (signed truncation toward zero), subtracts,
 * and wraps the result into a ~[-48, 48] band by adding or subtracting 96.
 * Looks like modular-angle arithmetic: @c 192 maps to a full circle and
 * @c 96 to a half turn, so this computes "shortest signed distance"
 * between two angle-like values.
 *
 * @param a First value (numerator scale 128).
 * @param b Second value (numerator scale 128).
 * @return Wrapped signed delta in approx [-48, 48].
 */
s32 func_800A475C(s32 a, s32 b) {
    s32 d = a / 128 - b / 128;
    if (d < 49) {
        if (d < -48) {
            d += 96;
        }
    } else {
        d -= 96;
    }
    return d;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A47A4);

s32 func_800A5E40(s32 x, s32 y);

/**
 * @brief Build a linked list of the world-map grid cells inside the camera
 *        viewport, writing it into @p out.
 *
 * Projects the two opposite corners of a fixed 0x2FFF-radius box around the
 * camera position @c D_800D23C0 to packed cell coordinates via
 * @c func_800A5E40, then walks every cell in the spanned rectangle. Columns
 * (the low @c %32 axis) wrap modulo 32 and rows (the @c /32 axis) wrap modulo
 * 24, so the enumeration covers a torus-shaped region across the world-map
 * grid edges. Each visited cell is written to a fresh @ref WorldObject node
 * (@c id = @c row*32+col), chained through @c next at a 0xC-byte stride, and
 * the final node's @c next is cleared to terminate the list.
 *
 * @param arg0 Unused (the incoming register is overwritten immediately).
 * @param out  Output buffer; receives the 0xC-byte node list, NULL-terminated.
 */
void buildViewportCellList(s32 arg0, WorldObject *out) {
    s32 r1, r2;
    s32 row, col;
    s32 colSpan, rowSpan;
    s32 i, j;

    r1 = func_800A5E40(D_800D23C0.x - 0x2FFF, D_800D23C0.y - 0x2FFF);
    r2 = func_800A5E40(D_800D23C0.x + 0x2FFF, D_800D23C0.y + 0x2FFF);

    {
        s32 f1 = r1 % 32;
        s32 f2 = r2 % 32;
        if (r2 % 32 < r1 % 32) {
            s32 lo = f2 + 32;
            colSpan = lo - f1;
        } else {
            colSpan = f2 - f1;
        }
    }
    {
        s32 c1 = r1 / 32;
        s32 c2 = r2 / 32;
        if (r2 / 32 < r1 / 32) {
            s32 lo = c2 + 24;
            rowSpan = lo - c1;
        } else {
            rowSpan = c2 - c1;
        }
    }

    row = r1 / 32;
    for (i = 0; i <= rowSpan; i++) {
        col = r1 % 32;
        for (j = 0; j <= colSpan; j++) {
            if (col >= 32) col -= 32;
            if (row >= 24) row -= 24;
            if (col < 0) col += 32;
            if (row < 0) row += 24;
            out->id = row * 32 + col;
            out->next = out + 1;
            out++;
            col++;
        }
        row++;
    }
    out[-1].next = 0;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A50A0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A568C);

void func_800A62E0(s16 val, u16 *coarse, u16 *fine);

/**
 * @brief Initialise the world-engine subsystem's object pools.
 *
 * Builds the 16-entry free list at @c D_800D3320 (linked via @c next),
 * publishes its head at @c D_800D3318, zeros the various object-list
 * roots (@c D_800D34E0, @c D_800D34E4, @c D_800D2284, @c D_800CA030)
 * and the 16-entry @c D_800D34A0 u32 slot table, then resets each of
 * the 16 entries in both @c D_800D33E0 and @c D_800C9EF0 to
 * @c { .next = NULL, .id = -1 }. Finally sets @c D_800C4D60 to its
 * sentinel value 0xFFFF and zeros @c D_800D34F0.
 */
void func_800A581C(void) {
    s32 i;

    for (i = 14; i >= 0; i--) {
        D_800D3320[i].next = &D_800D3320[i + 1];
    }
    D_800D3320[15].next = 0;
    D_800D3318 = &D_800D3320[0];
    D_800D34E0 = 0;
    D_800D34E4 = 0;
    D_800D2284 = 0;
    D_800CA030 = 0;

    for (i = 15; i >= 0; i--) {
        D_800D34A0[i] = 0;
    }

    for (i = 0; i < 16; i++) {
        D_800C9EF0[i].next = 0;
        D_800C9EF0[i].id = -1;
        D_800D33E0[i].next = 0;
        D_800D33E0[i].id = -1;
    }

    D_800C4D60 = 0xFFFF;
    D_800D34F0 = 0;
}

/**
 * @brief Register world objects from the master list that aren't yet tracked.
 *
 * Walks the master object list @c D_800C9EF0 and, for each entry whose @c id is
 * not already present in any of the three tracking lists (@c D_800CA030, the
 * active list @c D_800D34E0, or @c D_800D34E4), pops a node from the free pool
 * @c D_800D3318, stamps it with that @c id, and pushes it onto the active list
 * @c D_800D34E0. If the free pool is exhausted, @c func_8009C528(0x6E) raises a
 * system error.
 *
 * @note Each search re-reads @c node->id (re-loaded per list) and uses a
 *       found-pointer walk; objects already tracked anywhere are skipped.
 */
void registerNewWorldObjects(void) {
    WorldObject *node;
    WorldObject *p;
    WorldObject *found;
    WorldObject *freeNode;
    s16 id;

    for (node = D_800C9EF0; node != NULL; node = node->next) {
        id = node->id;
        for (p = D_800CA030; p != NULL; p = p->next) {
            if (id == p->id) { found = p; goto tracked0; }
        }
        found = NULL;
    tracked0:
        if (found != NULL) continue;

        id = node->id;
        for (p = D_800D34E0; p != NULL; p = p->next) {
            if (id == p->id) { found = p; goto tracked1; }
        }
        found = NULL;
    tracked1:
        if (found != NULL) continue;

        id = node->id;
        for (p = D_800D34E4; p != NULL; p = p->next) {
            if (id == p->id) { found = p; goto tracked2; }
        }
        found = NULL;
    tracked2:
        if (found != NULL) continue;

        freeNode = D_800D3318;
        if (freeNode == NULL) {
            func_8009C528(0x6E);
        }
        D_800D3318 = freeNode->next;
        freeNode->id = node->id;
        freeNode->next = D_800D34E0;
        D_800D34E0 = freeNode;
    }
}

/**
 * @brief Drain the pending @c WorldObject list at @c D_800D34E4.
 *
 * For each node on the @c D_800D34E4 list: look it up in the @c D_800C9EF0[16]
 * id table; if its @c id is present, append a copy (id + sectionIdx) to the
 * @c D_800CA030 active list — which grows in place through the @c D_800D33E0[16]
 * pool via @c tail->next = tail + 1 — otherwise clear @c D_800D34A0 for the
 * node's section. Either way the node is then pushed onto the @c D_800D3318
 * free list. Iterates until the pending list is empty.
 *
 * @note The match needs the search to leave its result in a @c found flag that
 *       is set only at the two loop exits (1 on a hit, 0 on exhaustion), which
 *       requires the @c goto over the @c found=0 — a plain @c break cannot skip
 *       it.
 */
void drainPendingObjects(void) {
    WorldObject *node;
    WorldObject *entry;

    node = D_800D34E4;
    if (node == 0) {
        return;
    }

    do {
        WorldObject *search;
        s32 found;

        D_800D34E4 = node->next;
        search = &D_800C9EF0[0];
        while (search != 0) {
            if (node->id == search->id) {
                found = 1;
                goto matched;
            }
            search = search->next;
        }
        found = 0;
    matched:
        if (found) {
            if (D_800CA030 != 0) {
                entry = D_800CA030;
                while (entry->next != 0) {
                    entry = entry->next;
                }
                entry->next = entry + 1;
                entry += 1;
            } else {
                entry = &D_800D33E0[0];
                D_800CA030 = &D_800D33E0[0];
            }
            entry->id = node->id;
            entry->sectionIdx = node->sectionIdx;
            entry->next = 0;
        } else {
            D_800D34A0[node->sectionIdx] = 0;
        }

        node->next = D_800D3318;
        D_800D3318 = node;

        node = D_800D34E4;
    } while (node != 0);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A5B48);


/**
 * @brief Draw a translucent dark-gray fullscreen quad and wait for the GPU.
 *
 * Sets up @c D_800D3300 as a 24-byte POLY_F4 covering the viewport
 * (0,0)-(width,height) with RGB(8,8,8) and code 0x2A (semi-transparent
 * 4-vertex polygon). Submits via @c DrawPrim, then @c DrawSync(0) blocks
 * until the GPU finishes. Likely a screen-darken pass for fades.
 */
void func_800A5D10(void) {
    POLY_F4 *prim = &D_800D3300;
    s16 w = D_800C97EA;
    s16 h = D_800C97E8;

    setlen(prim, 5);
    setcode(prim, 0x2A);

    prim->x0 = 0;
    prim->y0 = 0;
    prim->y1 = 0;
    prim->x2 = 0;

    prim->r0 = 8;
    prim->g0 = 8;
    prim->b0 = 8;

    prim->x1 = w;
    prim->y2 = h;
    prim->x3 = w;
    prim->y3 = h;

    DrawPrim(prim);
    DrawSync(0);
}

/**
 * @brief Linear search a world-object list for a node with matching @p id.
 *
 * @param id Signed 16-bit id to match.
 * @param head Head of the linked list (may be NULL).
 * @return Matching WorldObject, or NULL if none found.
 */
WorldObject *func_800A5D8C(s16 id, WorldObject *head) {
    while (head != NULL) {
        if (id == head->id) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief Compute a 2D tile index from world coordinates (128-wide grid).
 *
 * Like @c func_800A5E40 but finer-grained: divides by 0x800 (2048) instead
 * of 0x2000 and the Y multiplier is 128 (bit 7) instead of 32. Maps
 * (x, y) world coords to a linear tile index on a 128-column grid.
 *
 * @note The ugly pointer-alias expression @c ((*(new_var2 = &rnd)) >> 18)
 *       and the split @c rnd assignment are load-bearing — they coerce
 *       gcc's register allocator to keep the rounded quotient in @c a1
 *       throughout the signed-mod calculation, matching the target.
 *       Cleaner forms (inline cast, single-line rnd expression) drop the
 *       match rate by 1–5 instructions.
 *
 * @param x World X coordinate.
 * @param y World Y coordinate.
 * @return Linear tile index on a 128-column grid.
 */
s32 func_800A5DC8(s32 x, s32 y) {
    s32 xm;
    s32 *new_var2;
    s32 ym;
    s32 new_var;
    x += 0x60000;
    {
        s32 rnd = x;
        if (x < 0) {
            rnd = 0x3FFFF + x;
        }
        new_var = x;
        rnd = ((*(new_var2 = &rnd)) >> 18) << 18;
        xm = new_var - rnd;
    }
    ym = ((y + 0x48000) % 0x30000) >> 11;
    return (xm >> 11) + (ym << 7);
}

/**
 * @brief Compute a tile index from 2D world coordinates.
 *
 * Offsets @p x and @p y to be positive, wraps them into 0x40000 × 0x30000
 * ranges, divides by the tile size (0x2000), and linearises:
 *   - X tile = ((x + 0x60000) mod 0x40000) / 0x2000   — range [0, 32)
 *   - Y tile = ((y + 0x48000) mod 0x30000) / 0x2000   — range [0, 24)
 *   - index = X + Y × 32
 *
 * @note The declaration of @c ym is load-bearing despite being "unused" —
 *       removing it changes gcc's register allocation and breaks the match.
 *       The inline recomputation of the @c y expression in the return is
 *       similarly intentional; gcc folds the duplicate into a single
 *       computation at runtime.
 *
 * @param x World X coordinate.
 * @param y World Y coordinate.
 * @return Linear tile index.
 */
s32 func_800A5E40(s32 x, s32 y) {
    s32 xo = x + 0x60000;
    s32 xm = xo % 0x40000;
    s32 ym = (y + 0x48000) % 0x30000;
    s32 xtile = xm / 0x2000;
    return xtile + ((((y + 0x48000) % 0x30000) / 0x2000) * 32);
}

/**
 * @brief Resolve a world id to a pointer into its data section.
 *
 * @c func_800A62E0 splits @p id into a coarse key and a fine index. The coarse
 * key is matched against the @c id of each WorldObject in the @c D_800CA030
 * list; the matching node's @c sectionIdx selects a WorldSection in the
 * @c D_800C4D5C region table, and the section's @c offsets[fine] (low 2 flag
 * bits stripped by the @c >>2 word index) gives the target's location within
 * that section.
 *
 * @param id World id to resolve.
 * @return Pointer into the matched section, or NULL if no node matches.
 */
u32 *func_800A5EC4(s16 id) {
    s16 objId;
    u16 coarse;
    u16 fine;
    WorldObject *node;
    WorldObject *found;
    s32 idx;
    u32 *result;

    result = NULL;
    func_800A62E0(id, &coarse, &fine);
    objId = coarse;
    for (node = D_800CA030; node != NULL; node = node->next) {
        if (objId == node->id) {
            found = node;
            goto done;
        }
    }
    found = NULL;
done:
    if (found != NULL) {
        WorldSection *section = &D_800C4D5C[found->sectionIdx];
        idx = (s16)fine;
        /* idx[offsets] == offsets[idx]; index-first form matches the addu operand order */
        result = (u32 *)section + (idx[section->offsets] >> 2);
    }
    return result;
}

/**
 * @brief Set up a draw environment for screen @p screenIdx and submit it.
 *
 * Initialises a DRAWENV for a viewport at (screenIdx * width, 0) of size
 * (width × height) where width = @c D_800C97EA and height = @c D_800C97E8.
 * Patches @c tpage = 0x40 (default GPU texture page) and @c dfe = 1
 * (enable drawing to display area), then pushes it via @c PutDrawEnv.
 *
 * @param screenIdx Horizontal viewport index (multiplied by width).
 */
void func_800A5F78(s32 screenIdx) {
    DRAWENV env;
    SetDefDrawEnv(&env, screenIdx * D_800C97EA, 0, D_800C97EA, D_800C97E8);
    env.dfe = 1;
    env.tpage = 0x40;
    PutDrawEnv(&env);
}

/**
 * @brief Set up a display environment for screen @p screenIdx and submit it.
 *
 * Mirror of @c func_800A5F78 but for @c DISPENV (the VRAM-readout side
 * of the GPU pair). Patches the visible screen region to @c y=8,
 * @c h=224 (NTSC full-frame minus 16 overscan lines) before pushing.
 *
 * @param screenIdx Horizontal viewport index (multiplied by width).
 */
void func_800A5FD4(s32 screenIdx) {
    DISPENV env;
    SetDefDispEnv(&env, screenIdx * D_800C97EA, 0, D_800C97EA, D_800C97E8);
    env.screen.y = 8;
    env.screen.h = 0xE0;
    PutDispEnv(&env);
}

/**
 * @brief Walk a @c WorldObject list, moving every node whose @c id is
 *        not present in the @c D_800C9EF0 lookup table onto the front
 *        of the @c D_800D3318 free list.
 *
 * For each @c WorldObject in the list rooted at @c *pp, scan the
 * 16-entry @c D_800C9EF0 table (linked via @c next) for an entry with
 * a matching @c id. When no match is found the node is removed from
 * the source list (the predecessor's @c next pointer is patched via
 * the @c Node** indirection) and prepended to @c D_800D3318. When a
 * match is found the node is kept and the walker advances by
 * re-pointing @p pp at the node's @c next.
 *
 * The @c Node @c ** indirection (rather than a separate @c prev
 * pointer) is the key to byte-matching this routine: it lets gcc keep
 * the same register as both the head-pointer and the predecessor's
 * @c next pointer across iterations.
 *
 * @param pp Address of the head pointer for the list to filter.
 */
void func_800A6030(WorldObject **pp) {
    WorldObject *curr;
    WorldObject *search;
    s32 found;

    curr = *pp;
    if (curr == NULL) {
        return;
    }

    do {
        search = &D_800C9EF0[0];
        found = 0;

        while (search != NULL) {
            if (curr->id == search->id) {
                found = 1;
                break;
            }
            search = search->next;
            found = 0;
        }

        if (!found) {
            *pp = curr->next;
            curr->next = D_800D3318;
            D_800D3318 = curr;
        } else {
            pp = &curr->next;
        }
        curr = *pp;
    } while (curr != NULL);
}

/**
 * @brief Walk a WorldObject list and return the first node whose section
 *        key (first byte of @c D_800C4D5C[sectionIdx]) matches @p key.
 *
 * @param key Key byte to match (low 8 bits of @p key are used).
 * @param head Head of the WorldObject list (may be NULL).
 * @return Matching WorldObject, or NULL if none found.
 */
WorldObject *func_800A60B4(s32 key, WorldObject *head) {
    if (head) {
        WorldSection *base = D_800C4D5C;
        key &= 0xFF;
        do {
            WorldSection *section = &base[head->sectionIdx];
            if (key == section->key) return head;
            head = head->next;
        } while (head != 0);
    }
    return NULL;
}

/**
 * @brief Walk a WorldObject list and return the first section-byte that
 *        isn't 0xFF, @c D_800C5398[0], or @c D_800C5398[2]; else 0xFF.
 *
 * For each node in @p head, reads the first byte of its world-section
 * (@c D_800C4D5C[sectionIdx]). A byte is "interesting" when it differs from
 * all three skip values, at which point it's returned. If no interesting
 * byte is found (or @p head is NULL), returns 0xFF.
 *
 * @note @c D_800C5398[0] is cached once outside the loop; @c D_800C5398[2]
 *       is re-read each iteration (gcc emits it inside the loop because
 *       it's the third byte read — CSE doesn't hoist it).
 *
 * @param head Head of the WorldObject list (may be NULL).
 * @return An interesting section byte, or 0xFF if none found.
 */
s32 func_800A610C(WorldObject *head) {
    if (head) {
        u8 firstSect = D_800C5398[0];
        do {
            WorldSection *section = &D_800C4D5C[head->sectionIdx];
            u8 key = section->key;
            if (key != 0xFF && firstSect != key && D_800C5398[2] != key) {
                return key;
            }
            head = head->next;
        } while (head != 0);
    }
    return 0xFF;
}


/**
 * @brief Upload two subimages from a packed-TIM bundle into VRAM.
 *
 * @p tim points to a back-to-back pair of TIM image blocks. The first image
 * (256×16, 16bpp) is uploaded to the VRAM rect at @c D_800C5388[tableIdx];
 * the second image (128×256, 16bpp) is uploaded to @c D_800C5378[tableIdx].
 *
 * Uses @c D_800D32F0 as a scratch @c RECT for each upload.
 */
void func_800A6188(Tim *tim, u8 tableIdx) {
    u32 *img1 = (u32 *)tim->clut.data;
    /* The tim/img1 ++/-- pairs pin img1 into a callee-saved register so it
       survives the LoadImage/DrawSync calls and is reused for the second
       image; without them the register allocation doesn't match. */
    tim++;
    img1++;
    img1--;
    tim--;

    setRECT(&D_800D32F0, D_800C5388[tableIdx].x, D_800C5388[tableIdx].y, 256, 16);
    LoadImage(&D_800D32F0, img1);
    DrawSync(0);

    setRECT(&D_800D32F0, D_800C5378[tableIdx].x, D_800C5378[tableIdx].y, 128, 256);
    // FIXME: hard-coded offset to the second image block (0x803 u32 words =
    // 0x200C bytes past the first image). This should be derived from the TIM
    // itself (the first block's length) rather than baked in — as written it
    // silently assumes a fixed first-image size. Every structural form tried
    // (tim->image2, or walking clut.len) breaks the register-allocation match
    // with the current toolchain. Figure out how to reach the second block
    // structurally without losing the match.
    LoadImage(&D_800D32F0, img1 + 0x803);
    DrawSync(0);
}

/**
 * @brief Walk a WorldObject list and return 1 if any node's id hits D_800C9EF0's list.
 *
 * For each node in the chain starting at @p head, calls @c func_800A629C
 * (which checks whether the node's id is present in the @c D_800C9EF0
 * static list) and returns 1 as soon as one reports a match. Returns 0 if
 * the list is empty or no node matches.
 *
 * @param head Head of the WorldObject chain to test (may be NULL).
 * @return 1 if any node's id is in the D_800C9EF0 list, else 0.
 */
s32 func_800A6254(WorldObject *head) {
    while (head != NULL) {
        if (func_800A629C(head) != 0) {
            return 1;
        }
        head = head->next;
    }
    return 0;
}

/**
 * @brief Check whether any node in the D_800C9EF0 list shares @p target's id.
 *
 * Walks the static list starting at @c D_800C9EF0 and returns 1 if any
 * node's @c id matches @p target->id, else 0.
 *
 * @param target Query node — only its @c id field is read.
 * @return 1 if a matching id was found in the list, 0 otherwise.
 */
s32 func_800A629C(WorldObject *target) {
    WorldObject *node = &D_800C9EF0;
    if (node != NULL) {
        s16 key = target->id;
        do {
            if (key == node->id) return 1;
            node = node->next;
        } while (node != NULL);
    }
    return 0;
}

/**
 * @brief Split a signed 16-bit value into coarse/fine (q,r × 4,32) components.
 *
 * Divides @p val by 128 (signed) into quotient @c q and remainder @c r, then
 * splits each further by 4:
 *   - @c *coarse = (q/4)*32 + r/4    — 7-bit high portions joined
 *   - @c *fine   = (q%4)*4  + r%4    — 4-bit low portions joined
 *
 * Likely a packed grid-cell / tile-offset decomposition where the coarse
 * output identifies a 32-step bucket and the fine output carries the
 * residual position within it.
 *
 * @param val    s16 input to decompose.
 * @param coarse Output — coarse bucket (high bits of q and r).
 * @param fine   Output — fine residual (low bits of q and r).
 */
void func_800A62E0(s16 val, u16 *coarse, u16 *fine) {
    s32 r = val % 128;
    s32 q = val / 128;
    *coarse = (q / 4) * 32 + r / 4;
    *fine = (q % 4) * 4 + r % 4;
}

/**
 * @brief Free the WorldObject list at @c D_800D34E4 back to the free pool.
 *
 * Unconditionally sets @c D_800C4D60 = 0xFFFF as a marker/sentinel. If the
 * list at @c D_800D34E4 is non-empty, walks every node and clears its slot
 * in the @c D_800D34A0 table (keyed by @c sectionIdx), then splices the
 * whole list onto the front of the @c D_800D3318 free list and clears
 * @c D_800D34E4.
 *
 * @note Purpose uncertain — looks like a subsystem-reset that releases all
 *       active world objects into a reusable pool.
 */
void func_800A6358(void) {
    WorldObject *head;
    D_800C4D60 = 0xFFFF;
    head = D_800D34E4;
    if (head) {
        WorldObject *node = head;
        do {
            D_800D34A0[node->sectionIdx] = 0;
            node = node->next;
        } while (node != 0);

        node = D_800D34E4;
        while (node->next != 0) {
            node = node->next;
        }

        node->next = D_800D3318;
        D_800D3318 = D_800D34E4;
        D_800D34E4 = 0;
    }
}


/**
 * @brief Gated table swap — copies one of two source halfword tables into
 *        @c D_800C53B8 and @c D_800C53EC depending on the current map id.
 *
 * No-op when @c D_800C4D2C is set (system busy). Otherwise, when the map
 * id @c D_800C4D38 is @c 0x32, copies @c D_800C53C4 (5 halfwords) ->
 * @c D_800C53B8 and @c D_800C53DC (4 halfwords) -> @c D_800C53EC.
 * For any other map id, copies from @c D_800C53D0 and @c D_800C53E4
 * respectively.
 */
void func_800A63F0(void) {
    s32 i;
    if (D_800C4D2C != 0) return;
    if (D_800C4D38 == 0x32) {
        for (i = 0; i < 5; i++) D_800C53B8[i] = D_800C53C4[i];
        for (i = 0; i < 4; i++) D_800C53EC[i] = D_800C53DC[i];
        return;
    }
    for (i = 0; i < 5; i++) D_800C53B8[i] = D_800C53D0[i];
    for (i = 0; i < 4; i++) D_800C53EC[i] = D_800C53E4[i];
}
