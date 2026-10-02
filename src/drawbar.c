#include "common.h"
#include "psxsdk/libgpu.h"
#include "drawbar.h"
#include "ui/icon.h"

/* Window frames and backgrounds, drawn from the icon sheet's texture page. */

/** @brief The parts of a colour word a window keeps: RGB and the semi-transparency bit. */
#define WINDOW_COLOUR_MASK ((SPRT_CODE_ABE << SPRT_CODE_SHIFT) | 0xFFFFFF)

/** @brief Frame tiles, and the frame's corners and edges, are this many pixels across. */
#define WINDOW_FRAME_SIZE 8

/** @brief Texel coordinates of the frame's corner tiles in the icon sheet. */
#define WINDOW_FRAME_U_LEFT 16
#define WINDOW_FRAME_U_RIGHT 32
#define WINDOW_FRAME_V_TOP 0
#define WINDOW_FRAME_V_BOTTOM 16

/** @brief A sprite's u and v as the halfword they share. */
#define WINDOW_FRAME_UV(u, v) ((u) | ((v) << 8))

/** @brief GP0(E2h) texture windows that repeat one frame edge tile along an edge. */
#define WINDOW_TW_LEFT_EDGE 0xE2008BFF
#define WINDOW_TW_RIGHT_EDGE 0xE20093FF
#define WINDOW_TW_TOP_EDGE 0xE2000FFF
#define WINDOW_TW_BOTTOM_EDGE 0xE2010FFF

/** @brief A top or bottom edge sprite is at most this wide; longer edges take two. */
#define WINDOW_EDGE_MAX_W 240

/**
 * @brief Draw a window's frame around @p rect.
 *
 * Builds the frame from the icon sheet's 8x8 frame tiles: a sprite per corner,
 * and per edge a sprite whose texture window repeats one tile. @p sides picks
 * the sides: WINDOW_FRAME_LEFT draws the left corners and edge,
 * WINDOW_FRAME_RIGHT the right ones. The top and bottom edges are always drawn,
 * between the corners drawn, rounded up to an even width. With both sides, a
 * window under 16 pixels wide or tall gets half-size corners, cut so they keep
 * the tile's outer edge. The colour keeps its RGB and semi-transparency bit.
 *
 * @note Load-bearing spellings: the packets go through one byte cursor; one
 * scratch variable carries the left-side flag and then each corner's or edge's
 * UV word (a variable of its own for the flag moves the register allocation);
 * x and w are sign-extended in place, and w and h then become the right column's
 * x and the bottom row's y. The empty do/while(0) before the corner size is a
 * scheduling barrier: gcc 2.7.2 moves no code across a loop's start or end, and
 * without it the corner size is set before the rectangle is unpacked, unlike
 * the original (it may have been a debug macro compiled out to nothing).
 *
 * @param ot Ordering-table slot the packets are linked into.
 * @param prim First free packet.
 * @param rect Window rectangle.
 * @param color Colour word.
 * @param sides WINDOW_FRAME_LEFT and/or WINDOW_FRAME_RIGHT.
 * @return The first free packet after the ones written.
 */
DR_AREA *func_8002B3A0(void *ot, DR_AREA *prim, RECT *rect, s32 color, s32 sides) {
    DR_TWIN *twin;
    TSPRT *corner;
    ModeSprt *p;
    u32 drawMode;
    u32 link;
    s32 x;
    s32 y;
    s32 w;
    s32 h;
    s32 cornerW;
    s32 cornerH;
    u32 uvAdj;
    u32 tmp;
    u32 uAdj;
    s32 edgeX;
    s32 edgeY;
    s32 edgeW;
    s32 edgeH;
    u8 *pkt;

    pkt = (u8 *)prim;
    drawMode = _get_mode(1, 0, getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y));
    x = *(s32 *)&rect->x;
    w = *(s32 *)&rect->w;
    getAddrNewFast(ot, link);
    y = x >> 16;
    x <<= 16;
    x >>= 16;
    h = w >> 16;
    w <<= 16;
    w >>= 16;
    do {
    } while (0);
    cornerW = WINDOW_FRAME_SIZE;
    cornerH = cornerW;
    uvAdj = 0; /* how far a half-size corner shifts into its tile, packed like a UV */
    if (sides == (WINDOW_FRAME_LEFT | WINDOW_FRAME_RIGHT)) {
        if (w < 2 * WINDOW_FRAME_SIZE) {
            cornerW = w / 2;
            uvAdj = WINDOW_FRAME_SIZE - cornerW;
        }
        if (h < 2 * WINDOW_FRAME_SIZE) {
            cornerH = h / 2;
            uvAdj |= WINDOW_FRAME_UV(0, WINDOW_FRAME_SIZE - cornerH);
        }
    }
    edgeW = w;
    edgeH = h;
    edgeX = x;
    tmp = sides & WINDOW_FRAME_LEFT;
    edgeY = y;
    if (tmp) {
        edgeW -= cornerW;
        edgeH -= cornerH;
        edgeX += cornerW;
        edgeY += cornerH;
    }
    if (sides & WINDOW_FRAME_RIGHT) {
        edgeW -= cornerW;
        edgeH -= cornerH;
    }
    edgeW = (edgeW + 1) / 2 * 2;
    w -= cornerW;
    h -= cornerH;
    w += x;
    h += y;
    color &= WINDOW_COLOUR_MASK;
    color |= SPRT_CODE;
    twin = (DR_TWIN *)pkt;
    setlen(twin, 2);
    twin->code[0] = TEXWINDOW_OFF;
    twin->code[1] = 0;
    link = linkPacket(link, twin);
    pkt += sizeof(DR_TWIN);
    if (tmp) {
        corner = (TSPRT *)pkt;
        *(u32 *)&corner->r0 = color;
        setlen(corner, 5);
        corner->drawMode = drawMode;
        corner->x0 = x;
        corner->y0 = y;
        corner->w = cornerW;
        corner->h = cornerH;
        corner->clut = getClut(ICON_CLUT_X, ICON_CLUT_Y);
        *(u16 *)&corner->u0 = WINDOW_FRAME_UV(WINDOW_FRAME_U_LEFT, WINDOW_FRAME_V_TOP);
        link = linkPacket(link, corner);
        pkt += sizeof(TSPRT);
        p = (ModeSprt *)pkt;
        *(u32 *)&p->r0 = color;
        setlen(p, 7);
        p->drawMode = drawMode;
        p->texWindow[0] = TEXWINDOW_OFF;
        p->texWindow[1] = 0;
        p->x0 = x;
        p->y0 = h;
        p->w = cornerW;
        p->h = cornerH;
        p->clut = getClut(ICON_CLUT_X, ICON_CLUT_Y);
        tmp = (uvAdj & 0xFF00) + WINDOW_FRAME_UV(WINDOW_FRAME_U_LEFT, WINDOW_FRAME_V_BOTTOM);
        *(u16 *)&p->u0 = tmp;
        link = linkPacket(link, p);
        pkt += sizeof(ModeSprt);
        twin = (DR_TWIN *)pkt;
        setlen(twin, 2);
        twin->code[0] = TEXWINDOW_OFF;
        twin->code[1] = 0;
        link = linkPacket(link, twin);
        pkt += sizeof(DR_TWIN);
        if (edgeH > 0) {
            p = (ModeSprt *)pkt;
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->drawMode = drawMode;
            p->texWindow[0] = WINDOW_TW_LEFT_EDGE;
            p->texWindow[1] = 0;
            p->x0 = x;
            p->y0 = edgeY;
            p->w = WINDOW_FRAME_SIZE;
            p->h = edgeH;
            *(u32 *)&p->u0 = getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
            link = linkPacket(link, p);
            pkt += sizeof(ModeSprt);
        }
    }
    if (sides & WINDOW_FRAME_RIGHT) {
        uAdj = uvAdj & 0xFF;
        corner = (TSPRT *)pkt;
        *(u32 *)&corner->r0 = color;
        setlen(corner, 5);
        corner->drawMode = drawMode;
        corner->x0 = w;
        corner->y0 = y;
        corner->w = cornerW;
        corner->h = cornerH;
        corner->clut = getClut(ICON_CLUT_X, ICON_CLUT_Y);
        tmp = uAdj + WINDOW_FRAME_UV(WINDOW_FRAME_U_RIGHT, WINDOW_FRAME_V_TOP);
        *(u16 *)&corner->u0 = tmp;
        link = linkPacket(link, corner);
        pkt += sizeof(TSPRT);
        p = (ModeSprt *)pkt;
        *(u32 *)&p->r0 = color;
        setlen(p, 7);
        p->drawMode = drawMode;
        p->texWindow[0] = TEXWINDOW_OFF;
        p->texWindow[1] = 0;
        p->x0 = w;
        p->y0 = h;
        p->w = cornerW;
        p->h = cornerH;
        p->clut = getClut(ICON_CLUT_X, ICON_CLUT_Y);
        /* uvAdj has only 16 bits, so the mask changes no value, but the original has it */
        tmp = (uvAdj & 0xFFFF) + WINDOW_FRAME_UV(WINDOW_FRAME_U_RIGHT, WINDOW_FRAME_V_BOTTOM);
        *(u16 *)&p->u0 = tmp;
        link = linkPacket(link, p);
        pkt += sizeof(ModeSprt);
        twin = (DR_TWIN *)pkt;
        setlen(twin, 2);
        twin->code[0] = TEXWINDOW_OFF;
        twin->code[1] = 0;
        link = linkPacket(link, twin);
        pkt += sizeof(DR_TWIN);
        if (edgeH > 0) {
            p = (ModeSprt *)pkt;
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->drawMode = drawMode;
            p->texWindow[0] = WINDOW_TW_RIGHT_EDGE;
            p->texWindow[1] = 0;
            p->x0 = w;
            p->y0 = edgeY;
            p->w = WINDOW_FRAME_SIZE;
            p->h = edgeH;
            tmp = getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
            tmp = uAdj + tmp;
            *(u32 *)&p->u0 = tmp;
            link = linkPacket(link, p);
            pkt += sizeof(ModeSprt);
        }
    }
    if (edgeW > 0) {
        if (edgeW > WINDOW_EDGE_MAX_W) {
            p = (ModeSprt *)pkt;
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->drawMode = drawMode;
            p->texWindow[0] = WINDOW_TW_TOP_EDGE;
            p->texWindow[1] = 0;
            p->x0 = edgeX;
            p->y0 = y;
            p->w = WINDOW_EDGE_MAX_W;
            p->h = WINDOW_FRAME_SIZE;
            tmp = getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
            *(u32 *)&p->u0 = tmp;
            link = linkPacket(link, p);
            pkt += sizeof(ModeSprt);
            p = (ModeSprt *)pkt;
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->drawMode = drawMode;
            p->texWindow[0] = WINDOW_TW_BOTTOM_EDGE;
            p->texWindow[1] = 0;
            p->x0 = edgeX;
            p->y0 = h;
            p->w = WINDOW_EDGE_MAX_W;
            p->h = WINDOW_FRAME_SIZE;
            tmp += uvAdj & 0xFF00;
            *(u32 *)&p->u0 = tmp;
            link = linkPacket(link, p);
            pkt += sizeof(ModeSprt);
            edgeW -= WINDOW_EDGE_MAX_W;
            edgeX += WINDOW_EDGE_MAX_W;
        }
        p = (ModeSprt *)pkt;
        *(u32 *)&p->r0 = color;
        setlen(p, 7);
        p->drawMode = drawMode;
        p->texWindow[0] = WINDOW_TW_TOP_EDGE;
        p->texWindow[1] = 0;
        p->x0 = edgeX;
        p->y0 = y;
        p->w = edgeW;
        p->h = WINDOW_FRAME_SIZE;
        *(u32 *)&p->u0 = getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
        link = linkPacket(link, p);
        pkt += sizeof(ModeSprt);
        p = (ModeSprt *)pkt;
        *(u32 *)&p->r0 = color;
        setlen(p, 7);
        p->drawMode = drawMode;
        p->texWindow[0] = WINDOW_TW_BOTTOM_EDGE;
        p->texWindow[1] = 0;
        p->x0 = edgeX;
        p->y0 = h;
        p->w = edgeW;
        p->h = WINDOW_FRAME_SIZE;
        tmp = getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
        tmp += uvAdj & 0xFF00;
        *(u32 *)&p->u0 = tmp;
        link = linkPacket(link, p);
        pkt += sizeof(ModeSprt);
    }
    setAddrFast(ot, link);
    return (DR_AREA *)pkt;
}

/**
 * @brief Draw a window's whole frame: func_8002B3A0 with both sides.
 *
 * @param ot Ordering-table slot the packets are linked into.
 * @param prim First free packet.
 * @param rect Window rectangle.
 * @param color Colour word.
 * @return The first free packet after the ones written.
 */
DR_AREA *func_8002B898(void *ot, DR_AREA *prim, RECT *rect, s32 color) {
    return func_8002B3A0(ot, prim, rect, color, WINDOW_FRAME_LEFT | WINDOW_FRAME_RIGHT);
}

/** @brief Texture windows of the window background's tiles, in pattern order, then a zero word. */
extern u32 D_8005295C[];

/** @brief The window background texture repeats in tiles this many texels wide. */
#define WINDOW_BG_TILE_W 128

/** @brief CLUT row of the window background, the row below the icons'. */
#define WINDOW_BG_CLUT_Y (ICON_CLUT_Y + 1)

/**
 * @brief Draw a window's background texture over @p rect.
 *
 * Resets the texture window, then covers @p rect in strips at most one tile
 * (128 texels) wide. Each strip is a sprite from the icon sheet's texture page
 * that carries its own draw mode and its tile's texture window from
 * @c D_8005295C, so the pattern repeats across the window. @p bgOffset shifts
 * the pattern: its low 16 bits are the x offset in texels, and a first strip
 * then covers the rest of that tile; bits 16-19 are the v coordinate. The
 * colour keeps its RGB and semi-transparency bit, so a semi-transparent colour
 * draws a see-through background.
 *
 * @note Load-bearing spellings: @c v is built one operation per statement, and
 * @p bgOffset is reused in place for the offset and the first strip's width.
 * Each word of @p rect is read inside a do/while(0): gcc 2.7.2 moves no code
 * across a loop's start or end, which keeps the reads in source order with
 * their load delays unfilled, as in the original.
 *
 * @param ot Ordering-table slot the packets are linked into.
 * @param prim First free packet.
 * @param rect Window rectangle; its width is rounded down to even.
 * @param color Colour word.
 * @param bgOffset Pattern offset: x in bits 0-15, v in bits 16-19.
 * @return The first free packet after the ones written.
 */
DR_AREA *func_8002B8BC(void *ot, DR_AREA *prim, RECT *rect, s32 color, s32 bgOffset) {
    DR_TWIN *twin;
    ModeSprt *p;
    u32 *tw;
    u32 mode;
    u32 link;
    u32 x;
    u32 y;
    u32 w;
    u32 h;
    u32 v;

    mode = _get_mode(1, 0, getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y));
    getAddrNewFast(ot, link);
    tw = D_8005295C;
    v = bgOffset;
    v >>= 16;
    v &= 0xF;
    bgOffset &= 0xFFFF;
    tw += bgOffset / WINDOW_BG_TILE_W;
    bgOffset %= WINDOW_BG_TILE_W;
    /* setTexWindow cannot write this: it builds the word from a RECT, a NULL
     * one gives a no-op word, and a zeroed one needs a stack slot the
     * original does not have. */
    twin = (DR_TWIN *)prim;
    setlen(twin, 2);
    twin->code[0] = TEXWINDOW_OFF;
    twin->code[1] = 0;
    link = linkPacket(link, twin);
    p = (ModeSprt *)(twin + 1);
    do {
        x = *(u32 *)&rect->x;
    } while (0);
    y = x >> 16;
    x &= 0xFFFF;
    do {
        w = *(u32 *)&rect->w;
    } while (0);
    h = w >> 16;
    w &= 0xFFFE;
    color &= WINDOW_COLOUR_MASK;
    color |= SPRT_CODE;
    if (bgOffset != 0) {
        p->texWindow[0] = *tw++;
        p->u0 = bgOffset;
        bgOffset = WINDOW_BG_TILE_W - bgOffset;
        setlen(p, 7);
        *(u32 *)&p->r0 = color;
        p->texWindow[1] = 0;
        p->drawMode = mode;
        p->v0 = v;
        p->clut = getClut(ICON_CLUT_X, WINDOW_BG_CLUT_Y);
        bgOffset = (bgOffset < 0) ? 0 : (bgOffset > w) ? w : bgOffset;
        p->x0 = x;
        p->y0 = y;
        p->w = bgOffset;
        p->h = h;
        link = linkPacket(link, p);
        p++;
        w -= bgOffset;
        x += bgOffset;
    }
    if (w != 0) {
        while (1) {
            p->texWindow[0] = *tw++;
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->texWindow[1] = 0;
            p->drawMode = mode;
            p->x0 = x;
            p->y0 = y;
            p->h = h;
            *(u32 *)&p->u0 = getClut(ICON_CLUT_X, WINDOW_BG_CLUT_Y) << 16;
            p->v0 = v;
            link = linkPacket(link, p);
            if (w > WINDOW_BG_TILE_W) {
                p->w = WINDOW_BG_TILE_W;
                p++;
                w -= WINDOW_BG_TILE_W;
                x += WINDOW_BG_TILE_W;
            } else {
                p->w = w;
                p++;
                break;
            }
        }
    }
    setAddrFast(ot, link);
    return (DR_AREA *)p;
}

/**
 * @brief Draw a window's background: func_8002B8BC with a bgOffset of 0.
 *
 * A semi-transparent colour word draws a see-through box.
 *
 * @param ot Ordering table.
 * @param prim Primitive buffer cursor.
 * @param rect The window rect.
 * @param color Colour word.
 * @return The primitive cursor after the packets.
 */
DR_AREA *drawWindowBackground(void *ot, DR_AREA *prim, RECT *rect, s32 color) {
    return func_8002B8BC(ot, prim, rect, color, 0);
}
