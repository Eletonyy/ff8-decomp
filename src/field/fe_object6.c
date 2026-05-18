#include "common.h"
#include "field.h"
#include "gamestate.h"
#include "battle.h"
#include "psxsdk/libgte.h"

extern SeedState *g_seedState;
extern u8 D_80070652;
extern u8 D_800704CA;
extern u8 D_8007064A;
extern u8 D_8007064B;
extern u8 D_8007064D;
extern u8 D_8007064F[];
extern u8 D_8007065C[];

extern BattleFieldEntity *D_80085224;
extern u8 *D_800D5EA4;
extern u8 *func_8003974C(u8 *base, s32 idx);
extern s32 sndPlayBankSfx(s32 a0, s32 a1, s32 a2, s32 a3);
extern void sndCmd21(s32 a0, s32 a1);
extern s32 func_800131A8(void);
extern u8 *func_800A8DAC(s32 entityIdx, s32 mode, void *buf, s32 flag);
extern void func_800406A4(u8 *p);
extern void func_80040734(u8 *p);
extern s32 func_80040DE4(SVECTOR *v0, s32 *sxy, s32 *p, s32 *flag);
extern Eline *D_80085230[];
extern void setCameraShakeParams(s32 a, s32 b);
extern void setCameraVibrateState(s32 enable);
extern u8 D_8007064E;
extern void func_800A97E4(s32 a, s32 b, s32 c, s32 d);

/**
 * Pops 3 stack values (target, volume, pan), looks up an SFX entry in
 * the dispatcher-keyed bank table @c D_800D5EA4, and plays the resulting
 * bank SFX with the popped parameters (volume masked to 8 bits, pan to 7).
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied SFX bank index.
 * @return 2 (continue processing).
 */
s32 func_800B2348(Eline *eline, s32 a1) {
    s32 val1, val2, val3;
    val1 = POP(eline);
    val2 = POP(eline);
    val3 = POP(eline);
    val2 &= 0xFF;
    val3 &= 0x7F;
    sndPlayBankSfx((s32)func_8003974C(D_800D5EA4, a1), val1, val2, val3);
    return 2;
}

/**
 * Pops a parameter and calls sndSetMasterVolume, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B23F4(Eline *eline) {
    sndSetMasterVolume(POP(eline));
    return 2;
}

/**
 * Pops two parameters from the stack, shifts the first left by 1,
 * and calls sndSetChannelVolume with them.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2434(Eline *eline) {
    s32 val1, val2;
    val1 = POP(eline);
    val2 = POP(eline);
    sndSetChannelVolume(val2, val1 << 1);
    return 2;
}

/**
 * Pops a parameter and calls sndSeqSetTempo, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B248C(Eline *eline) {
    sndSeqSetTempo(POP(eline));
    return 2;
}

/**
 * Pops two parameters from the stack, shifts the first left by 1,
 * and calls sndSeqSetChannelTempo.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B24CC(Eline *eline) {
    s32 val1, val2;
    val1 = POP(eline);
    val2 = POP(eline);
    sndSeqSetChannelTempo(val2, val1 << 1);
    return 2;
}

/**
 * Pops two parameters from the stack and calls sndSeqPlay7bit(0, val2, val1).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2524(Eline *eline) {
    s32 val1, val2;
    val1 = POP(eline);
    val2 = POP(eline);
    sndSeqPlay7bit(0, val2, val1);
    return 2;
}

/**
 * Pops three parameters from the stack and calls
 * sndSeqPlayPan7bit(0, val3, val2 << 1, val1).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B257C(Eline *eline) {
    s32 val1, val2, val3;
    val1 = POP(eline);
    val2 = POP(eline);
    val3 = POP(eline);
    sndSeqPlayPan7bit(0, val3, val2 << 1, val1);
    return 2;
}

/**
 * Pops two parameters from the stack and calls sndSeqPlay8bit(0, val2, val1).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B25F0(Eline *eline) {
    s32 val1, val2;
    val1 = POP(eline);
    val2 = POP(eline);
    sndSeqPlay8bit(0, val2, val1);
    return 2;
}

/**
 * Pops three parameters from the stack and calls
 * sndSeqPlayPan8bit(0, val3, val2 << 1, val1).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2648(Eline *eline) {
    s32 val1, val2, val3;
    val1 = POP(eline);
    val2 = POP(eline);
    val3 = POP(eline);
    sndSeqPlayPan8bit(0, val3, val2 << 1, val1);
    return 2;
}

/**
 * Peek the top stack value as an SFX target; if the entity is the active
 * script slot, dispatch @c sndCmd21(0, top). If the SPU is still busy
 * with a matching channel (queried via @c func_800131A8), keep the stack
 * intact and return 1 (wait). Otherwise pop the value and return 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 once the SPU is free, 1 while still waiting.
 */
s32 func_800B26BC(Eline *eline) {
    s32 top = PEEK(eline);
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        sndCmd21(0, top);
    }
    if ((func_800131A8() & top) != 0) {
        return 1;
    }
    eline->stackPtr--;
    return 2;
}

/**
 * Pops a parameter, calls func_80013210, stores result at offset 0x140.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B273C(Eline *eline) {
    eline->resultSlots[0] = func_80013210(POP(eline));
    return 2;
}

/** @brief Pop value, bitwise-NOT, store to D_8007065C. Returns 2. */
s32 func_800B2790(Eline *eline) {
    *(s32 *)D_8007065C = ~POP(eline);
    return 2;
}

/**
 * Compute the on-screen projection of entity @p entityIdx through the
 * GTE. Loads the entity's rotation and translation matrices (via the
 * per-entity matrix buffer from @c func_800A8DAC), projects the local
 * origin @c (0,0,0), writes the perspective parameter into @p *out, and
 * narrows the projected packed @c SXY pair to its low @c s16 (X only,
 * sign-extended) at @p *sxy.
 *
 * @param entityIdx Entity slot whose matrices to set up.
 * @param sxy       In/out: receives the projected screen X (low s16,
 *                  sign-extended back to s32).
 * @param out       Output: GTE perspective Z parameter.
 */
void func_800B27C4(s32 entityIdx, s32 *sxy, s32 *out) {
    SVECTOR v;
    s32 p, flag;
    func_800406A4(func_800A8DAC(entityIdx, 0x1F, 0, 0));
    func_80040734(func_800A8DAC(entityIdx, 0x1F, 0, 0));
    v.vx = 0;
    v.vy = 0;
    v.vz = 0;
    *out = func_80040DE4(&v, sxy, &p, &flag);
    *sxy = (s16)*sxy;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B2864);

/**
 * Pops a byte from the stack, stores it at offset 0x188, and stores
 * the second argument at offset 0x189. Returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1 Value to store at offset 0x189.
 * @return 2 (continue processing).
 */
s32 func_800B2A40(Eline *eline, s32 a1) {
    eline->unk188 = POP_BYTE(eline);
    eline->unk189 = a1;
    return 2;
}

/**
 * Broadcast the current Eline's @c unk188 / @c unk189 script parameter
 * bytes to every active @c BattleFieldEntity slot. Iterates over the
 * @c D_80085224 array (count @c D_80085388, stride 0x264) and writes
 * the same pair into each entry's mirror fields at +0x188 / +0x189.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2A70(Eline *eline) {
    s32 i;
    BattleFieldEntity *p = D_80085224;
    for (i = 0; i < D_80085388; i++) {
        p->unk188 = eline->unk188;
        p->unk189 = eline->unk189;
        p++;
    }
    return 2;
}

/**
 * Sets bit 0x80 in the flags at offset 0x160, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2AC0(Eline *eline) {
    eline->flags |= 0x80;
    return 2;
}

/**
 * Clears bit 0x80 in the flags at offset 0x160, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2AD8(Eline *eline) {
    eline->flags &= ~0x80;
    return 2;
}

/**
 * Clear bit 0x80 in the flags word of every active @c BattleFieldEntity
 * slot (count @c D_80085388, stride 0x264 at @c D_80085224). Used by a
 * script opcode to reset a per-entity activation flag across the party.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B2AF0(Eline *eline) {
    s32 i;
    BattleFieldEntity *p = D_80085224;
    for (i = 0; i < D_80085388; i++) {
        p->flags &= ~0x80;
        p++;
    }
    return 2;
}

/**
 * Sets bytes at offsets 0x188 and 0x189 to 0xFF, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2B34(Eline *eline) {
    *(s8 *)&eline->unk188 = -1;
    *(s8 *)&eline->unk189 = -1;
    return 2;
}

/**
 * Restore the @c unk188 / @c unk189 script-parameter halfword from one of
 * two saved snapshots, conditioned on flag bits set in @c eline->flags:
 *  - bit 0x2000: copy from @c unk18C and return (no flag mutation).
 *  - bit 0x8000: copy from @c unk18E, clear high-mode bits in flags
 *    (mask @c 0xFFFF07FF), set the @c 0x1000 marker, fall through.
 * Then set the @c 0x800 marker regardless.
 *
 * @param eline Pointer to the Eline event-script context.
 */
void func_800B2B48(Eline *eline) {
    s32 flags = eline->flags;
    if (flags & 0x2000) {
        *(u16 *)&eline->unk188 = eline->unk18C;
        return;
    }
    if (flags & 0x8000) {
        eline->flags = (flags & 0xFFFF07FF) | 0x1000;
        *(u16 *)&eline->unk188 = eline->unk18E;
    }
    eline->flags |= 0x800;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B2BA0);

/**
 * Initialize a movement sweep from @p a1 toward @p a2: stores both
 * endpoints (scaled by 64) into @c unk18C / @c unk18E, primes
 * @c unk188 with the start, clears the high-mode flag bits and the
 * direction bit, then sets the appropriate direction marker and
 * pre-offsets the start position by 0x3F.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Source value (scaled by 64 into @c unk18E).
 * @param a2    Destination value (scaled by 64 into @c unk18C).
 */
void func_800B2D40(Eline *eline, s32 a1, s32 a2) {
    eline->unk18C = a2 << 6;
    eline->unk18E = a1 << 6;

    eline->flags &= 0xFFFF07FF;
    eline->flags &= ~0x100;
    *(u16 *)&eline->unk188 = eline->unk18C;

    if (a1 < a2) {
        eline->flags |= 0x100;
        *(u16 *)&eline->unk188 = eline->unk18C + 0x3F;
        eline->unk18C = eline->unk18C + 0x3F;
    } else {
        eline->unk18E += 0x3F;
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B2DC0);

/**
 * Pops two parameters from the stack, calls func_800B2D40, then sets
 * bit 0x8000 in flags at offset 0x160.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
/**
 * Pop two stack values (source then destination), forward them to the
 * movement-sweep setup helper @c func_800B2D40, and arm the @c 0x8000
 * marker flag for the subsequent movement tick.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2E68(Eline *eline) {
    s8 idx = eline->stackPtr;
    s8 idx2 = idx - 1;
    eline->stackPtr = idx - 2;
    func_800B2D40(eline, ((s32 *)eline)[(s8)idx],
                         ((s32 *)eline)[(s8)idx2]);
    eline->flags |= 0x8000;
    return 2;
}

/**
 * Variant of @c func_800B2E68 that arms the @c 0x2000 marker flag
 * instead of @c 0x8000 after running the movement-sweep init.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2EDC(Eline *eline) {
    s8 idx = eline->stackPtr;
    s8 idx2 = idx - 1;
    eline->stackPtr = idx - 2;
    func_800B2D40(eline, ((s32 *)eline)[(s8)idx],
                         ((s32 *)eline)[(s8)idx2]);
    eline->flags |= 0x2000;
    return 2;
}

/**
 * Returns 2 if bit 0x800 is set in the flags at offset 0x160, otherwise 1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 if flag 0x800 is set, else 1.
 */
s32 func_800B2F50(Eline *eline) {
    if (eline->flags & 0x800) {
        return 2;
    }
    return 1;
}

/**
 * Pop a single value @c v and stage a zero-length movement at that
 * position: call @c func_800B2D40(v, v) (start equals end), set both
 * walk-speed countdown halfwords to 1, and arm the @c 0x1000 stop
 * marker so the next tick records completion immediately.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2F70(Eline *eline) {
    s32 val = POP(eline);
    func_800B2D40(eline, val, val);
    ((FieldEntity *)eline)->walkSpeed2 = 1;
    ((FieldEntity *)eline)->walkSpeed = 1;
    eline->flags |= 0x1000;
    return 2;
}

/**
 * Reset movement parameters: set walk speed to 1, clear directions, update flags.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
/**
 * Reset all movement-sweep state to "stopped at -1": set the current
 * position halfword to @c -1, prime walk-speed countdown halfwords to 1,
 * clear both saved endpoints, then mask the flags to clear high-mode
 * bits + direction bit and arm the @c 0x1000 stop marker.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B2FD8(Eline *eline) {
    *(s16 *)&eline->unk188 = -1;
    ((FieldEntity *)eline)->walkSpeed2 = 1;
    ((FieldEntity *)eline)->walkSpeed = 1;
    eline->unk18E = 0;
    eline->unk18C = 0;
    eline->flags &= 0xFFFF07FF;
    eline->flags &= ~0x100;
    eline->flags |= 0x1000;
    return 2;
}

/** @brief Pop halfword, store to both walkSpeed2 and walkSpeed. Returns 2. */
s32 func_800B301C(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    e->walkSpeed2 = (u16)POP(eline);
    e->walkSpeed = e->walkSpeed2;
    return 2;
}

/** @brief Pop halfword, store to runSpeed. Returns 2. */
s32 func_800B3050(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    e->runSpeed = (u16)POP(eline);
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3080);

/**
 * Set movement state from a stack-pushed parameter block. Sets the
 * @c 0x600 dual-axis flag bits, zeros @c unk19E, pops six pairs of
 * "current/target" bytes into the active/saved slot ranges
 * (@c unk1A7..unk1AC mirroring @c unk1AD..unk1B2), pops the timer
 * halfword @c unk1A2, pops the loop counter halfword @c unk1A0 /
 * @c unk19C (broadcast), and primes the per-tick counter @c unk1A4
 * from the saved tick total @c unk1A5.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B31B4(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    u8 v;
    u16 hw;

    eline->flags |= 0x600;
    e->unk19E = 0;

    e->unk1A6 = POP_BYTE(eline);
    e->unk1A5 = POP_BYTE(eline);

    v = POP_BYTE(eline);
    e->unk1AC = v;
    e->unk1B2 = v;

    v = POP_BYTE(eline);
    e->unk1AB = v;
    e->unk1B1 = v;

    v = POP_BYTE(eline);
    e->unk1AA = v;
    e->unk1B0 = v;

    v = POP_BYTE(eline);
    e->unk1A9 = v;
    e->unk1AF = v;

    v = POP_BYTE(eline);
    e->unk1A8 = v;
    e->unk1AE = v;

    v = POP_BYTE(eline);
    e->unk1A7 = v;
    e->unk1AD = v;

    e->unk1A2 = (u16)POP(eline);

    hw = (u16)POP(eline);
    e->unk1A0 = hw;
    e->unk19C = hw;

    e->unk1A4 = e->unk1A5;
    return 2;
}

/**
 * Clears bits 0x600 in the entity flags and zeroes the halfword at
 * @c unk19C. Returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3334(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    e->unk19C = 0;
    e->unk160 = e->unk160 & ~0x600;
    return 2;
}

/**
 * Clear movement state: zero many fields, clear bits 0x600 in flags,
 * copy byte @c unk1A5 to @c unk1A4. Returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3350(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    s32 flags = e->unk160;
    u8 val;
    *(volatile u8 *)&e->unk1A5 = 0;
    val = *(volatile u8 *)&e->unk1A5;
    e->unk19E = 0;
    e->unk1A6 = 0;
    e->unk1AC = 0;
    e->unk1B2 = 0;
    e->unk1AB = 0;
    e->unk1B1 = 0;
    e->unk1AA = 0;
    e->unk1B0 = 0;
    e->unk1A9 = 0;
    e->unk1AF = 0;
    e->unk1A8 = 0;
    e->unk1AE = 0;
    e->unk1A7 = 0;
    e->unk1AD = 0;
    e->unk1A2 = 0;
    e->unk1A0 = 0;
    e->unk19C = 0;
    e->unk160 = flags & ~0x600;
    e->unk1A4 = val;
    return 2;
}

/**
 * Pop four stack halfwords into @c D_800704A8 system-state slots and
 * arm the activation markers @c unk122 and @c unk130. Pops are stored
 * in reverse order across the four slot pairs at 0x126, 0x12C, 0x134
 * and 0x13A.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B33B8(Eline *eline) {
    D_800704A8.unk122 = 1;
    D_800704A8.unk130 = 1;
    D_800704A8.unk13A = (u16)POP(eline);
    D_800704A8.unk134 = (u16)POP(eline);
    D_800704A8.unk12C = (u16)POP(eline);
    D_800704A8.unk126 = (u16)POP(eline);
    return 3;
}

/**
 * Clears the bytes at D_800704A8+0x122 and D_800704A8+0x130, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B3474(Eline *eline) {
    D_800704A8.unk122 = 0;
    D_800704A8.unk130 = 0;
    return 2;
}

/**
 * Pop two halfwords into the @c unk030 / @c unk032 SystemState parameter
 * pair, then write mode byte @c unk020 = 3 and clear submode @c unk022.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B348C(Eline *eline) {
    D_800704A8.slots[0].p2 = (u16)POP(eline);
    D_800704A8.slots[0].p1 = (u16)POP(eline);
    D_800704A8.slots[0].mode = 3;
    D_800704A8.slots[0].submode = 0;
    return 3;
}

/**
 * Pop three halfwords (third param, then @c unk032 / @c unk030 pair)
 * into SystemState, set mode @c unk020 = 4 and clear submode
 * @c unk022. Return value differs from @c func_800B348C (returns 2
 * here, ending the opcode normally).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B34EC(Eline *eline) {
    D_800704A8.slots[0].timer = (u16)POP(eline);
    D_800704A8.slots[0].p2 = (u16)POP(eline);
    D_800704A8.slots[0].p1 = (u16)POP(eline);
    D_800704A8.slots[0].mode = 4;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Pop three halfwords into SystemState parameter slots and set mode
 * @c unk020 = 5. Identical structure to @c func_800B34EC apart from the
 * mode byte.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3574(Eline *eline) {
    D_800704A8.slots[0].timer = (u16)POP(eline);
    D_800704A8.slots[0].p2 = (u16)POP(eline);
    D_800704A8.slots[0].p1 = (u16)POP(eline);
    D_800704A8.slots[0].mode = 5;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Pop a script index, look up the corresponding @c FieldEntity pointer
 * via the @c D_80085230 table, and stage its @c field_0x256 byte into
 * SystemState @c unk021 with mode @c unk020 = 0 and submode @c unk022
 * = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B35FC(Eline *eline) {
    s32 val = POP(eline);
    u8 byte = D_80085230[val]->field_0x256;
    D_800704A8.slots[0].mode = 0;
    D_800704A8.slots[0].submode = 0;
    D_800704A8.slots[0].param = byte;
    return 3;
}

/**
 * Pop two stack values, store the first into SystemState halfword
 * @c unk024 (mode-1 timer), look up the second through the @c D_80085230
 * entity-pointer table and copy @c field_0x256 into @c unk021. Mode
 * byte is set to 1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3650(Eline *eline) {
    s32 val1 = POP(eline);
    s32 val2 = POP(eline);
    D_800704A8.slots[0].timer = val1;
    D_800704A8.slots[0].param = D_80085230[val2]->field_0x256;
    D_800704A8.slots[0].mode = 1;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Variant of @c func_800B3650 with mode @c unk020 = 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B36C8(Eline *eline) {
    s32 val1 = POP(eline);
    s32 val2 = POP(eline);
    D_800704A8.slots[0].timer = val1;
    D_800704A8.slots[0].param = D_80085230[val2]->field_0x256;
    D_800704A8.slots[0].mode = 2;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Pop a party-slot index, look up the active battle-field-entity index
 * for that slot in @c g_seedState->memberSlot, and stage it into
 * SystemState @c unk021 with mode @c unk020 = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B3740(Eline *eline) {
    SeedState *ss = g_seedState;
    s32 val = POP(eline);
    D_800704A8.slots[0].param = ss->memberSlot[val];
    D_800704A8.slots[0].mode = 0;
    D_800704A8.slots[0].submode = 0;
    return 3;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3788);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B37F8);

/**
 * Returns 2 if D_800704CA equals 2, otherwise returns 1.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 if D_800704CA is 2, else 1.
 */
s32 func_800B3868(Eline *eline) {
    if (D_800704CA == 2) {
        return 2;
    }
    return 1;
}

/**
 * Peek the top stack value as a SystemState slot index. If that slot's
 * @c submode is 2 (terminal state), pop and return 2 — the script
 * advances past the wait opcode. Otherwise keep the stack intact and
 * return 1 to retry next frame.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 once the slot reaches submode 2, 1 while still waiting.
 */
s32 func_800B388C(Eline *eline) {
    s32 idx = PEEK(eline);
    if (D_800704A8.slots[idx].submode == 2) {
        eline->stackPtr--;
        return 2;
    }
    return 1;
}

/**
 * Pop two halfword parameters and a slot index, store the parameters
 * into @c slots[idx].p1 / @c .p2, then arm the slot with @c mode = 3
 * and @c submode = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B38E0(Eline *eline) {
    u16 p2 = (u16)POP(eline);
    u16 p1 = (u16)POP(eline);
    s32 idx = POP(eline);
    D_800704A8.slots[idx].p2 = p2;
    D_800704A8.slots[idx].p1 = p1;
    D_800704A8.slots[idx].mode = 3;
    D_800704A8.slots[idx].submode = 0;
    return 3;
}

/**
 * Pop a timer halfword, two parameter halfwords, and a slot index;
 * arm @c slots[idx] with @c mode = 4 and the popped parameters.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3964(Eline *eline) {
    u16 timer = (u16)POP(eline);
    u16 p2 = (u16)POP(eline);
    u16 p1 = (u16)POP(eline);
    s32 idx = POP(eline);
    D_800704A8.slots[idx].timer = timer;
    D_800704A8.slots[idx].p2 = p2;
    D_800704A8.slots[idx].p1 = p1;
    D_800704A8.slots[idx].mode = 4;
    D_800704A8.slots[idx].submode = 0;
    return 2;
}

/**
 * Variant of @c func_800B3964 that arms the slot with @c mode = 5.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3A04(Eline *eline) {
    u16 timer = (u16)POP(eline);
    u16 p2 = (u16)POP(eline);
    u16 p1 = (u16)POP(eline);
    s32 idx = POP(eline);
    D_800704A8.slots[idx].timer = timer;
    D_800704A8.slots[idx].p2 = p2;
    D_800704A8.slots[idx].p1 = p1;
    D_800704A8.slots[idx].mode = 5;
    D_800704A8.slots[idx].submode = 0;
    return 2;
}

/**
 * Pop an entity index and a slot index, look up the entity's
 * @c field_0x256 byte via @c D_80085230 and stage it into
 * @c slots[slotIdx].param with @c mode = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B3AA4(Eline *eline) {
    s32 entityIdx = POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].param = D_80085230[entityIdx]->field_0x256;
    D_800704A8.slots[slotIdx].mode = 0;
    D_800704A8.slots[slotIdx].submode = 0;
    return 3;
}

/**
 * Pop a timer halfword, an entity index, and a slot index; arm
 * @c slots[slotIdx] with @c mode = 1, the timer, and the entity's
 * @c field_0x256 byte from @c D_80085230.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3B20(Eline *eline) {
    u16 timer = (u16)POP(eline);
    s32 entityIdx = POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].timer = timer;
    D_800704A8.slots[slotIdx].param = D_80085230[entityIdx]->field_0x256;
    D_800704A8.slots[slotIdx].mode = 1;
    D_800704A8.slots[slotIdx].submode = 0;
    return 2;
}

/**
 * Variant of @c func_800B3B20 that arms @c mode = 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B3BC0(Eline *eline) {
    u16 timer = (u16)POP(eline);
    s32 entityIdx = POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].timer = timer;
    D_800704A8.slots[slotIdx].param = D_80085230[entityIdx]->field_0x256;
    D_800704A8.slots[slotIdx].mode = 2;
    D_800704A8.slots[slotIdx].submode = 0;
    return 2;
}

/**
 * Pop a party-slot index and a slot index; read the active battle-entity
 * index from @c g_seedState->memberSlot and stage it into
 * @c slots[slotIdx].param with @c mode = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B3C60(Eline *eline) {
    SeedState *ss = g_seedState;
    s32 partySlot = POP(eline);
    u8 byte = ss->memberSlot[partySlot];
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].param = byte;
    D_800704A8.slots[slotIdx].mode = 0;
    D_800704A8.slots[slotIdx].submode = 0;
    return 3;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3CD0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3D60);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3DF0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3EA0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3F18);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B3F9C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B4074);

/**
 * @brief Pop a byte from the stack and store to D_80070652.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B414C(Eline *eline) {
    D_80070652 = POP_BYTE(eline);
    return 2;
}

/**
 * Pop a value and store it into @c g_gameState.mainData.battleStateFlag
 * (the battle state word at @c g_gameState+0xCD4 used by camera shake).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B417C(Eline *eline) {
    volatile GameState *gs = &g_gameState;
    gs->mainData.battleStateFlag = POP(eline);
    return 2;
}

/**
 * @brief Copy the global battle state flag into the script result register.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B41B0(Eline *eline) {
    volatile GameState *gs = &g_gameState;
    eline->resultSlots[0] = gs->mainData.battleStateFlag;
    return 2;
}

/**
 * Trigger camera-shake mode: pop two intensity bytes into
 * @c g_seedState->cameraShakeY / @c cameraShakeX, arm the related
 * field flag bits and the battle-config bit, then drive
 * @c setCameraShakeParams + @c setCameraVibrateState(1).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B41CC(Eline *eline) {
    g_seedState->cameraShakeY = POP_BYTE(eline);
    g_seedState->cameraShakeX = POP_BYTE(eline);
    g_seedState->stateFlags |= 0x40;
    g_seedState->fieldB6 |= 0x4;
    g_battleConfig.unk2 |= 0x4;
    setCameraShakeParams(g_seedState->cameraShakeX, g_seedState->cameraShakeY);
    setCameraVibrateState(1);
    return 2;
}

/**
 * Stop camera-shake mode: clear the @c stateFlags bit 0x40, the
 * @c fieldB6 bit 0x4 and the @c g_battleConfig.unk2 bit 0x4, then call
 * @c setCameraVibrateState(0). Inverse of @c func_800B41CC.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4288(Eline *eline) {
    g_seedState->stateFlags &= ~0x40;
    g_seedState->fieldB6 &= ~0x4;
    g_battleConfig.unk2 &= ~0x4;
    setCameraVibrateState(0);
    return 2;
}

/**
 * Pops a parameter and calls setCameraVibrateIntensity, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B42E0(Eline *eline) {
    setCameraVibrateIntensity(POP(eline));
    return 2;
}

/**
 * If this entity is the active script and the mode-7 reentry guard
 * (@c D_800704A8.unk1A2) is clear, pop three halfwords into the
 * mode-7 parameter slots @c unk00E / @c unk00C / @c counter, switch
 * @c mode to 7, and call @c func_800B4F40 to launch the operation.
 * If @c unk1A2 is set (operation already in progress), rewind the
 * stack pointer by 3 instead so the script re-runs the opcode.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 func_800B4320(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        if (D_800704A8.unk1A2 != 0) {
            eline->stackPtr -= 3;
        } else {
            D_800704A8.mode = 7;
            D_800704A8.unk00E = (u16)POP(eline);
            D_800704A8.unk00C = (u16)POP(eline);
            D_800704A8.counter = (u16)POP(eline);
            func_800B4F40();
        }
    }
    return 1;
}

/**
 * Pop a value (masked to 2 bits) into the global mode byte @c D_8007064E
 * and then fire @c func_800A97E4(i, 0x25, 0, 0) once per active
 * @c BattleFieldEntity slot (count from @c D_80085388).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B43FC(Eline *eline) {
    s32 i;
    D_8007064E = POP(eline) & 0x3;
    for (i = 0; i < D_80085388; i++) {
        func_800A97E4(i, 0x25, 0, 0);
    }
    return 2;
}

/** @brief Pop byte, store to global D_8007064F. Returns 2. */
s32 func_800B448C(Eline *eline) {
    *(u8 *)D_8007064F = POP_BYTE(eline);
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B44BC);

/**
 * Pops a parameter and calls func_800A5A14, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B45CC(Eline *eline) {
    func_800A5A14(POP(eline));
    return 2;
}

/**
 * If active, set mode = 1 and stage the dispatcher-supplied @p a1 into
 * @c unk00C, then pop four halfwords into @c unk00E / @c unk006 /
 * @c unk004 / @c counter.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied mode-1 trigger word.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 func_800B460C(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 1;
        D_800704A8.unk00C = a1;
        D_800704A8.unk00E = (u16)POP(eline);
        D_800704A8.unk006 = (u16)POP(eline);
        D_800704A8.unk004 = (u16)POP(eline);
        D_800704A8.counter = (u16)POP(eline);
    }
    return 1;
}

/**
 * Variant of @c func_800B460C that pops one extra halfword into
 * @c unk008 (between @c unk00E and @c unk006).
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied mode-1 trigger word.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 func_800B46E4(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 1;
        D_800704A8.unk00C = a1;
        D_800704A8.unk00E = (u16)POP(eline);
        D_800704A8.unk008 = (u16)POP(eline);
        D_800704A8.unk006 = (u16)POP(eline);
        D_800704A8.unk004 = (u16)POP(eline);
        D_800704A8.counter = (u16)POP(eline);
    }
    return 1;
}

/**
 * Mode-6 init: identical structure to @c func_800B46E4 but sets
 * @c mode = 6 and also flags @c slotActive[0] = 1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied mode-6 trigger word.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 func_800B47E4(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 6;
        D_800704A8.unk1A0 = 1;
        D_800704A8.unk00C = a1;
        D_800704A8.unk00E = (u16)POP(eline);
        D_800704A8.unk008 = (u16)POP(eline);
        D_800704A8.unk006 = (u16)POP(eline);
        D_800704A8.unk004 = (u16)POP(eline);
        D_800704A8.counter = (u16)POP(eline);
    }
    return 1;
}

/**
 * Mode-1 init seeded with @c 0x7FFF sentinels in the inner-loop bounds
 * and a single-pop @c unk00C parameter. Mode is set to 1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 func_800B48EC(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 1;
        D_800704A8.unk00E = 0;
        D_800704A8.unk00C = (u16)POP(eline);
        D_800704A8.unk004 = 0x7FFF;
        D_800704A8.unk006 = 0x7FFF;
        D_800704A8.counter = (u16)POP(eline);
    }
    return 1;
}

/**
 * Clears the global byte D_8007064A, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B497C(Eline *eline) {
    D_8007064A = 0;
    return 2;
}

/**
 * Sets the global byte D_8007064A to 1, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B498C(Eline *eline) {
    D_8007064A = 1;
    return 2;
}

/**
 * Sets the global byte D_8007064D to 1, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B49A0(Eline *eline) {
    D_8007064D = 1;
    return 2;
}

/**
 * Clears the global byte D_8007064D, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B49B4(Eline *eline) {
    D_8007064D = 0;
    return 2;
}

/**
 * Sets the global byte D_8007064B to 1, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B49C4(Eline *eline) {
    D_8007064B = 1;
    return 2;
}

/**
 * Clears the global byte D_8007064B, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 func_800B49D8(Eline *eline) {
    D_8007064B = 0;
    return 2;
}

/** @brief Set D_800704A8 command to 5, clear halfword, copy entity byte 0xD1. Returns 3. */
s32 func_800B49E8(void) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0;
    D_800704A8.unk1AB = g_seedState->fieldD1;
    return 3;
}

/**
 * Sets D_800704A8 to 5, sets the halfword at D_800704A8+2 to 1,
 * sets the byte at D_800704A8+0x1AB to 2, returns 3.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 3.
 */
s32 func_800B4A18(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 1;
    D_800704A8.unk1AB = 2;
    return 3;
}

/**
 * Mode-5 init: set mode = 5, counter = 0x17, pop a byte into @c unk1AB.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B4A40(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x17;
    D_800704A8.unk1AB = POP_BYTE(eline);
    return 3;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B4A88);

/**
 * Sets D_800704A8 to 5, sets the halfword at D_800704A8+2 to 0x1A,
 * sets the byte at D_800704A8+0x1AB to 1, returns 3.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 3.
 */
s32 func_800B4D0C(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x1A;
    D_800704A8.unk1AB = 1;
    return 3;
}

/**
 * Mode-5 init with counter 0x1D — variant of @c func_800B4A40.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 func_800B4D34(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x1D;
    D_800704A8.unk1AB = POP_BYTE(eline);
    return 3;
}

/**
 * Pops a parameter, masks it with 0x7F, and calls setFieldFlag, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4D7C(Eline *eline) {
    setFieldFlag(POP(eline) & 0x7F);
    return 2;
}

/**
 * Calls func_80037240 and returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4DBC(Eline *eline) {
    func_80037240(eline);
    return 2;
}

/**
 * Calls func_800ADC04 and returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4DDC(Eline *eline) {
    func_800ADC04(eline);
    return 2;
}

/**
 * Pop a halfword HP value and a character index, then set
 * @c g_gameState.chars[charId].currentHp to the popped value.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4DFC(Eline *eline) {
    u16 hp = (u16)POP(eline);
    s32 charId = POP(eline);
    g_gameState.chars[charId].currentHp = hp;
    return 2;
}

/**
 * Pop a character index, read @c g_gameState.chars[charId].currentHp
 * and stage it into the script result slot 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4E60(Eline *eline) {
    s32 charId = POP(eline);
    eline->resultSlots[0] = g_gameState.chars[charId].currentHp;
    return 2;
}

/**
 * Sets D_800704A8 to 5, sets the halfword at D_800704A8+2 to 0x18,
 * sets the byte at D_800704A8+0x1AB to 1, returns 3.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 3.
 */
s32 func_800B4EB0(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x18;
    D_800704A8.unk1AB = 1;
    return 3;
}

/**
 * Pops a parameter from the stack. If nonzero, sets bit 0x01 in
 * g_seedState+0xD1. Otherwise clears it. Returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4ED8(Eline *eline) {
    if (POP(eline) != 0) {
        g_seedState->fieldD1 |= 0x01;
    } else {
        g_seedState->fieldD1 &= ~0x01;
    }
    return 2;
}

/**
 * Updates bit 2 of the entity byte at offset 0xD1 based on flag 0x200
 * in the flags word at offset 0x68.
 *
 * If bit 0x200 is set in the flags, clears bit 2 of the byte at 0xD1.
 * Otherwise, sets bit 2.
 */
void func_800B4F40(void) {
    if (g_seedState->stateFlags & 0x200) {
        g_seedState->fieldD1 &= 0xFD;
    } else {
        g_seedState->fieldD1 |= 0x2;
    }
}

/**
 * Pop a flag value. Nonzero clears @c stateFlags bit @c 0x200; zero
 * sets bit @c 0x200 and additionally clears bit @c 0x02 of @c fieldD1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4F80(Eline *eline) {
    if (POP(eline) != 0) {
        g_seedState->stateFlags &= ~0x200;
    } else {
        g_seedState->stateFlags |= 0x200;
        g_seedState->fieldD1 &= ~0x02;
    }
    return 2;
}

/**
 * Pop a flag value. If @c stateFlags bit @c 0x200 is clear, set or
 * clear bit @c 0x02 in @c fieldD1 based on the popped value. If
 * @c 0x200 is set, leave @c fieldD1 alone.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B4FF8(Eline *eline) {
    s32 val = POP(eline);
    if (!(g_seedState->stateFlags & 0x200)) {
        if (val != 0) {
            g_seedState->fieldD1 |= 0x02;
        } else {
            g_seedState->fieldD1 &= ~0x02;
        }
    }
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object6", func_800B505C);

/**
 * Pops a parameter, calls func_800C0410, stores result at offset 0x140.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B5134(Eline *eline) {
    eline->resultSlots[0] = func_800C0410(POP(eline));
    return 2;
}

/**
 * Pops two parameters from the stack and calls addItemToInventory(val1, val2).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B5188(Eline *eline) {
    s32 val1, val2;

    val1 = POP(eline);
    val2 = POP(eline);
    addItemToInventory(val1, val2);
    return 2;
}

/**
 * Pop a gil delta and add it to @c g_gameState.mainData.party.gil,
 * clamped to the FF gil cap of 99,999,999. Mirror the new total into
 * @c g_seedState->gilMirror.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B51E0(Eline *eline) {
    s32 delta = POP(eline);
    g_gameState.mainData.party.gil += delta;
    if (g_gameState.mainData.party.gil > 0x05F5E0FE) {
        g_gameState.mainData.party.gil = 0x05F5E0FF;
    }
    g_seedState->gilMirror = g_gameState.mainData.party.gil;
    return 2;
}

/**
 * Variant of @c func_800B51E0 that operates on @c dreamGil instead of
 * @c gil and mirrors the result into @c g_seedState->dreamGilMirror.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B5248(Eline *eline) {
    s32 delta = POP(eline);
    g_gameState.mainData.party.dreamGil += delta;
    if (g_gameState.mainData.party.dreamGil > 0x05F5E0FE) {
        g_gameState.mainData.party.dreamGil = 0x05F5E0FF;
    }
    g_seedState->dreamGilMirror = g_gameState.mainData.party.dreamGil;
    return 2;
}

/**
 * Pop a delta and add it to @c g_seedState->seedExp, clamping the
 * resulting value to the legal SeeD-rank range [@c 100, @c 3100].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B52B0(Eline *eline) {
    s32 delta = POP(eline);
    g_seedState->seedExp += delta;
    if ((s16)g_seedState->seedExp < 100) {
        g_seedState->seedExp = 100;
    } else if ((s16)g_seedState->seedExp >= 3100) {
        g_seedState->seedExp = 3100;
    }
    return 2;
}

/**
 * Pops a parameter, calls markItemPresent, stores result in resultSlots[0].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B5318(Eline *eline) {
    eline->resultSlots[0] = markItemPresent(POP(eline));
    return 2;
}

/**
 * Pops two values, calls modifyItemQuantity(val1, val2), stores result in resultSlots[0].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B536C(Eline *eline) {
    s32 val1, val2;

    val1 = POP(eline);
    val2 = POP(eline);
    eline->resultSlots[0] = modifyItemQuantity(val1, val2);
    return 2;
}

/**
 * Pops a parameter, calls func_80023B14, stores result in resultSlots[0].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 func_800B53D8(Eline *eline) {
    eline->resultSlots[0] = func_80023B14(POP(eline));
    return 2;
}
