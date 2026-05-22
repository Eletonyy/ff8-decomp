#ifndef FE_OBJECT5_H
#define FE_OBJECT5_H

#include "common.h"
#include "field.h"

extern s32  opHandler_SEALEDOFF(Eline *e);          /* 0x800B085C  op159 */
extern s32  func_800B08CC(Eline *e);
extern s32  opHandler_HOLD(Eline *e);                /* op131 HOLD */
extern s32  func_800B0A08(Eline *e);
extern s32  func_800B0A7C(Eline *e);
extern s32  func_800B0B04(Eline *e);
extern s32  func_800B0B10(Eline *e);
extern s32  func_800B0B20(Eline *e);
extern s32  func_800B0B2C(Eline *e);
extern s32  opHandler_FOLLOWOFF(Eline *e);                /* op0AB */
extern s32  opHandler_FOLLOWON(Eline *e);                /* op0AC */
extern s32  func_800B0C48(Eline *e);
extern s32  func_800B0C58(Eline *e);
extern s32  func_800B0C64(Eline *e);
extern s32  func_800B0CCC(Eline *e);
extern s32  func_800B0CFC(Eline *e);
extern s32  func_800B0D2C(Eline *e);
extern s32  opHandler_PGETINFO(Eline *e);                /* op070 PGETINFO */
extern s32  func_800B0E68(Eline *e);
extern s32  opHandler_JUNCTION(Eline *e);                /* op??? JUNCTION */
extern s32  func_800B1034(Eline *e);
extern s32  func_800B10F8(Eline *e);
extern s32  opHandler_ACTORMODE(Eline *e);                /* op12D ACTORMODE */
extern s32  opHandler_MOVIEREADY(Eline *e);                /* op0A3 MOVIEREADY */
extern s32  opHandler_MOVIE(Eline *e);                /* op04F MOVIE */
extern void func_800B14C8(void);                    /* MOVIE postlude halve */
extern s32  opHandler_MOVIESYNC(u8 *a0);                  /* op050 MOVIESYNC */
extern s32  opHandler_SPUREADY(Eline *e);                /* op056 SPUREADY */
extern s32  opHandler_SPUSYNC(Eline *e);            /* 0x800B16B0 op164 */
extern s32  func_800B1730(u8 *a0);
extern s32  opHandler_SETVIBRATE(Eline *e);                /* op0A1 SETVIBRATE */
extern s32  opHandler_STOPVIBRATE(u8 *a0);                  /* op0A2 */
extern s32  opHandler_LOADSYNC(Eline *e);
extern s32  opHandler_INITSOUND(void);                    /* op0CF reset SPU vol */
extern s32  opHandler_SETBATTLEMUSIC(Eline *e);     /* 0x800B1870 op0CB */
extern s32  opHandler_MUSICLOAD(Eline *e);                /* op0B5 MUSICLOAD */
extern void func_800B19D4(void);                    /* MUSICCHANGE helper */
extern s32  opHandler_MUSICCHANGE(void);            /* 0x800B1A20 op0B4 */
extern s32  opHandler_MUSICREPLAY(void);                    /* op141 */
extern s32  opHandler_MUSICSKIP(Eline *e);                /* op144 */
extern s32  opHandler_CHOICEMUSIC(Eline *e);                /* op135 */
extern s32  opHandler_CROSSMUSIC(Eline *e);                /* op0BA */
extern s32  opHandler_DUALMUSIC(Eline *e);                /* op0BB */
extern s32  func_800B1DF4(Eline *e);
extern s32  opHandler_MUSICSTOP(Eline *e);          /* 0x800B1E34 op0BF */
extern s32  opHandler_MUSICSTATUS(Eline *e);
extern s32  func_800B1F04(Eline *e);
extern s32  opHandler_MUSICVOL(Eline *e);           /* 0x800B1F48 op0C0 */
extern s32  opHandler_MUSICVOLTRANS(Eline *e);      /* 0x800B1FE0 op0C1 */
extern s32  opHandler_MUSICVOLFADE(Eline *e);                /* op0C2 */
extern s32  opHandler_MUSICVOLSYNC(u8 *a0);
extern void func_800B2188(void);                    /* SPU upload helper */
extern void func_800B21E0(void);                    /* EFFECTLOAD CD-read helper */
extern s32  opHandler_EFFECTLOAD(Eline *e);                /* op0BD EFFECTLOAD */
extern s32  opHandler_EFFECTPLAY(Eline *e);         /* 0x800B22C0 op0BC */

#endif
