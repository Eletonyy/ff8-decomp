#include "common.h"
#include "field.h"
#include "gamestate.h"
#include "battle.h"
#include "sound.h"
#include "psxsdk/libgte.h"
#include "field/fe_object6.h"

/**
 * Pops 3 stack values (target, volume, pan), looks up an SFX entry in
 * the dispatcher-keyed bank table @c D_800D5EA4, and plays the resulting
 * bank SFX with the popped parameters (volume masked to 8 bits, pan to 7).
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied SFX bank index.
 * @return 2 (continue processing).
 */
s32 opHandler_EFFECTPLAY2(Eline *eline, s32 a1) {
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
s32 opHandler_ALLSEVOL(Eline *eline) {
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
s32 opHandler_ALLSEVOLTRANS(Eline *eline) {
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
s32 opHandler_ALLSEPOS(Eline *eline) {
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
s32 opHandler_ALLSEPOSTRANS(Eline *eline) {
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
s32 opHandler_SEVOL(Eline *eline) {
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
s32 opHandler_SEVOLTRANS(Eline *eline) {
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
s32 opHandler_SEPOS(Eline *eline) {
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
s32 opHandler_SEPOSTRANS(Eline *eline) {
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
s32 opHandler_SESTOP(Eline *eline) {
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
s32 opHandler_COUNTERCLOCKWISETURN(Eline *eline) {
    eline->resultSlots[0] = func_80013210(POP(eline));
    return 2;
}

/** @brief Pop value, bitwise-NOT, store to D_8007065C. Returns 2. */
s32 opHandler_CLOCKWISETURN2(Eline *eline) {
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

/**
 * Play a positional SFX for the entity. If the entity's @c 0x80 flag
 * is set, project the entity's screen position via @c func_800B27C4,
 * clamp the resulting screen X to @c [-0x200, 0x1FC] and remap to
 * @c [0x40, 0xFF] for stereo pan, clamp the perspective Z to
 * @c [8, 0x6E] and invert to volume (@c 0x7F-p). Then dispatch by
 * channel: @c 0 plays via the left bus (@c 0x400000), @c 1 plays via
 * the right bus (@c 0x800000). Within each channel, when @c msgActive
 * is @c 3 or @c 4 (special states), a fixed SFX (@c 0x40 or @c 0x3F)
 * is queued; otherwise if the per-channel SFX index byte
 * (@c unk188 / @c unk189) is non-sentinel (not @c -1 as @c s8), the
 * looked-up bank SFX is played directly; otherwise a fallback SFX is
 * selected based on the @c 0x4000000 emphasis flag.
 *
 * @param eline   Pointer to the Eline event-script context.
 * @param channel Output channel (0 = left bus, 1 = right bus).
 */
void func_800B2864(Eline *eline, s32 channel, s32 unused2, s32 unused3) {
    s32 pitch;
    s32 volume;

    if (!(eline->flags & 0x80)) return;

    func_800B27C4(eline->field_0x256, &pitch, &volume);

    if (pitch < -0x200) pitch = -0x200;
    else if (pitch >= 0x1FC) pitch = 0x1FC;
    pitch = pitch / 4 + 0x80;

    volume /= 24;
    if (volume < 8) volume = 8;
    else if (volume >= 0x6E) volume = 0x6E;
    volume = 0x7F - volume;

    switch (channel) {
    case 0:
        if ((u32)(eline->msgActive - 3) < 2) {
            sndPlaySfx(0x40, 0x400000, pitch, volume);
        } else if ((s8)eline->unk188 != -1) {
            sndPlayBankSfx((s32)func_8003974C(D_800D5EA4, (s8)eline->unk188), 0x400000, pitch, volume);
        } else if (eline->flags & 0x4000000) {
            sndPlaySfx(0x36, 0x400000, pitch, volume);
        } else {
            sndPlaySfx(3, 0x400000, pitch, volume);
        }
        break;
    case 1:
        if ((u32)(eline->msgActive - 3) < 2) {
            sndPlaySfx(0x3F, 0x400000, pitch, volume);
        } else if ((s8)eline->unk189 != -1) {
            sndPlayBankSfx((s32)func_8003974C(D_800D5EA4, (s8)eline->unk189), 0x800000, pitch, volume);
        } else if (eline->flags & 0x4000000) {
            sndPlaySfx(0x35, 0x400000, pitch, volume);
        } else {
            sndPlaySfx(2, 0x400000, pitch, volume);
        }
        break;
    }
}

/**
 * Pops a byte from the stack, stores it at offset 0x188, and stores
 * the second argument at offset 0x189. Returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1 Value to store at offset 0x189.
 * @return 2 (continue processing).
 */
s32 opHandler_FOOTSTEP(Eline *eline, s32 a1) {
    eline->unk188 = POP_BYTE(eline);
    eline->unk189 = a1;
    return 2;
}

/**
 * Broadcast the current Eline's @c unk188 / @c unk189 script parameter
 * bytes to every active @c Eline slot. Iterates over the
 * @c D_80085224 array (count @c D_80085388, stride 0x264) and writes
 * the same pair into each entry's mirror fields at +0x188 / +0x189.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_FOOTSTEPCOPY(Eline *eline) {
    s32 i;
    Eline *p = D_80085224;
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
s32 opHandler_FOOTSTEPON(Eline *eline) {
    eline->flags |= 0x80;
    return 2;
}

/**
 * Clears bit 0x80 in the flags at offset 0x160, returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_FOOTSTEPOFF(Eline *eline) {
    eline->flags &= ~0x80;
    return 2;
}

/**
 * Clear bit 0x80 in the flags word of every active @c Eline
 * slot (count @c D_80085388, stride 0x264 at @c D_80085224). Used by a
 * script opcode to reset a per-entity activation flag across the party.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 opHandler_FOOTSTEPOFFALL(Eline *eline) {
    s32 i;
    Eline *p = D_80085224;
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
s32 opHandler_FOOTSTEPCUT(Eline *eline) {
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

/**
 * Per-frame movement-step tick. If the @c 0x200 axis-active flag is set
 * and the inner counter @c unk19C is zero, refill the active movement
 * state from one of two saved snapshots (selected by the @c 0x400
 * direction bit) and decrement the tick counter @c unk1A4. Then decrement
 * the @c walkSpeed countdown; when it hits zero, reload it from
 * @c walkSpeed2, apply a @c +/- @c unk18A step to @c unk188 (direction
 * gated by the @c 0x100 flag, suppressed by the @c 0x1000 stop marker),
 * and either advance toward the @c unk18E target (helper
 * @c func_800B2B48) or — once the target is reached — clear the
 * @c 0x800 done marker.
 *
 * @param e Pointer to the FieldEntity movement view of the entity.
 */
void func_800B2BA0(FieldEntity *e) {
    s32 flags = e->unk160;

    if ((flags & 0x200) && (s16)e->unk19C == 0) {
        if (e->unk1A4 == 0) {
            if (flags & 0x400) {
                e->unk160 = flags & ~0x400;
                e->unk19E = 0;
                e->unk1A4 = e->unk1A6;
                e->unk19C = e->unk1A2;
                e->unk1AC = e->unk1AF;
                e->unk1AB = e->unk1AE;
                e->unk1AA = e->unk1AD;
                e->unk1A9 = e->unk1B2;
                e->unk1A8 = e->unk1B1;
                e->unk1A7 = e->unk1B0;
            } else {
                e->unk160 = flags | 0x400;
                e->unk19E = 0;
                e->unk1A4 = e->unk1A5;
                e->unk19C = e->unk1A0;
                e->unk1AC = e->unk1B2;
                e->unk1AB = e->unk1B1;
                e->unk1AA = e->unk1B0;
                e->unk1A9 = e->unk1AF;
                e->unk1A8 = e->unk1AE;
                e->unk1A7 = e->unk1AD;
            }
        }
        e->unk1A4--;
    }

    e->walkSpeed -= 1;
    if ((s16)e->walkSpeed <= 0) {
        s32 f1, f2;
        e->walkSpeed = e->walkSpeed2;
        f1 = e->unk160;
        if (!(f1 & 0x1000)) {
            if (f1 & 0x100) {
                *(u16 *)&e->unk188 = *(u16 *)&e->unk188 - e->unk18A;
            } else {
                *(u16 *)&e->unk188 = *(u16 *)&e->unk188 + e->unk18A;
            }
        }
        f2 = e->unk160;
        if (f2 & 0x100) {
            if (*(s16 *)&e->unk188 < (s16)e->unk18E) {
                func_800B2B48(e);
            } else {
                e->unk160 = f2 & ~0x800;
            }
        } else {
            if (*(s16 *)&e->unk188 >= (s16)e->unk18E) {
                func_800B2B48(e);
            } else {
                e->unk160 = f2 & ~0x800;
            }
        }
    }
}

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

/**
 * Per-frame movement-sweep arm helper. When the entity's @c activeMask
 * bit for the current @c scriptGroup is set, pop two parameters from
 * the script stack, forward them to @c func_800B2D40 (which sets up
 * the sweep state), and set the @c 0x8000 marker flag. When the bit
 * is clear, return @c 2 once the @c 0x800 done flag is set (otherwise
 * @c 1 to keep waiting for the bit to flip).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 1 to keep the opcode active, 2 when @c 0x800 is set on the
 *         else-branch path.
 */
s32 opHandler_BGANIME(Eline *eline) {
    Eline *self = eline;
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        s8 idx = self->stackPtr;
        s8 idx2 = idx - 1;
        self->stackPtr = idx - 2;
        func_800B2D40(self, ((s32 *)self)[(s8)idx],
                            ((s32 *)self)[(s8)idx2]);
        self->flags |= 0x8000;
    } else if (self->flags & 0x800) {
        return 2;
    }
    return 1;
}

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
s32 opHandler_RBGANIME(Eline *eline) {
    s8 idx = eline->stackPtr;
    s8 idx2 = idx - 1;
    eline->stackPtr = idx - 2;
    func_800B2D40(eline, ((s32 *)eline)[(s8)idx],
                         ((s32 *)eline)[(s8)idx2]);
    eline->flags |= 0x8000;
    return 2;
}

/**
 * Variant of @c opHandler_RBGANIME that arms the @c 0x2000 marker flag
 * instead of @c 0x8000 after running the movement-sweep init.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_RBGANIMELOOP(Eline *eline) {
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
s32 opHandler_BGANIMESYNC(Eline *eline) {
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
s32 opHandler_BGDRAW(Eline *eline) {
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
s32 opHandler_BGOFF(Eline *eline) {
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
s32 opHandler_BGANIMESPEED(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    e->walkSpeed2 = (u16)POP(eline);
    e->walkSpeed = e->walkSpeed2;
    return 2;
}

/** @brief Pop halfword, store to runSpeed. Returns 2. */
s32 opHandler_BGANIMEFLAG(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    e->runSpeed = (u16)POP(eline);
    return 2;
}

/**
 * Per-frame helper for the dual-axis movement state when the entity's
 * @c activeMask bit for the current @c scriptGroup is set: clears the
 * @c 0x600 axis-active flags, zeros @c unk19E, and pops a fresh
 * parameter block of six bytes (@c unk1AC..@c unk1A7) plus a halfword
 * (@c unk19C) from the script stack. When the bit is clear, returns
 * @c 2 only if the @c unk19C counter has hit zero (otherwise @c 1 to
 * keep waiting).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 1 to keep the opcode active, 2 once the wait counter drains.
 */
s32 opHandler_BGSHADE(Eline *eline) {
    FieldEntity *e = (FieldEntity *)eline;
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        e->unk160 &= ~0x600;
        e->unk19E = 0;
        e->unk1AC = POP_BYTE(eline);
        e->unk1AB = POP_BYTE(eline);
        e->unk1AA = POP_BYTE(eline);
        e->unk1A9 = POP_BYTE(eline);
        e->unk1A8 = POP_BYTE(eline);
        e->unk1A7 = POP_BYTE(eline);
        e->unk19C = (u16)POP(eline);
    } else if ((s16)e->unk19C == 0) {
        return 2;
    }
    return 1;
}

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
s32 opHandler_RBGSHADELOOP(Eline *eline) {
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
s32 opHandler_BGSHADESTOP(Eline *eline) {
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
s32 opHandler_BGSHADEOFF(Eline *eline) {
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
s32 opHandler_SHAKE(Eline *eline) {
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
s32 opHandler_SHAKEOFF(Eline *eline) {
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
s32 opHandler_DSCROLL(Eline *eline) {
    D_800704A8.slots[0].p2 = (u16)POP(eline);
    D_800704A8.slots[0].p1 = (u16)POP(eline);
    D_800704A8.slots[0].mode = 3;
    D_800704A8.slots[0].submode = 0;
    return 3;
}

/**
 * Pop three halfwords (third param, then @c unk032 / @c unk030 pair)
 * into SystemState, set mode @c unk020 = 4 and clear submode
 * @c unk022. Return value differs from @c opHandler_DSCROLL (returns 2
 * here, ending the opcode normally).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_LSCROLL(Eline *eline) {
    D_800704A8.slots[0].timer = (u16)POP(eline);
    D_800704A8.slots[0].p2 = (u16)POP(eline);
    D_800704A8.slots[0].p1 = (u16)POP(eline);
    D_800704A8.slots[0].mode = 4;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Pop three halfwords into SystemState parameter slots and set mode
 * @c unk020 = 5. Identical structure to @c opHandler_LSCROLL apart from the
 * mode byte.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLL(Eline *eline) {
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
s32 opHandler_DSCROLLA(Eline *eline) {
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
s32 opHandler_LSCROLLA(Eline *eline) {
    s32 val1 = POP(eline);
    s32 val2 = POP(eline);
    D_800704A8.slots[0].timer = val1;
    D_800704A8.slots[0].param = D_80085230[val2]->field_0x256;
    D_800704A8.slots[0].mode = 1;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Variant of @c opHandler_LSCROLLA with mode @c unk020 = 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLLA(Eline *eline) {
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
 * for that slot in @c g_fieldVars->memberSlot, and stage it into
 * SystemState @c unk021 with mode @c unk020 = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 opHandler_DSCROLLP(Eline *eline) {
    FieldVars *ss = g_fieldVars;
    s32 val = POP(eline);
    D_800704A8.slots[0].param = ss->memberSlot[val];
    D_800704A8.slots[0].mode = 0;
    D_800704A8.slots[0].submode = 0;
    return 3;
}

/**
 * Pop a timer halfword, cache the seedState pointer, then pop a
 * party-slot index; arm slot[0] with @c mode = 1 and the active
 * battle-entity index from @c g_fieldVars->memberSlot.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_LSCROLLP(Eline *eline) {
    u16 timer = (u16)POP(eline);
    FieldVars *ss = g_fieldVars;
    s32 partySlot;
    D_800704A8.slots[0].timer = timer;
    partySlot = POP(eline);
    D_800704A8.slots[0].param = ss->memberSlot[partySlot];
    D_800704A8.slots[0].mode = 1;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Variant of @c opHandler_LSCROLLP that arms @c mode = 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLLP(Eline *eline) {
    u16 timer = (u16)POP(eline);
    FieldVars *ss = g_fieldVars;
    s32 partySlot;
    D_800704A8.slots[0].timer = timer;
    partySlot = POP(eline);
    D_800704A8.slots[0].param = ss->memberSlot[partySlot];
    D_800704A8.slots[0].mode = 2;
    D_800704A8.slots[0].submode = 0;
    return 2;
}

/**
 * Returns 2 if D_800704CA equals 2, otherwise returns 1.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 if D_800704CA is 2, else 1.
 */
s32 opHandler_SCROLLSYNC(Eline *eline) {
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
s32 opHandler_SCROLLSYNC2(Eline *eline) {
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
s32 opHandler_DSCROLL2(Eline *eline) {
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
s32 opHandler_LSCROLL2(Eline *eline) {
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
 * Variant of @c opHandler_LSCROLL2 that arms the slot with @c mode = 5.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLL2(Eline *eline) {
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
s32 opHandler_DSCROLLA2(Eline *eline) {
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
s32 opHandler_LSCROLLA2(Eline *eline) {
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
 * Variant of @c opHandler_LSCROLLA2 that arms @c mode = 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLLA2(Eline *eline) {
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
 * index from @c g_fieldVars->memberSlot and stage it into
 * @c slots[slotIdx].param with @c mode = 0.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 opHandler_DSCROLLP2(Eline *eline) {
    FieldVars *ss = g_fieldVars;
    s32 partySlot = POP(eline);
    u8 byte = ss->memberSlot[partySlot];
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].param = byte;
    D_800704A8.slots[slotIdx].mode = 0;
    D_800704A8.slots[slotIdx].submode = 0;
    return 3;
}

/**
 * Pop a timer halfword, cache the seedState pointer, pop a party-slot
 * index and look up the active battle-entity index, then pop the
 * target slot index; arm @c slots[slotIdx] with @c mode = 1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_LSCROLLP2(Eline *eline) {
    u16 timer = (u16)POP(eline);
    FieldVars *ss = g_fieldVars;
    s32 partySlot = POP(eline);
    u8 byte = ss->memberSlot[partySlot];
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].timer = timer;
    D_800704A8.slots[slotIdx].param = byte;
    D_800704A8.slots[slotIdx].mode = 1;
    D_800704A8.slots[slotIdx].submode = 0;
    return 2;
}

/**
 * Variant of @c opHandler_LSCROLLP2 that arms @c mode = 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLLP2(Eline *eline) {
    u16 timer = (u16)POP(eline);
    FieldVars *ss = g_fieldVars;
    s32 partySlot = POP(eline);
    u8 byte = ss->memberSlot[partySlot];
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].timer = timer;
    D_800704A8.slots[slotIdx].param = byte;
    D_800704A8.slots[slotIdx].mode = 2;
    D_800704A8.slots[slotIdx].submode = 0;
    return 2;
}

/**
 * Pop four extra parameter halfwords and a slot index; stage them into
 * @c slots[slotIdx].p3..p6 (tail parameters used by the wider mode
 * bodies). Does not change @c mode or @c submode.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_SCROLLMODE2(Eline *eline) {
    u16 p6 = (u16)POP(eline);
    u16 p5 = (u16)POP(eline);
    u16 p4 = (u16)POP(eline);
    u16 p3 = (u16)POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].p3 = p3;
    D_800704A8.slots[slotIdx].p4 = p4;
    D_800704A8.slots[slotIdx].p5 = p5;
    D_800704A8.slots[slotIdx].p6 = p6;
    return 2;
}

/**
 * Pop two tail parameters and a slot index; stage them into
 * @c slots[slotIdx].p5 / @c .p6.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_SCROLLRATIO2(Eline *eline) {
    u16 p6 = (u16)POP(eline);
    u16 p5 = (u16)POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].p5 = p5;
    D_800704A8.slots[slotIdx].p6 = p6;
    return 2;
}

/**
 * Pop two parameters and a slot index; stage them into
 * @c slots[slotIdx].p1 / @c .p2 and arm with @c mode = 3.
 * Sibling of @c opHandler_DSCROLL2 with stores in @c p1 / @c p2 order.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 opHandler_DSCROLL3(Eline *eline) {
    u16 p2 = (u16)POP(eline);
    u16 p1 = (u16)POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].p1 = p1;
    D_800704A8.slots[slotIdx].p2 = p2;
    D_800704A8.slots[slotIdx].mode = 3;
    D_800704A8.slots[slotIdx].submode = 0;
    return 3;
}

/**
 * Pop a timer halfword, four parameter halfwords (@c q1 / @c q2 / @c p1 /
 * @c p2), and a slot index; arm @c slots[slotIdx] with @c mode = 4 and
 * the popped values. Used by movement-with-vector mode opcodes.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_LSCROLL3(Eline *eline) {
    u16 timer = (u16)POP(eline);
    u16 p2 = (u16)POP(eline);
    u16 p1 = (u16)POP(eline);
    u16 q2 = (u16)POP(eline);
    u16 q1 = (u16)POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].q1 = q1;
    D_800704A8.slots[slotIdx].q2 = q2;
    D_800704A8.slots[slotIdx].p1 = p1;
    D_800704A8.slots[slotIdx].p2 = p2;
    D_800704A8.slots[slotIdx].timer = timer;
    D_800704A8.slots[slotIdx].mode = 4;
    D_800704A8.slots[slotIdx].submode = 0;
    return 2;
}

/**
 * Variant of @c opHandler_LSCROLL3 that arms @c mode = 5.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_CSCROLL3(Eline *eline) {
    u16 timer = (u16)POP(eline);
    u16 p2 = (u16)POP(eline);
    u16 p1 = (u16)POP(eline);
    u16 q2 = (u16)POP(eline);
    u16 q1 = (u16)POP(eline);
    s32 slotIdx = POP(eline);
    D_800704A8.slots[slotIdx].q1 = q1;
    D_800704A8.slots[slotIdx].q2 = q2;
    D_800704A8.slots[slotIdx].p1 = p1;
    D_800704A8.slots[slotIdx].p2 = p2;
    D_800704A8.slots[slotIdx].timer = timer;
    D_800704A8.slots[slotIdx].mode = 5;
    D_800704A8.slots[slotIdx].submode = 0;
    return 2;
}

/**
 * @brief Pop a byte from the stack and store to D_80070652.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 opHandler_BGCLEAR(Eline *eline) {
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
s32 opHandler_SETTIMER(Eline *eline) {
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
s32 opHandler_GETTIMER(Eline *eline) {
    volatile GameState *gs = &g_gameState;
    eline->resultSlots[0] = gs->mainData.battleStateFlag;
    return 2;
}

/**
 * Trigger camera-shake mode: pop two intensity bytes into
 * @c g_fieldVars->cameraShakeY / @c cameraShakeX, arm the related
 * field flag bits and the battle-config bit, then drive
 * @c setCameraShakeParams + @c setCameraVibrateState(1).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_DISPTIMER(Eline *eline) {
    g_fieldVars->cameraShakeY = POP_BYTE(eline);
    g_fieldVars->cameraShakeX = POP_BYTE(eline);
    g_fieldVars->stateFlags |= 0x40;
    g_fieldVars->fieldB6 |= 0x4;
    g_battleConfig.unk2 |= 0x4;
    setCameraShakeParams(g_fieldVars->cameraShakeX, g_fieldVars->cameraShakeY);
    setCameraVibrateState(1);
    return 2;
}

/**
 * Stop camera-shake mode: clear the @c stateFlags bit 0x40, the
 * @c fieldB6 bit 0x4 and the @c g_battleConfig.unk2 bit 0x4, then call
 * @c setCameraVibrateState(0). Inverse of @c opHandler_DISPTIMER.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_KILLTIMER(Eline *eline) {
    g_fieldVars->stateFlags &= ~0x40;
    g_fieldVars->fieldB6 &= ~0x4;
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
s32 opHandler_SHADETIMER(Eline *eline) {
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
s32 opHandler_WORLDMAPJUMP(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        if (D_800704A8.unk1A2 != 0) {
            eline->stackPtr -= 3;
        } else {
            D_800704A8.mode = 7;
            D_800704A8.anim_state = (u16)POP(eline);
            D_800704A8.rotation = (u16)POP(eline);
            D_800704A8.counter = (u16)POP(eline);
            func_800B4F40();
        }
    }
    return 1;
}

/**
 * Pop a value (masked to 2 bits) into the global mode byte @c D_8007064E
 * and then fire @c func_800A97E4(i, 0x25, 0, 0) once per active
 * @c Eline slot (count from @c D_80085388).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_SETCAMERA(Eline *eline) {
    s32 i;
    D_8007064E = POP(eline) & 0x3;
    for (i = 0; i < D_80085388; i++) {
        func_800A97E4(i, 0x25, 0, 0);
    }
    return 2;
}

/** @brief Pop byte, store to global D_8007064F. Returns 2. */
s32 opHandler_SETDCAMERA(Eline *eline) {
    *(u8 *)D_8007064F = POP_BYTE(eline);
    return 2;
}

/**
 * Find the first free slot in @c D_8005F0F8->entries (the entry whose
 * @c field16 is the sentinel @c 0x7FFF), pop four halfwords into its
 * @c field04 / @c field06 / @c field08 / @c field16, set @c field14 =
 * @c 0xFFFF to mark the slot armed, and re-arm the next entry's
 * @c field16 to @c 0x7FFF as the new sentinel.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_PREMAPJUMP(Eline *eline) {
    s32 t = 0;
    if (D_8005F0F8->entries[0].field16 != 0x7FFF) {
        do {
            t++;
        } while (D_8005F0F8->entries[t].field16 != 0x7FFF);
    }
    D_8005F0F8->entries[t].field08 = (u16)POP(eline);
    D_8005F0F8->entries[t].field06 = (u16)POP(eline);
    D_8005F0F8->entries[t].field04 = (u16)POP(eline);
    D_8005F0F8->entries[t].field16 = (u16)POP(eline);
    D_8005F0F8->entries[t].field14 = 0xFFFF;
    D_8005F0F8->entries[t + 1].field16 = 0x7FFF;
    return 2;
}

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
s32 opHandler_MAPJUMP(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 1;
        D_800704A8.rotation = a1;
        D_800704A8.anim_state = (u16)POP(eline);
        D_800704A8.position_y = (u16)POP(eline);
        D_800704A8.position_x = (u16)POP(eline);
        D_800704A8.counter = (u16)POP(eline);
    }
    return 1;
}

/**
 * Variant of @c opHandler_MAPJUMP that pops one extra halfword into
 * @c unk008 (between @c unk00E and @c unk006).
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied mode-1 trigger word.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 opHandler_MAPJUMP3(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 1;
        D_800704A8.rotation = a1;
        D_800704A8.anim_state = (u16)POP(eline);
        D_800704A8.unk008 = (u16)POP(eline);
        D_800704A8.position_y = (u16)POP(eline);
        D_800704A8.position_x = (u16)POP(eline);
        D_800704A8.counter = (u16)POP(eline);
    }
    return 1;
}

/**
 * Mode-6 init: identical structure to @c opHandler_MAPJUMP3 but sets
 * @c mode = 6 and also flags @c slotActive[0] = 1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param a1    Dispatcher-supplied mode-6 trigger word.
 * @return 1 (yield to dispatcher without advancing PC).
 */
s32 opHandler_DISCJUMP(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 6;
        D_800704A8.unk1A0 = 1;
        D_800704A8.rotation = a1;
        D_800704A8.anim_state = (u16)POP(eline);
        D_800704A8.unk008 = (u16)POP(eline);
        D_800704A8.position_y = (u16)POP(eline);
        D_800704A8.position_x = (u16)POP(eline);
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
s32 opHandler_MAPJUMPO(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800704A8.mode = 1;
        D_800704A8.anim_state = 0;
        D_800704A8.rotation = (u16)POP(eline);
        D_800704A8.position_x = 0x7FFF;
        D_800704A8.position_y = 0x7FFF;
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
s32 opHandler_MAPJUMPON(Eline *eline) {
    D_8007064A = 0;
    return 2;
}

/**
 * Sets the global byte D_8007064A to 1, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 opHandler_MAPJUMPOFF(Eline *eline) {
    D_8007064A = 1;
    return 2;
}

/**
 * Sets the global byte D_8007064D to 1, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 opHandler_MAPFADEOFF(Eline *eline) {
    D_8007064D = 1;
    return 2;
}

/**
 * Clears the global byte D_8007064D, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 opHandler_MAPFADEON(Eline *eline) {
    D_8007064D = 0;
    return 2;
}

/**
 * Sets the global byte D_8007064B to 1, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 opHandler_MENUDISABLE(Eline *eline) {
    D_8007064B = 1;
    return 2;
}

/**
 * Clears the global byte D_8007064B, returns 2.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 2 (continue processing).
 */
s32 opHandler_MENUENABLE(Eline *eline) {
    D_8007064B = 0;
    return 2;
}

/** @brief Set D_800704A8 command to 5, clear halfword, copy entity byte 0xD1. Returns 3. */
s32 opHandler_MENUNORMAL(void) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0;
    D_800704A8.unk1AB = g_fieldVars->fieldD1;
    return 3;
}

/**
 * Sets D_800704A8 to 5, sets the halfword at D_800704A8+2 to 1,
 * sets the byte at D_800704A8+0x1AB to 2, returns 3.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 3.
 */
s32 opHandler_MENUPHS(Eline *eline) {
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
s32 opHandler_MENUSHOP(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x17;
    D_800704A8.unk1AB = POP_BYTE(eline);
    return 3;
}

/**
 * Mode-5 init dispatched by a 21-case switch on the popped value. The
 * counter halfword @c D_800704AA is set per case (mostly @c val+2 with
 * two paired swaps and special cases for @c 19 = chocobo-world enable
 * and @c 20 = stand-alone counter 28). Cases 3..18 also call
 * @c setGfExists for the corresponding GF id; the @c 8 / @c 9 and
 * @c 14 / @c 15 pairs are deliberately written swapped in source to
 * match the original case body order.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3 (yield to dispatcher with state change).
 */
s32 opHandler_MENUNAME(Eline *eline) {
    s32 val;
    D_800704A8.mode = 5;
    D_800704A8.unk1AB = 1;
    val = POP(eline);
    switch (val) {
    case 0:  D_800704AA = 2;  break;
    case 1:  D_800704AA = 3;  break;
    case 2:  D_800704AA = 4;  break;
    case 3:  D_800704AA = 5;  setGfExists(0);  break;
    case 4:  D_800704AA = 6;  setGfExists(1);  break;
    case 5:  D_800704AA = 7;  setGfExists(2);  break;
    case 6:  D_800704AA = 8;  setGfExists(3);  break;
    case 7:  D_800704AA = 9;  setGfExists(4);  break;
    case 9:  D_800704AA = 10; setGfExists(5);  break;
    case 8:  D_800704AA = 11; setGfExists(6);  break;
    case 10: D_800704AA = 12; setGfExists(7);  break;
    case 11: D_800704AA = 13; setGfExists(8);  break;
    case 12: D_800704AA = 14; setGfExists(9);  break;
    case 13: D_800704AA = 15; setGfExists(0xA); break;
    case 15: D_800704AA = 16; setGfExists(0xB); break;
    case 14: D_800704AA = 17; setGfExists(0xC); break;
    case 16: D_800704AA = 18; setGfExists(0xD); break;
    case 17: D_800704AA = 19; setGfExists(0xE); break;
    case 18: D_800704AA = 20; setGfExists(0xF); break;
    case 19: D_800704AA = 27; enableChocoboWorld(); break;
    case 20: D_800704AA = 28; break;
    }
    return 3;
}

/**
 * Sets D_800704A8 to 5, sets the halfword at D_800704A8+2 to 0x1A,
 * sets the byte at D_800704A8+0x1AB to 1, returns 3.
 *
 * @param eline Pointer to the Eline event-script context (unused).
 * @return 3.
 */
s32 opHandler_MENUTUTO(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x1A;
    D_800704A8.unk1AB = 1;
    return 3;
}

/**
 * Mode-5 init with counter 0x1D — variant of @c opHandler_MENUSHOP.
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
s32 opHandler_MENUTIPS(Eline *eline) {
    setFieldFlag(POP(eline) & 0x7F);
    return 2;
}

/**
 * Calls func_80037240 and returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_REST(Eline *eline) {
    func_80037240(eline);
    return 2;
}

/**
 * Calls func_800ADC04 and returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_DYING(Eline *eline) {
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
s32 opHandler_SETHP(Eline *eline) {
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
s32 opHandler_GETHP(Eline *eline) {
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
s32 opHandler_MENUSAVE(Eline *eline) {
    D_800704A8.mode = 5;
    D_800704A8.counter = 0x18;
    D_800704A8.unk1AB = 1;
    return 3;
}

/**
 * Pops a parameter from the stack. If nonzero, sets bit 0x01 in
 * g_fieldVars+0xD1. Otherwise clears it. Returns 2.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_SAVEENABLE(Eline *eline) {
    if (POP(eline) != 0) {
        g_fieldVars->fieldD1 |= 0x01;
    } else {
        g_fieldVars->fieldD1 &= ~0x01;
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
    if (g_fieldVars->stateFlags & 0x200) {
        g_fieldVars->fieldD1 &= 0xFD;
    } else {
        g_fieldVars->fieldD1 |= 0x2;
    }
}

/**
 * Pop a flag value. Nonzero clears @c stateFlags bit @c 0x200; zero
 * sets bit @c 0x200 and additionally clears bit @c 0x02 of @c fieldD1.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_PHSPOWER(Eline *eline) {
    if (POP(eline) != 0) {
        g_fieldVars->stateFlags &= ~0x200;
    } else {
        g_fieldVars->stateFlags |= 0x200;
        g_fieldVars->fieldD1 &= ~0x02;
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
s32 opHandler_PHSENABLE(Eline *eline) {
    s32 val = POP(eline);
    if (!(g_fieldVars->stateFlags & 0x200)) {
        if (val != 0) {
            g_fieldVars->fieldD1 |= 0x02;
        } else {
            g_fieldVars->fieldD1 &= ~0x02;
        }
    }
    return 2;
}

/**
 * Pop a retry count, an action code, and a character ID; look up the
 * character's party slot via @c func_80037C6C, and if that character
 * is in the active party, call @c func_800211B4(partyMember, code)
 * up to @p count times, exiting early on the first non-zero return.
 * Always returns 2 — the action is purely side-effecting.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_ADDMAGIC(Eline *eline) {
    s32 count = POP(eline);
    s32 val2 = POP(eline);
    s32 charId = POP(eline);
    s32 slot = func_80037C6C(charId);
    if (slot != 0xFF) {
        s32 i;
        for (i = 0; i < count; i++) {
            if (func_800211B4(g_gameState.mainData.party.party[slot], val2) != 0) {
                return 2;
            }
        }
    }
    return 2;
}

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
s32 opHandler_ADDITEM(Eline *eline) {
    s32 val1, val2;

    val1 = POP(eline);
    val2 = POP(eline);
    addItemToInventory(val1, val2);
    return 2;
}

/**
 * Pop a gil delta and add it to @c g_gameState.mainData.party.gil,
 * clamped to the FF gil cap of 99,999,999. Mirror the new total into
 * @c g_fieldVars->gilMirror.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_ADDGIL(Eline *eline) {
    s32 delta = POP(eline);
    g_gameState.mainData.party.gil += delta;
    if (g_gameState.mainData.party.gil > 0x05F5E0FE) {
        g_gameState.mainData.party.gil = 0x05F5E0FF;
    }
    g_fieldVars->gilMirror = g_gameState.mainData.party.gil;
    return 2;
}

/**
 * Variant of @c opHandler_ADDGIL that operates on @c dreamGil instead of
 * @c gil and mirrors the result into @c g_fieldVars->dreamGilMirror.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_ADDPASTGIL(Eline *eline) {
    s32 delta = POP(eline);
    g_gameState.mainData.party.dreamGil += delta;
    if (g_gameState.mainData.party.dreamGil > 0x05F5E0FE) {
        g_gameState.mainData.party.dreamGil = 0x05F5E0FF;
    }
    g_fieldVars->dreamGilMirror = g_gameState.mainData.party.dreamGil;
    return 2;
}

/**
 * Pop a delta and add it to @c g_fieldVars->seedExp, clamping the
 * resulting value to the legal SeeD-rank range [@c 100, @c 3100].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_ADDSEEDLEVEL(Eline *eline) {
    s32 delta = POP(eline);
    g_fieldVars->seedExp += delta;
    if ((s16)g_fieldVars->seedExp < 100) {
        g_fieldVars->seedExp = 100;
    } else if ((s16)g_fieldVars->seedExp >= 3100) {
        g_fieldVars->seedExp = 3100;
    }
    return 2;
}

/**
 * Pops a parameter, calls markItemPresent, stores result in resultSlots[0].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_GETCARD(Eline *eline) {
    eline->resultSlots[0] = markItemPresent(POP(eline));
    return 2;
}

/**
 * Pops two values, calls modifyItemQuantity(val1, val2), stores result in resultSlots[0].
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (continue processing).
 */
s32 opHandler_SETCARD(Eline *eline) {
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
s32 opHandler_HOWMANYCARD(Eline *eline) {
    eline->resultSlots[0] = func_80023B14(POP(eline));
    return 2;
}
