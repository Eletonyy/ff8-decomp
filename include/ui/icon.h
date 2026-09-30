#ifndef UI_ICON_H
#define UI_ICON_H

#include "common.h"
#include "psxsdk/libgpu.h"

/* --- Icon table --- */

/**
 * @brief One 8-byte sprite cell of an icon in @c g_iconTable.
 *
 * An icon is drawn from one or more of these cells, and both words are baked in
 * the layout the GPU packet wants so the emitters only mask and add.
 *
 * @c texInfo: bits 0-15 are u and v; bits 16-19 and 22-26 are the two pieces of
 * a CLUT offset that is added to the icons' CLUT (@ref ICON_UVCLUT_MASK keeps
 * exactly these and u/v); bit 27 is the semi-transparency flag and bits 30-31
 * the blend rate. Bit 21 is set in every cell of the shipped table, but no
 * emitter reads it.
 *
 * @c metrics packs four bytes: the sprite width, a signed X offset, the sprite
 * height and a signed Y offset, so width + X offset is the cell's right edge
 * and height + Y offset its bottom edge.
 */
typedef struct {
    /* 0x00 */ u32 texInfo; /**< u | v<<8 | CLUT offset bits | abe<<27 | abr<<30. */
    /* 0x04 */ u32 metrics; /**< w | (s8)xOffset<<8 | h<<16 | (s8)yOffset<<24. */
} IconCell;

/**
 * @brief Header view of @c g_iconTable (baked into executable data).
 *
 * An icon count followed by one descriptor per icon. Each descriptor packs the
 * icon's cell @c count (high 16 bits) and the byte offset from the table base
 * to that icon's @ref IconCell list (low 16 bits). The cell lists themselves
 * live in the trailing area the descriptors point at.
 */
typedef struct {
    /* 0x00 */ s32 iconCount;
    /* 0x04 */ u32 descriptors[1]; /**< cellCount<<16 | byteOffsetToCells. */
} IconTable;

/**
 * @brief The icon table: icon count, per-icon descriptors and cell lists. The same
 * bytes as the PC release's menu/icon.sp1.
 */
extern IconTable g_iconTable;

/** @brief Every icon of @c g_iconTable, by index. The indices not listed are empty 8x8 cells. */
enum {
    ICON_CHOICE_CURSOR = 0x00, /**< The hand cursor, pointing right. */
    ICON_CHOICE_CURSOR_LEFT = 0x01, /**< The hand cursor, pointing left. */
    ICON_TARGET_RETICLE = 0x04,
    ICON_TARGET_RETICLE_2 = 0x05,
    ICON_NEXT_PAGE_MARKER = 0x06, /**< Drawn in a message window's bottom-right corner. */
    ICON_BLANK = 0x07, /**< An empty cell, for blanked leading zeros. */
    ICON_PARTY_NAME_0 = 0x08, /**< A 96x16 strip the battle renders party member 0's name into (func_80037308). */
    ICON_PARTY_NAME_1 = 0x09,
    ICON_PARTY_NAME_2 = 0x0A,
    ICON_GIL = 0x0B, /**< "G" */
    ICON_LETTER_P = 0x0C,
    ICON_DASH = 0x0D,
    ICON_LV_LARGE = 0x0E,
    ICON_TARGET_RED = 0x0F,
    ICON_MISS = 0x10,
    ICON_BULLET = 0x14,
    ICON_LV_YELLOW_SMALL = 0x15,
    ICON_COMPLETE = 0x16,
    ICON_LV = 0x17,
    ICON_DIGIT_0 = 0x18, /**< White digits. */
    ICON_DIGIT_1 = 0x19,
    ICON_DIGIT_2 = 0x1A,
    ICON_DIGIT_3 = 0x1B,
    ICON_DIGIT_4 = 0x1C,
    ICON_DIGIT_5 = 0x1D,
    ICON_DIGIT_6 = 0x1E,
    ICON_DIGIT_7 = 0x1F,
    ICON_DIGIT_8 = 0x20,
    ICON_DIGIT_9 = 0x21,
    ICON_SLASH = 0x22,
    ICON_GREEN_AC = 0x23, /**< Green letters that read "AC". */
    ICON_VERTICAL_BAR = 0x25,
    ICON_GAUGE_LEFT_CAP = 0x26,
    ICON_GAUGE_RIGHT_CAP = 0x27,
    ICON_SMALL_DIGIT_0 = 0x28, /**< Small grey digits. */
    ICON_SMALL_DIGIT_1 = 0x29,
    ICON_SMALL_DIGIT_2 = 0x2A,
    ICON_SMALL_DIGIT_3 = 0x2B,
    ICON_SMALL_DIGIT_4 = 0x2C,
    ICON_SMALL_DIGIT_5 = 0x2D,
    ICON_SMALL_DIGIT_6 = 0x2E,
    ICON_SMALL_DIGIT_7 = 0x2F,
    ICON_SMALL_DIGIT_8 = 0x30,
    ICON_SMALL_DIGIT_9 = 0x31,
    ICON_PAGE = 0x32, /**< "P.", in front of a page number. */
    ICON_DOT = 0x37,
    ICON_BIG_DIGIT_0 = 0x38, /**< Big white digits. */
    ICON_BIG_DIGIT_1 = 0x39,
    ICON_BIG_DIGIT_2 = 0x3A,
    ICON_BIG_DIGIT_3 = 0x3B,
    ICON_BIG_DIGIT_4 = 0x3C,
    ICON_BIG_DIGIT_5 = 0x3D,
    ICON_BIG_DIGIT_6 = 0x3E,
    ICON_BIG_DIGIT_7 = 0x3F,
    ICON_BIG_DIGIT_8 = 0x40,
    ICON_BIG_DIGIT_9 = 0x41,
    ICON_BIG_ARROW_LEFT = 0x43,
    ICON_BAR_FRAME = 0x44,
    ICON_DOUBLE_BANNER = 0x45,
    ICON_TRIPLE_BANNER = 0x46,
    ICON_PRICE = 0x47, /**< Grey labels. */
    ICON_COMMAND = 0x48,
    ICON_NAME = 0x49,
    ICON_MAGIC = 0x4A,
    ICON_PAUSE = 0x4B,
    ICON_ITEM = 0x4C,
    ICON_NUM = 0x4D,
    ICON_SPECIAL = 0x4E,
    ICON_CHOICE = 0x4F,
    ICON_GF = 0x50,
    ICON_TARGET = 0x51,
    ICON_STATUS = 0x52,
    ICON_DOUBLE = 0x53,
    ICON_TRIPLE = 0x54,
    ICON_HELP = 0x55,
    ICON_NOTICE = 0x56,
    ICON_INFO = 0x57,
    ICON_NAME_2 = 0x58,
    ICON_CARDS = 0x59,
    ICON_LONG_FRAME = 0x5A,
    ICON_HP = 0x5B,
    ICON_ARROW_LEFT = 0x5C,
    ICON_ARROW_RIGHT = 0x5D,
    ICON_ABILITY = 0x5E,
    ICON_CHOICE_2 = 0x5F,
    ICON_THIN_DIGIT_0 = 0x60, /**< Thin digits. */
    ICON_THIN_DIGIT_1 = 0x61,
    ICON_THIN_DIGIT_2 = 0x62,
    ICON_THIN_DIGIT_3 = 0x63,
    ICON_THIN_DIGIT_4 = 0x64,
    ICON_THIN_DIGIT_5 = 0x65,
    ICON_THIN_DIGIT_6 = 0x66,
    ICON_THIN_DIGIT_7 = 0x67,
    ICON_THIN_DIGIT_8 = 0x68,
    ICON_THIN_DIGIT_9 = 0x69,
    ICON_THIN_SLASH = 0x6B,
    ICON_ARROW_DOWN = 0x6D, /**< A down-pointing triangle. */
    ICON_ARROW_UP = 0x6E, /**< An up-pointing triangle. */
    ICON_TIME = 0x6F,
    ICON_BOLD_DIGIT_0 = 0x70, /**< Bold digits. */
    ICON_BOLD_DIGIT_1 = 0x71,
    ICON_BOLD_DIGIT_2 = 0x72,
    ICON_BOLD_DIGIT_3 = 0x73,
    ICON_BOLD_DIGIT_4 = 0x74,
    ICON_BOLD_DIGIT_5 = 0x75,
    ICON_BOLD_DIGIT_6 = 0x76,
    ICON_BOLD_DIGIT_7 = 0x77,
    ICON_BOLD_DIGIT_8 = 0x78,
    ICON_BOLD_DIGIT_9 = 0x79,
    ICON_BOLD_COLON = 0x7A,
    ICON_BOLD_A = 0x7B,
    ICON_SMALL_ARROW_LEFT = 0x7C,
    ICON_SMALL_DOUBLE_ARROW_LEFT = 0x7D,
    ICON_SMALL_ARROW_RIGHT = 0x7E,
    ICON_SMALL_DOUBLE_ARROW_RIGHT = 0x7F,
    ICON_BUTTON_L2 = 0x80, /**< Pad buttons, in pad bit order. */
    ICON_BUTTON_R2 = 0x81,
    ICON_BUTTON_L1 = 0x82,
    ICON_BUTTON_R1 = 0x83,
    ICON_BUTTON_TRIANGLE = 0x84,
    ICON_BUTTON_CIRCLE = 0x85,
    ICON_BUTTON_CROSS = 0x86,
    ICON_BUTTON_SQUARE = 0x87,
    ICON_BUTTON_SELECT = 0x88,
    ICON_BUTTON_L3 = 0x89,
    ICON_BUTTON_R3 = 0x8A,
    ICON_BUTTON_START = 0x8B,
    ICON_DPAD_UP = 0x8C,
    ICON_DPAD_RIGHT = 0x8D,
    ICON_DPAD_DOWN = 0x8E,
    ICON_DPAD_LEFT = 0x8F,
    ICON_VIBRATION = 0xA0,
    ICON_ON = 0xA1,
    ICON_OFF = 0xA2,
    ICON_PERFECT = 0xA4,
    ICON_TRIGGER = 0xA5,
    ICON_HIT_COUNT_0 = 0xA6, /**< "0 Hit!" to "7 Hit!". */
    ICON_HIT_COUNT_1 = 0xA7,
    ICON_HIT_COUNT_2 = 0xA8,
    ICON_HIT_COUNT_3 = 0xA9,
    ICON_HIT_COUNT_4 = 0xAA,
    ICON_HIT_COUNT_5 = 0xAB,
    ICON_HIT_COUNT_6 = 0xAC,
    ICON_HIT_COUNT_7 = 0xAD,
    ICON_GREEN_STAR = 0xAF,
    ICON_SEED_LEVEL = 0xB0, /**< "S-Lv." */
    ICON_SEED_GIL = 0xB1, /**< The "G" after a SeeD salary. */
    ICON_SEED_RANK_A = 0xB2,
    ICON_CHOICE_CURSOR_DOWN = 0xB8, /**< The hand cursor, pointing down. */
    ICON_CHOICE_CURSOR_DOWN_2 = 0xB9, /**< The same hand 4 pixels lower: the other frame of its bob. */
    ICON_RED_CROSS = 0xBA,
    ICON_JUNCTION_MARK = 0xC0, /**< A boxed J: the GF is junctioned. */
    ICON_TINY_LEFT_CAP = 0xC8,
    ICON_TINY_RIGHT_CAP = 0xC9,
    ICON_BIG_ARROW_RIGHT = 0xCF,
    ICON_TITLE_STATUS_ATTACK = 0xD0,
    ICON_TITLE_STATUS_DEFENSE = 0xD1,
    ICON_TITLE_ELEMENTAL_ATTACK = 0xD2,
    ICON_TITLE_ELEMENTAL_DEFENSE = 0xD3,
    ICON_YELLOW_ZERO = 0xD4,
    ICON_YELLOW_STAR = 0xD5,
    ICON_PARTY_MEMBER = 0xD6, /**< Marks a character who is in the party. */
    ICON_CARD = 0xD7,
    ICON_ABILITY_JUNCTION = 0xD8, /**< Ability categories, by getAbilityCategory. */
    ICON_ABILITY_COMMAND = 0xD9,
    ICON_ABILITY_CHARACTER = 0xDA,
    ICON_ABILITY_CHARACTER_2 = 0xDB,
    ICON_ABILITY_PARTY = 0xDC,
    ICON_ABILITY_GF = 0xDD,
    ICON_ABILITY_MENU = 0xDE,
    ICON_ITEM_TYPE_0 = 0xDF, /**< Item types, by the shop's type-to-icon table. */
    ICON_ITEM_TYPE_1 = 0xE0,
    ICON_ITEM_TYPE_2 = 0xE1,
    ICON_ITEM_TYPE_3 = 0xE2,
    ICON_ITEM_TYPE_4 = 0xE3,
    ICON_ITEM_TYPE_5 = 0xE4,
    ICON_ITEM_TYPE_6 = 0xE5,
    ICON_STAT_HP = 0xE6, /**< White stat labels. */
    ICON_STAT_STR = 0xE7,
    ICON_STAT_VIT = 0xE8,
    ICON_STAT_MAG = 0xE9,
    ICON_STAT_SPR = 0xEA,
    ICON_STAT_SPD = 0xEB,
    ICON_STAT_EVA = 0xEC,
    ICON_STAT_HIT = 0xED,
    ICON_STAT_LUCK = 0xEE,
    ICON_FRAMED_FIGURE = 0xEF,
    ICON_BUTTON_L2_SMALL = 0xF0,
    ICON_BUTTON_R2_SMALL = 0xF1,
    ICON_BUTTON_L1_SMALL = 0xF2,
    ICON_BUTTON_R1_SMALL = 0xF3,
    ICON_BUTTON_TRIANGLE_SMALL = 0xF4,
    ICON_BUTTON_CIRCLE_SMALL = 0xF5,
    ICON_BUTTON_CROSS_SMALL = 0xF6,
    ICON_BUTTON_SQUARE_SMALL = 0xF7,
    ICON_TITLE_STATUS_ATTACK_ES = 0xFC, /**< The titles in Spanish. */
    ICON_TITLE_STATUS_DEFENSE_ES = 0xFD,
    ICON_TITLE_ELEMENTAL_ATTACK_ES = 0xFE,
    ICON_TITLE_ELEMENTAL_DEFENSE_ES = 0xFF,
    ICON_YELLOW_DIGIT_0 = 0x101, /**< Yellow digits. */
    ICON_YELLOW_DIGIT_1 = 0x102,
    ICON_YELLOW_DIGIT_2 = 0x103,
    ICON_YELLOW_DIGIT_3 = 0x104,
    ICON_YELLOW_DIGIT_4 = 0x105,
    ICON_YELLOW_DIGIT_5 = 0x106,
    ICON_YELLOW_DIGIT_6 = 0x107,
    ICON_YELLOW_DIGIT_7 = 0x108,
    ICON_YELLOW_DIGIT_8 = 0x109,
    ICON_YELLOW_DIGIT_9 = 0x10A,
    ICON_PERCENT = 0x10B,
    ICON_YELLOW_SLASH = 0x10C,
    ICON_YELLOW_COLON = 0x10D,
    ICON_STATUS_KO = 0x110, /**< Persistent statuses, in status bit order. */
    ICON_STATUS_POISON = 0x111,
    ICON_STATUS_PETRIFY = 0x112,
    ICON_STATUS_DARKNESS = 0x113,
    ICON_STATUS_SILENCE = 0x114,
    ICON_STATUS_BERSERK = 0x115,
    ICON_STATUS_ZOMBIE = 0x116,
    ICON_STATUS_SLEEP = 0x117,
    ICON_STATUS_CLOCK = 0x118,
    ICON_STATUS_HAND = 0x119,
    ICON_STATUS_PENTACLE = 0x11A,
    ICON_STATUS_SPARKLES = 0x11B,
    ICON_STATUS_PYRAMID = 0x11C,
    ICON_ELEMENT_FIRE = 0x120,
    ICON_ELEMENT_ICE = 0x121,
    ICON_ELEMENT_THUNDER = 0x122,
    ICON_ELEMENT_EARTH = 0x123,
    ICON_ELEMENT_POISON = 0x124,
    ICON_ELEMENT_WIND = 0x125,
    ICON_ELEMENT_WATER = 0x126,
    ICON_ELEMENT_HOLY = 0x127,
    ICON_JUNCTION_STATUS_ATTACK = 0x128,
    ICON_JUNCTION_STATUS_DEFENSE = 0x129,
    ICON_JUNCTION_ELEMENTAL_ATTACK = 0x12A,
    ICON_JUNCTION_ELEMENTAL_DEFENSE = 0x12B,
    ICON_STAT_HP_YELLOW = 0x130, /**< Yellow stat labels. */
    ICON_STAT_STR_YELLOW = 0x131,
    ICON_STAT_VIT_YELLOW = 0x132,
    ICON_STAT_MAG_YELLOW = 0x133,
    ICON_STAT_SPR_YELLOW = 0x134,
    ICON_STAT_SPD_YELLOW = 0x135,
    ICON_STAT_EVA_YELLOW = 0x136,
    ICON_STAT_HIT_YELLOW = 0x137,
    ICON_STAT_LUCK_YELLOW = 0x138,
    ICON_SLOW = 0x140,
    ICON_FAST = 0x141,
    ICON_PLAY = 0x142,
    ICON_SEED = 0x143,
    ICON_HP_YELLOW = 0x144,
    ICON_LV_YELLOW = 0x145,
    ICON_TIME_YELLOW = 0x146,
    ICON_A_YELLOW = 0x147,
    ICON_DISC = 0x148
};

/** @brief u, v and CLUT-offset bits of @c IconCell.texInfo. */
#define ICON_UVCLUT_MASK 0x07CFFFFF

/** @brief Width and height bytes of @c IconCell.metrics. */
#define ICON_WH_MASK 0x00FF00FF

/** @brief Shift that brings the blend rate of @c IconCell.texInfo (bits 30-31)
 * down to bit 0. */
#define ICON_ABR_SHIFT 30

/** @brief Width of the blend rate once shifted down. getTPage masks again, but
 * dropping this one costs the match. */
#define ICON_ABR_MASK 3

/** @brief Shift that lands the semi-transparency flag of @c IconCell.texInfo
 * (bit 27) on @ref SPRT_CODE_ABE. */
#define ICON_ABE_SHIFT 26

/** @brief Semi-transparency option bit of a primitive's code byte. */
#define SPRT_CODE_ABE 0x02

/** @brief Position of the code byte inside the colour word. */
#define SPRT_CODE_SHIFT 24

/** @brief r, g, b and the two option bits of the code byte in the colour word. */
#define SPRT_RGB_MASK 0x03FFFFFF

/** @brief Primitive code 0x64 (SPRT) in the colour word. */
#define SPRT_CODE 0x64000000

/** @brief VRAM position of the icons' CLUT row; a cell's CLUT offset is added to it. */
#define ICON_CLUT_X 256
#define ICON_CLUT_Y 224

/** @brief VRAM position of the icons' texture page. */
#define ICON_TPAGE_X 896
#define ICON_TPAGE_Y 256

/* Whole-word setters for a TSPRT's r0/g0/b0/code, u0/v0/clut and w/h groups: the
 * icon cells hold those groups ready-made, so they are stored in one piece.
 * The do/while(0) of setIconUVClut is load-bearing: the scheduler moves nothing
 * across it, which keeps the u/v/CLUT store ahead of the texture-page code.
 * Wrapping the colour setter the same way breaks drawNextPageMarker's match. */
#define setIconRGBC(p, word) (*(u32 *)&(p)->r0 = (word))
#define setIconUVClut(p, word) do { *(u32 *)&(p)->u0 = (word); } while (0)
#define setIconWH(p, word) (*(u32 *)&(p)->w = (word))

extern void setNextPageMarkerBrightness(s32 intensity);
extern void resetNextPageMarkerBrightness(void);
extern void iconStub(void);

void *drawIcon(void *ot, void *head, s32 idx, s32 x, s32 y, s32 color);
void *drawIconClut(void *ot, TSPRT *p, s32 idx, s32 x, s32 y, s32 color, s32 clut);


#endif
