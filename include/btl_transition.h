#ifndef BTL_TRANSITION_H
#define BTL_TRANSITION_H

#include "common.h"

/**
 * @brief Start the screen transition that plays while a battle loads.
 * @param special Non-zero for the battle scenes that get the mesh transition
 *                instead of the normal one.
 */
extern void func_80023D60(s32 special);

/** @brief Draw one step of the normal battle transition. */
extern void func_800242C8(void);

#endif /* BTL_TRANSITION_H */
