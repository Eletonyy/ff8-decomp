#ifndef FE_OBJECT7_H
#define FE_OBJECT7_H

#include "common.h"
#include "field.h"

extern s32  opHandler_WHERECARD(Actor *actor);
extern s32  opHandler_CARDGAME(Actor *actor);
extern u8  *func_800B574C(u8 *src);
extern u8  *func_800B578C(s32 search, s32 offset);
extern u8  *func_800B57E8(s32 maxCount, s32 abilityId);
extern s32  func_800B5990(void);
extern s32  opHandler_DRAWPOINT(Actor *actor);
extern s32  opHandler_SETDRAWPOINT(Actor *actor);
extern s32  opHandler_UNKNOWN10(Actor *actor);
extern s32  opHandler_PARTICLEON(Actor *actor);
extern s32  opHandler_PARTICLEOFF(Actor *actor);
extern s32  opHandler_PARTICLESET(Actor *actor);
extern s32  opHandler_SETWITCH(Actor *actor);
extern s32  opHandler_SETODIN(Actor *actor);
extern s32  func_800B6420(Actor *actor);
extern s32  opHandler_SETPLACE(Actor *actor);
extern s32  opHandler_BATTLEMODE(Actor *actor);
extern s32  opHandler_BATTLE(Actor *actor);
extern s32  opHandler_BATTLERESULT(Actor *actor);
extern s32  opHandler_BATTLEON(void);
extern s32  opHandler_BATTLEOFF(void);
extern s32  opHandler_BATTLECUT(Actor *actor);
extern s32  opHandler_GAMEOVER(Actor *actor);
extern s32  opHandler_ENDING(Actor *actor);
extern s32  opHandler_DISC(Actor *actor);
extern void func_800B663C(Actor *actor);
extern void func_800B66A8(Actor *actor);
extern void func_800B6738(Actor *actor);
extern void func_800B67F4(Actor *actor);
extern void func_800B6854(Actor *actor);
extern s32  opHandler_MSPEED(Actor *actor);
extern s32  opHandler_MOVE(Actor *actor);
extern s32  opHandler_MOVEA(Actor *actor);
extern s32  opHandler_PMOVEA(Actor *actor);
extern s32  opHandler_CMOVE(Actor *actor);
extern s32  opHandler_FMOVE(Actor *actor);
extern s32  opHandler_FMOVEA(Actor *actor);
extern s32  opHandler_FMOVEP(Actor *actor);
extern s32  opHandler_RMOVE(Actor *actor);
extern s32  opHandler_RMOVEA(Actor *actor);
extern s32  opHandler_RPMOVEA(Actor *actor);
extern s32  opHandler_RCMOVE(Actor *actor);
extern s32  opHandler_RFMOVE(Actor *actor);
extern s32  opHandler_MOVESYNC(Actor *actor);
extern s32  opHandler_MOVECANCEL(Actor *actor);
extern s32  opHandler_PMOVECANCEL(Actor *actor);
extern s32  opHandler_MOVEFLUSH(Actor *actor);
extern s32  opHandler_MLIMIT(Actor *actor);
extern s32  func_800B76A4(Actor *actor);
extern s32  opHandler_MACCEL(Actor *actor);
extern void func_800B788C(Actor *self, Actor *target);
extern s32  opHandler_JOIN(Actor *actor);
extern void func_800B7D44(Actor *actor, s32 x, s32 y, s32 z);
extern s32  opHandler_SPLIT(Actor *actor);
extern s32  opHandler_JUMP(Actor *actor, s32 a1);
extern s32  opHandler_JUMP3(Actor *actor, s32 a1);
extern s32  opHandler_PJUMPA(Actor *actor);
extern s32  opHandler_COUNTERCLOCKWISETURN2(Actor *actor);
extern s32  opHandler_LADDERUP(Actor *actor, s32 a1);
extern s32  opHandler_LADDERDOWN(Actor *actor, s32 a1);
extern s32  opHandler_LADDERUP2(Actor *actor, s32 a1);
extern s32  opHandler_LADDERDOWN2(Actor *actor, s32 a1);
extern s32  opHandler_DOFFSET(Actor *actor, s32 a1);
extern s32  opHandler_LOFFSETS(Actor *actor, s32 a1);
extern s32  opHandler_COFFSETS(Actor *actor, s32 a1);
extern s32  opHandler_LOFFSET(Actor *actor, s32 a1);
extern s32  opHandler_COFFSET(Actor *actor, s32 a1);
extern s32  opHandler_OFFSETSYNC(Actor *actor);
extern s32  opHandler_RUNDISABLE(u8 *a0);
extern s32  opHandler_RUNENABLE(u8 *a0);
extern s32  opHandler_INITTRACE(u8 *a0);
extern s32  opHandler_AXISSYNC(Actor *actor);
extern s32  opHandler_AXIS(Actor *actor);
extern s32  opHandler_UNKNOWN4(Actor *actor);
extern s32  opHandler_OPENEYES(Actor *actor);

#endif
