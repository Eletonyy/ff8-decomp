/**
 * @file menuitem2.h
 * @brief Symbols owned by menuitem2.c, the item overlay's second unit.
 *
 * The overlay is one binary split across two translation units at
 * 0x801E9F94. Data and types shared by both live in menuitem.h; this
 * header holds only what menuitem2.c defines.
 */
#ifndef MENUITEM2_H
#define MENUITEM2_H

#include "common.h"
#include "menuitem.h"

void func_801E9F94(s32 arg0);
s32  func_801EA500(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5);
s32  func_801EA538(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5);
s32  func_801EA714(s32 a0, s32 a1, s32 a2, s32 a3);
s32  func_801EA7E0(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5);
s32  func_801EAB00(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5);
s32  func_801EAB8C(s32 a0, s32 a1, s32 a2, s32 a3);
s32  func_801EAC54(s32 a0, s32 a1, s32 a2);

#endif /* MENUITEM2_H */
