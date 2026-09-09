/**
 * @file fill.c
 * @brief Fill words.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/fill.h"

/** @brief Fill @p count words at @p dst with @p value. */
void effectFillWords(s32 *dst, s32 value, s32 count) {
    s32 i;

    for (i = count - 1; i != -1; i--) {
        *dst++ = value;
    }
}
