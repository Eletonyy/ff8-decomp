#ifndef FE_OBJECT12_H
#define FE_OBJECT12_H

#include "common.h"

extern void func_800C0384(void);
extern void func_800C03A0(void);
extern void func_800C03BC(void);
extern void func_800C03D8(void);
extern void func_800C03F4(void);
extern s32  func_800C0410(s32 itemId);
extern void func_800C0448(void);
/** @brief Apply a stocked spell (magicId, quantity) to the party magic pool. */
extern void func_800C048C(s32 magicId, s32 quantity);
extern void func_800C0634(void);

/** @brief Slot-7 magic list; aliases @c &g_gameState.chars[7].magic (64 bytes). */
extern u8 D_80077C40[];

/* INCLUDE_ASM stub — body still in assembly, signature unknown.
 * Declared K&R-style; refine when it gets decomped to C. */
extern int  func_800C0098();

/** @brief Field-engine state initializer (new game / field reset). */
extern void func_800C00C8(s32 fullReset);

#endif
