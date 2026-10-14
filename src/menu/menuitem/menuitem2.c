#include "common.h"
#include "gamestate.h"
#include "menu.h"
#include "menumain.h"
#include "menuitem.h"
#include "party_stats.h"
#include "psxsdk/libetc.h"
#include "snd_sfx.h"
#include "ui/icon.h"
#include "ui/text.h"

/* Second translation unit of the item menu. splat's jumptable-alignment
 * heuristic placed the file boundary at 0x801E9F94, and the jump tables from
 * 0x801EB1A0 onward are used only by the functions below, which confirms it.
 */

/** @brief Entity naming menu options. */
enum {
    ENTITY_NAMING_OPTION_ENGLISH = 0,
    ENTITY_NAMING_OPTION_SYMBOLS = 1,
    ENTITY_NAMING_OPTION_SPACE   = 2,
    ENTITY_NAMING_OPTION_CONFIRM = 3,
    ENTITY_NAMING_OPTION_DELETE  = 4,
    ENTITY_NAMING_OPTION_DEFAULT = 5,
};

/** @brief String IDs passed as argument to func_801E9E7C. */
enum {
    STRING_NAME_INPUT = 6,
    STRING_NAME_THE_CHARACTER = 7,
};

#define ENTITY_NAMING_CONFIG_COUNT 21
extern EntityNamingConfig D_801EB240[ENTITY_NAMING_CONFIG_COUNT]; /**< Entity naming config table. */

static void func_801E9F94(EntityNamingMenuState *);
static void *func_801EA500(void *, void *, s32, s32, s32);
static void *func_801EA538(EntityNamingMenuState *, void *, DR_TPAGE *, s32, s32);
static void *func_801EA714(void *, void *, s32, s32);
static void *func_801EA7E0(EntityNamingMenuState *, void *, void *, s32, s32);
static void *func_801EA8F0(void *, DR_TPAGE *, s32, s32, u16, s32);
static void *func_801EAA04(EntityNamingMenuState *, void *, void *, s32, s32);
static void *func_801EAB00(EntityNamingMenuState *, void *, void *, s32, s32);
static void *func_801EAB8C(void *, void *, s32, s32);
static void *func_801EAC54(EntityNamingMenuState *, void *, void *);
static void func_801EAD64(void);

/**
 * @brief Updates the entity naming menu state machine.
 *
 * Handles name editing, character grid switching, action selection,
 * and menu fade in/out.
 *
 * @param s Pointer to the entity naming menu state.
 */
static void func_801E9F94(EntityNamingMenuState *s) {
    CharacterGrid *charGrid;
    u16 btnFlags;
    u16 cfgFlags;
    u16 *statePtr;

    func_801F0948(s->menuBrightness);
    btnFlags = g_menuDisplayCfg.inputRepeat;
    cfgFlags = g_menuDisplayCfg.inputNew;
    charGrid = &D_801EB320[s->charGridIndex];
    statePtr = &s->state;

    switch (*statePtr) {
    case 0:
        s->menuBrightness = 0;
        *statePtr = 1;
        break;

    case 1:
        s->menuBrightness += 256;
        if (s->menuBrightness >= 4096) {
            s->menuBrightness = 4096;
            *statePtr = 2;
        }
        func_801E9CD4(s);
        break;

    case 2:
        s->cursorPanelIndex = 0;
        *statePtr = 3;
        func_801E9CD4(s);
        break;

    case 3: {
        s32 row;
        s32 col;
        col = s->charGridCursorIndex % 14;
        row = s->charGridCursorIndex / 14;
        if (btnFlags & PADLup) {
            row--;
            if (row < 0) {
                row = 0;
            } else {
                sendSpuCommand(1);
            }
        }
        if (btnFlags & PADLdown) {
            if (row < charGrid->maxRowIndex) {
                sendSpuCommand(1);
            }
            row++;
        }
        if (row > charGrid->maxRowIndex) {
            row = charGrid->maxRowIndex;
        }
        if (btnFlags & PADLright) {
            col++;
            if (col > 13) {
                col = 13;
            } else {
                sendSpuCommand(1);
            }
        }
        if (btnFlags & PADLleft) {
            sendSpuCommand(1);
            col--;
            if (col < 0) {
                col = 0;
                *statePtr = 4;
            }
        }
        s->charGridCursorIndex = col + row * 14;
        func_801E9CD4(s);
        if (cfgFlags & PADRup) {
            sendSpuCommand(3);
            func_801E9DE4(s->namePtr);
        } else if (btnFlags & PADRdown) {
            sendSpuCommand(2);
            func_801E9EA8(s, 1);
        } else {
            if (cfgFlags & PADR1) {
                sendSpuCommand(1);
                s->charGridIndex++;

                // Dead code added to match with the original game binary.
                if (s) { s->charGridIndex++; s->charGridIndex--; }

                s->charGridIndex &= 1;
            }
            if (cfgFlags & PADL1) {
                s32 charGridIndex;
                sendSpuCommand(1);
                charGridIndex = s->charGridIndex;
                charGridIndex--;
                if (charGridIndex < 0) {
                    charGridIndex += 2;
                }
                charGridIndex = charGridIndex % 2;
                s->charGridIndex = charGridIndex;
            }
            if (cfgFlags & PADstart) {
                sendSpuCommand(1);
                s->menuOptionCursorIndex = ENTITY_NAMING_OPTION_CONFIRM;
                *statePtr = 4;
            }
        }
        break;
    }

    case 4:
        s->cursorPanelIndex = 1;
        *statePtr = 5;
        break;

    case 5: {
        s32 menuOptionCursorIndex;
        menuOptionCursorIndex = s->menuOptionCursorIndex;
        if (cfgFlags & PADR1) {
            sendSpuCommand(1);
            s->charGridIndex++;

            // Dead code added to match with the original game binary.
            if (s) { s->charGridIndex++; s->charGridIndex--; }

            s->charGridIndex &= 1;
        }
        if (cfgFlags & PADL1) {
            s32 charGridIndex;
            sendSpuCommand(1);
            charGridIndex = s->charGridIndex;
            charGridIndex--;
            if (charGridIndex < 0) {
                charGridIndex += 2;
            }
            charGridIndex = charGridIndex % 2;
            s->charGridIndex = charGridIndex;
        }
        if (btnFlags & PADLdown) {
            sendSpuCommand(1);
            menuOptionCursorIndex++;
            if (menuOptionCursorIndex > 5) {
                menuOptionCursorIndex = 0;
            }
        }
        if (btnFlags & PADLup) {
            sendSpuCommand(1);
            menuOptionCursorIndex--;
            if (menuOptionCursorIndex < 0) {
                menuOptionCursorIndex = 5;
            }
        }
        if (cfgFlags & PADRdown) {
            switch (menuOptionCursorIndex) {
            case ENTITY_NAMING_OPTION_SYMBOLS:
                sendSpuCommand(2);
                s->charGridIndex = ENTITY_NAMING_OPTION_SYMBOLS;
                break;
            case ENTITY_NAMING_OPTION_ENGLISH:
                sendSpuCommand(2);
                s->charGridIndex = ENTITY_NAMING_OPTION_ENGLISH;
                break;
            case ENTITY_NAMING_OPTION_SPACE:
                sendSpuCommand(2);
                func_801E9EA8(s, 0);
                break;
            case ENTITY_NAMING_OPTION_DELETE:
                sendSpuCommand(3);
                func_801E9DE4(s->namePtr);
                break;
            case ENTITY_NAMING_OPTION_CONFIRM:
                if (func_801E9E10(s->namePtr) != 0) {
                    sendSpuCommand(2);
                    *statePtr = 6;
                    break;
                }
                sendSpuCommand(5);
                break;
            case ENTITY_NAMING_OPTION_DEFAULT:
                sendSpuCommand(2);
                copyString(s->namePtr, func_801E9E7C(s->namingCfg->defaultNameIdx));
                break;
            }
        }
        if (cfgFlags & PADstart) {
            sendSpuCommand(1);
            menuOptionCursorIndex = ENTITY_NAMING_OPTION_CONFIRM;
        }
        s->menuOptionCursorIndex = menuOptionCursorIndex;
        func_801E9CD4(s);
        if (btnFlags & PADLright) {
            sendSpuCommand(2);
            *statePtr = 1;
        }
        break;
    }

    case 6:
        recalcPartyStats();
        func_801E9CD4(s);
        *statePtr = 7;
        break;

    case 7:
        s->menuBrightness -= 256;
        if (s->menuBrightness < 0) {
            s->menuBrightness = 0;
            func_801F18FC(s);
            func_801F0BB0();
            *statePtr = 2;
        }
        func_801E9CD4(s);
        break;
    }

    s->nameLength = func_801E9C90(s->namePtr, s->namingCfg->nameMaxLength);
    func_801F0948(s->menuBrightness);
}

/**
 * @brief Renders the entity's portrait.
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the portrait.
 * @param y Y coordinate of the portrait.
 * @param portraitIdx Index of the entity's portrait sprite.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EA500(void *ot, void *prim, s32 x, s32 y, s32 portraitIdx) {
    return func_800375A0(ot, prim, portraitIdx, x, y, g_menuTint[MENU_TINT_NORMAL]);
}

/**
 * @brief Renders the name panel for the entity naming menu.
 *
 * Renders the characters of the name stored in `namePtr`, up to the maximum
 * length specified by `namingCfg->nameMaxLength`. Each character is displayed
 * above a dash icon, with 14 pixels of horizontal spacing. Characters after
 * the first null terminator are not rendered, leaving the remaining positions
 * empty. The character at `nameLength` blinks to indicate the current editing
 * position.
 *
 * @param s Pointer to the entity naming menu state.
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the panel.
 * @param y Y coordinate of the panel.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EA538(EntityNamingMenuState *s, void *ot, DR_TPAGE *prim, s32 x, s32 y) {
    s32 charY;
    s32 nameMaxLength;
    s32 flag;
    s32 i;
    s32 charX;
    MenuDisplayConfig *gs;
    u8* ptr;
    u32 color;
    s32 ch;
    s32 head;
    s32 head2;
    s32 head3;
    s32 primOffset;
    primOffset = 20;
    gs = &g_menuDisplayCfg;
    charX = x + 18;
    charY = y + 10;
    flag = 1;
    nameMaxLength = s->namingCfg->nameMaxLength;
    ptr = s->namePtr;
    for (i = 0; i < nameMaxLength; i++) {
        color = g_menuTint[MENU_TINT_NORMAL];
        if (i == s->nameLength && (gs->animCounter & 1)) {
            color = (color >> 1) & 0xFFFFFF;
        }
        ch = *ptr++;
        if (ch == 0) {
            flag = 0;
        }
        if (flag != 0) {
            getAddrFast(ot, head);
            head2 = func_801F0F20(head, prim, ch - 32, 7, (charY << 0x10) | (charX & 0xFFFF));
            prim = (DR_TPAGE *)((u8 *)prim + primOffset);
            setlen(prim, 1);
            prim->code[0] = 0xE100041F;

            // Dead code added to match with the original game binary.
            if (head3) { head3++; head3--; }

            head3 = (s32)prim << 8;
            setAddrFast(prim, head2);
            prim++;
            setAddrFast(ot, head3);
        }
        prim = drawIcon(ot, prim, ICON_DASH, charX, charY, color);
        charX += 14;
    }
    gs->iconType = ICON_NAME;
    gs->iconSubType = 0;
    gs->x = x;
    gs->y = y;
    gs->w = 258;
    gs->h = 32;
    return func_801EF9AC(ot, prim, 4096, g_menuTint[MENU_TINT_NORMAL]);
}

/**
 * @brief Renders the help panel for the entity naming menu.
 * 
 * The panel displays the string "Name the character".
 * 
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the panel.
 * @param y Y coordinate of the panel.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EA714(void *ot, void *prim, s32 x, s32 y) {
    s32 strX = x + 8;
    s32 strY = y + 8;
    prim = func_801F0FEC(ot, prim, strX, strY, func_801E9E7C(STRING_NAME_THE_CHARACTER), MENU_TEXT_COLOR_WHITE);
    g_menuDisplayCfg.iconType = ICON_HELP;
    g_menuDisplayCfg.iconSubType = 0;
    g_menuDisplayCfg.x = x;
    g_menuDisplayCfg.y = y;
    g_menuDisplayCfg.w = 258;
    g_menuDisplayCfg.h = 26;
    return func_801EF9AC(ot, prim, 4096, g_menuTint[MENU_TINT_NORMAL]);
}

/**
 * @brief Renders the action panel for the entity naming menu.
 * 
 * Renders the six options: "English", "Symbols", "Space", "Confirm",
 * "Delete" and "Default", highlighting the option corresponding to
 * the selected character grid.
 * 
 * @param s Pointer to the entity naming menu state.
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the panel.
 * @param y Y coordinate of the panel.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EA7E0(EntityNamingMenuState *s, void *ot, void *prim, s32 x, s32 y) {
    MenuDisplayConfig *gs = &g_menuDisplayCfg;
    s32 strX = x + 10;
    s32 strY = y + 10;
    s32 i;
    s32 color;
    for (i = 0; i < 6; i++) {
        color = MENU_TEXT_COLOR_WHITE;
        if (i == s->charGridIndex) {
            color = MENU_TEXT_COLOR_YELLOW;
        }
        prim = func_801F0FEC(ot, prim, strX, strY, func_801E9E7C(i), color);
        strY += 16;
    }
    gs->iconType = 0;
    gs->iconSubType = 0;
    gs->x = x;
    gs->y = y;
    gs->w = 80;
    gs->h = 125;
    prim = func_801EF9AC(ot, prim, 4096, g_menuTint[MENU_TINT_NORMAL]);
    return prim;
}

/**
 * @brief Renders a string of up to seven characters.
 *
 * Retrieves the character sequence associated with the string ID and renders
 * up to seven characters horizontally, advancing the X coordinate by 18 pixels
 * for each character. The generated primitives are linked into the ordering
 * table.
 *
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the first character.
 * @param y Y coordinate of the characters.
 * @param strId Menu string ID for the entry to draw.
 * @param minRowIndex Unused.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EA8F0(void *ot, DR_TPAGE *prim, s32 x, s32 y, u16 strId, s32 minRowIndex) {
    s32 head;
    s32 head2;
    s32 i;
    s32 ch;
    u8* ptr;
    u32 clutFlags;

    ptr = func_801E9E7C(strId);
    getAddrFast(ot, head);
    clutFlags = 7;

    // Dead code added to match with the original game binary.
    x++; x--;

    for (i = 0; i < 7; i++) {
        ch = *ptr++;
        getNibbleValue(ch - 32);
        head = func_801F0F20(head, prim, ch - 32, clutFlags, (y << 0x10) | (x & 0xFFFF));
        prim = (DR_TPAGE *)((s32)prim + 20);
        x += 18;
    }
    setlen(prim, 1);
    prim->code[0] = 0xE100041F;

    // Dead code added to match with the original game binary.
    if (head2) { head2++; head2--; }

    head2 = (s32)prim << 8;
    setAddrFast(prim, head);
    head = head2;

    // Dead code added to match with the original game binary.
    if (head2) { head2++; head2--; }
    
    prim++;
    setAddrFast(ot, head);
    return prim;
}

/**
 * @brief Renders the character grid.
 *
 * The grid is rendered from a sequence of strings, each containing up to
 * seven characters. Even-indexed strings are placed in the left column,
 * while odd-indexed strings are placed in the right column. Each pair of
 * consecutive strings shares the same row, with 16 pixels of vertical
 * spacing between rows and an additional 8 pixels after every two rows.
 *
 * @param s Pointer to the entity naming menu state.
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the character grid.
 * @param y Y coordinate of the character grid.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EAA04(EntityNamingMenuState *s, void *ot, void *prim, s32 x, s32 y) {
    s32 i;
    CharacterGrid *charGrid;
    u16 *strIds;
    s32 strXOffset;
    s32 minRowIndex;
    u16 strId;
    s32 strX;
    s32 strY;
    i = 0;
    charGrid = &D_801EB320[s->charGridIndex];
    strIds = charGrid->strIds;
    strXOffset = charGrid->strXOffset;
    minRowIndex = charGrid->minRowIndex;
    while (1) {
        strId = *strIds++;
        if (strId == 0xFFFF) {
            break;
        }
        strX = x + strXOffset * (i % 2);
        strY = y + (i / 2) * 16;
        strY += (i / 4) * 8;
        prim = func_801EA8F0(ot, prim, strX, strY, strId, minRowIndex);
        i++;
    }
    return prim;
}

/**
 * @brief Renders the character grid panel for the entity naming menu.
 *
 * Character grid rendering is handled by func_801EAA04.
 *
 * @param s Pointer to the entity naming menu state.
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the panel.
 * @param y Y coordinate of the panel.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EAB00(EntityNamingMenuState *s, void *ot, void *prim, s32 x, s32 y) {
    prim = func_801EAA04(s, ot, prim, x + 8, y + 10);
    g_menuDisplayCfg.iconType = 0;
    g_menuDisplayCfg.iconSubType = 0;
    g_menuDisplayCfg.x = x;
    g_menuDisplayCfg.y = y;
    g_menuDisplayCfg.w = 258;
    g_menuDisplayCfg.h = 125;
    return func_801EF9AC(ot, prim, 4096, g_menuTint[MENU_TINT_NORMAL]);
}

/**
 * @brief Draws the title panel for the entity naming menu.
 * 
 * The panel displays the title "Name Input".
 * 
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @param x X coordinate of the panel.
 * @param y Y coordinate of the panel.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EAB8C(void *ot, void *prim, s32 x, s32 y) {
    s32 strX = x + 7;
    s32 strY = y + 6;
    prim = func_801F0FEC(ot, prim, strX, strY, func_801E9E7C(STRING_NAME_INPUT), MENU_TEXT_COLOR_WHITE);
    g_menuDisplayCfg.iconType = 0;
    g_menuDisplayCfg.iconSubType = 0;
    g_menuDisplayCfg.x = x;
    g_menuDisplayCfg.y = y;
    g_menuDisplayCfg.w = 90;
    g_menuDisplayCfg.h = 22;
    return func_801EF9AC(ot, prim, 4096, g_menuTint[MENU_TINT_NORMAL]);
}

/**
 * @brief Renders the entity naming menu.
 *
 * Update the menu brightness from the entity naming state and draws
 * the entity's portrait, editable name, help text, character grid,
 * action options, title, and finally updates the menu brightness.
 * Skips rendering if the display mode is not 15.
 *
 * @param s Pointer to the entity naming menu state.
 * @param ot Pointer to the ordering table.
 * @param prim Next free address in the primitive buffer.
 * @return Updated next free address in the primitive buffer.
 */
static void *func_801EAC54(EntityNamingMenuState *s, void *ot, void *prim) {
    s32 y;
    s32 menuBrightness = g_menuBrightness;

    setMenuBrightness(s->menuBrightness);
    if (func_801F0D84() != 15) {
        return prim;
    }
    prim = func_801EA500(ot, prim, 48, 34, s->namingCfg->portraitIdx);
    y = 55;
    prim = func_801EA538(s, ot, prim, 106, y);
    prim = func_801EA714(ot, prim, 106, 29);
    y = 88;
    prim = func_801EA7E0(s, ot, prim, 24, y);
    prim = func_801EAB00(s, ot, prim, 106, y);
    prim = func_801EAB8C(ot, prim, 270, 6);
    setMenuBrightness(menuBrightness);
    return prim;
}

/**
 * @brief Initializes the entity naming menu.
 *
 * Registers the entity naming menu update and render callbacks,
 * reads the naming configuration index from the current menu value.
 * When the index identifies a GF (3 through 18), sets a field flag.
 * Initializes the name pointer with the current assigned value.
 * If bit 7 flag is set, the name pointer is initialized with the
 * default value instead.
 */
static void func_801EAD64(void) {
    EntityNamingMenuState *s;
    u32 index;
    u8* defaultName;
    GameState *gs;
    s = func_801F179C(func_801E9F94, func_801EAC54);
    func_801F0948(0);
    index = func_801F000C() & 0x7F;
    if (index - 3 < 16) {
        setFieldFlag(index + 70);
    }
    if (s != NULL) {
        gs = &g_gameState;
        s->namingCfg = &D_801EB240[func_801F000C() & 0x7F];
        s->namePtr = (u8 *)gs + s->namingCfg->assignedNameOffset;
        defaultName = func_801E9E7C(s->namingCfg->defaultNameIdx);
        if (!(func_801F000C() & 0x80)) {
            copyString(s->namePtr, defaultName);
        }
        s->charGridIndex = 0;
        s->cursorPanelIndex = 0;
        s->charGridCursorIndex = 0;
        s->menuBrightness = 0;
        s->menuOptionCursorIndex = s->charGridIndex;
        func_801E9F94(s);
    }
}
