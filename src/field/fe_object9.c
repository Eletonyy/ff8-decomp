#include "common.h"
#include "field.h"
#include "gamestate.h"

extern u8 *D_800704C0;
extern s32 D_800DE4DC;
extern u8 D_800DE8D2;
extern u8 D_80085398[];
extern u8 D_80085300[];

extern s32 getSfxGlobalFlag(void);
extern void setSfxGlobalFlag(s32 idx);
extern void startSfxSlow(s32 idx);
extern void fadeOutSfxSlow(s32 idx);
extern s32 getSfxField1C(s32 idx);
extern s32 func_8002CE84(s32 idx);
extern u8 *func_8003974C(u8 *base, s32 idx);
extern s32 func_8002E680(u8 *text);
extern void func_8002E064(s32 idx, s16 *rect);
extern void func_8002D784(s32 a0, u8 *text, s32 a2, s32 a3, s32 a4, s32 a5);
extern void func_800BC258(s16 *rect);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB2A4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB3D8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB510);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB5E0);

/**
 * @brief Query relative offset (kind 0x20) and split into entity halfwords.
 *
 * Calls @c func_800A8DAC with mode @c 0x20 to fetch three relative-offset
 * halfwords for the active spatial entity. Stores @c buf[1]/16 into
 * @c field_0x22C, @c buf[2]/16 into @c field_0x230, and clears
 * @c field_0x228. (@c buf[0] is queried but discarded.)
 */
s32 func_800BB650(Eline *eline) {
    s16 buf[4];

    func_800A8DAC(eline->field_0x256, 0x20, (u8 *)buf, 0);
    eline->field_0x228 = 0;
    eline->field_0x22C = buf[1] / 16;
    eline->field_0x230 = buf[2] / 16;
    return 2;
}

/**
 * @brief Snapshot the 12-halfword dialog-state block from @c D_800704A8
 *        into @c g_seedState.
 *
 * Copies @c D_800704A8.dialogState..field_0x11E (12 halfwords) into
 * @c g_seedState->dialogStateMirror..fieldEE. The first four writes
 * are emitted out of source order (dst@D8, DC, DA, DE) for codegen
 * matching; the remaining eight are sequential.
 */
void func_800BB6C8(void) {
    SeedState   *dst = g_seedState;
    SystemState *src = &D_800704A8;

    dst->dialogStateMirror = src->dialogState;
    dst->fieldDC           = src->dialogCount;
    dst->fieldDA           = src->dialogTimer;
    dst->fieldDE           = src->field_0x10E;
    dst->fieldE0           = src->field_0x110;
    dst->fieldE2           = src->field_0x112;
    dst->fieldE4           = src->field_0x114;
    dst->fieldE6           = src->field_0x116;
    dst->fieldE8           = src->field_0x118;
    dst->fieldEA           = src->field_0x11A;
    dst->fieldEC           = src->field_0x11C;
    dst->fieldEE           = src->field_0x11E;
}

/**
 * @brief Initialise the dialog-state block and mirror it to @c g_seedState.
 *
 * Sets the global mode marker @c D_800DE8D2 to @c 2 and seeds the
 * 6-halfword dialog state at @c D_800704A8.dialogState..field_0x112
 * with the boot values (state=2, timer=0xFF, count=0x10, then 0xFF
 * sentinels). @c func_800BB6C8 copies the whole 12-halfword block
 * into @c g_seedState->dialogStateMirror..fieldEE.
 */
s32 func_800BB768(void) {
    D_800DE8D2 = 2;
    D_800704A8.dialogState  = 2;
    D_800704A8.dialogTimer  = 0xFF;
    D_800704A8.dialogCount  = 0x10;
    D_800704A8.field_0x10E  = 0xFF;
    D_800704A8.field_0x110  = 0xFF;
    D_800704A8.field_0x112  = 0xFF;
    func_800BB6C8();
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB7BC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB810);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB888);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB8B4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB90C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB958);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BB9A8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBA3C);


INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBB20);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBBB4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBC08);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBC64);

/**
 * @brief Wait for the dialog-state countdown to match the timer.
 *
 * Returns @c 1 while @c D_800704A8+0x10C (countdown) and @c +0x10A
 * (target/timer) differ. Once they match, copy the current dialog
 * state word at @c +0x108 into @c g_seedState->dialogStateMirror and return @c 2.
 */
s32 func_800BBDA8(void) {
    if (D_800704A8.dialogCount != D_800704A8.dialogTimer) {
        return 1;
    }
    g_seedState->dialogStateMirror = D_800704A8.dialogState;
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBDE0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBE50);

/**
 * @brief Force the dialog state to @c 4 and mirror it into @c g_seedState->dialogStateMirror.
 *
 * The @c volatile cast is required for matching codegen: the asm reads
 * the just-written value back through memory rather than reusing the
 * literal @c 4.
 */
s32 func_800BBE78(void) {
    SystemState *src = &D_800704A8;
    *(volatile u16 *)&src->dialogState = 4;
    g_seedState->dialogStateMirror = *(volatile u16 *)&src->dialogState;
    return 2;
}

/**
 * @brief Pop two stack slots and dispatch @c func_8002E1B4 with them.
 *
 * Pops two values from the script stack and calls
 * @c func_8002E1B4(val2 & 7, val1) — val1 is the top slot, val2 the
 * next. The @c & @c 7 mask suggests @c val2 is a 3-bit selector
 * (e.g. SFX channel id).
 */
s32 func_800BBEA4(Eline *eline) {
    s32 val1 = POP(eline);
    s32 val2 = POP(eline);
    func_8002E1B4(val2 & 7, val1);
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BBEFC);

/**
 * @brief Peek top two stack slots and pass them to @c setSfxPitch.
 *
 * Reads @c stack[ptr-1] and @c stack[ptr] without decrementing
 * @c stackPtr, then calls @c setSfxPitch(stack[ptr-1], stack[ptr]).
 */
s32 func_800BBFFC(Eline *eline) {
    s8 idx = (s8)eline->stackPtr;
    setSfxPitch(eline->stack[idx - 1], eline->stack[idx]);
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC034);

/**
 * @brief Write one 16-byte entry into the @c D_80085300 table.
 *
 * Each entry is laid out as four input halfwords (copied from
 * @c src) followed by an @c s32 payload (@c val) at @c +0x8. The
 * trailing @c +0xC..+0xF bytes are left untouched.
 *
 * @param idx  Entry index (stride 16 bytes).
 * @param val  s32 payload stored at @c entry+0x8.
 * @param src  4-halfword source block, copied to @c entry+0x0..+0x7.
 */
void func_800BC12C(s32 idx, s32 val, u8 *src) {
    u8 *base = D_80085300;
    u8 *entry;
    idx <<= 4;
    entry = base + idx;
    *(s32 *)(entry + 0x8) = val;
    *(u16 *)(entry + 0x0) = *(u16 *)(src + 0x0);
    *(u16 *)(entry + 0x2) = *(u16 *)(src + 0x2);
    *(u16 *)(entry + 0x4) = *(u16 *)(src + 0x4);
    *(u16 *)(entry + 0x6) = *(u16 *)(src + 0x6);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC170);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC258);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC2E0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC44C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC58C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BC6F0);

/**
 * @brief Field-VM SFX trigger / state-machine handler.
 *
 * Reads 8 stack params (top two as packed u16 halfwords for an x/y rect
 * source, six s32 args including the SFX slot id, a text/data id, and
 * four playback parameters). When the entity's @c activeMask bit for the
 * current @c scriptGroup is set:
 *   - Returns 5 if the SFX slot is already active (bit set in @c field_0xD2).
 *   - Otherwise, saves the current global SFX flag, resolves a text pointer,
 *     measures it via @c func_8002E680, fills the rect's bottom-right corner
 *     (+0x30 X, +0x11 Y), runs the display setup chain, kicks off the slow
 *     SFX, and sets the slot's bits in both @c field_0xD2 and @c field_0xD3.
 * Otherwise, runs a 2-state machine on @c field_0x204:
 *   - State 0: set the global SFX flag, query remaining duration
 *     (@c func_8002CE84), store to @c result; if non-negative, fade out and
 *     advance the state.
 *   - State 1: when the SFX has fully released (@c getSfxField1C == 0),
 *     clear the slot's bit in @c field_0xD2, pop 8 stack slots, restore
 *     the saved SFX flag, and return 3.
 *
 * @return 1 while still working, 3 when the state-1 release completes,
 *         5 when the slot was already active.
 */
s32 func_800BC8CC(Eline *e) {
    s16 buf[4];
    s32 sfxIdx;
    s32 textIdx;
    s32 paramY;
    s32 paramZ;
    s32 paramW;
    s32 paramV;
    u8 *text;
    s32 dims;
    s32 r;
    s32 state;

    buf[1] = *(u16 *)&e->stack[e->stackPtr];
    buf[0] = *(u16 *)&e->stack[e->stackPtr - 1];
    paramV  = e->stack[e->stackPtr - 2];
    paramW  = e->stack[e->stackPtr - 3];
    paramZ  = e->stack[e->stackPtr - 4];
    paramY  = e->stack[e->stackPtr - 5];
    textIdx = e->stack[e->stackPtr - 6];
    sfxIdx  = e->stack[e->stackPtr - 7];

    if ((e->activeMask >> e->scriptGroup) & 1) {
        if ((g_seedState->sfxActiveMask >> sfxIdx) & 1) {
            return 5;
        }
        D_800DE4DC = getSfxGlobalFlag();
        text = func_8003974C(D_800704C0, textIdx);
        dims = func_8002E680(text);
        buf[2] = (dims & 0xFFFF) + 0x30;
        buf[3] = (dims >> 16) + 0x11;
        func_800BC258(buf);
        func_8002E064(sfxIdx, buf);
        func_8002D784(sfxIdx, text, paramY, paramZ, paramW, paramV);
        startSfxSlow(sfxIdx);
        e->field_0x204 = 0;
        g_seedState->sfxStartMask |= (1 << sfxIdx);
        g_seedState->sfxActiveMask |= (1 << sfxIdx);
    } else {
        state = e->field_0x204;
        switch (state) {
        case 0:
            setSfxGlobalFlag(sfxIdx);
            r = func_8002CE84(sfxIdx);
            e->resultSlots[0] = r;
            if (r >= 0) {
                fadeOutSfxSlow(sfxIdx);
                e->field_0x204++;
            }
            break;
        case 1:
            if (getSfxField1C(sfxIdx) == 0) {
                g_seedState->sfxActiveMask &= ~(state << sfxIdx);
                e->stackPtr -= 8;
                setSfxGlobalFlag(D_800DE4DC);
                return 3;
            }
            break;
        }
    }
    return 1;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BCB14);

/**
 * @brief Pop the top stack slot and pass it to @c setSfxGlobalFlag.
 */
s32 func_800BCC6C(Eline *eline) {
    setSfxGlobalFlag(POP(eline));
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BCCAC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BCDA0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BCE44);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BCECC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object9", func_800BD024);

/**
 * @brief Pop value, clear D_80085398 table entry, call sound handler.
 *
 * Pops the stack to get an index, clears the halfword at D_80085398[idx * 16],
 * then calls clearAnimEntryActive with the popped value.
 *
 * @param entity Script entity context.
 * @return 2.
 */
s32 func_800BD1A4(Eline *eline) {
    s32 val = POP(eline);
    *(u16 *)(D_80085398 + val * 16) = 0;
    clearAnimEntryActive(val);
    return 2;
}
