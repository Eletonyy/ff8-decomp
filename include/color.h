#ifndef COLOR_H
#define COLOR_H

#include "common.h"

/**
 * @brief Draw a menu element using a palette selected from the menu-color table.
 *
 * Splits @p color into a palette index (bit 3 selects @c g_menuColor[1] vs
 * @c g_menuColor[0]) and the low 3-bit color, then forwards to the underlying
 * draw routine. Tail-calls @c func_800330F4, so the callee's return value is
 * passed straight through in @c v0.
 *
 * @param renderCtx Render context / OT handle.
 * @param cursorY   Vertical cursor position.
 * @param packedXY  Packed X/Y placement word.
 * @param value     Value to render.
 * @param color     Menu color selector (bit 3 = palette bank, low 3 bits = shade).
 * @return Whatever @c func_800330F4 returns.
 */
s32 drawColorByMenuPalette(s32 renderCtx, s32 cursorY, s32 packedXY, s32 value, s32 color);

#endif /* COLOR_H */
