#ifndef FE_OBJECT7_H
#define FE_OBJECT7_H

#include "common.h"
#include "field.h"

extern s32  opHandler_WHERECARD(Eline *eline);
extern s32  opHandler_CARDGAME(Eline *eline);
extern u8  *func_800B574C(u8 *src);
extern u8  *func_800B578C(s32 search, s32 offset);
extern u8  *func_800B57E8(s32 maxCount, s32 abilityId);
extern s32  func_800B5990(void);
extern s32  opHandler_DRAWPOINT(Eline *eline);
extern s32  opHandler_SETDRAWPOINT(Eline *eline);
extern s32  opHandler_UNKNOWN10(Eline *eline);
extern s32  opHandler_PARTICLEON(Eline *eline);
extern s32  opHandler_PARTICLEOFF(Eline *eline);
extern s32  opHandler_PARTICLESET(Eline *eline);
extern s32  opHandler_SETWITCH(Eline *eline);
extern s32  opHandler_SETODIN(Eline *eline);
extern s32  func_800B6420(Eline *eline);
extern s32  opHandler_SETPLACE(Eline *eline);
extern s32  opHandler_BATTLEMODE(Eline *eline);
extern s32  opHandler_BATTLE(Eline *eline);
extern s32  opHandler_BATTLERESULT(Eline *eline);
extern s32  opHandler_BATTLEON(void);
extern s32  opHandler_BATTLEOFF(void);
extern s32  opHandler_BATTLECUT(Eline *eline);
extern s32  opHandler_GAMEOVER(Eline *eline);
extern s32  opHandler_ENDING(Eline *eline);
extern s32  opHandler_DISC(Eline *eline);
extern void func_800B663C(Eline *eline);
extern void func_800B66A8(Eline *eline);
extern void func_800B6738(Eline *eline);
extern void func_800B67F4(Eline *eline);
extern void func_800B6854(Eline *eline);
extern s32  opHandler_MSPEED(Eline *eline);
extern s32  opHandler_MOVE(Eline *eline);
extern s32  opHandler_MOVEA(Eline *eline);
extern s32  opHandler_PMOVEA(Eline *eline);
extern s32  opHandler_CMOVE(Eline *eline);
extern s32  opHandler_FMOVE(Eline *eline);
extern s32  opHandler_FMOVEA(Eline *eline);
extern s32  opHandler_FMOVEP(Eline *eline);
extern s32  opHandler_RMOVE(Eline *eline);
extern s32  opHandler_RMOVEA(Eline *eline);
extern s32  opHandler_RPMOVEA(Eline *eline);
extern s32  opHandler_RCMOVE(Eline *eline);
extern s32  opHandler_RFMOVE(Eline *eline);
extern s32  opHandler_MOVESYNC(Eline *eline);
extern s32  opHandler_MOVECANCEL(Eline *eline);
extern s32  opHandler_PMOVECANCEL(Eline *eline);
extern s32  opHandler_MOVEFLUSH(Eline *eline);
extern s32  opHandler_MLIMIT(Eline *eline);
extern s32  func_800B76A4(Eline *self);
extern s32  opHandler_MACCEL(Eline *self);
extern void func_800B788C(Eline *self, Eline *target);
extern s32  opHandler_JOIN(Eline *eline);
extern void func_800B7D44(Eline *eline, s32 x, s32 y, s32 z);
extern s32  opHandler_SPLIT(Eline *eline);
extern s32  opHandler_JUMP(Eline *eline, s32 a1);
extern s32  opHandler_JUMP3(Eline *eline, s32 a1);
extern s32  opHandler_PJUMPA(Eline *eline);
extern s32  opHandler_COUNTERCLOCKWISETURN2(Eline *eline);
extern s32  opHandler_LADDERUP(Eline *eline, s32 a1);
extern s32  opHandler_LADDERDOWN(Eline *eline, s32 a1);
extern s32  opHandler_LADDERUP2(Eline *eline, s32 a1);
extern s32  opHandler_LADDERDOWN2(Eline *eline, s32 a1);
extern s32  opHandler_DOFFSET(Eline *eline, s32 a1);
extern s32  opHandler_LOFFSETS(Eline *eline, s32 a1);
extern s32  opHandler_COFFSETS(Eline *eline, s32 a1);
extern s32  opHandler_LOFFSET(Eline *eline, s32 a1);
extern s32  opHandler_COFFSET(Eline *eline, s32 a1);
extern s32  opHandler_OFFSETSYNC(Eline *eline);
extern s32  opHandler_RUNDISABLE(u8 *a0);
extern s32  opHandler_RUNENABLE(u8 *a0);
extern s32  opHandler_INITTRACE(u8 *a0);
extern s32  opHandler_AXISSYNC(Eline *eline);
extern s32  opHandler_AXIS(Eline *eline);
extern s32  opHandler_UNKNOWN4(Eline *eline);
extern s32  opHandler_OPENEYES(Eline *eline);

#endif
