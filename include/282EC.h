#ifndef UNIT_282EC_H
#define UNIT_282EC_H

#include "common.h"

/** @brief Pool sizer for entity/script tables. */
extern s32 func_80037AEC(u8 *header, u16 *table, s32 **outBase);

extern u16 *func_80037B44(u8 *);
extern void func_80037B7C(s32 *base, s32 *outA, s32 *outB);

#endif
