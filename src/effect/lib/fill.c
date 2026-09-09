/**
 * @file fill.c
 * @brief Fill words.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/fill.h"

/**
 * @brief Fill @p count words at @p dst with @p value.
 *
 * The index is never read, which makes it look like a loop that should count
 * up. It is not: the ascending form generates different code, so counting down
 * to @c -1 is load-bearing.
 */
void effectFillWords(s32 *dst, s32 value, s32 count) {
    s32 i;

    for (i = count - 1; i != -1; i--) {
        *dst++ = value;
    }
}
