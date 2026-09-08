/**
 * @file effect_025_scroll.c
 * @brief Cura: scroll the textured quad list's V coordinates.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/emit_mode.h"
#include "effect/effect_025_scroll.h"

/**
 * @name Scrolled UV words
 *
 * A UV pair is one halfword, U in the low byte and V in the high one, so a
 * step of one V unit is 0x100; the third and fourth pairs of a quad share a
 * word. A scroll masks V out, steps it and puts the rest back, and a step that
 * carries out of the byte has wrapped.
 * @{
 */
#define EFFECT_UV_V 0xFF00           /**< V of a pair, in place. */
#define EFFECT_UV_KEEP 0xFFFF00FF    /**< What a scroll of one pair leaves alone. */
#define EFFECT_UV_KEEP23 0x00FF00FF  /**< Both U bytes of the shared third and fourth pair. */
#define EFFECT_UV_WRAP 0x8000        /**< One V byte's worth of wrap. */
#define EFFECT_UV_SCROLL_STEP (4 << 8) /**< The fixed scroll: four whole V units. */
/** @} */

/**
 * @brief Words of a mesh stream's block header ahead of the quad list's count.
 *
 * The scroll walkers step over the stream's own header, then this, to reach
 * the count of gouraud textured quads and the quads themselves.
 */
#define EFFECT_QUAD_LIST_HEADER 7

static void func_801A4BFC(s32 *header);

/**
 * @brief Scroll the textured quad list's V coordinates by one fixed step.
 *
 * The list @p header names holds the gouraud textured quads; each carries four
 * UV pairs, the last two packed into one word. Only the V byte of each moves,
 * and all four wrap together so a quad never tears.
 *
 * @param header Mesh stream to walk.
 */
static void func_801A4BFC(s32 *header) {
    EffectEmitGouraudTexQuad *e;
    u32 keep[3];
    u32 ch[4];
    u32 uv23;
    s32 count;
    s32 i;

    header = (s32 *)((u8 *)header + header[0] / 4 * 4);
    header += EFFECT_QUAD_LIST_HEADER;
    count = *header;
    header++;
    for (i = 0; i < count; i++) {
        e = (EffectEmitGouraudTexQuad *)header;
        ch[0] = e->uv0;
        keep[0] = ch[0] & EFFECT_UV_KEEP;
        ch[0] &= EFFECT_UV_V;
        ch[1] = e->uv1;
        keep[1] = ch[1] & EFFECT_UV_KEEP;
        ch[1] &= EFFECT_UV_V;
        uv23 = e->uv23;
        ch[3] = uv23;
        ch[2] = uv23;
        keep[2] = uv23 & EFFECT_UV_KEEP23;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] >> 16) & EFFECT_UV_V;
        ch[0] -= EFFECT_UV_SCROLL_STEP;
        ch[1] -= EFFECT_UV_SCROLL_STEP;
        ch[2] -= EFFECT_UV_SCROLL_STEP;
        ch[3] -= EFFECT_UV_SCROLL_STEP;
        if (ch[0] > EFFECT_UV_V || ch[1] > EFFECT_UV_V || ch[2] > EFFECT_UV_V || ch[3] > EFFECT_UV_V) {
            ch[0] += EFFECT_UV_WRAP;
            ch[1] += EFFECT_UV_WRAP;
            ch[2] += EFFECT_UV_WRAP;
            ch[3] += EFFECT_UV_WRAP;
        }
        ch[0] &= EFFECT_UV_V;
        ch[1] &= EFFECT_UV_V;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] & EFFECT_UV_V) << 16;
        e->uv0 = keep[0] | ch[0];
        e->uv1 = keep[1] | ch[1];
        e->uv23 = keep[2] | ch[2] | ch[3];
        header += sizeof(EffectEmitGouraudTexQuad) / sizeof(s32);
    }
}

/**
 * @brief Scroll the textured quad list's V coordinates by @p delta.
 *
 * @ref func_801A4BFC with the step supplied by the caller.
 *
 * @param header Mesh stream to walk.
 * @param delta  Step in whole V units.
 */
void func_801A4D9C(s32 *header, s32 delta) {
    EffectEmitGouraudTexQuad *e;
    u32 keep[3];
    u32 ch[4];
    u32 uv23;
    s32 count;
    s32 i;

    header = (s32 *)((u8 *)header + header[0] / 4 * 4);
    header += EFFECT_QUAD_LIST_HEADER;
    count = *header;
    header++;
    delta <<= 8;
    for (i = 0; i < count; i++) {
        e = (EffectEmitGouraudTexQuad *)header;
        ch[0] = e->uv0;
        keep[0] = ch[0] & EFFECT_UV_KEEP;
        ch[0] &= EFFECT_UV_V;
        ch[1] = e->uv1;
        keep[1] = ch[1] & EFFECT_UV_KEEP;
        ch[1] &= EFFECT_UV_V;
        uv23 = e->uv23;
        ch[3] = uv23;
        ch[2] = uv23;
        keep[2] = uv23 & EFFECT_UV_KEEP23;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] >> 16) & EFFECT_UV_V;
        ch[0] += delta;
        ch[1] += delta;
        ch[2] += delta;
        ch[3] += delta;
        if (ch[0] > EFFECT_UV_V || ch[1] > EFFECT_UV_V || ch[2] > EFFECT_UV_V || ch[3] > EFFECT_UV_V) {
            ch[0] -= EFFECT_UV_WRAP;
            ch[1] -= EFFECT_UV_WRAP;
            ch[2] -= EFFECT_UV_WRAP;
            ch[3] -= EFFECT_UV_WRAP;
        }
        ch[0] &= EFFECT_UV_V;
        ch[1] &= EFFECT_UV_V;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] & EFFECT_UV_V) << 16;
        e->uv0 = keep[0] | ch[0];
        e->uv1 = keep[1] | ch[1];
        e->uv23 = keep[2] | ch[2] | ch[3];
        header += sizeof(EffectEmitGouraudTexQuad) / sizeof(s32);
    }
}
