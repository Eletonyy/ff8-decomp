#ifndef SND_CD_H
#define SND_CD_H

#include "common.h"

// Public prototypes

/** @brief memset-style clear: zeroes @p size bytes at @p dst. */
extern void func_800396E0(void *dst, s32 size);

/** @brief LZSS-decompress @p src into @p dest. */
s32 func_80039444(u8 *src, u8 *dest);

#endif /* SND_CD_H */
