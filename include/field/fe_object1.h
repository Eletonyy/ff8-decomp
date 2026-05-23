#ifndef FE_OBJECT1_H
#define FE_OBJECT1_H

#include "common.h"
#include "field.h"
#include "psxsdk/libgte.h"

/** @brief 12-byte signed integer 3D position (x, y, z). */
typedef struct {
    s32 x;
    s32 y;
    s32 z;
} Vec3i;

/** @brief Animation parameter entry. */
typedef struct {
    /* 0x00 */ u8 pad00[0x09];
    /* 0x09 */ s8 field_09;
    /* 0x0A */ u8 field_0A;
    /* 0x0B */ u8 field_0B;
} AnimParam;

/** @brief 32-byte slot stride for indexing into a particle system buffer. */
typedef struct {
    u8 b[32];
} ParticleBlock;

/**
 * @brief Particle system buffer.
 *
 * Modeled as a flat array of 32-byte slots: the first ~313 slots hold the
 * emitter table and other buffer metadata; particle records overlay the
 * remaining slots starting at slot index 313 (byte offset 0x2720).
 * Casting a slot's address to @c Particle* gives access to that slot's
 * particle data via the absolute-offset view above.
 */
typedef struct {
    ParticleBlock slots[1];
} ParticleSystem;

extern void func_80098934(void);
extern void func_80099124(void);
extern void func_8009912C(void);
extern void func_8009B74C(s16 slotIdx, u16 paramIdx, AnimParam *params, s16 multiplier);
extern void func_8009BB18(void);
extern void func_8009BD50(Eline *e, s16 mode, u8 b9, u8 b8);
extern s16  func_8009D234(s32 a0);
extern s16  func_8009D254(s32 a0);
extern void func_8009DED8(u8 *a0, u8 *a1, u8 *a2);
extern s32  func_8009E468(s16 selfIdx, Vec3i *pos);
extern s32  func_8009E604(Eline *a, Eline *b);
extern void func_800A17A4(u8 *a0);
extern void func_800A1C64(void);
extern void func_800A1CC0(void);
extern void func_800A2EE0(u8 *a0);
extern void func_800A2F28(s32 a0, u8 *a1);
extern void func_800A303C(s16 emIdx, ParticleSystem *sys, s16 *pos, s16 count);
extern void func_800A355C(FieldActor *actor, s32 slot, s32 a2);
extern void func_800A44D8(void);
extern void func_800A4550(s16 a0);
extern s32  func_800A4910(s32 a0, s32 a1, s32 a2, s32 a3);
extern void func_800A59D0();  /* K&R: a0 declared but ignored in body; callers vary 0/1-arg */
extern void func_800A5A14(s16 a0);
extern u8   func_800A5CF8(void);

/* INCLUDE_ASM stubs — bodies still in assembly, signatures unknown.
 * Declared K&R-style; refine when these get decomped to C. */
extern void func_80098314(void);
extern int  func_800983F0();
extern int  func_8009895C();
extern int  func_80099180();
extern int  func_80099348();
extern int  func_8009A0E8();
extern int  func_8009A2BC();
extern int  func_8009A4C0();
extern int  func_8009A7E8();
extern int  func_8009A8E0();
extern int  func_8009A920();
extern int  func_8009AA64();
extern int  func_8009AAC8();
extern int  func_8009AC9C();
extern int  func_8009AEC0();
extern int  func_8009B4A8();
extern int  func_8009BEC8();
extern int  func_8009CEE8();
extern int  func_8009D274();
extern int  func_8009D500();
extern int  func_8009D598();
extern int  func_8009DF18();
extern int  func_8009E338();
extern int  func_8009E660();
extern int  func_8009ECA4();
extern int  func_8009F74C();
extern int  func_8009F7F4();
extern int  func_8009F8D0();
extern int  func_8009F990();
extern int  func_8009FE18();
extern int  func_800A0640();
extern int  func_800A06F0();
extern int  func_800A0D6C();
extern int  func_800A0E54();
extern int  func_800A0EB8();
extern int  func_800A0F34();
extern int  func_800A0FB8();
extern int  func_800A10F4();
extern int  func_800A11E0();
extern int  func_800A1318();
extern int  func_800A15C0();
extern int  func_800A17B8();
extern int  func_800A19B8();
extern int  func_800A1BB8();
extern int  func_800A1CFC();
extern int  func_800A2128();
extern int  func_800A222C();
extern int  func_800A29C0();
extern int  func_800A2A30();
extern int  func_800A2AF8();
extern int  func_800A2D2C();
extern s16  func_800A2EA4(s16 range);
extern int  func_800A2F48();
extern int  func_800A2F70();
extern s16  func_800A2FE0(ParticleSystem *sys);
extern int  func_800A3018();
extern void func_800A327C(Eline *actor, SVECTOR *out);
extern void func_800A3488(Eline *actor, SVECTOR *out);
extern int  func_800A3534();
extern int  func_800A37A8();
extern int  func_800A38B4();
extern int  func_800A39D8();
extern int  func_800A3FE0();
extern int  func_800A42EC();
extern int  func_800A4500();
extern int  func_800A455C();
extern int  func_800A4758();
extern int  func_800A48CC();
extern int  func_800A4934();
extern int  func_800A4C14();
extern int  func_800A5224();
extern int  func_800A5360();
extern int  func_800A553C();
extern int  func_800A5698();
extern int  func_800A5700();
extern int  func_800A5748();
extern int  func_800A5788();
extern int  func_800A5898();
extern int  func_800A5A20();
extern int  func_800A5C9C();
extern int  func_800A5D28();
extern int  func_800A5FA4();
extern int  func_800A6100();
extern int  func_800A62EC();
extern int  func_800A63AC();
extern int  func_800A6A80();
extern int  func_800A7194();
extern int  func_800A7224();
extern int  func_800A736C();
extern int  func_800A74B4();
extern int  func_800A7564();
extern int  func_800A8058();
extern int  func_800A81AC();
extern int  func_800A8CDC();
extern u8  *func_800A8DAC(s32 spatialIdx, s32 cmd, u32 arg, void *out);
extern int  func_800A91C8();
extern int  func_800A9434();
extern void func_800A97E4(s32 spatialIdx, s32 cmd, s32 arg2, s32 arg3);
extern void func_800AA46C(u8 spatialIdx, s32 cmd, s32 arg, s32 arg4);
extern int  func_800AA5F8();
extern int  func_800AA870();
extern int  func_800AA8A0();

#endif
