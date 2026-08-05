#ifndef FE_OBJECT7_H
#define FE_OBJECT7_H

#include "common.h"
#include "field.h"

extern s32  opHandler_WHERECARD(Actor *eline);
extern s32  opHandler_CARDGAME(Actor *eline);
extern u8  *func_800B574C(u8 *src);
extern u8  *func_800B578C(s32 search, s32 offset);
extern u8  *func_800B57E8(s32 maxCount, s32 abilityId);
extern s32  func_800B5990(void);
extern s32  opHandler_DRAWPOINT(Actor *eline);
extern s32  opHandler_SETDRAWPOINT(Actor *eline);
extern s32  opHandler_UNKNOWN10(Actor *eline);
extern s32  opHandler_PARTICLEON(Actor *eline);
extern s32  opHandler_PARTICLEOFF(Actor *eline);
extern s32  opHandler_PARTICLESET(Actor *eline);
extern s32  opHandler_SETWITCH(Actor *eline);
extern s32  opHandler_SETODIN(Actor *eline);
extern s32  func_800B6420(Actor *eline);
extern s32  opHandler_SETPLACE(Actor *eline);
extern s32  opHandler_BATTLEMODE(Actor *eline);
extern s32  opHandler_BATTLE(Actor *eline);
extern s32  opHandler_BATTLERESULT(Actor *eline);
extern s32  opHandler_BATTLEON(void);
extern s32  opHandler_BATTLEOFF(void);
extern s32  opHandler_BATTLECUT(Actor *eline);
extern s32  opHandler_GAMEOVER(Actor *eline);
extern s32  opHandler_ENDING(Actor *eline);
extern s32  opHandler_DISC(Actor *eline);
extern void func_800B663C(Actor *eline);
extern void func_800B66A8(Actor *eline);
extern void func_800B6738(Actor *eline);
extern void func_800B67F4(Actor *eline);
extern void func_800B6854(Actor *eline);
extern s32  opHandler_MSPEED(Actor *eline);
extern s32  opHandler_MOVE(Actor *eline);
extern s32  opHandler_MOVEA(Actor *eline);
extern s32  opHandler_PMOVEA(Actor *eline);
extern s32  opHandler_CMOVE(Actor *eline);
extern s32  opHandler_FMOVE(Actor *eline);
extern s32  opHandler_FMOVEA(Actor *eline);
extern s32  opHandler_FMOVEP(Actor *eline);
extern s32  opHandler_RMOVE(Actor *eline);
extern s32  opHandler_RMOVEA(Actor *eline);
extern s32  opHandler_RPMOVEA(Actor *eline);
extern s32  opHandler_RCMOVE(Actor *eline);
extern s32  opHandler_RFMOVE(Actor *eline);
extern s32  opHandler_MOVESYNC(Actor *eline);
extern s32  opHandler_MOVECANCEL(Actor *eline);
extern s32  opHandler_PMOVECANCEL(Actor *eline);
extern s32  opHandler_MOVEFLUSH(Actor *eline);
extern s32  opHandler_MLIMIT(Actor *eline);
extern s32  func_800B76A4(Actor *self);
extern s32  opHandler_MACCEL(Actor *self);
extern void func_800B788C(Actor *self, Actor *target);
extern s32  opHandler_JOIN(Actor *eline);
extern void func_800B7D44(Actor *eline, s32 x, s32 y, s32 z);
extern s32  opHandler_SPLIT(Actor *eline);
extern s32  opHandler_JUMP(Actor *eline, s32 a1);
extern s32  opHandler_JUMP3(Actor *eline, s32 a1);
extern s32  opHandler_PJUMPA(Actor *eline);
extern s32  opHandler_COUNTERCLOCKWISETURN2(Actor *eline);
extern s32  opHandler_LADDERUP(Actor *eline, s32 a1);
extern s32  opHandler_LADDERDOWN(Actor *eline, s32 a1);
extern s32  opHandler_LADDERUP2(Actor *eline, s32 a1);
extern s32  opHandler_LADDERDOWN2(Actor *eline, s32 a1);
extern s32  opHandler_DOFFSET(Actor *eline, s32 a1);
extern s32  opHandler_LOFFSETS(Actor *eline, s32 a1);
extern s32  opHandler_COFFSETS(Actor *eline, s32 a1);
extern s32  opHandler_LOFFSET(Actor *eline, s32 a1);
extern s32  opHandler_COFFSET(Actor *eline, s32 a1);
extern s32  opHandler_OFFSETSYNC(Actor *eline);
extern s32  opHandler_RUNDISABLE(u8 *a0);
extern s32  opHandler_RUNENABLE(u8 *a0);
extern s32  opHandler_INITTRACE(u8 *a0);
extern s32  opHandler_AXISSYNC(Actor *eline);
extern s32  opHandler_AXIS(Actor *eline);
extern s32  opHandler_UNKNOWN4(Actor *eline);
extern s32  opHandler_OPENEYES(Actor *eline);

#endif
