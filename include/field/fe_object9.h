#ifndef FE_OBJECT9_H
#define FE_OBJECT9_H

#include "common.h"
#include "field.h"

extern s32  opHandler_RFACEDIRA(Actor *actor);
extern s32  opHandler_RFACEDIRP(Actor *actor);
extern s32  opHandler_RFACEDIROFF(Actor *actor);
extern s32  opHandler_FACEDIRLIMIT(Actor *actor);
extern s32  opHandler_FACEDIRINIT(Actor *actor);
extern void func_800BB6C8(void);
extern s32  opHandler_FADEIN(void);
extern s32  opHandler_FADEOUT(void);
extern s32  opHandler_DCOLADD(Actor *actor);
extern s32  opHandler_DCOLSUB(Actor *actor);
extern s32  opHandler_TCOLADD(Actor *actor);
extern s32  opHandler_TCOLSUB(Actor *actor);
extern s32  opHandler_FCOLADD(Actor *actor);
extern s32  opHandler_FCOLSUB(Actor *actor);
extern s32  opHandler_COLSYNC(void);
extern s32  opHandler_FADESYNC(void);
extern s32  opHandler_FADENONE(void);
extern s32  opHandler_FADEBLACK(void);
extern s32  opHandler_MESVAR(Actor *actor);
extern s32  opHandler_MESMODE(Actor *actor);
extern s32  opHandler_SETMESSPEED(Actor *actor);
extern s32  opHandler_MESW(Actor *actor);
extern void func_800BC12C(s32 idx, s32 val, u16 *src);
extern s32  opHandler_MES(Actor *actor);
extern void func_800BC258(Rect *r);
extern s32  opHandler_AMESW(Actor *actor);
extern s32  opHandler_AMES(Actor *actor);
extern s32  opHandler_RAMESW(Actor *actor);
extern s32  opHandler_ASK(Actor *e);
extern s32  opHandler_AASK(Actor *e);
extern s32  opHandler_MESSYNC(Actor *e);
extern s32  opHandler_MESFORCUS(Actor *actor);
extern s32  opHandler_WINSIZE(Actor *e);
extern s32  opHandler_WINCLOSE(Actor *e);
extern s32  opHandler_SETBAR(Actor *e);
extern s32  opHandler_DISPBAR(Actor *e);
extern s32  opHandler_BROKEN(Actor *e);
extern s32  opHandler_KILLBAR(Actor *actor);

#endif
