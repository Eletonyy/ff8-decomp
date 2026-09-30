#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/kernel.h"
#include "battle.h"
#include "thread.h"

INCLUDE_ASM("asm/nonmatchings/thread", func_80026ADC);


INCLUDE_ASM("asm/nonmatchings/thread", func_80026CA0);


INCLUDE_ASM("asm/nonmatchings/thread", func_80026CF0);


INCLUDE_ASM("asm/nonmatchings/thread", func_80026D10);


INCLUDE_ASM("asm/nonmatchings/thread", func_80026D8C);


INCLUDE_ASM("asm/nonmatchings/thread", func_80026E20);


INCLUDE_ASM("asm/nonmatchings/thread", func_80026E70);


/**
 * @brief Open a thread with interrupt protection.
 * @param a0 Thread program counter / entry point address.
 * @param a1 Thread stack pointer.
 * @return Thread handle from OpenTh.
 * @note Wraps PsyQ OpenTh with func_800472E4/func_800472F4 (likely interrupt disable/enable).
 */
s32 openThreadSafe(s32 a0, s32 a1) {
    s32 result;
    func_800472E4(a0);
    result = OpenTh(a0, a1, 0);
    func_800472F4();
    return result;
}


/**
 * @brief Close a thread with interrupt protection.
 * @param a0 Thread handle to close.
 * @note Wraps PsyQ CloseTh with func_800472E4/func_800472F4 (likely interrupt disable/enable).
 */
void closeThreadSafe(s32 a0) {
    func_800472E4(a0);
    CloseTh(a0);
    func_800472F4();
}


/**
 * @brief Manually switch the current thread without a syscall.
 *
 * Resolves the thread control block for @p handle through the kernel
 * table of tables and, if that thread is active, stores it as the
 * current thread in the TCB header.
 *
 * @param handle Thread handle (low 24 bits index the TCB table).
 */
void func_80026F4C(s32 handle) {
    ToT *tot = KERNEL_TOT;
    TCB *tcbTable = (TCB *)tot[2].head;
    TCBH *tcbh = (TCBH *)tot[1].head;
    TCB *tcb = &tcbTable[handle & 0xFFFFFF];
    if (tcb->status == TcbStACTIVE) {
        tcbh->entry = tcb;
    }
}


/**
 * @brief Compute an address offset from a 24-bit color value.
 *
 * Masks the input to 24 bits, multiplies by 192, and adds to the base
 * address loaded from kernel memory at 0x110.
 *
 * @param a0 Input value (only lower 24 bits used).
 * @return Base address + masked input * 192.
 */
s32 getThreadControlBlock(s32 a0) {
    s32 base = *(s32 *)0x110;
    a0 &= 0xFFFFFF;
    return base + a0 * 192;
}


/** @brief Wrapper that calls func_80047384 (returns interrupt/thread status). */
void getInterruptStatus(void) { func_80047384(); }


INCLUDE_ASM("asm/nonmatchings/thread", func_80026FD4);


/**
 * @brief Switch to a thread, using a fallback address if a0 is 0.
 * @param a0 Thread handle to switch to; 0 defaults to 0xFF000000.
 * @note If func_80047384 returns bit 2 set, uses func_80026F4C instead of PsyQ ChangeTh.
 */
void switchThread(s32 a0) {
    if (a0 == 0) {
        a0 = (s32)0xFF000000;
    }
    if (func_80047384() & 4) {
        func_80026F4C(a0);
    } else {
        ChangeTh(a0);
    }
}


INCLUDE_ASM("asm/nonmatchings/thread", func_80027038);


INCLUDE_ASM("asm/nonmatchings/thread", func_800270B0);


/**
 * @brief Poll the CD stream state and update a pad port accordingly.
 *
 * Queries the current stream status via func_8003AC10 and stores it in
 * @c field1A, then dispatches on it:
 * - 1: flag the port active (field0A) and pending (field19).
 * - 2: same, but also clear vibrationMask.
 * - 4, 5: no-op.
 * - 6: on the first tick after a pending flag (field19 == 1), start the
 *      stream (func_800270B0, func_8003AF50, func_8003AFD0); on later
 *      ticks refresh field01/field02 from motor[0]/motor[1] masked by
 *      vibrationMask.
 * - other (0, 3, out of range): reset to active/pending with a zero vibrationMask.
 *
 * @param a0 Base context passed through to func_800270B0.
 * @param port Port to update.
 * @param a2 Stream channel selector passed to the CD helpers.
 */
void func_80027220(s32 a0, PadPort *port, s32 a2) {
    s32 status;
    func_80047384();
    status = func_8003AC10(a2);
    port->field1A = status;
    switch (status) {
    case 1:
        port->field0B = 0;
        port->field0A = 1;
        port->field19 = 1;
        break;
    case 2:
        port->field0B = 0;
        port->field19 = 1;
        port->field0A = 1;
        port->vibrationMask = 0;
        break;
    case 4:
    case 5:
        break;
    case 6: {
        u8 mask = port->vibrationMask;
        if (port->field19 == 1) {
            func_800270B0(a0, port, a2);
            port->field0A = 1;
            port->field19 = 0;
            port->field01 = 0;
            port->field02 = 0;
            func_8003AF50(a2, port->fieldBC);
            func_8003AFD0(a2, port, 6);
        } else {
            u8 f6 = port->motor[0];
            u8 f7 = port->motor[1];
            u8 sh7 = port->field09;
            u8 sh6;
            s32 v6;
            s32 v7;
            /* The do-while(0) wrapper is load-bearing for the byte-match: it
               keeps the scheduler from tearing the statement pair apart,
               and likely mirrors a macro in the original source. */
            do { port->field0A = 1; v6 = f6 & mask; } while (0);
            v7 = f7 & mask;
            sh6 = port->field08;
            port->field02 = v7 >> sh7;
            port->field01 = v6 >> sh6;
        }
        break;
    }
    case 0:
    case 3:
    default:
        port->field0B = 0;
        port->field0A = 1;
        port->vibrationMask = 0;
        port->field19 = 1;
        break;
    }
}


/**
 * @brief Check the CD stream status, restarting playback if it has ended.
 *
 * @param a0 Stream channel selector.
 * @return 1 if the stream is in state 2, the result of func_8003AF88 if
 *         it is in state 6, 0 otherwise.
 */
s32 func_80027360(s32 a0) {
    s32 status;
    s32 ret;
    status = func_8003AC10(a0);
    ret = 0;
    switch (status) {
    case 2:
        ret = 1;
        break;
    case 6:
        ret = func_8003AF88(a0, 1, 1);
        break;
    case 0:
    case 1:
    case 3:
    case 4:
    case 5:
        break;
    }
    return ret;
}


/**
 * @brief Mark a pad port as active.
 * @param idx Index into g_engine.ports.
 */
void activateBattleAnim(s32 idx) {
    PadPort *port = &g_engine.ports[idx];
    port->field19 = 1;
    port->field0A = 1;
}


/**
 * @brief Initialize two consecutive PadPort entries via func_80027220.
 * @param a0 Base address of the first entry.
 * @note Initializes the first entry with mode 0, and the second (at +0xC4) with mode 0x10.
 */
void initBattleAnimPair(s32 a0) {
    func_80027220(a0, (PadPort *)a0, 0);
    func_80027220(a0, (PadPort *)(a0 + 0xC4), 0x10);
}


/**
 * @brief Step both pad ports until they settle.
 *
 * For each port: clears both motor levels, then repeatedly waits on
 * cdReadStatusWrapper and steps the port via func_80027220 until its state
 * (field1A) is 0 or 2, or has read 6 twice.
 */
void func_80027448(void) {
    PadPort *port = &g_engine.ports[0];
    PadPort *ports = g_engine.ports;
    s32 count;
    /* Without this barrier the register allocator gives s0 to count
       instead of port. */
    REGALLOC_BARRIER(port);
    count = 0;
    port->motor[0] = 0;
    port->motor[1] = 0;
    for (;;) {
        while (cdReadStatusWrapper() == 0) {}
        func_80027220((s32)ports, port, 0);
        if (port->field1A == 0 || port->field1A == 2) {
            break;
        }
        if (port->field1A == 6) {
            count++;
            if (count >= 2) {
                break;
            }
        }
    }
    port = &ports[1];
    count = 0;
    port->motor[0] = 0;
    port->motor[1] = 0;
    for (;;) {
        while (cdReadStatusWrapper() == 0) {}
        func_80027220((s32)ports, port, 0x10);
        if (port->field1A == 0 || port->field1A == 2) {
            break;
        }
        if (port->field1A == 6) {
            count++;
            if (count >= 2) {
                break;
            }
        }
    }
}


/**
 * @brief Copy a rectangle's bounds to g_engine clip region.
 *
 * Copies x, y from @p rect to clipLeft/clipTop, and computes
 * x+w-1, y+h-1 for clipRight/clipBottom.
 *
 * @param rect Source clip rectangle.
 */
void setBattleAnimClipRect(RECT *rect) {
    g_engine.clipLeft = rect->x;
    g_engine.clipTop = rect->y;
    g_engine.clipRight = rect->x + rect->w - 1;
    g_engine.clipBottom = rect->y + rect->h - 1;
}


/**
 * @brief Read field0B from a pad port.
 * @param idx Port index.
 */
s32 getBattleAnimField0B(s32 idx) {
    PadPort *port = &g_engine.ports[idx];
    return port->field0B;
}


INCLUDE_ASM("asm/nonmatchings/thread", func_800275D4);


/**
 * @brief Get an animation frame parameter from a linked entity's frame buffer.
 * @param idx Port index (masked to 0 or 1).
 * @param offset Frame offset subtracted from the current frame counter.
 * @return field02 of the resolved AnimFrame.
 * @note Resolves a secondary port via linkedIdx, then indexes into its
 *       frames[] circular buffer using (frameCounter - offset) & 7.
 */
u16 getAnimFrameParam(s32 idx, s32 offset) {
    PadPort *ports;
    PadPort *port;
    PadPort *linked;
    s32 sub_idx;
    idx &= 1;
    ports = g_engine.ports;
    port = ports + idx;
    linked = ports + port->linkedIdx;
    sub_idx = (linked->frameCounter - offset) & 7;
    return ((AnimFrame *)((u8 *)linked + 0x1C))[sub_idx].field02;
}


/**
 * @brief Get combined status flags from a linked entity's animation frame.
 * @param idx Port index (masked to 0 or 1).
 * @param offset Frame offset subtracted from the current frame counter.
 * @return Bitwise OR of field08, field0A, field0C, field0E in the resolved AnimFrame.
 * @note Same lookup as getAnimFrameParam but ORs 4 adjacent u16 values.
 */
u16 getAnimFrameStatusFlags(s32 idx, s32 offset) {
    PadPort *ports;
    PadPort *port;
    PadPort *linked;
    s32 sub_idx;
    AnimFrame *frame;
    idx &= 1;
    ports = g_engine.ports;
    port = ports + idx;
    linked = ports + port->linkedIdx;
    sub_idx = (linked->frameCounter - offset) & 7;
    frame = &linked->frames[sub_idx];
    return frame->field08 | frame->field0A | frame->field0C | frame->field0E;
}


/**
 * @brief Get field10 from a linked entity's animation frame.
 *
 * Same lookup pattern as getAnimFrameParam: resolves via linkedIdx,
 * indexes into frames[] circular buffer, but returns field10.
 *
 * @param idx Port index (only bit 0 used).
 * @param offset Frame offset subtracted from the current frame counter.
 * @return field10 of the resolved AnimFrame.
 */
u16 func_80027A58(s32 idx, s32 offset) {
    PadPort *ports;
    PadPort *port;
    PadPort *linked;
    s32 sub_idx;
    idx &= 1;
    ports = g_engine.ports;
    port = ports + idx;
    linked = ports + port->linkedIdx;
    sub_idx = (linked->frameCounter - offset) & 7;
    return ((AnimFrame *)((u8 *)linked + 0x1C))[sub_idx].field10;
}


/**
 * @brief Get field12 from a linked entity's animation frame.
 *
 * Same lookup pattern as getAnimFrameParam: resolves via linkedIdx,
 * indexes into frames[] circular buffer, but returns field12.
 *
 * @param idx Port index (only bit 0 used).
 * @param offset Frame offset subtracted from the current frame counter.
 * @return field12 of the resolved AnimFrame.
 */
u16 func_80027AC8(s32 idx, s32 offset) {
    PadPort *ports;
    PadPort *port;
    PadPort *linked;
    s32 sub_idx;
    idx &= 1;
    ports = g_engine.ports;
    port = ports + idx;
    linked = ports + port->linkedIdx;
    sub_idx = (linked->frameCounter - offset) & 7;
    return ((AnimFrame *)((u8 *)linked + 0x1C))[sub_idx].field12;
}


/**
 * @brief Compute angle from (a0, a1) with special-case axis handling.
 *
 * Returns a fixed angle when either component is zero:
 * - (0, <=0): 0xC00 (270 degrees)
 * - (0, >0):  0x400 (90 degrees)
 * - (<0, 0):  0x800 (180 degrees)
 * - (>0, 0):  0x000 (0 degrees)
 * Otherwise delegates to ratan2.
 *
 * @param a0 X component (horizontal).
 * @param a1 Y component (vertical).
 * @return Angle in fixed-point (0x1000 = 360 degrees).
 */
s32 computeAngle(s32 a0, s32 a1) {
    s32 v0;
    if (a0 != 0) goto a0_nonzero;
    v0 = 0xC00;
    if (a1 <= 0) goto end;
    v0 = 0x400;
    goto end;
a0_nonzero:
    v0 = (a0 < 1);
    if (a1 != 0) goto both_nonzero;
    v0 <<= 11;
    goto end;
both_nonzero:
    v0 = ratan2(a0, a1);
end:
    return v0;
}


/**
 * @brief Map an angle to a quadrant bit mask.
 *
 * Rotates the angle by -0x600, inverts it, and reduces the result to a
 * quadrant index 0-3 (each quadrant spanning 0x400 units), returning a
 * single bit in the range 0x1000-0x8000.
 *
 * @param a0 Angle in fixed-point (0x1000 = 360 degrees).
 * @return 1 << (quadrant + 12).
 */
s32 func_80027B7C(s32 a0) {
    s32 v = a0 + 0x600;
    v = ~v & 0xFFF;
    v = v / 0x400;
    return 1 << (v + 0xC);
}

/**
 * @brief Look up a battle animation byte through a two-level table.
 *
 * Uses the low bit of a0 as an index into g_engine.ports (stride 196).
 * Reads the byte at offset 0xC2 of that entry as a second index,
 * then returns bits [6:0] of offset 0xC3 of the second entry, shifted left 8.
 *
 * @param a0 Slot selector (only bit 0 used).
 * @return (g_engine.ports[slot].fieldC3 & 0x7F) << 8.
 */
s32 getBattleAnimLinkedValue(s32 a0) {
    s32 slot;
    a0 &= 1;
    slot = g_engine.ports[a0].linkedIdx;
    return (g_engine.ports[slot].fieldC3 & 0x7F) << 8;
}


/**
 * @brief Set the low 7 bits of a linked entity's fieldC3 from a squared level.
 *
 * Clamps @p a1 to [0, 0x7F], squares it, and divides by 256, giving a
 * quadratic response curve. The result replaces bits [6:0] of the linked
 * entity's fieldC3 while preserving the high bit
 * (see setBattleAnimLinkedHighBit).
 *
 * @param a0 Port index (only bit 0 used).
 * @param a1 Input level, clamped to [0, 0x7F].
 */
void func_80027C00(s32 a0, s32 a1) {
    s32 slot;
    PadPort *linked;
    s32 sq;
    s32 clamped;
    sq = a1;
    a0 &= 1;
    slot = g_engine.ports[a0].linkedIdx;
    linked = &g_engine.ports[slot];
    clamped = (sq < 0) ? 0 : (sq > 0x7F) ? 0x7F : a1;
    sq = clamped * clamped;
    sq = sq / 0x100;
    linked->fieldC3 = (linked->fieldC3 & 0x80) | sq;
}


/**
 * @brief Set or clear the high bit (0x80) of a linked entity's fieldC3.
 *
 * Looks up g_engine.ports[a0 & 1], follows its linkedIdx to a
 * second entity, then sets bit 7 of fieldC3 if a1 is nonzero, or clears
 * it if a1 is zero.
 *
 * @param a0 Port index (only bit 0 used).
 * @param a1 If nonzero, set bit 7; if zero, clear bit 7.
 */
void setBattleAnimLinkedHighBit(s32 a0, s32 a1) {
    s32 slot;
    PadPort *linked;
    a0 &= 1;
    slot = g_engine.ports[a0].linkedIdx;
    linked = &g_engine.ports[slot];
    if (a1) {
        linked->fieldC3 |= 0x80;
    } else {
        linked->fieldC3 &= 0x7F;
    }
}


INCLUDE_ASM("asm/nonmatchings/thread", func_80027CF8);


INCLUDE_ASM("asm/nonmatchings/thread", func_80027DB4);


/**
 * @brief Get a pad port's vibration mask.
 * @param idx Port index (masked to 0 or 1).
 * @return 0xFF with vibration on, 0 with it off.
 */
s32 getPadVibration(s32 idx) {
    PadPort *port = &g_engine.ports[idx & 1];
    return port->vibrationMask;
}


