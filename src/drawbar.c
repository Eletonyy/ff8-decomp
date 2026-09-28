#include "common.h"
#include "psxsdk/libgpu.h"
#include "drawbar.h"

// This is likely something like drawTextBox.

/*
 * Clipped colour-bar GPU primitive builders.
 *
 * func_8002B3A0 unpacks its RECT argument and emits GP0 packets (e.g. the 0xE100041E
 * draw-mode word); the rest wrap it or func_8002B8BC.
 */

INCLUDE_ASM("asm/nonmatchings/drawbar", func_8002B3A0);

/**
 * @brief Draw a clipped colour bar with fill mode 3 — a thin wrapper around func_8002B3A0.
 *
 * @param ot    Ordering table the bar links into.
 * @param prim  Running primitive cursor.
 * @param rect  Bar rectangle.
 * @param color Fill colour.
 * @return The advanced primitive cursor.
 */
DR_AREA *func_8002B898(P_TAG *ot, DR_AREA *prim, RECT *rect, s32 color) {
    return func_8002B3A0(ot, prim, rect, color, 3);
}

INCLUDE_ASM("asm/nonmatchings/drawbar", func_8002B8BC);

/**
 * @brief Draw a window's background: func_8002B8BC with @p a4 = 0.
 *
 * A semi-transparent colour word draws a see-through box.
 *
 * @param ot Ordering table.
 * @param prim Primitive buffer cursor.
 * @param rect The window rect.
 * @param color Colour word.
 * @return The primitive cursor after the packets.
 */
DR_AREA *drawWindowBackground(P_TAG *ot, DR_AREA *prim, RECT *rect, s32 color) {
    return func_8002B8BC(ot, prim, rect, color, 0);
}
