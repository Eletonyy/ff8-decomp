#ifndef INTRO_ASSETS_H
#define INTRO_ASSETS_H

#include "common.h"

/* Named indices into g_introAssetTable for entries with a fixed role. */
#define INTRO_ASSET_WRONG_DISC  34  /**< "CAUTION WRONG DISC" 580x406 warning frame. */

/**
 * @brief CD asset index for the intro overlay (defined in src/intro_assets.c).
 *
 * Flat table of @c (sector, size) @c u32 pairs — 36 entries × 2 words.
 * Index 2*N gives the LBA, 2*N+1 the size in bytes. @c func_80098338
 * dispatches by stage, @c loadWrongDiscWarning reads entry 34 by raw offset.
 *
 * Generated from the disc layout; in a CD-mastering pipeline this file
 * (and the matching @c src/intro_assets.c) would be regenerated.
 */
extern u32 g_introAssetTable[];

#endif /* INTRO_ASSETS_H */
