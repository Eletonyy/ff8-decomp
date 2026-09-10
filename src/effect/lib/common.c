/**
 * @file common.c
 * @brief Debug text, screen quads and the basic matrix builders.
 */
#include "common.h"
#include "game.h"
#include "effect.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/common.h"
#include "btl_entity.h"

/**
 * @brief Cache the sixteen hex digit glyphs and reset the debug text cursor.
 *
 * @note The lookup runs once per glyph -- the target holds 16 separate calls,
 *       so a loop or a cached pointer does not match.
 */
void effectTextInit(void) {
    g_effectHexGlyphs[0] = getMenuString(11)[1];
    g_effectHexGlyphs[1] = getMenuString(11)[2];
    g_effectHexGlyphs[2] = getMenuString(11)[3];
    g_effectHexGlyphs[3] = getMenuString(11)[4];
    g_effectHexGlyphs[4] = getMenuString(11)[5];
    g_effectHexGlyphs[5] = getMenuString(11)[6];
    g_effectHexGlyphs[6] = getMenuString(11)[7];
    g_effectHexGlyphs[7] = getMenuString(11)[8];
    g_effectHexGlyphs[8] = getMenuString(11)[9];
    g_effectHexGlyphs[9] = getMenuString(11)[10];
    g_effectHexGlyphs[10] = getMenuString(11)[11];
    g_effectHexGlyphs[11] = getMenuString(11)[12];
    g_effectHexGlyphs[12] = getMenuString(11)[13];
    g_effectHexGlyphs[13] = getMenuString(11)[14];
    g_effectHexGlyphs[14] = getMenuString(11)[15];
    g_effectHexGlyphs[15] = getMenuString(11)[16];
    g_effectHexGlyphs[16] = 0;
    g_effectTextX = 10;
    g_effectTextY = 20;
    g_effectTextColour = 3;
}

/** @brief Draw @p value as eight hex glyphs at the current debug text cursor. */
void effectTextHex(u32 value) {
    u32 *ot = D_800FA5E8->frontOT;
    u8 text[9];
    s32 i;

    for (i = 7; i >= 0; i--) {
        text[i] = g_effectHexGlyphs[value & 0xF];
        value >>= 4;
    }
    text[8] = 0;
    g_effectPrimCursor = func_8002C56C(ot, g_effectPrimCursor, g_effectTextX,
                                       g_effectTextY, text, g_effectTextColour);
    g_effectTextX += 0x48;
}

/** @brief Move the debug text cursor to the start of the next line. */
void effectTextNewline(void) {
    g_effectTextX = 10;
    g_effectTextY += 10;
}

/** @brief Opcode handler: no-op. */
void effectNop(EffectEntity *entity) {
}

/**
 * @brief Emit a 128x128 textured quad at the head of the prim buffer.
 *
 * @param x     Left edge in screen space.
 * @param y     Top edge in screen space.
 * @param tx    Texture page X.
 * @param ty    Texture page Y.
 * @param clutX Palette X.
 * @param clutY Palette Y.
 * @param abr   Semi-transparency rate for the page.
 */
void effectDrawQuad128(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                       s32 abr) {
    POLY_FT4 *poly = g_effectPrimCursor;

    setPolyFT4(poly);
    setSemiTrans(poly, 1);
    setShadeTex(poly, 1);
    poly->tpage = getTPage(1, abr, tx, ty);
    poly->clut = getClut(clutX, clutY);
    poly->u0 = poly->u2 = 0;
    poly->u1 = poly->u3 = 0xFF;
    poly->v0 = poly->v1 = 0;
    poly->v2 = poly->v3 = 0xFF;
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = x + 0x7F;
    poly->y0 = poly->y1 = y;
    poly->y2 = poly->y3 = y + 0x7F;
    AddPrim(D_800FA5E8, poly);
    poly++;
    g_effectPrimCursor = poly;
}

/**
 * @brief Emit a 256x256 textured quad at the head of the prim buffer.
 *
 * @param x     Left edge in screen space.
 * @param y     Top edge in screen space.
 * @param tx    Texture page X.
 * @param ty    Texture page Y.
 * @param clutX Palette X.
 * @param clutY Palette Y.
 * @param abr   Semi-transparency rate for the page.
 */
void effectDrawQuad256(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                       s32 abr) {
    POLY_FT4 *poly = g_effectPrimCursor;

    setPolyFT4(poly);
    setSemiTrans(poly, 1);
    setShadeTex(poly, 1);
    poly->tpage = getTPage(1, abr, tx, ty);
    poly->clut = getClut(clutX, clutY);
    poly->u0 = poly->u2 = 0;
    poly->u1 = poly->u3 = 0xFF;
    poly->v0 = poly->v1 = 0;
    poly->v2 = poly->v3 = 0xFF;
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = x + 0xFF;
    poly->y0 = poly->y1 = y;
    poly->y2 = poly->y3 = y + 0xFF;
    AddPrim(D_800FA5E8, poly);
    poly++;
    g_effectPrimCursor = poly;
}

/** @brief Load the identity matrix. */
void effectMatrixIdentity(MATRIX *m) {
    m->m[0][0] = ONE;
    m->m[1][0] = 0;
    m->m[2][0] = 0;
    m->m[0][1] = 0;
    m->m[1][1] = ONE;
    m->m[2][1] = 0;
    m->m[0][2] = 0;
    m->m[1][2] = 0;
    m->m[2][2] = ONE;
    m->t[0] = 0;
    m->t[1] = 0;
    m->t[2] = 0;
}

/** @brief Post-multiply @p m by a rotation of @p angle about X. */
void effectMatrixRotX(MATRIX *m, s32 angle) {
    EffectRotScratch *rot = func_800B3698(sizeof(EffectRotScratch));

    rot->sin = rsin(angle);
    rot->cos = rsin((angle + 0x400) & 0xFFF);
    rot->m.m[0][0] = ONE;
    rot->m.m[0][1] = 0;
    rot->m.m[0][2] = 0;
    rot->m.m[1][0] = 0;
    rot->m.m[1][1] = rot->cos;
    rot->m.m[1][2] = -rot->sin;
    rot->m.m[2][0] = 0;
    rot->m.m[2][1] = rot->sin;
    rot->m.m[2][2] = rot->cos;
    gte_MulMatrix0(m, &rot->m, m);
    func_800B36B8(sizeof(EffectRotScratch));
}

/**
 * @brief Compose a rotation about the Y axis into @p m.
 *
 * @param m     Matrix rotated in place.
 * @param angle Rotation, in the 0x1000-per-turn units @ref rsin takes.
 */
void effectMatrixRotY(MATRIX *m, s32 angle) {
    EffectRotScratch *rot = func_800B3698(sizeof(EffectRotScratch));

    rot->sin = rsin(angle);
    rot->cos = rsin((angle + 0x400) & 0xFFF);
    rot->m.m[0][0] = rot->cos;
    rot->m.m[0][1] = 0;
    rot->m.m[0][2] = rot->sin;
    rot->m.m[1][0] = 0;
    rot->m.m[1][1] = ONE;
    rot->m.m[1][2] = 0;
    rot->m.m[2][0] = -rot->sin;
    rot->m.m[2][1] = 0;
    rot->m.m[2][2] = rot->cos;
    gte_MulMatrix0(m, &rot->m, m);
    func_800B36B8(sizeof(EffectRotScratch));
}
