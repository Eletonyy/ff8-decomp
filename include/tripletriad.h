#ifndef TRIPLETRIAD_H
#define TRIPLETRIAD_H

#include "common.h"
#include "psxsdk/libgte.h"  /* SVECTOR (CardRenderWork) */

/* Types, constants, and globals for the Triple Triad card mini-game
   (played inside the battle overlay). */

/**
 * @brief One slot on the Triple Triad board (8 bytes).
 *
 * The board is laid out as a 5-cell-wide grid (@c TT_BOARD_COLS) with the
 * 3x3 active play area at rows/cols 1..3 and a 1-cell sentinel border
 * around it. Border slots have @c flags = 0, so neighbor lookups at the
 * edges of the play area fall through cleanly without bounds checks.
 *
 * Multiple bits in @c flags are stateful across a turn:
 *   - @c 0x01 : sentinel/wall slot (set only on border cells; used by the
 *               Same-Wall rule when a rank-A edge faces a wall).
 *   - @c 0x02 : occupied — a card has been placed here.
 *   - @c 0x04 : just-placed — the card was placed this turn, triggering
 *               rule evaluation. Cleared by the orchestrator after each turn.
 *   - @c 0x08..0x40 : captured-from-direction marks (bit @c 0x08<<dir set
 *               when this slot was captured by a neighbor in direction
 *               @c dir, where dir is 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT).
 *   - @c 0x80 : matched in the Same-rule pass-1 (matching edge value).
 *   - @c 0x100 : involved in a Plus-rule combo this turn (set on both the
 *               placer and the captured neighbors).
 */
typedef struct {
    /* 0x00 */ u16 flags;       /**< See @c TT_CELL_* flag table below. */
    /* 0x02 */ u8  cardId;      /**< Index into @c g_tripleTriadCardStats. */
    /* 0x03 */ u8  entityIdx;   /**< @c D_801D31C0 slot index (battle object) driving this cell's animation. */
    /* 0x04 */ u8  owner;       /**< Player 0 or 1. */
    /* 0x05 */ u8  element;     /**< Cell element bitmask (board-tile element; 0 = none). */
    /* 0x06 */ s8  elementMod;  /**< FF8 Elemental rule: +1/-1 added to each edge if card's
                                     element matches/differs from cell's element. */
    /* 0x07 */ u8  pad07;
} TripleTriadBoardSlot;

/** @brief Bits in @c TripleTriadBoardSlot.flags. */
#define TT_CELL_WALL          0x0001  /**< Sentinel border slot (rank-A vs wall triggers Same-Wall). */
#define TT_CELL_OCCUPIED      0x0002  /**< A card has been placed in this slot. */
#define TT_CELL_JUST_PLACED   0x0004  /**< Placed this turn; triggers rule evaluation. */
#define TT_CELL_CAP_FROM_BASE 0x0008  /**< Shift left by @c dir (0..3) for captured-from-direction bit. */
#define TT_CELL_CAP_FROM_UP   0x0008
#define TT_CELL_CAP_FROM_DOWN 0x0010
#define TT_CELL_CAP_FROM_LEFT 0x0020
#define TT_CELL_CAP_FROM_RIGHT 0x0040
#define TT_CELL_CAP_FROM_MASK 0x0078  /**< All four captured-from-direction bits (UP|DOWN|LEFT|RIGHT). */
#define TT_CELL_SAME_MATCHED  0x0080  /**< Matched in Same-rule pass 1, or "Same fired here" on the placer. */
#define TT_CELL_PLUS_COMBO    0x0100  /**< Involved in a Plus-rule combo this turn. */

/**
 * @brief Per-card stat block (8 bytes) — one entry of @c g_tripleTriadCardStats.
 *
 * The four @c sides values are the edge ranks (1..10, where rank 10 = "A")
 * stored in the order {top, bottom, left, right}. This ordering is chosen
 * specifically so @c i^1 yields the opposing edge: 0(top)↔1(bottom),
 * 2(left)↔3(right). The same ordering is used by @c TripleTriadDirection,
 * letting rule evaluators compare @c myCard.sides[dir] against
 * @c neighborCard.sides[dir^1] without a lookup table.
 */
typedef struct {
    /* 0x00 */ u8 sides[4];   /**< Edge ranks 1..10, in {top, bottom, left, right} order. */
    /* 0x04 */ u8 element;     /**< Card element bitmask (0 = none). */
    /* 0x05 */ u8 pad05[3];    /**< Level / other per-card metadata. */
} TripleTriadCard;

/**
 * @brief 4-cardinal neighbor offset (4 bytes).
 *
 * One entry of @c g_tripleTriadDirectionOffsets, which holds the four
 * cardinal direction vectors in this exact order:
 *   index 0: UP    (0, -1)
 *   index 1: DOWN  (0, +1)
 *   index 2: LEFT  (-1, 0)
 *   index 3: RIGHT (+1, 0)
 *
 * The pairing is deliberate: @c i^1 flips a direction to its opposite
 * (0↔1, 2↔3), matching the @c TripleTriadCard.sides[] layout so that
 * "my edge facing direction @c i" lines up with "my neighbor's edge
 * facing direction @c i^1".
 */
typedef struct {
    /* 0x00 */ s16 dx;        /**< Column delta. */
    /* 0x02 */ s16 dy;        /**< Row delta. */
} TripleTriadDirection;

/**
 * @brief One bucket in the Plus-rule edge-sum histogram (2 bytes).
 *
 * Used only inside @c applyPlusRule. After a placed card is identified,
 * one bucket per possible edge sum (0..20) accumulates how many of the
 * four neighbors produced that sum. The Plus rule fires when any bucket
 * reaches @c count >= 2, at which point @c dirMask tells which neighbor
 * directions to flip.
 */
typedef struct {
    /* 0x00 */ u8 count;      /**< Number of neighbors that hit this edge sum. */
    /* 0x01 */ u8 dirMask;    /**< Bitmask of which directions (bit @c i set if dir @c i hit). */
} TripleTriadPlusBucket;

/** @brief Number of columns per row, including the 1-cell sentinel border. */
#define TT_BOARD_COLS  5
/** @brief Number of rows in the board, including sentinel borders. */
#define TT_BOARD_ROWS  5

/**
 * @brief Full 5x5 Triple Triad board (rows × cols, 200 bytes total).
 *
 * The active play area is at rows/cols 1..3; the 0th and 4th rows/cols
 * are sentinel slots with @c flags=0 used to keep neighbor lookups
 * branch-free at the edges of the play area.
 */
typedef struct {
    /* 0x00 */ TripleTriadBoardSlot cells[TT_BOARD_ROWS][TT_BOARD_COLS];
} TripleTriadBoard;

/** @brief Global 5x5 Triple Triad board (sentinel-padded). */
extern TripleTriadBoard D_801D3398;

/**
 * @brief 60-byte work buffer staged by @c func_80098B80 for one card
 *        render pass (used by @c func_8009AE6C and related helpers).
 *
 * Holds the 4 digit-corner SVECTORs computed for each rank inside the
 * digit loop, the 4 transformed screen positions of the card outline
 * (from @c RotTransPers4), and the GTE P/flag outputs.
 */
typedef struct {
    /* 0x00 */ SVECTOR digitVerts[4];   /**< Per-rank digit quad corners. */
    /* 0x20 */ s32     outXY[4];         /**< Packed @c (s16 x, s16 y) screen verts. */
    /* 0x30 */ s32     P;                /**< RotTransPers4 perspective output. */
    /* 0x34 */ s32     flag;             /**< RotTransPers4 flag output. */
    /* 0x38 */ u8      pad38[4];
} CardRenderWork;                        /* 60 bytes */

/** @brief 4-cardinal direction indices into @c g_tripleTriadDirectionOffsets and
 *         @c TripleTriadCard.sides[]. The pairing @c dir^1 yields the opposite. */
typedef enum {
    TT_DIR_UP    = 0,
    TT_DIR_DOWN  = 1,
    TT_DIR_LEFT  = 2,
    TT_DIR_RIGHT = 3,
    TT_DIR_COUNT = 4
} TripleTriadDir;

/** @brief Rank value that triggers the Same-Wall rule when facing a wall. */
#define TT_RANK_A          0x0A

/** @brief Bits in @c g_tripleTriadRules controlling which optional rules are active. */
#define TT_RULE_SAME       0x02   /**< Same rule enabled. */
#define TT_RULE_PLUS       0x04   /**< Plus rule enabled. */
#define TT_RULE_SAME_WALL  0x40   /**< Same-Wall extension (A facing wall counts as a match). */
#define TT_RULE_ELEMENTAL  0x80   /**< FF8 Elemental rule (tile elements give +1/-1 edge modifiers). */

extern TripleTriadCard      g_tripleTriadCardStats[];          /**< Card stats table (~110 cards). */
extern TripleTriadDirection g_tripleTriadDirectionOffsets[4];  /**< UP, DOWN, LEFT, RIGHT (see TripleTriadDirection). */
extern s32                  g_tripleTriadRules;                /**< Active rule flags (TT_RULE_*). */

#endif /* TRIPLETRIAD_H */
