#ifndef FE_OBJECT9_H
#define FE_OBJECT9_H

#include "common.h"
#include "field.h"

extern s32  opHandler_RFACEDIRA(Actor *eline);
extern s32  opHandler_RFACEDIRP(Actor *eline);
extern s32  opHandler_RFACEDIROFF(Actor *eline);
extern s32  opHandler_FACEDIRLIMIT(Actor *eline);
extern s32  opHandler_FACEDIRINIT(Actor *eline);
extern void func_800BB6C8(void);
extern s32  opHandler_FADEIN(void);
extern s32  opHandler_FADEOUT(void);
extern s32  opHandler_DCOLADD(Actor *eline);
extern s32  opHandler_DCOLSUB(Actor *eline);
extern s32  opHandler_TCOLADD(Actor *eline);
extern s32  opHandler_TCOLSUB(Actor *eline);
extern s32  opHandler_FCOLADD(Actor *eline);
extern s32  opHandler_FCOLSUB(Actor *eline);
extern s32  opHandler_COLSYNC(void);
extern s32  opHandler_FADESYNC(void);
extern s32  opHandler_FADENONE(void);
extern s32  opHandler_FADEBLACK(void);
extern s32  opHandler_MESVAR(Actor *eline);
extern s32  opHandler_MESMODE(Actor *eline);
extern s32  opHandler_SETMESSPEED(Actor *eline);
extern s32  opHandler_MESW(Actor *eline);
extern void func_800BC12C(s32 idx, s32 val, u16 *src);
extern s32  opHandler_MES(Actor *eline);
extern void func_800BC258(Rect *r);
extern s32  opHandler_AMESW(Actor *eline);
extern s32  opHandler_AMES(Actor *eline);
extern s32  opHandler_RAMESW(Actor *eline);
extern s32  opHandler_ASK(Actor *e);
extern s32  opHandler_AASK(Actor *e);
extern s32  opHandler_MESSYNC(Actor *e);
extern s32  opHandler_MESFORCUS(Actor *eline);
extern s32  opHandler_WINSIZE(Actor *e);
extern s32  opHandler_WINCLOSE(Actor *e);
extern s32  opHandler_SETBAR(Actor *e);
extern s32  opHandler_DISPBAR(Actor *e);
extern s32  opHandler_BROKEN(Actor *e);
extern s32  opHandler_KILLBAR(Actor *eline);

#endif
