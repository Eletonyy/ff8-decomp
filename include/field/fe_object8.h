#ifndef FE_OBJECT8_H
#define FE_OBJECT8_H

#include "common.h"
#include "field.h"

extern s32  opHandler_CLOSEEYES(Actor *eline);
extern s32  opHandler_BLINKEYES(Actor *eline);
extern void func_800B912C(Actor *eline, s16 a1);
extern void func_800B91D8(Actor *eline, s32 a1, s32 a2, s32 a3);
extern void func_800B9288(Actor *eline);
extern s32  opHandler_PUSHANIME(Actor *eline);
extern s32  opHandler_POPANIME(Actor *eline);
extern s32  opHandler_ANIMESPEED(Actor *eline);
extern s32  opHandler_ANIMESYNC(Actor *eline);
extern s32  opHandler_ANIMESTOP(Actor *eline);
extern s32  opHandler_ANIME(Actor *eline, s32 a1);
extern s32  opHandler_ANIMEKEEP(Actor *eline, s32 a1);
extern s32  opHandler_CANIME(Actor *eline, s32 a1);
extern s32  opHandler_CANIMEKEEP(Actor *eline, s32 a1);
extern s32  opHandler_RANIME(Actor *eline, s32 a1);
extern s32  opHandler_RANIMEKEEP(Actor *eline, s32 a1);
extern s32  opHandler_RCANIME(Actor *eline, s32 a1);
extern s32  opHandler_RCANIMEKEEP(Actor *eline, s32 a1);
extern s32  opHandler_RANIMELOOP(Actor *eline, s32 a1);
extern s32  opHandler_RCANIMELOOP(Actor *eline, s32 a1);
extern s32  opHandler_POLYCOLOR(Actor *eline);
extern s32  opHandler_POLYCOLORALL(Actor *eline);
extern s32  opHandler_SETGETA(Actor *eline);
extern s32  opHandler_SETROOTTRANS(Actor *eline);
extern s32  opHandler_SHADESET(Actor *eline);
extern s32  opHandler_SHADEFORM(Actor *eline);
extern s32  opHandler_SHADELEVEL(Actor *eline);
extern s32  opHandler_DIR(Actor *eline);
extern s32  opHandler_DIRP(Actor *eline);
extern s32  opHandler_DIRA(Actor *eline);
extern s32  opHandler_PDIRA(Actor *eline);
extern s32  opHandler_OP16B(Actor *eline);
extern s32  opHandler_OP16C(Actor *eline);
extern s32  opHandler_OP16D(Actor *eline);
extern s32  opHandler_OP16E(Actor *eline);
extern void func_800BA3E0(Actor *eline);
extern s32  opHandler_LTURNR(Actor *eline);
extern s32  opHandler_LTURNL(Actor *eline);
extern s32  opHandler_CTURNR(Actor *eline);
extern s32  opHandler_CTURNL(Actor *eline);
extern s32  opHandler_LTURN(Actor *eline);
extern s32  opHandler_CTURN(Actor *eline);
extern s32  opHandler_PLTURN(Actor *eline);
extern s32  opHandler_PCTURN(Actor *eline);
extern s32  opHandler_HASITEM(Actor *eline);
extern s32  opHandler_CLOCKWISETURN(Actor *eline);
extern s32  opHandler_FACEDIRSYNC(Actor *eline, s32 arg1);
extern s32  opHandler_FACEDIRI(Actor *eline, s32 arg1);
extern s32  opHandler_FACEDIR(Actor *eline, s32 arg1);
extern s32  opHandler_FACEDIRA(Actor *eline, s32 arg1);
extern s32  opHandler_FACEDIRP(Actor *eline, s32 arg1);
extern s32  opHandler_FACEDIROFF(Actor *eline, s32 arg1);
extern s32  opHandler_RFACEDIRI(Actor *eline);
extern s32  opHandler_RFACEDIR(Actor *eline);

#endif
