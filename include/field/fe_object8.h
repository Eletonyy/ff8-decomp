#ifndef FE_OBJECT8_H
#define FE_OBJECT8_H

#include "common.h"
#include "field.h"

extern s32  opHandler_CLOSEEYES(Eline *eline);
extern s32  opHandler_BLINKEYES(Eline *eline);
extern void func_800B912C(Eline *eline, s16 a1);
extern void func_800B91D8(Eline *eline, s32 a1, s32 a2, s32 a3);
extern void func_800B9288(Eline *eline);
extern s32  opHandler_PUSHANIME(Eline *eline);
extern s32  opHandler_POPANIME(Eline *eline);
extern s32  opHandler_ANIMESPEED(Eline *eline);
extern s32  opHandler_ANIMESYNC(Eline *eline);
extern s32  opHandler_ANIMESTOP(Eline *eline);
extern s32  opHandler_ANIME(Eline *eline, s32 a1);
extern s32  opHandler_ANIMEKEEP(Eline *eline, s32 a1);
extern s32  opHandler_CANIME(Eline *eline, s32 a1);
extern s32  opHandler_CANIMEKEEP(Eline *eline, s32 a1);
extern s32  opHandler_RANIME(Eline *eline, s32 a1);
extern s32  opHandler_RANIMEKEEP(Eline *eline, s32 a1);
extern s32  opHandler_RCANIME(Eline *eline, s32 a1);
extern s32  opHandler_RCANIMEKEEP(Eline *eline, s32 a1);
extern s32  opHandler_RANIMELOOP(Eline *eline, s32 a1);
extern s32  opHandler_RCANIMELOOP(Eline *eline, s32 a1);
extern s32  opHandler_POLYCOLOR(Eline *eline);
extern s32  opHandler_POLYCOLORALL(Eline *eline);
extern s32  opHandler_SETGETA(Eline *eline);
extern s32  opHandler_SETROOTTRANS(Eline *eline);
extern s32  opHandler_SHADESET(Eline *eline);
extern s32  opHandler_SHADEFORM(Eline *eline);
extern s32  opHandler_SHADELEVEL(Eline *eline);
extern s32  opHandler_DIR(Eline *eline);
extern s32  opHandler_DIRP(Eline *eline);
extern s32  opHandler_DIRA(Eline *eline);
extern s32  opHandler_PDIRA(Eline *eline);
extern s32  func_800BA120(Eline *eline);
extern s32  func_800BA1D0(Eline *eline);
extern s32  func_800BA280(Eline *eline);
extern s32  func_800BA330(Eline *eline);
extern void func_800BA3E0(Eline *eline);
extern s32  opHandler_LTURNR(Eline *eline);
extern s32  opHandler_LTURNL(Eline *eline);
extern s32  opHandler_CTURNR(Eline *eline);
extern s32  opHandler_CTURNL(Eline *eline);
extern s32  opHandler_LTURN(Eline *eline);
extern s32  opHandler_CTURN(Eline *eline);
extern s32  opHandler_PLTURN(Eline *eline);
extern s32  opHandler_PCTURN(Eline *eline);
extern s32  opHandler_HASITEM(Eline *eline);
extern s32  opHandler_CLOCKWISETURN(Eline *eline);
extern s32  opHandler_FACEDIRSYNC(Eline *eline, s32 arg1);
extern s32  opHandler_FACEDIRI(Eline *eline, s32 arg1);
extern s32  opHandler_FACEDIR(Eline *eline, s32 arg1);
extern s32  opHandler_FACEDIRA(Eline *eline, s32 arg1);
extern s32  opHandler_FACEDIRP(Eline *eline, s32 arg1);
extern s32  opHandler_FACEDIROFF(Eline *eline, s32 arg1);
extern s32  opHandler_RFACEDIRI(Eline *eline);
extern s32  opHandler_RFACEDIR(Eline *eline);

#endif
