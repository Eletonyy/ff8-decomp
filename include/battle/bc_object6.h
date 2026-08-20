/**
 * @file bc_object6.h
 * @brief Public symbols owned by bc_object6 — the battle flow controller.
 *
 * bc_object6 drives the battle's top-level sequencing: the per-frame step
 * functions the other battle units call into, the readiness queries they
 * poll, and the stat lookup shared with bc_object2.
 */
#ifndef BC_OBJECT6_H
#define BC_OBJECT6_H

#include "common.h"

extern void func_800AD4A4(s32 a0);
extern void func_800AE3D4(s32 a0);
extern void func_800AE524(s32 a0);

/** @brief Advance the battle sequencer one step. */
extern void func_800AE6C0(void);

/** @brief Readiness queries polled by bc_object1 and bc_object5. */
extern s32 func_800AE730(void);
extern s32 func_800AE788(void);

/** @brief Per-frame stages driven from bc_object1's battle loop. */
extern void func_800AEC04(void);
extern void func_800AECD4(void);
extern void func_800AED30(void);

/**
 * @brief Look up a stat entry for an entity.
 *
 * @param entityIdx Battle entity index.
 * @param outStat   Receives the stat byte.
 * @param outCount  Receives the entry count.
 * @param typeByte  Selects which table is searched.
 */
extern s32 func_800AF134(s32 entityIdx, u8 *outStat, u8 *outCount, s32 typeByte);

#endif /* BC_OBJECT6_H */
