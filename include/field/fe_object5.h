#ifndef FE_OBJECT5_H
#define FE_OBJECT5_H

#include "common.h"
#include "field.h"

extern s32 opHandler_SEALEDOFF(Eline *e);
extern s32 opHandler_SETBATTLEMUSIC(Eline *e);
extern s32 func_800B08CC(u8 *a0);
extern s32 func_800B0A08(u8 *a0);
extern s32 func_800B0A7C(u8 *a0);
extern s32 func_800B0B04(u8 *a0);
extern s32 func_800B0B10(u8 *a0);
extern s32 func_800B0B20(u8 *a0);
extern s32 func_800B0B2C(u8 *a0);
extern s32 func_800B0C48(u8 *a0);
extern s32 func_800B0C58(u8 *a0);
extern s32 func_800B0C64(u8 *a0);
extern s32 func_800B0CCC(u8 *a0);
extern s32 func_800B0CFC(u8 *a0);
extern s32 func_800B0D2C(u8 *a0);
extern s32 func_800B0E68(u8 *a0);
extern s32 func_800B158C(u8 *a0);
extern s32 func_800B1730(u8 *a0);
extern s32 func_800B1738(u8 *a0);
extern s32 func_800B17A0(u8 *a0);
extern s32 func_800B17A8(u8 *a0);
extern s32 func_800B1DF4(u8 *a0);
extern s32 func_800B1ED4(u8 *a0);
extern s32 func_800B1F04(u8 *a0);
extern s32 func_800B2158(u8 *a0);

/* INCLUDE_ASM stubs — bodies still in assembly, signatures unknown.
 * Declared K&R-style; refine when these get decomped to C. */
extern int  func_800B0924();
extern s32  func_800B0B3C(Eline *e);
extern s32  func_800B0BE4(Eline *e);
extern int  func_800B0D94();
extern int  func_800B0EBC();
extern int  func_800B1034();
extern int  func_800B10F8();
extern int  func_800B11BC();
extern int  func_800B12A4();
extern int  func_800B13EC();
extern int  func_800B14C8();
extern int  func_800B15BC();
extern s32  opHandler_SPUSYNC(Eline *e);
extern s32  func_800B17D8(void);
extern int  func_800B18A4();
extern void func_800B19D4(void);
extern s32  opHandler_MUSICCHANGE(void);
extern s32  func_800B1AA0(void);
extern s32  func_800B1B10(Eline *e);
extern int  func_800B1BB8();
extern int  func_800B1C7C();
extern int  func_800B1D40();
extern s32  opHandler_MUSICSTOP(Eline *e);
extern s32  opHandler_MUSICVOL(Eline *e);
extern s32  opHandler_MUSICVOLTRANS(Eline *e);
extern int  func_800B2090();
extern void func_800B2188(void);
extern int  func_800B21E0();
extern int  func_800B2248();
extern s32  opHandler_EFFECTPLAY(Eline *e);

#endif
