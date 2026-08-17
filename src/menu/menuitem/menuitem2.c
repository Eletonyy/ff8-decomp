#include "common.h"
#include "menu.h"
#include "menuitem2.h"

/* Second translation unit of the item menu. splat's jumptable-alignment
 * heuristic placed the file boundary at 0x801E9F94, and the jump tables from
 * 0x801EB1A0 onward are used only by the functions below, which confirms it.
 */

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801E9F94);

/**
 * @brief Call func_800375A0 with rearranged args and g_menuColor as 6th arg.
 * @param a0 First parameter passed through
 * @param a1 Second parameter passed through
 * @param a2 Becomes 4th argument to callee
 * @param a3 Becomes 5th argument (on stack) to callee
 * @param arg5 Becomes 3rd argument to callee
 */
s32 func_801EA500(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5) {

    return func_800375A0(a0, a1, arg5, a2, a3, g_menuColor);
}

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EA538);

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EA714);

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EA7E0);

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EA8F0);

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EAA04);

/**
 * @brief Render a menu panel with text and display configuration.
 *
 * Calls func_801EAA04 with adjusted position args (a3+8 for width,
 * arg5+0xA pushed to stack). Configures g_menuDisplayCfg with panel position,
 * size, and display properties, then calls func_801EF9AC with the result
 * and g_menuColor as the OT pointer.
 *
 * @param a0 First parameter passed through.
 * @param a1 Text data parameter.
 * @param a2 Second parameter passed through.
 * @param a3 X position for panel.
 * @param arg5 Y position for panel.
 */
s32 func_801EAB00(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5) {
    s32 result;

    result = func_801EAA04(a0, a1, a2, a3 + 8, arg5 + 0xA);
    g_menuDisplayCfg.iconType = 0;
    g_menuDisplayCfg.iconSubType = 0;
    g_menuDisplayCfg.x = a3;
    g_menuDisplayCfg.w = 0x102;
    g_menuDisplayCfg.y = arg5;
    g_menuDisplayCfg.h = 0x7D;
    return func_801EF9AC(a1, result, 0x1000, g_menuColor);
}

INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EAB8C);

/**
 * @brief Render item detail sub-menu with multiple panel sections.
 *
 * Saves and restores a global state via setMenuColorIntensity. If the display mode
 * returned by func_801F0D84 is 0xF, renders several sub-panels: item name
 * (func_801EA500), description (func_801EA538), icon (func_801EA714),
 * stats (func_801EA7E0), info (func_801EAB00), and list (func_801EAB8C).
 * Otherwise returns the current rendering state unchanged.
 *
 * @param a0 Item menu context pointer.
 * @param a1 Rendering context pointer.
 * @param a2 Current rendering state.
 * @return Updated rendering state after all panels are drawn.
 */
s32 func_801EAC54(s32 a0, s32 a1, s32 a2) {
    s32 ctx = a0;
    s32 render = a1;
    s32 state = a2;
    s32 saved = D_80083850;
    s32 result;
    s32 qty;

    setMenuColorIntensity(*(s32 *)(ctx + 0x28));
    if (func_801F0D84() != 0xF) {
        return state;
    }
    qty = *(u8 *)(*(s32 *)(ctx + 0x20) + 1);
    result = func_801EA500(render, state, 0x30, 0x22, qty);
    state = 0x37;
    result = func_801EA538(ctx, render, result, 0x6A, state);
    result = func_801EA714(render, result, 0x6A, 0x1D);
    state = 0x58;
    result = func_801EA7E0(ctx, render, result, 0x18, state);
    result = func_801EAB00(ctx, render, result, 0x6A, state);
    result = func_801EAB8C(render, result, 0x10E, 0x6);
    state = result;
    setMenuColorIntensity(saved);
    return state;
}

/**
 * @brief Initialize item sub-menu.
 *
 * Sets up the sub-menu handler via func_801F179C, initializes display state,
 * reads button input to determine item type. If the context pointer is valid,
 * sets up the data table pointer, string, and various byte fields, then
 * calls func_801E9F94 to render.
 */
INCLUDE_ASM("asm/ovl/menuitem/nonmatchings/menuitem2", func_801EAD64);
