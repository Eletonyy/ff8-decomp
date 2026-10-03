#ifndef BTL_TRANSITION_H
#define BTL_TRANSITION_H

#include "common.h"

/**
 * @brief Start the screen transition that plays while a battle loads.
 * @param special Non-zero for the battle scenes that get the mesh transition
 * instead of the normal one.
 */
extern void func_80023D60(s32 special);

/** @brief Count VSyncs while the battle transition runs, and switch to its thread every second (normal) or third (mesh) one. */
extern void func_80026D8C(void);

#endif /* BTL_TRANSITION_H */
