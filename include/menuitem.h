/**
 * @file menuitem.h
 * @brief Symbols and types owned by the menuitem overlay.
 *
 * The overlay is split into two translation units, menuitem.c and menuitem2.c
 * (the file boundary is at 0x801E9F94), so these declarations are shared.
 */
#ifndef MENUITEM_H
#define MENUITEM_H

#include "common.h"

// FIXME: move to menu.h
/** @brief Colors used to draw text in the menu. */
typedef enum {
    MENU_TEXT_COLOR_GRAY = 1,
    MENU_TEXT_COLOR_YELLOW = 2,
    MENU_TEXT_COLOR_RED = 3,
    MENU_TEXT_COLOR_WHITE = 7,
} MenuTextColor;

/**
 * @brief Defines the layout and characters of a character selection grid.
 *
 * The strIds field points to a sequence of string IDs terminated by
 * 0xFFFF. Each string ID represents a string of up to 7 characters
 * to be displayed in the character grid.
 */
typedef struct {
    u16 *strIds;        /**< 0x0: Pointer to the null-terminated sequence of string IDs. */
    u16 strXOffset;     /**< 0x4: Horizontal offset applied to strings at odd indexes. */
    u8 minRowIndex;     /**< 0x6: First character row index. */
    u8 maxRowIndex;     /**< 0x7: Last character row index. */
} CharacterGrid; /* 0x8 bytes */

/**
 * @brief Defines the naming configuration for an entity.
 */
typedef struct {
    u8 nameMaxLength;       /**< 0x0: Maximum allowed name length. */
    u8 portraitIdx;         /**< 0x1: Index of the entity's portrait sprite. */
    u16 defaultNameIdx;     /**< 0x2: Index of the entity's default name. */
    u16 assignedNameOffset; /**< 0x4: Offset to the entity's assigned name, relative to g_gameState. */
    u8 pad6[2];
} EntityNamingConfig; /* 0x8 bytes */

/**
 * @brief State for the entity naming menu.
 */
typedef struct {
    u8 pad00[0x10];
    u16 state;                      /**< 0x10: Current menu state (0-7). */
    u8 pad12[0xE];
    EntityNamingConfig *namingCfg;  /**< 0x20: Pointer to the entity naming configuration. */
    u8 *namePtr;                    /**< 0x24: Pointer to the name being edited. */
    s32 menuBrightness;             /**< 0x28: Menu brightness value (0-4096), used for fade-in/out effects. */
    u8 charGridIndex;               /**< 0x2C: Selected character grid index: 0 = English, 1 = Symbols. */
    u8 charGridCursorIndex;         /**< 0x2D: Cursor index within the selected character grid. */
    u8 menuOptionCursorIndex;       /**< 0x2E: Cursor index within the menu options (0-5). */
    u8 cursorPanelIndex;            /**< 0x2F: Index of the panel containing the cursor: 0 = menu options, 1 = character grid. */
    u8 nameLength;                  /**< 0x30: Current name length. */
} EntityNamingMenuState;

#define CHARACTER_GRID_COUNT 2
extern CharacterGrid D_801EB320[CHARACTER_GRID_COUNT]; /**< Character grid table. */

s32 func_801E9C90(u8 *, s32);
void func_801E9CD4(EntityNamingMenuState *);
void func_801E9DE4(u8 *);
s32 func_801E9E10(u8 *);
u8 *func_801E9E7C(s32);
void func_801E9EA8(EntityNamingMenuState *, s32);

#endif /* MENUITEM_H */
