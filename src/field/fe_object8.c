#include "common.h"
#include "field.h"
#include "gamestate.h"

/*
 * Per-TU prototype overrides. Each of these is a single defined
 * function (mostly in fe_object1.c / fe_object6.c), but the declared
 * signature affects caller codegen — different TUs need different
 * prototypes for byte-matching output. @c func_800B2864 is actually
 * defined with 2 args in fe_object6.c yet this TU's target asm shows
 * 4 register args passed at the @c jal, so the prototype here lies
 * about the arity to make gcc emit those arg-register loads.
 */
extern void func_800A97E4(s32 spatialIdx, s32 a1, s32 a2, s32 a3);
extern void func_800A8DAC(u8 spatialIdx, s32 cmd, u32 arg, void *out);
extern void func_800B2864(Eline *eline, s32 channel, s32 a2, s32 a3);
extern s32  func_8009E604();

/**
 * Clear bits @c 0x180000 and set bit @c 0x200000 in @c flags, then call
 * @c func_800A97E4(spatialIndex, 0x2F, 0, 0).
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (advance PC).
 */
s32 func_800B9078(Eline *eline) {
    eline->flags = (eline->flags & 0xFFE7FFFF) | 0x200000;
    func_800A97E4(eline->field_0x256, 0x2F, 0, 0);
    return 2;
}

/**
 * Update entity flags by clearing bits @c 0x300000 and setting bit
 * @c 0x80000. Then pop one byte from the script stack and write it to
 * the byte at offset @c 0x263 of the entity. Finally call
 * @c func_800A97E4 with the entity's @c spatialIndex (offset 0x256),
 * opcode @c 0x27, @c 0, and the popped byte.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (advance PC).
 */
s32 func_800B90C0(Eline *eline) {
    u8 byte;

    eline->flags = (eline->flags & 0xFFCFFFFF) | 0x80000;
    byte = POP_BYTE(eline);
    eline->field_0x263 = byte;
    func_800A97E4(eline->field_0x256, 0x27, 0, eline->field_0x263);
    return 2;
}

/**
 * @brief Dispatch motion command 0xD with the given byte arg.
 *
 * Sends command @c 0xD to the entity at @c field_0x256 (via the global
 * cmd table @c func_800AA46C) carrying the sign-extended low byte of
 * @p a1, mirrors @p a1 into @c field_0x24E, zeroes the live motion
 * halfwords (@c field_0x206/20A), snapshots the render slot's
 * @c unk0C into @c field_0x20C, clears the same render slot's
 * @c unk52, and clears the @c 0xF800 flag band. Called by the RANIME
 * family handlers in this file.
 */
void func_800B912C(Eline *eline, s16 a1) {
    func_800AA46C(eline->field_0x256, 0xD, a1, 0);
    eline->field_0x24E = a1;
    eline->field_0x206 = 0;
    eline->field_0x20A = 0;
    eline->field_0x20C = D_800D9630[eline->field_0x256]->unk0C;
    D_800D9630[eline->field_0x256]->unk52 = eline->field_0x206;
    eline->flags &= ~0xF800;
}

/**
 * @brief CANIME-family helper — queue curved animation + dispatch cmd 0xD.
 *
 * Asserts the active entity in @c D_800DE4FC matches @c eline->field_0x256
 * (infinite-loops on mismatch). Encodes the popped halfwords @p a2 and
 * @p a3 as @c (n-1)<<4 into @c field_0x20A and @c field_0x20C, mirrors
 * the byte arg into @c field_0x24E, copies @c field_0x20A into the live
 * motion halfword @c field_0x206 and into the render slot's @c unk52,
 * then dispatches motion command @c 0xD with byte arg @p a1 via
 * @c func_800AA46C and clears flag band @c 0xF800.
 */
void func_800B91D8(Eline *eline, s32 a1, s32 a2, s32 a3) {
    if (D_800DE4FC != eline->field_0x256) {
        while (1) {}
    }
    eline->field_0x20A = (a2 - 1) << 4;
    eline->field_0x20C = (a3 - 1) << 4;
    eline->field_0x24E = a1;
    eline->field_0x206 = eline->field_0x20A;
    D_800D9630[eline->field_0x256]->unk52 = eline->field_0x20A;
    func_800AA46C(eline->field_0x256, 0xD, a1, 0);
    eline->flags &= ~0xF800;
}

/**
 * @brief Per-frame motion tick for the entity.
 *
 * Advances @c field_0x206 (current position) by @c field_0x208 (step)
 * each frame. When @c flags & 0x80 is set and the entity is in
 * @c msgActive == 1, fires @c func_800B2864 hooks if the half-range
 * boundary or zero-crossing on @c (val - 0x80) flips during the step.
 *
 * When @c field_0x206 reaches @c field_0x20C, dispatches based on
 * flag bits 0x2000 (loop-rewind), 0x8000 (loop-step-back), or
 * 0x4000 (one-shot via @c func_800B912C), and sets the completion
 * flag 0x800. Otherwise clears flag 0x800. Always writes
 * @c field_0x206 into the render slot's @c unk52 at the end (unless
 * the early @c 0x4 disable bit was set on entry).
 */
void func_800B9288(Eline *eline) {
    s32 flags;
    s32 newPos;
    s32 oldSigned;
    s32 oldMid;
    s32 newMid;
    s32 halfRange;
    s32 s1_v;
    s32 s2_v;

    flags = eline->flags;
    if (flags & 0x4) {
        return;
    }
    if (!(flags & 0x1000)) {
        u16 oldUnsigned = eline->field_0x206;
        oldSigned = (s16)eline->field_0x206;
        newPos = oldUnsigned;
        newPos = newPos + (u16)eline->field_0x208;
        eline->field_0x206 = newPos;
        if ((eline->flags & 0x80) && eline->msgActive == 1) {
            newMid = (s16)newPos - 0x80;
            oldMid = 0x80;
            oldMid = oldSigned - oldMid;
            halfRange = ((s16)eline->field_0x20C - (s16)eline->field_0x208) >> 1;
            s2_v = newMid - halfRange;
            s1_v = oldMid - halfRange;
            if ((((u32)newMid >> 31) & ((u32)~oldMid >> 31))
                || (((u32)oldMid >> 31) & ((u32)~newMid >> 31))) {
                func_800B2864(eline, 1, 0x40, 0x80);
            }
            if ((((u32)s2_v >> 31) & ((u32)~s1_v >> 31))
                || (((u32)s1_v >> 31) & ((u32)~s2_v >> 31))) {
                func_800B2864(eline, 0, 0x40, 0x80);
            }
        }
    }
    if ((s16)eline->field_0x206 >= (s16)eline->field_0x20C) {
        flags = eline->flags;
        if (flags & 0x2000) {
            eline->field_0x206 = eline->field_0x20A;
        } else if (flags & 0x8000) {
            eline->flags = (flags & ~0xF800) | 0x1000;
            eline->field_0x20C -= eline->field_0x208;
            eline->field_0x206 = eline->field_0x20C;
        } else if (flags & 0x4000) {
            func_800B912C(eline, eline->field_0x24F);
            eline->flags = (eline->flags & ~0xF800) | 0x2000;
        }
        eline->flags |= 0x800;
    } else {
        eline->flags &= ~0x800;
    }
    D_800D9630[eline->field_0x256]->unk52 = eline->field_0x206;
}

/**
 * @brief Snapshot live animation state into the 0x210-0x216 backup slots.
 *
 * Copies the low halfword of @c flags into @c field_0x20E, mirrors
 * @c field_0x24E into @c field_0x24D, and saves the four halfwords
 * @c field_0x206/208/20A/20C into @c field_0x210/212/214/216.
 */
s32 func_800B9488(Eline *eline) {
    eline->field_0x20E = eline->flags;
    eline->field_0x24D = eline->field_0x24E;
    eline->field_0x210 = eline->field_0x206;
    eline->field_0x212 = eline->field_0x208;
    eline->field_0x214 = eline->field_0x20A;
    eline->field_0x216 = eline->field_0x20C;
    return 2;
}

/**
 * @brief Restore animation state from the 0x210-0x216 backup slots.
 *
 * Inverse of @c func_800B9488: copies the saved halfwords back into
 * the live motion state (@c field_0x206/208/20A/20C), mirrors
 * @c field_0x24D into @c field_0x24E, dispatches motion command
 * @c 0xD via @c func_800AA46C, copies @c field_0x206 into the render
 * slot's @c unk52, and restores the saved 0xF800 flag band from
 * @c field_0x20E.
 */
s32 func_800B94C0(Eline *eline) {
    eline->field_0x24E = eline->field_0x24D;
    eline->field_0x206 = eline->field_0x210;
    eline->field_0x208 = eline->field_0x212;
    eline->field_0x20A = eline->field_0x214;
    eline->field_0x20C = eline->field_0x216;
    func_800AA46C(eline->field_0x256, 0xD, eline->field_0x24E, 0);
    D_800D9630[eline->field_0x256]->unk52 = eline->field_0x206;
    eline->flags &= ~0xF800;
    eline->field_0x20E &= 0xF800;
    eline->flags |= eline->field_0x20E;
    return 2;
}

/**
 * @brief Pop the top stack slot as a halfword into @c field_0x208.
 */
s32 func_800B9570(Eline *eline) {
    eline->field_0x208 = POP(eline);
    return 2;
}

/**
 * @brief Returns 2 if the animation-complete flag (0x800) is set, else 1.
 */
s32 func_800B95A0(Eline *eline) {
    if (eline->flags & 0x800) {
        return 2;
    }
    return 1;
}

/**
 * Call @c func_800B912C with the entity's byte at offset @c 0x24F as
 * the second arg, then set bit @c 0x2000 in @c flags. Returns @c 3.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 3.
 */
s32 func_800B95C0(Eline *eline) {
    func_800B912C(eline, eline->field_0x24F);
    eline->flags |= 0x2000;
    return 3;
}

/**
 * @brief ANIME opcode 0x02D handler — start animation and wait for completion.
 *
 * While the entity's @c activeMask bit is set: call @c func_800B912C with
 * the sign-extended @p a1, set the animation-active flag (0x4000), and
 * return 1 (yield, retry next frame). On subsequent frames, the entity
 * is no longer active for this script group; return 3 once flag 0x800
 * (animation complete) is set, else keep yielding with return 1.
 */
s32 func_800B9604(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        func_800B912C(eline, (s16)a1);
        eline->flags |= 0x4000;
    } else if (eline->flags & 0x800) {
        return 3;
    }
    return 1;
}

/**
 * @brief ANIMEKEEP opcode 0x02E handler — start animation (keep variant).
 *
 * Same shape as @c func_800B9604 (ANIME) but sets flag bit @c 0x8000
 * instead of @c 0x4000 to preserve the final frame after completion.
 */
s32 func_800B9678(Eline *eline, s32 a1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        func_800B912C(eline, (s16)a1);
        eline->flags |= 0x8000;
    } else if (eline->flags & 0x800) {
        return 3;
    }
    return 1;
}

/**
 * @brief CANIME opcode 0x02F handler — start curved animation, wait for completion.
 *
 * Blocking variant of RCANIME. While the entity's @c activeMask bit is
 * set: pop two signed halfwords from the stack, dispatch via
 * @c func_800B91D8 with the bytecode arg, set the animation-active
 * flag (0x4000), and yield. Once the script-group bit clears, check
 * flag 0x800 (animation complete) to decide between return 3 and
 * return 1.
 */
s32 func_800B96EC(Eline *eline, s32 a1) {
    s32 v2;
    s32 v1;
    s32 tmp;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        v2 = POP_HALF(eline);
        v1 = POP_HALF(eline);
        func_800B91D8(eline, a1, v2, v1);
        tmp = eline->flags | 0x4000;
        eline->flags = tmp;
    } else if (eline->flags & 0x800) {
        return 3;
    }
    return 1;
}

/**
 * @brief CANIMEKEEP opcode 0x030 handler — same as CANIME but flag 0x8000.
 */
s32 func_800B9798(Eline *eline, s32 a1) {
    s32 v2;
    s32 v1;
    s32 tmp;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        v2 = POP_HALF(eline);
        v1 = POP_HALF(eline);
        func_800B91D8(eline, a1, v2, v1);
        tmp = eline->flags | 0x8000;
        eline->flags = tmp;
    } else if (eline->flags & 0x800) {
        return 3;
    }
    return 1;
}

/**
 * Sign-extend @p a1 to @c s16 and call @c func_800B912C, then set bit
 * @c 0x4000 in @c flags. Returns @c 3.
 */
s32 func_800B9844(Eline *eline, s32 a1) {
    func_800B912C(eline, (s16)a1);
    eline->flags |= 0x4000;
    return 3;
}

/**
 * Sign-extend @p a1 to @c s16 and call @c func_800B912C, then set bit
 * @c 0x8000 in @c flags. Returns @c 3.
 */
s32 func_800B9888(Eline *eline, s32 a1) {
    func_800B912C(eline, (s16)a1);
    eline->flags |= 0x8000;
    return 3;
}

/**
 * @brief RCANIME opcode 0x033 handler — fire curved animation, return 3.
 *
 * Non-blocking variant of CANIME: pops two signed halfwords from the
 * bytecode stack (top → @c v2, next → @c v1), calls @c func_800B91D8
 * with them and the bytecode arg, sets animation-active flag (0x4000),
 * then returns 3 to advance the script PC immediately.
 */
s32 func_800B98CC(Eline *eline, s32 a1) {
    s32 v2;
    s32 v1;
    s32 tmp;

    v2 = POP_HALF(eline);
    v1 = POP_HALF(eline);
    func_800B91D8(eline, a1, v2, v1);
    tmp = eline->flags | 0x4000;
    eline->flags = tmp;
    return 3;
}

/**
 * @brief RCANIMEKEEP opcode 0x034 handler — like RCANIME but sets flag 0x8000.
 */
s32 func_800B9944(Eline *eline, s32 a1) {
    s32 v2;
    s32 v1;
    s32 tmp;

    v2 = POP_HALF(eline);
    v1 = POP_HALF(eline);
    func_800B91D8(eline, a1, v2, v1);
    tmp = eline->flags | 0x8000;
    eline->flags = tmp;
    return 3;
}

/**
 * Sign-extend @p a1 to @c s16 and call @c func_800B912C, then set bit
 * @c 0x2000 in @c flags. Returns @c 3.
 */
s32 func_800B99BC(Eline *eline, s32 a1) {
    func_800B912C(eline, (s16)a1);
    eline->flags |= 0x2000;
    return 3;
}

/**
 * @brief RCANIMELOOP opcode 0x036 handler — like RCANIME but sets flag 0x2000.
 */
s32 func_800B9A00(Eline *eline, s32 a1) {
    s32 v2;
    s32 v1;
    s32 tmp;

    v2 = POP_HALF(eline);
    v1 = POP_HALF(eline);
    func_800B91D8(eline, a1, v2, v1);
    tmp = eline->flags | 0x2000;
    eline->flags = tmp;
    return 3;
}

/**
 * @brief Pop 3 bytes into the 0x18A vector slot and dispatch cmd 0x10.
 *
 * Pops three bytes from the script stack — top goes to @c eline->unk18A
 * byte+2 (= offset 0x18C), then byte+1 (= 0x18B), then byte+0 (= 0x18A).
 * If @c D_800DE8CC bit @c 0x2 is clear, dispatches command @c 0x10 to
 * the entity at @c field_0x256 with the pointer to the 3-byte vector.
 */
s32 func_800B9A78(Eline *eline) {
    ((u8 *)&eline->unk18A)[2] = POP_BYTE(eline);
    ((u8 *)&eline->unk18A)[1] = POP_BYTE(eline);
    ((u8 *)&eline->unk18A)[0] = POP_BYTE(eline);
    if (!(D_800DE8C8[1] & 0x2)) {
        func_800A97E4(eline->field_0x256, 0x10, (s32)&eline->unk18A, 0);
    }
    return 2;
}

/**
 * @brief Broadcast 3 popped bytes into the unk18A vector of every active entity.
 *
 * Pops three bytes from the script stack into a local buffer, then
 * walks @c D_80085224[0..D_80085388-1] writing the same triple into
 * each entry's @c unk18A/18B/18C slot. Each iteration where
 * @c D_800DE8CC & 0x2 is clear also dispatches command @c 0x10 to
 * the entity (with the local buffer as the arg).
 */
s32 func_800B9B24(Eline *eline) {
    u8 bytes[3];
    s32 i;
    Eline *p;

    bytes[2] = POP_BYTE(eline);
    bytes[1] = POP_BYTE(eline);
    bytes[0] = POP_BYTE(eline);

    p = D_80085224;
    for (i = 0; i < D_80085388; i++) {
        if (!(D_800DE8C8[1] & 0x2)) {
            func_800A97E4(i, 0x10, (s32)bytes, 0);
        }
        ((u8 *)&p->unk18A)[0] = bytes[0];
        ((u8 *)&p->unk18A)[1] = bytes[1];
        ((u8 *)&p->unk18A)[2] = bytes[2];
        p++;
    }
    return 2;
}

/**
 * @brief Pop a byte into @c field_0x257 and mirror to the active render slot.
 *
 * Pops one byte from the script stack, stores it into
 * @c eline->field_0x257. If @c D_800DE8CC bit @c 0x2 is clear,
 * also stores the same byte into the active entity's render slot
 * (@c D_800D9630[D_800DE4FC]->unk61).
 */
s32 func_800B9C58(Eline *eline) {
    u8 byte = POP_BYTE(eline);
    eline->field_0x257 = byte;
    if (!(D_800DE8CC & 0x2)) {
        D_800D9630[D_800DE4FC]->unk61 = byte;
    }
    return 2;
}

/**
 * Pop one halfword from the script stack and store it into
 * @c eline->field_0x220. If @c D_800DE8CC bit @c 0x2 is clear, also
 * store the same halfword into the entity's render slot at
 * @c D_800D9630[eline->field_0x256]->unk62.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (advance PC).
 */
s32 func_800B9CBC(Eline *eline) {
    u16 half = POP(eline);
    *(volatile u16 *)&eline->field_0x220 = half;
    if (!(D_800DE8CC & 0x2)) {
        D_800D9630[eline->field_0x256]->unk62 = half;
    }
    return 2;
}

/**
 * Pop value, divide by 4 (signed, round toward zero), store to 8 entity bytes.
 *
 * @param a0 Pointer to the script/object structure.
 * @return 2 (continue processing).
 */
s32 func_800B9D20(Eline *eline) {
    s32 val = POP(eline) / 4;
    eline->field_0x25C = val;
    eline->field_0x25B = val;
    eline->field_0x25A = val;
    eline->field_0x259 = val;
    eline->field_0x260 = val;
    eline->field_0x25F = val;
    eline->field_0x25E = val;
    eline->field_0x25D = val;
    return 2;
}

/**
 * Pop 8 values from the script stack, divide each by 4 (signed,
 * round-toward-zero), and store them as bytes into the entity's
 * direction-table fields @c field_0x259..field_0x260.
 *
 * Store order: @c 0x25C, @c 0x25B, @c 0x25A, @c 0x259, @c 0x260,
 * @c 0x25F, @c 0x25E, @c 0x25D — the 8-entry vertical strip is
 * filled outward from the centre.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (advance PC).
 */
s32 func_800B9D7C(Eline *eline) {
    eline->field_0x25C = POP(eline) / 4;
    eline->field_0x25B = POP(eline) / 4;
    eline->field_0x25A = POP(eline) / 4;
    eline->field_0x259 = POP(eline) / 4;
    eline->field_0x260 = POP(eline) / 4;
    eline->field_0x25F = POP(eline) / 4;
    eline->field_0x25E = POP(eline) / 4;
    eline->field_0x25D = POP(eline) / 4;
    return 2;
}

/**
 * @brief Pop a byte from the script stack into @c field_0x261.
 */
s32 func_800B9F28(Eline *eline) {
    eline->field_0x261 = POP_BYTE(eline);
    return 2;
}

/**
 * @brief Pop a byte from the script stack into @c field_0x241.
 */
s32 func_800B9F58(Eline *eline) {
    eline->field_0x241 = POP_BYTE(eline);
    return 2;
}

/**
 * @brief Conditionally activate a message based on the entity's active bit.
 *
 * If bit @c scriptGroup of @c activeMask is set, peek the top three
 * stack slots, shift each left by 12, and write them as the message's
 * @c textPtr / @c posX / @c posY (fixed-point). Also clears
 * @c msgState, @c windowId, @c savedChannel and sets @c msgActive=1.
 * Returns 1 (wait for message) without popping.
 *
 * If the bit is clear, the message is skipped: @c msgState=2,
 * @c msgActive=0, the current @c msgChannel is preserved into
 * @c savedChannel, and three stack entries are discarded.
 * Returns 2 (advance PC).
 *
 * @param eline Pointer to the Eline event-script context.
 */
s32 func_800B9F88(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive    = 1;
        eline->msgState     = 0;
        eline->windowId     = 0;
        eline->savedChannel = 0;
        eline->msgTextPtr = eline->stack[(s8)eline->stackPtr - 2] << 12;
        eline->msgPosX    = eline->stack[(s8)eline->stackPtr - 1] << 12;
        eline->msgPosY    = eline->stack[(s8)eline->stackPtr]     << 12;
        return 1;
    }
    eline->msgState     = 2;
    eline->msgActive    = 0;
    eline->stackPtr    -= 3;
    eline->savedChannel = eline->msgChannel;
    return 2;
}

/**
 * @brief Pop an entity index, dispatch the bearing-resolver, store byte to @c field_0x241.
 *
 * Pops one s32 from the script stack and uses it as an index into
 * @c D_80085230 to fetch a target @c Eline. Calls @c func_8009E604 with
 * the current entity and the target, and writes the byte result into
 * @c field_0x241 (seeds turn-state for the next CTURN-family helper).
 */
s32 func_800BA034(Eline *eline) {
    eline->field_0x241 = func_8009E604(eline, D_80085230[POP(eline)]);
    return 2;
}

/**
 * @brief Variant of @c func_800BA034 that looks up the target via the
 *        SeeD party-member slot table.
 *
 * Pops one s32 from the stack as a slot index, uses it to read
 * @c g_seedState->memberSlot[slot], indexes that into the entity
 * array @c D_80085224 to fetch a target @c Eline, dispatches
 * @c func_8009E604 with the current entity and the target, and writes
 * the byte result into @c field_0x241.
 *
 * @param eline Pointer to the Eline event-script context.
 * @return 2 (advance PC).
 */
s32 func_800BA09C(Eline *eline) {
    s32 slot = POP(eline);
    eline->field_0x241 = func_8009E604(eline, &D_80085224[g_seedState->memberSlot[slot]]);
    return 2;
}

/**
 * @brief Helper — pop byte + halfword, queue turn (subtract variant, kind 1).
 *
 * Same shape as @c func_800BA1D0 but with reversed comparison: subtracts
 * @c 0x100 from the heading if @c field_0x1DC is less than @c (s16)raw.
 * Sets @c field_0x244 to @c 1.
 */
s32 func_800BA120(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 1;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        if (eline->field_0x1DC < (s16)raw) {
            eline->field_0x1DE = raw - 0x100;
        }
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Helper — pop byte + halfword, queue turn state, wrap to +0x100 if needed.
 *
 * Active path: pops a byte (top), then sets up turn state with
 * @c field_0x244=1, @c field_0x243=0, @c field_0x1DC seeded from
 * @c field_0x241, @c field_0x242 set to the popped byte. Then pops
 * the next halfword as the requested heading, stores it to
 * @c field_0x1DE, and if the signed value is less than @c field_0x1DC
 * adds @c 0x100 (forces forward rotation).
 *
 * Inactive path: return 2 unless @c field_0x244 == 3, otherwise 1.
 */
s32 func_800BA1D0(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 1;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        if ((s16)raw < eline->field_0x1DC) {
            eline->field_0x1DE = raw + 0x100;
        }
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Helper — pop byte + halfword, queue turn (subtract variant, kind 2).
 *
 * Same as @c func_800BA120 but sets @c field_0x244 to @c 2.
 */
s32 func_800BA280(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 2;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        if (eline->field_0x1DC < (s16)raw) {
            eline->field_0x1DE = raw - 0x100;
        }
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Helper — pop byte + halfword, queue turn (add variant, kind 2).
 *
 * Same as @c func_800BA1D0 but sets @c field_0x244 to @c 2.
 */
s32 func_800BA330(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 2;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        if ((s16)raw < eline->field_0x1DC) {
            eline->field_0x1DE = raw + 0x100;
        }
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * Adjust halfword at 0x1DE based on distance from 0x1DC.
 * If abs(0x1DE - 0x1DC) >= 0x81, adjusts by +/- 0x100.
 * Also clears byte at 0x243.
 *
 * @param a0 Pointer to the script/object structure.
 */
/**
 * @brief Wrap the target heading to the shorter arc when |target-current|>0x80.
 *
 * Computes the absolute heading delta between @c field_0x1DC (current) and
 * @c field_0x1DE (target). If the gap is more than half a turn (@c >=0x81),
 * adjusts @c field_0x1DE by @c ±0x100 so the rotation takes the short way
 * around. Also clears the per-step kind byte @c field_0x243.
 */
void func_800BA3E0(Eline *eline) {
    s16 de = eline->field_0x1DE;
    s16 dc = eline->field_0x1DC;
    u16 deu = eline->field_0x1DE;
    s32 diff = de - dc;
    if (diff < 0) {
        diff = -diff;
    }
    eline->field_0x243 = 0;
    if (diff >= 0x81) {
        if (dc < de) {
            eline->field_0x1DE = deu - 0x100;
        } else {
            eline->field_0x1DE = deu + 0x100;
        }
    }
}

/**
 * @brief Op 0x082 handler — pop byte+halfword, queue turn (kind 1, shortest path).
 *
 * Same as @c func_800BA1D0 but delegates the heading-wrap math to
 * @c func_800BA3E0 (which picks the shortest rotation direction).
 * Sets @c field_0x244 to @c 1.
 */
s32 func_800BA424(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 1;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        func_800BA3E0(eline);
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Op 0x083 handler — identical to @c func_800BA424.
 */
s32 func_800BA4D4(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 1;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        func_800BA3E0(eline);
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief CTURNR opcode 0x084 handler — same shape as @c func_800BA424 with kind 2.
 */
s32 func_800BA584(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 2;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        func_800BA3E0(eline);
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief CTURNL opcode 0x085 handler — identical to CTURNR (@c func_800BA584).
 */
s32 func_800BA634(Eline *eline) {
    s32 raw;
    u8 byte1;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        byte1 = POP_BYTE(eline);
        eline->field_0x244 = 2;
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x242 = byte1;
        raw = (u16)POP(eline);
        eline->field_0x1DE = raw;
        func_800BA3E0(eline);
    } else if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Op 0x???  — start an N-step rotation if the entity's bit is set.
 *
 * If the entity's @c activeMask bit (selected by @c scriptGroup) is set,
 * pops two values: the target bearing byte (@c first) and a SeeD-party
 * member index. Looks up the target Eline via @c D_80085230[idx],
 * snapshots @c field_0x241 into @c field_0x1DC, dispatches
 * @c func_8009E604 to compute the target bearing into @c field_0x1DE,
 * and writes @c first into @c field_0x242. If the snapshot matches the
 * new bearing (no turn needed) returns @c 2; otherwise marks
 * @c field_0x244 = 1, dispatches @c func_800BA3E0, and returns @c 1.
 *
 * If the bit is clear and @c field_0x244 == 3, returns @c 2 (turn
 * complete). Otherwise returns @c 1 (wait).
 *
 * @param eline Pointer to the Eline event-script context.
 */
s32 func_800BA6E4(Eline *eline) {
    s32 first;
    s32 idx;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        first = POP(eline);
        idx = POP(eline);
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x1DE = func_8009E604(eline, D_80085230[idx]) & 0xFF;
        eline->field_0x242 = first;
        if (eline->field_0x1DC == eline->field_0x1DE) {
            return 2;
        }
        eline->field_0x244 = 1;
        func_800BA3E0(eline);
        return 1;
    }
    if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Variant of @c func_800BA6E4 that marks the turn state as @c 2.
 *
 * Same control flow as @c func_800BA6E4, but writes @c field_0x244 = 2
 * (instead of @c 1) when a turn needs to start. The "turn complete"
 * sentinel is still @c 3 in the bit-clear branch.
 *
 * @param eline Pointer to the Eline event-script context.
 */
s32 func_800BA7DC(Eline *eline) {
    s32 first;
    s32 idx;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        first = POP(eline);
        idx = POP(eline);
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x1DE = func_8009E604(eline, D_80085230[idx]) & 0xFF;
        eline->field_0x242 = first;
        if (eline->field_0x1DC == eline->field_0x1DE) {
            return 2;
        }
        eline->field_0x244 = 2;
        func_800BA3E0(eline);
        return 1;
    }
    if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Variant of @c func_800BA6E4 that looks up the target via the
 *        SeeD party-member slot table.
 *
 * Same control flow as @c func_800BA6E4, but the index popped after
 * the bearing byte is treated as a SeeD-party slot. The target Eline
 * is obtained via @c &D_80085224[g_seedState->memberSlot[slot]].
 *
 * @param eline Pointer to the Eline event-script context.
 */
s32 func_800BA8D4(Eline *eline) {
    s32 first;
    s32 slot;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        first = POP(eline);
        slot = POP(eline);
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x1DE = func_8009E604(eline, &D_80085224[g_seedState->memberSlot[slot]]) & 0xFF;
        eline->field_0x242 = first;
        if (eline->field_0x1DC == eline->field_0x1DE) {
            return 2;
        }
        eline->field_0x244 = 1;
        func_800BA3E0(eline);
        return 1;
    }
    if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Variant of @c func_800BA8D4 that marks the turn state as @c 2.
 *
 * Same as @c func_800BA8D4 but writes @c field_0x244 = 2 when a turn
 * needs to start.
 *
 * @param eline Pointer to the Eline event-script context.
 */
s32 func_800BA9E8(Eline *eline) {
    s32 first;
    s32 slot;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        first = POP(eline);
        slot = POP(eline);
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x1DE = func_8009E604(eline, &D_80085224[g_seedState->memberSlot[slot]]) & 0xFF;
        eline->field_0x242 = first;
        if (eline->field_0x1DC == eline->field_0x1DE) {
            return 2;
        }
        eline->field_0x244 = 2;
        func_800BA3E0(eline);
        return 1;
    }
    if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief One-shot bearing-set variant.
 *
 * Similar pop+lookup pattern as @c func_800BA8D4 / @c func_800BA9E8.
 * If the entity's group bit is set, pops the bearing byte and a SeeD
 * party-slot index, computes the target bearing via @c func_8009E604,
 * and either marks @c field_0x244 = 3 (no turn needed) or starts a
 * turn (@c field_0x244 = 2, dispatch @c func_800BA3E0).
 *
 * Always returns @c 2 — the caller never has to wait.
 *
 * @param eline Pointer to the Eline event-script context.
 */
s32 func_800BAAFC(Eline *eline) {
    s32 first;
    s32 slot;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        first = POP(eline);
        slot = POP(eline);
        eline->field_0x243 = 0;
        eline->field_0x1DC = eline->field_0x241;
        eline->field_0x1DE = func_8009E604(eline, &D_80085224[g_seedState->memberSlot[slot]]) & 0xFF;
        eline->field_0x242 = first;
        if (eline->field_0x1DC == eline->field_0x1DE) {
            eline->field_0x244 = 3;
        } else {
            eline->field_0x244 = 2;
            func_800BA3E0(eline);
        }
    }
    return 2;
}

/**
 * @brief Returns 2 once the turn-state kind byte reaches 3, otherwise 1.
 */
s32 func_800BABFC(Eline *eline) {
    if (eline->field_0x244 == 3) {
        return 2;
    }
    return 1;
}

/**
 * @brief Returns 2 once the queued facing matches the current facing.
 *
 * @p arg1 is ignored; it exists in the prototype because every field
 * opcode handler is called with the dispatcher's @c (eline, arg1)
 * pair, and several wrappers in this file forward their own @c arg1
 * straight through (e.g. @c func_800BADCC) so the function pointer
 * call lands with a real value in @c a1.
 */
s32 func_800BAC18(Eline *eline, s32 arg1) {
    if (eline->field_0x234 == eline->field_0x236) {
        return 2;
    }
    return 1;
}

/**
 * @brief Op 0x108 handler — like @c func_800BB140 then dispatch via @c func_800BAC18.
 *
 * Pops 4 halfwords into @c field_0x22A/0x22E/0x232/0x234 (clearing
 * @c field_0x236 and @c field_0x23B), then tail-calls
 * @c func_800BAC18 to apply the queued state.
 */
s32 func_800BAC38(Eline *eline, s32 arg1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        eline->field_0x232 = POP(eline);
        eline->field_0x22E = POP(eline);
        eline->field_0x22A = POP(eline);
        eline->field_0x236 = 0;
        eline->field_0x23B = 0;
    }
    return func_800BAC18(eline, arg1);
}

/**
 * @brief Op 0x0FD handler — pop 4 halfwords into facing slot then dispatch.
 *
 * Like @c func_800BB1F0 (active path) but ends by calling
 * @c func_800BAC18 to apply the queued facing state.
 */
s32 func_800BAD00(Eline *eline, s32 arg1) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        eline->field_0x226 = POP(eline);
        eline->field_0x224 = POP(eline);
        eline->field_0x222 = POP(eline);
        eline->field_0x236 = 0;
        eline->field_0x23B = 1;
    }
    return func_800BAC18(eline, arg1);
}

/**
 * @brief Snapshot the target entity's grid-cell position into the
 *        queued turn-state fields, then dispatch @c func_800BAC18.
 *
 * If the entity's group bit is set, pops a bearing halfword (saved to
 * @c field_0x234) and a field-entity index, queries the navigation
 * helper @c func_800A8DAC at the target's spatial index, divides the
 * target's @c posX / @c posY / @c posZ by 4096 (signed, round toward
 * zero) into @c field_0x222 / @c field_0x224, and combines posZ with
 * the queried halfword (@c buf[2]) into @c field_0x226. Clears
 * @c field_0x236 and sets @c field_0x23B to @c 1 to mark the data ready.
 *
 * Always dispatches @c func_800BAC18 at the end (which compares the
 * queued bearing against the current one) and returns its result.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param arg1  Forwarded as the second argument to @c func_800BAC18.
 */
s32 func_800BADCC(Eline *eline, s32 arg1) {
    s32 idx;
    s16 buf[4];

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        idx = POP(eline);
        func_800A8DAC(D_80085230[idx]->field_0x256, 0x1E, D_800C71F8, buf);
        eline->field_0x222 = D_80085230[idx]->posX / 4096;
        eline->field_0x224 = D_80085230[idx]->posY / 4096;
        eline->field_0x226 = buf[2] + D_80085230[idx]->posZ / 4096;
        eline->field_0x23B = 1;
        eline->field_0x236 = 0;
    }
    return func_800BAC18(eline, arg1);
}

/**
 * @brief Variant of @c func_800BADCC that resolves the target via the
 *        SeeD party-member slot table.
 *
 * Same body as @c func_800BADCC, but the index popped from the stack
 * is treated as a SeeD party-slot. The target entity is the one whose
 * Eline lives at @c &D_80085224[g_seedState->memberSlot[slot]], and
 * that same byte index is the spatial argument to @c func_800A8DAC.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param arg1  Forwarded as the second argument to @c func_800BAC18.
 */
s32 func_800BAF14(Eline *eline, s32 arg1) {
    u8 slot;
    s16 buf[4];

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        slot = g_seedState->memberSlot[POP(eline)];
        func_800A8DAC(slot, 0x1E, D_800C71F8, buf);
        eline->field_0x222 = D_80085224[slot].posX / 4096;
        eline->field_0x224 = D_80085224[slot].posY / 4096;
        eline->field_0x226 = buf[2] + D_80085224[slot].posZ / 4096;
        eline->field_0x23B = 1;
        eline->field_0x236 = 0;
    }
    return func_800BAC18(eline, arg1);
}

/**
 * @brief Queue a relative offset turn target.
 *
 * If the entity's group bit is set, pops one halfword as the target
 * bearing (saved to @c field_0x234), then queries
 * @c func_800A8DAC(field_0x256, @c 0x20, @c buf, @c 0) to fill three
 * halfwords describing the relative offset. Each entry is divided by
 * @c 16 (signed, round toward zero) and stored to @c field_0x22A /
 * @c field_0x22E / @c field_0x232. Clears @c field_0x236 and
 * @c field_0x23B to keep this as a step-relative (not snapshot) turn.
 *
 * Always dispatches @c func_800BAC18 at the end.
 *
 * @param eline Pointer to the Eline event-script context.
 * @param arg1  Forwarded as the second argument to @c func_800BAC18.
 */
s32 func_800BB05C(Eline *eline, s32 arg1) {
    s16 buf[4];

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        ((void (*)(u8, s32, void *, void *))func_800A8DAC)(eline->field_0x256, 0x20, buf, 0);
        eline->field_0x22A = buf[0] / 16;
        eline->field_0x22E = buf[1] / 16;
        eline->field_0x232 = buf[2] / 16;
        eline->field_0x236 = 0;
        eline->field_0x23B = 0;
    }
    return func_800BAC18(eline, arg1);
}

/**
 * @brief Helper that pops 4 halfwords and stores them as a facing-state block.
 *
 * While the entity's @c activeMask bit is set: pops four halfwords from
 * the script stack (top → @c field_0x234, then @c field_0x232, then
 * @c field_0x22E, then @c field_0x22A); clears @c field_0x236 and
 * @c field_0x23B. Returns 2.
 */
s32 func_800BB140(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        eline->field_0x232 = POP(eline);
        eline->field_0x22E = POP(eline);
        eline->field_0x22A = POP(eline);
        eline->field_0x236 = 0;
        eline->field_0x23B = 0;
    }
    return 2;
}

/**
 * @brief Helper that pops 4 halfwords into another facing-state slot.
 *
 * Like @c func_800BB140 but stores into @c field_0x222/0x224/0x226/0x234
 * and sets @c field_0x23B to 1 (instead of 0). Returns 2.
 */
s32 func_800BB1F0(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x234 = POP(eline);
        eline->field_0x226 = POP(eline);
        eline->field_0x224 = POP(eline);
        eline->field_0x222 = POP(eline);
        eline->field_0x236 = 0;
        eline->field_0x23B = 1;
    }
    return 2;
}
