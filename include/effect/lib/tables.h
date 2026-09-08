#ifndef EFFECT_LIB_TABLES_H
#define EFFECT_LIB_TABLES_H

#include "effect/lib/particle.h"

/**
 * @file tables.h
 * @brief Relocate a pose-script table set and start a child script on it.
 */

/** @brief Pool the pose-script children are allocated from. */
extern s32 g_effectChildPool;

/** @brief Turn every offset in @p tables into an address. */
void effectPoseTablesRelocate(EffectPoseTables *tables);

/** @brief Start @p task as a child of @p owner and seed its script fields. */
void effectStartChildScript(EffectEntity *owner, void *task, void *arg,
                            s16 stopFrame, s16 a, s16 b);

#endif /* EFFECT_LIB_TABLES_H */
