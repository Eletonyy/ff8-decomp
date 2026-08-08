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
extern s32  opHandler_DCOLADD(ScriptContext *context);
extern s32  opHandler_DCOLSUB(ScriptContext *context);
extern s32  opHandler_TCOLADD(ScriptContext *context);
extern s32  opHandler_TCOLSUB(ScriptContext *context);
extern s32  opHandler_FCOLADD(ScriptContext *context);
extern s32  opHandler_FCOLSUB(ScriptContext *context);
extern s32  opHandler_COLSYNC(void);
extern s32  opHandler_FADESYNC(void);
extern s32  opHandler_FADENONE(void);
extern s32  opHandler_FADEBLACK(void);
extern s32  opHandler_MESVAR(ScriptContext *context);
extern s32  opHandler_MESMODE(ScriptContext *context);
extern s32  opHandler_SETMESSPEED(ScriptContext *context);
extern s32  opHandler_MESW(ScriptContext *context);
extern void func_800BC12C(s32 idx, s32 val, u16 *src);
extern s32  opHandler_MES(ScriptContext *context);
extern void func_800BC258(Rect *r);
extern s32  opHandler_AMESW(ScriptContext *context);
extern s32  opHandler_AMES(ScriptContext *context);
extern s32  opHandler_RAMESW(ScriptContext *context);
extern s32  opHandler_ASK(Actor *actor);
extern s32  opHandler_AASK(Actor *actor);
extern s32  opHandler_MESSYNC(ScriptContext *context);
extern s32  opHandler_MESFORCUS(ScriptContext *context);
extern s32  opHandler_WINSIZE(ScriptContext *context);
extern s32  opHandler_WINCLOSE(ScriptContext *context);
extern s32  opHandler_SETBAR(ScriptContext *context);
extern s32  opHandler_DISPBAR(ScriptContext *context);
extern s32  opHandler_BROKEN(ScriptContext *context);
extern s32  opHandler_KILLBAR(ScriptContext *context);

#endif
