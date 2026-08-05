#ifndef FE_OBJECT8_H
#define FE_OBJECT8_H

#include "common.h"
#include "field.h"

extern s32  opHandler_CLOSEEYES(Actor *actor);
extern s32  opHandler_BLINKEYES(Actor *actor);
extern void func_800B912C(Actor *actor, s16 a1);
extern void func_800B91D8(Actor *actor, s32 a1, s32 a2, s32 a3);
extern void func_800B9288(Actor *actor);
extern s32  opHandler_PUSHANIME(Actor *actor);
extern s32  opHandler_POPANIME(Actor *actor);
extern s32  opHandler_ANIMESPEED(Actor *actor);
extern s32  opHandler_ANIMESYNC(Actor *actor);
extern s32  opHandler_ANIMESTOP(Actor *actor);
extern s32  opHandler_ANIME(Actor *actor, s32 a1);
extern s32  opHandler_ANIMEKEEP(Actor *actor, s32 a1);
extern s32  opHandler_CANIME(Actor *actor, s32 a1);
extern s32  opHandler_CANIMEKEEP(Actor *actor, s32 a1);
extern s32  opHandler_RANIME(Actor *actor, s32 a1);
extern s32  opHandler_RANIMEKEEP(Actor *actor, s32 a1);
extern s32  opHandler_RCANIME(Actor *actor, s32 a1);
extern s32  opHandler_RCANIMEKEEP(Actor *actor, s32 a1);
extern s32  opHandler_RANIMELOOP(Actor *actor, s32 a1);
extern s32  opHandler_RCANIMELOOP(Actor *actor, s32 a1);
extern s32  opHandler_POLYCOLOR(Actor *actor);
extern s32  opHandler_POLYCOLORALL(Actor *actor);
extern s32  opHandler_SETGETA(Actor *actor);
extern s32  opHandler_SETROOTTRANS(Actor *actor);
extern s32  opHandler_SHADESET(Actor *actor);
extern s32  opHandler_SHADEFORM(Actor *actor);
extern s32  opHandler_SHADELEVEL(Actor *actor);
extern s32  opHandler_DIR(Actor *actor);
extern s32  opHandler_DIRP(Actor *actor);
extern s32  opHandler_DIRA(Actor *actor);
extern s32  opHandler_PDIRA(Actor *actor);
extern s32  opHandler_OP16B(Actor *actor);
extern s32  opHandler_OP16C(Actor *actor);
extern s32  opHandler_OP16D(Actor *actor);
extern s32  opHandler_OP16E(Actor *actor);
extern void func_800BA3E0(Actor *actor);
extern s32  opHandler_LTURNR(Actor *actor);
extern s32  opHandler_LTURNL(Actor *actor);
extern s32  opHandler_CTURNR(Actor *actor);
extern s32  opHandler_CTURNL(Actor *actor);
extern s32  opHandler_LTURN(Actor *actor);
extern s32  opHandler_CTURN(Actor *actor);
extern s32  opHandler_PLTURN(Actor *actor);
extern s32  opHandler_PCTURN(Actor *actor);
extern s32  opHandler_HASITEM(Actor *actor);
extern s32  opHandler_CLOCKWISETURN(Actor *actor);
extern s32  opHandler_FACEDIRSYNC(Actor *actor, s32 arg1);
extern s32  opHandler_FACEDIRI(Actor *actor, s32 arg1);
extern s32  opHandler_FACEDIR(Actor *actor, s32 arg1);
extern s32  opHandler_FACEDIRA(Actor *actor, s32 arg1);
extern s32  opHandler_FACEDIRP(Actor *actor, s32 arg1);
extern s32  opHandler_FACEDIROFF(Actor *actor, s32 arg1);
extern s32  opHandler_RFACEDIRI(Actor *actor);
extern s32  opHandler_RFACEDIR(Actor *actor);

#endif
