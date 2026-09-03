/**
 * @file menuitem.h
 * @brief Symbols and types owned by the menuitem overlay.
 *
 * The overlay is split into two translation units, menuitem.c and menuitem2.c
 * (the file boundary is at 0x801E9F94), so these declarations are shared.
 */
#ifndef MENUITEM_H
#define MENUITEM_H

#include "common.h"
#include "gamestate.h"
#include "menumain.h"

extern s32 D_80083850;
extern s32 D_801ECC10;
extern s32 D_801ECE20;
extern s32 D_801ECE24;
extern s32 D_801ECE28;
extern s32 D_801ECE2C;
extern s32 D_801ECE30;
extern s32 D_801ECE34;
extern s32 D_801ECE38;
extern s32 D_801ECEDC;
extern s32 D_801ECEE0;
extern s32 D_801ECEE4;
extern s32 D_801ECEE8;
extern u8 D_801EB17C[];
extern u8 D_801EB188[];
extern u8 D_801EB194[];
extern u8 D_801EB1D8[];
extern u8 D_801EB330[];
extern u8 D_801EB4BC[];
extern u8 D_801EC710[];
extern u8 D_801ECB20[];
extern u8 D_801ECB60[];
extern s32 func_801E2EA8(s32);
extern s32 func_801EFFD4(void);
extern void func_801E80D0();

void func_801E4EA4(s32);
void func_801E95C4(void);

#endif /* MENUITEM_H */
