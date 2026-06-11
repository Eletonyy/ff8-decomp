#ifndef TRIPLETRIAD_BE_OBJECT2_H
#define TRIPLETRIAD_BE_OBJECT2_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object2.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object2.c into here as you go. */

/**
 * @brief Capture-direction descriptor — one entry per cardinal direction.
 *
 * @c D_80182D54 holds four of these (UP, DOWN, LEFT, RIGHT). For a cell that
 * was captured this turn, @c resolveCaptures matches the cell's
 * @c TT_CELL_CAP_FROM_* bit against @c capBit to find the direction, then
 * drives the flip animation by setting the entity's type to @c animType.
 */
typedef struct {
    s16 capBit;    /* 0x00 — matching TT_CELL_CAP_FROM_* bit */
    s16 animType;  /* 0x02 — entity type passed to setCardEntityType (selects flip animation) */
} CaptureDir;      /* 0x04 */

extern CaptureDir D_80182D54[4];

/** @brief Set a battle entity's type (which selects its animation); triggers a
 *         flip effect for types 2..5. @c entityIdx indexes @c g_tripleTriadCardHands. */
extern void setCardEntityType(s32 entityIdx, s32 type);

/** @brief Resolve captured cells: trigger each captured cell's flip animation
 *         and clear its captured-from bits. */
extern void resolveCaptures(TripleTriadBoard *board);

#endif /* TRIPLETRIAD_BE_OBJECT2_H */
