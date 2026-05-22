#ifndef FE_OBJECT9_H
#define FE_OBJECT9_H

#include "common.h"
#include "field.h"

extern s32  opHandler_RFACEDIRA(Eline *eline);
extern s32  opHandler_RFACEDIRP(Eline *eline);
extern s32  opHandler_RFACEDIROFF(Eline *eline);
extern s32  opHandler_FACEDIRLIMIT(Eline *eline);
extern s32  opHandler_FACEDIRINIT(Eline *eline);
extern void func_800BB6C8(void);
extern s32  opHandler_FADEIN(void);
extern s32  opHandler_FADEOUT(void);
extern s32  opHandler_DCOLADD(Eline *eline);
extern s32  opHandler_DCOLSUB(Eline *eline);
extern s32  opHandler_TCOLADD(Eline *eline);
extern s32  opHandler_TCOLSUB(Eline *eline);
extern s32  opHandler_FCOLADD(Eline *eline);
extern s32  opHandler_FCOLSUB(Eline *eline);
extern s32  opHandler_COLSYNC(void);
extern s32  opHandler_FADESYNC(void);
extern s32  opHandler_FADENONE(void);
extern s32  opHandler_FADEBLACK(void);
extern s32  opHandler_MESVAR(Eline *eline);
extern s32  opHandler_MESMODE(Eline *eline);
extern s32  opHandler_SETMESSPEED(Eline *eline);
extern s32  opHandler_MESW(Eline *eline);
extern void func_800BC12C(s32 idx, s32 val, u16 *src);
extern s32  opHandler_MES(Eline *eline);
extern void func_800BC258(Rect *r);
extern s32  opHandler_AMESW(Eline *eline);
extern s32  opHandler_AMES(Eline *eline);
extern s32  opHandler_RAMESW(Eline *eline);
extern s32  opHandler_ASK(Eline *e);
extern s32  opHandler_AASK(Eline *e);
extern s32  opHandler_MESSYNC(Eline *e);
extern s32  opHandler_MESFORCUS(Eline *eline);
extern s32  opHandler_WINSIZE(Eline *e);
extern s32  opHandler_WINCLOSE(Eline *e);
extern s32  opHandler_SETBAR(Eline *e);
extern s32  opHandler_DISPBAR(Eline *e);
extern s32  opHandler_BROKEN(Eline *e);
extern s32  opHandler_KILLBAR(Eline *eline);

#endif
