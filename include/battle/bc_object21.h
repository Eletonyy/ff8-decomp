#ifndef BC_OBJECT21_H
#define BC_OBJECT21_H

#include "common.h"

extern s32 func_800DD238(void);
extern void func_800DD274(s32 a0);
extern void func_800DD280(s32 a0);
extern void func_800DDF6C(void);
extern void func_800DEA58(s32 a0);
extern void func_800DF794(void);
extern void func_800DF7C8(s32 a0, s32 a1);

/*
 * Wrappers of dialog functions, defined without parameters: each forwards
 * whatever $a0-$a3 hold, and callers pass the arguments of the function it wraps.
 */
extern void func_800DF804(void);
extern void func_800DF824(void);
extern void func_800DF844(void);
extern void func_800DF864(void);
extern void func_800DF884(void);
extern void func_800DF8A4(void);
extern void func_800DF8C4(void);
extern void func_800DF8E4(void);
extern void func_800DF904(void);

#endif /* BC_OBJECT21_H */
