/**
 * @file bc_object5.h
 * @brief Public symbols owned by bc_object5 — battle command/state queries.
 *
 * @note Two of this unit's public functions are deliberately absent.
 *       @c func_800A97FC is still declared in @c battle.h, where bc_object3
 *       and bc_object8 pick it up; moving it here means giving those two
 *       units their own headers first. @c func_800A9784 is defined here
 *       taking a @c u16 offset but called from bc_object6 through an
 *       @c (s32, s32) view -- an inconsistent-ABI pair, so that prototype
 *       stays file-local to bc_object6 where the narrower parameter cannot
 *       change its call-site codegen.
 */
#ifndef BC_OBJECT5_H
#define BC_OBJECT5_H

#include "common.h"

extern void func_800A97D4(void);

/** @brief State queries polled by the battle flow controller (bc_object6). */
extern s32 func_800A980C(void);
extern s32 func_800A9888(void);

#endif /* BC_OBJECT5_H */
