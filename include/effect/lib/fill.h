#ifndef EFFECT_LIB_FILL_H
#define EFFECT_LIB_FILL_H

#include "effect.h"

/**
 * @file fill.h
 * @brief Fill words.
 */

/** @brief Fill @p count words at @p dst with @p value. */
void effectFillWords(s32 *dst, s32 value, s32 count);

#endif /* EFFECT_LIB_FILL_H */
