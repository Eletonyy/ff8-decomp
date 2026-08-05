#ifndef FE_OBJECT5_H
#define FE_OBJECT5_H

#include "common.h"
#include "field.h"

extern s32  opHandler_SEALEDOFF(Actor *e);          /* 0x800B085C  op159 */
extern s32  opHandler_RESETGF(Actor *e);
extern s32  opHandler_HOLD(Actor *e);                /* op131 HOLD */
extern s32  opHandler_SHOW(Actor *e);
extern s32  opHandler_HIDE(Actor *e);
extern s32  opHandler_TALKON(Actor *e);
extern s32  opHandler_TALKOFF(Actor *e);
extern s32  opHandler_PUSHON(Actor *e);
extern s32  opHandler_PUSHOFF(Actor *e);
extern s32  opHandler_FOLLOWOFF(Actor *e);                /* op0AB */
extern s32  opHandler_FOLLOWON(Actor *e);                /* op0AC */
extern s32  opHandler_THROUGHON(Actor *e);
extern s32  opHandler_THROUGHOFF(Actor *e);
extern s32  opHandler_ISTOUCH(Actor *e);
extern s32  opHandler_TALKRADIUS(Actor *e);
extern s32  opHandler_PUSHRADIUS(Actor *e);
extern s32  opHandler_GETINFO(Actor *e);
extern s32  opHandler_PGETINFO(Actor *e);                /* op070 PGETINFO */
extern s32  opHandler_WHOAMI(Actor *e);
extern s32  opHandler_JUNCTION(Actor *e);                /* op??? JUNCTION */
extern s32  opHandler_COPYINFO(Actor *e);
extern s32  opHandler_PCOPYINFO(Actor *e);
extern s32  opHandler_ACTORMODE(Actor *e);                /* op12D ACTORMODE */
extern s32  opHandler_MOVIEREADY(Actor *e);                /* op0A3 MOVIEREADY */
extern s32  opHandler_MOVIE(Actor *e);                /* op04F MOVIE */
extern void func_800B14C8(void);                    /* MOVIE postlude halve */
extern s32  opHandler_MOVIESYNC(u8 *a0);                  /* op050 MOVIESYNC */
extern s32  opHandler_SPUREADY(Actor *e);                /* op056 SPUREADY */
extern s32  opHandler_SPUSYNC(Actor *e);            /* 0x800B16B0 op164 */
extern s32  opHandler_MOVIECUT(u8 *a0);
extern s32  opHandler_SETVIBRATE(Actor *e);                /* op0A1 SETVIBRATE */
extern s32  opHandler_STOPVIBRATE(u8 *a0);                  /* op0A2 */
extern s32  opHandler_LOADSYNC(Actor *e);
extern s32  opHandler_INITSOUND(void);                    /* op0CF reset SPU vol */
extern s32  opHandler_SETBATTLEMUSIC(Actor *e);     /* 0x800B1870 op0CB */
extern s32  opHandler_MUSICLOAD(Actor *e);                /* op0B5 MUSICLOAD */
extern void func_800B19D4(void);                    /* MUSICCHANGE helper */
extern s32  opHandler_MUSICCHANGE(void);            /* 0x800B1A20 op0B4 */
extern s32  opHandler_MUSICREPLAY(void);                    /* op141 */
extern s32  opHandler_MUSICSKIP(Actor *e);                /* op144 */
extern s32  opHandler_CHOICEMUSIC(Actor *e);                /* op135 */
extern s32  opHandler_CROSSMUSIC(Actor *e);                /* op0BA */
extern s32  opHandler_DUALMUSIC(Actor *e);                /* op0BB */
extern s32  opHandler_KEYSIGHNCHANGE(Actor *e);
extern s32  opHandler_MUSICSTOP(Actor *e);          /* 0x800B1E34 op0BF */
extern s32  opHandler_MUSICSTATUS(Actor *e);
extern s32  opHandler_OP16F(Actor *e);
extern s32  opHandler_MUSICVOL(Actor *e);           /* 0x800B1F48 op0C0 */
extern s32  opHandler_MUSICVOLTRANS(Actor *e);      /* 0x800B1FE0 op0C1 */
extern s32  opHandler_MUSICVOLFADE(Actor *e);                /* op0C2 */
extern s32  opHandler_MUSICVOLSYNC(u8 *a0);
extern void func_800B2188(void);                    /* SPU upload helper */
extern void func_800B21E0(void);                    /* EFFECTLOAD CD-read helper */
extern s32  opHandler_EFFECTLOAD(Actor *e);                /* op0BD EFFECTLOAD */
extern s32  opHandler_EFFECTPLAY(Actor *e);         /* 0x800B22C0 op0BC */

#endif
