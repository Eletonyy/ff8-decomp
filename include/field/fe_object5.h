#ifndef FE_OBJECT5_H
#define FE_OBJECT5_H

#include "common.h"
#include "field.h"

extern s32 opHandler_SEALEDOFF(Eline *e);
extern s32 opHandler_SETBATTLEMUSIC(Eline *e);
extern s32 func_800B08CC(Eline *e);
extern s32 func_800B0A08(Eline *e);
extern s32 func_800B0A7C(Eline *e);
extern s32 func_800B0B04(Eline *e);
extern s32 func_800B0B10(Eline *e);
extern s32 func_800B0B20(Eline *e);
extern s32 func_800B0B2C(Eline *e);
extern s32 func_800B0C48(Eline *e);
extern s32 func_800B0C58(Eline *e);
extern s32 func_800B0C64(Eline *e);
extern s32 func_800B0CCC(Eline *e);
extern s32 func_800B0CFC(Eline *e);
extern s32 func_800B0D2C(Eline *e);
extern s32 func_800B0E68(Eline *e);
extern s32 func_800B158C(u8 *a0);
extern s32 func_800B1730(u8 *a0);
extern s32 func_800B1738(Eline *e);
extern s32 func_800B17A0(u8 *a0);
extern s32 func_800B17A8(Eline *e);
extern s32 func_800B1DF4(Eline *e);
extern s32 func_800B1ED4(Eline *e);
extern s32 func_800B1F04(Eline *e);
extern s32 func_800B2158(u8 *a0);

/* INCLUDE_ASM stubs — bodies still in assembly, signatures unknown.
 * Declared K&R-style; refine when these get decomped to C. */
extern s32  func_800B0924(Eline *e);
extern s32  func_800B0B3C(Eline *e);
extern s32  func_800B0BE4(Eline *e);
extern s32  func_800B0D94(Eline *e);
extern s32  func_800B0EBC(Eline *e);
extern s32  func_800B1034(Eline *e);
extern s32  func_800B10F8(Eline *e);
extern s32  func_800B11BC(Eline *e);
extern s32  func_800B12A4(Eline *e);
extern s32  func_800B13EC(Eline *e);
extern void func_800B14C8(void);
extern int  func_800B15BC();
extern s32  opHandler_SPUSYNC(Eline *e);
extern s32  func_800B17D8(void);
extern int  func_800B18A4();
extern void func_800B19D4(void);
extern s32  opHandler_MUSICCHANGE(void);
extern s32  func_800B1AA0(void);
extern s32  func_800B1B10(Eline *e);
extern s32  func_800B1BB8(Eline *e);
extern s32  func_800B1C7C(Eline *e);
extern s32  func_800B1D40(Eline *e);
extern s32  opHandler_MUSICSTOP(Eline *e);
extern s32  opHandler_MUSICVOL(Eline *e);
extern s32  opHandler_MUSICVOLTRANS(Eline *e);
extern s32  func_800B2090(Eline *e);
extern void func_800B2188(void);
extern int  func_800B21E0();
extern int  func_800B2248();
extern s32  opHandler_EFFECTPLAY(Eline *e);

#endif
