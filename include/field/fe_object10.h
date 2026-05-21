#ifndef FE_OBJECT10_H
#define FE_OBJECT10_H

#include "common.h"
#include "field.h"

extern s32  func_800BD1F4(FieldEntity *entity);
extern void func_800BD250(s32 a0, u8 *a1);
extern s32  func_800BD2AC(FieldEntity *entity);
extern s32  func_800BD318(FieldEntity *entity);
extern void updateSeedLevel(void);
extern void func_800BD5E0(void);
extern void func_800BD64C(void);
extern void func_800BD6EC(void);
extern void func_800BD794(void);
extern u8   func_800BE264(void);
extern s32  func_800BE274(void);
extern void func_800BE2AC(void);
extern void func_800BE2DC(void);
extern void func_800BE30C(u8 *header);
extern s32  func_800BE44C(s32 val);
extern void func_800BF230(FieldEntity *entity);
extern void func_800BF4A4(void);

/* INCLUDE_ASM stubs — bodies still in assembly, signatures unknown.
 * Declared K&R-style; refine when these get decomped to C. */
extern int  func_800BD804();
extern int  func_800BD9C4();
extern int  func_800BE36C();
extern int  func_800BE4B0();
extern int  func_800BE5E4();
extern int  func_800BE7F4();
extern int  func_800BE924();
extern int  func_800BEA84();
extern int  func_800BEBD0();
extern int  func_800BF080();
extern void func_800BF28C(s32 a0);

#endif
