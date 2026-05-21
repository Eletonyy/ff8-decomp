#ifndef FE_OBJECT4_H
#define FE_OBJECT4_H

#include "common.h"
#include "field.h"

extern s32  func_800ADC9C(Eline *e);
extern void func_800ADCA4(Eline *e);
extern void func_800ADCD8(Eline *e);
extern void func_800ADD0C(Eline *e);
extern void func_800ADD30(Eline *e);
extern void func_800ADD68(Eline *e);
extern void func_800ADDA0(Eline *e);
extern void func_800ADDD8(Eline *e);
extern void func_800ADE10(Eline *e);
extern void func_800ADE44(Eline *e);
extern void func_800ADE7C(Eline *e);
extern void func_800ADEB0(Eline *e);
extern void func_800ADEE8(Eline *e);
extern void func_800ADF20(Eline *e);
extern void func_800ADF54(Eline *e);
extern void func_800ADF88(Eline *e);
extern void func_800ADFBC(Eline *e);
extern void func_800ADFE0(Eline *e);
extern void func_800AE014(Eline *e);
extern s32  func_800AE048(Eline *eline, s32 opcode);
extern s32  func_800AE080(Eline *e, s32 a1);
extern s32  func_800AE098(Eline *e, s32 a1);
extern s32  func_800AE0DC(Eline *e, s32 a1);
extern s32  func_800AE124(Eline *e, s32 a1);
extern void func_800AE184(Eline *e);
extern s32  func_800AE4C4(Eline *e, s32 a1);
extern s32  func_800AE518(Eline *e, s32 a1);
extern s32  func_800AE574(Eline *e, s32 a1);
extern s32  func_800AE5D4(Eline *e, s32 a1);
extern s32  func_800AE634(Eline *e, s32 a1);
extern s32  func_800AE694(Eline *e, s32 a1);
extern s32  func_800AE6F4(Eline *e, s32 a1);
extern s32  func_800AE754(Eline *e, s32 a1);
extern s32  func_800AE7B4(Eline *e, s32 a1);
extern s32  func_800AE7E4(Eline *e, s32 a1);
extern s32  func_800AE81C(Eline *e, s32 a1);
extern s32  func_800AE854(Eline *e, s32 a1);
extern s32  func_800AE88C(Eline *e, s32 a1);
extern s32  func_800AEEC4(Eline *e);
extern s32  func_800AEECC(Eline *e);
extern s32  func_800AF070(Eline *e);
extern s32  func_800AF0E0(FieldEntity *entity);
extern s32  func_800AF114(Eline *e, s32 a1);
extern s32  func_800AF1AC(Eline *e);
extern s32  func_800AF2C4(Eline *e);
extern s32  func_800AF314(Eline *e);
extern s32  func_800AF364(Eline *e);
extern s32  func_800AF3B4(Eline *e);
extern s32  func_800AF404(Eline *e);
extern s32  func_800AF47C(void);
extern s32  func_800AF4A0(void);
extern s32  func_800AF5A8(Eline *e);
extern s32  func_800AF5B8(Eline *e);
extern s32  func_800AF5C4(Eline *e);
extern s32  func_800AF5D4(Eline *e);
extern s32  func_800AF5E0(Eline *e);
extern s32  func_800AF5F8(Eline *e);
extern s32  func_800B0280(Eline *e);
extern s32  func_800B02A0(Eline *e);
extern s32  func_800B0818(Eline *e);

extern void func_800ADB68(u8 *buf, s32 arg);

/* INCLUDE_ASM stubs — bodies still in assembly, signatures unknown.
 * Declared K&R-style; refine when these get decomped to C. */
extern void func_800ADC04(void);
extern int  func_800AE1AC();
extern s32  func_800AE3A4(s32 value, s32 mode);
extern s32  func_800AE8B4(Eline *e, u8 newGroup, u16 pcIdx);
extern int  func_800AE978();
extern s32  func_800AEA44(Eline *e, s32 a1);
extern s32  func_800AEB0C(Eline *e, s32 a1);
extern s32  func_800AEBD8(Eline *e, s32 a1);
extern s32  func_800AEC78(Eline *e, s32 a1);
extern s32  func_800AED9C(Eline *e, s32 a1);
extern s32  func_800AEED4(Eline *e, s32 a1);
extern s32  func_800AEF4C(Eline *e, s32 a1);
extern s32  func_800AEFE8(Eline *e, s32 bit);
extern s32  func_800AF02C(Eline *e, s32 bit);
extern s32  func_800AF0B4(void);
extern s32  func_800AF120(Eline *e);
extern s32  func_800AF224(Eline *e, s32 a1);
extern s32  func_800AF274(Eline *e, s32 a1);
extern s32  func_800AF444();
extern s32  func_800AF4C4(Eline *e);
extern s32  func_800AF610(void);
extern s32  func_800AF7E4(void);
extern s32  func_800AFD20(Eline *e);
extern s32  func_800AFD68(Eline *e);
extern s32  func_800AFE24(Eline *e);
extern s32  func_800AFF64(Eline *e);
extern s32  func_800B002C(Eline *e);
extern s32  func_800B0124(Eline *e);
extern s32  func_800B0304(Eline *e);
extern int  func_800B0344();
extern int  func_800B0444();
extern s32  func_800B0570(Eline *e);
extern s32  func_800B0638(Eline *e, s32 a1);
extern s32  func_800B06D0(Eline *e);
extern s32  func_800B0784(Eline *e, s32 a1);

#endif
