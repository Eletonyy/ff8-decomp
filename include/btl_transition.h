#ifndef BTL_TRANSITION_H
#define BTL_TRANSITION_H

#include "common.h"

/**
 * @brief Start the screen transition that plays while a battle loads.
 * @param boss Non-zero for a boss battle, which gets the boss transition
 * instead of the normal one.
 */
extern void func_80023D60(s32 boss);

/** @brief Count VSyncs while the battle transition runs, and switch to its thread every second (normal) or third (boss) one. */
extern void func_80026D8C(void);

/** @brief Does nothing. The battle effect overlays call it. */
extern void func_80026CF0(void);

#endif /* BTL_TRANSITION_H */
