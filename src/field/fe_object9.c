#include "common.h"
#include "field.h"
#include "gamestate.h"
#include "battle.h"

/*
 * The animation/SFX helpers below are declared @c void in battle.h
 * (their canonical signature from btl_color.c / btl_entity.c), but
 * this translation unit's call sites only produce byte-matching
 * codegen when gcc thinks the helpers return @c u8. Redeclaring them
 * locally with @c u8 returns is a per-TU prototype override — the
 * linker resolves to the canonical symbol regardless of return type.
 */
extern u8 setupAnimEntry(s32 idx_bit, s32 v4, u16 *buf, s32 v3, s32 v2, s32 v1);
extern u8 setupAnimEntryFull(s32 idx_bit, s32 v4, u16 *buf, s32 v3, s32 v2, s32 v1, s32 v0);
extern u8 updateAnimEntry(s32 idx, s32 val);
extern u8 setSfxEntryVolume(s32 idx, s32 vol);
extern u8 setSfxEntityType(s32 idx, s32 type);

extern void func_8002D784(s32 sfxIdx, u8 *data, s32 paramY, s32 paramZ, s32 paramW, s32 paramV);

/*
 * @c func_800A8DAC is polymorphic (see field.h); declare with the
 * spatial-snapshot signature used by this TU.
 */
extern void func_800A8DAC(u8 spatialIdx, s32 cmd, u32 arg, void *out);

/**
 * @brief Snapshot a target entity's grid-cell position into the queued
 *        turn-state fields.
 *
 * Same body as @c func_800BADCC in fe_object8 but standalone — no
 * @c func_800BAC18 tail call, always returns 2.
 */
s32 func_800BB2A4(Eline *eline) {
    s16 buf[4];
    s32 idx;

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
    return 2;
}

/**
 * @brief Variant of @c func_800BB2A4 that resolves the target via the
 *        SeeD party-member slot table.
 *
 * Same body as @c func_800BB2A4 but the popped index is treated as a
 * SeeD party slot — look up @c g_seedState->memberSlot[slot] to get
 * the entity index into @c D_80085224, and pass that same byte as the
 * spatial argument to @c func_800A8DAC.
 */
s32 func_800BB3D8(Eline *eline) {
    s16 buf[4];
    u8 slot;

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
    return 2;
}

/**
 * @brief Queue a relative-offset turn target (standalone variant).
 *
 * Same body as @c func_800BB05C in fe_object8 but doesn't tail-call
 * @c func_800BAC18 — always returns 2. Queries @c func_800A8DAC with
 * kind @c 0x20 (relative offsets, written to @c buf), divides each
 * entry by 16, and stores into @c field_0x22A / @c field_0x22E /
 * @c field_0x232. Clears @c field_0x236 and @c field_0x23B.
 */
s32 func_800BB510(Eline *eline) {
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
    return 2;
}

/**
 * @brief Pop three bytes into @c field_0x238 / @c field_0x239 /
 *        @c field_0x23A (top-of-stack first).
 */
s32 func_800BB5E0(Eline *eline) {
    eline->field_0x23A = POP_BYTE(eline);
    eline->field_0x239 = POP_BYTE(eline);
    eline->field_0x238 = POP_BYTE(eline);
    return 2;
}

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

/**
 * @brief Initialise the dialog-state block (variant: state=3, count=8).
 *
 * Similar to @c func_800BB768 but with the boot values
 * (D_800DE8D2=3, dialogState=3, dialogTimer=0, dialogCount=8, then
 * three 0xFF sentinels). The empty @c do/while(0) breaks gcc's
 * scheduler at the right point so the @c sh @c zero (dialogTimer)
 * lands before the @c sh @c v1 (dialogCount) — without it gcc emits
 * the count store first and burns an extra @c li.
 */
s32 func_800BB7BC(void) {
    D_800DE8D2 = 3;
    D_800704A8.dialogState = 3;
    D_800704A8.dialogTimer = 0;
    D_800704A8.dialogCount = 8;
    do { } while (0);
    D_800704A8.field_0x10E = 0xFF;
    D_800704A8.field_0x110 = 0xFF;
    D_800704A8.field_0x112 = 0xFF;
    func_800BB6C8();
    return 2;
}

/**
 * @brief Initialise dialog state (variant 7) and pop three halfwords.
 *
 * Sets @c dialogState=7 / @c dialogTimer=0, then pops three halfwords
 * top-down into @c field_0x112 / @c field_0x110 / @c field_0x10E
 * (so the first push lands in @c field_0x10E). Then dispatches
 * @c func_800BB6C8 to mirror the block into @c g_seedState.
 *
 * @note Originally split by splat at @c func_800BB888 (the tail of
 * this function) — the symbol was removed from @c symbol_addrs.field
 * so the two halves merge back into one function.
 */
s32 func_800BB810(Eline *eline) {
    D_800704A8.dialogState = 7;
    D_800704A8.dialogTimer = 0;
    D_800704A8.field_0x112 = POP(eline);
    D_800704A8.field_0x110 = POP(eline);
    D_800704A8.field_0x10E = POP(eline);
    func_800BB6C8();
    return 2;
}


/**
 * @brief Initialise dialog state (variant 8) and pop three halfwords.
 *
 * Same as @c func_800BB810 but with @c dialogState=8.
 *
 * @note Originally split by splat at @c func_800BB90C — symbol removed
 * from @c symbol_addrs.field so the two halves merge back.
 */
s32 func_800BB8B4(Eline *eline) {
    D_800704A8.dialogState = 8;
    D_800704A8.dialogTimer = 0;
    D_800704A8.field_0x112 = POP(eline);
    D_800704A8.field_0x110 = POP(eline);
    D_800704A8.field_0x10E = POP(eline);
    func_800BB6C8();
    return 2;
}


/**
 * @brief Initialise dialog state 5 + pop 4 halfwords + copy 3.
 *
 * Sets @c dialogState=5, @c dialogTimer=0. Pops 4 halfwords into
 * @c dialogCount / @c field_0x11E / @c field_0x11C / @c field_0x11A
 * (in that order), then copies @c field_0x112 / @c field_0x110 /
 * @c field_0x10E into @c field_0x118 / @c field_0x116 / @c field_0x114
 * (snapshotting the previous values). Dispatches @c func_800BB6C8.
 *
 * @note Originally split by splat at @c func_800BB9A8 — symbol removed
 * from @c symbol_addrs.field so the two halves merge back.
 */
s32 func_800BB958(Eline *eline) {
    D_800704A8.dialogState = 5;
    D_800704A8.dialogTimer = 0;
    D_800704A8.dialogCount = POP(eline);
    D_800704A8.field_0x11E = POP(eline);
    D_800704A8.field_0x11C = POP(eline);
    D_800704A8.field_0x11A = POP(eline);
    D_800704A8.field_0x118 = D_800704A8.field_0x112;
    D_800704A8.field_0x116 = D_800704A8.field_0x110;
    D_800704A8.field_0x114 = D_800704A8.field_0x10E;
    func_800BB6C8();
    return 2;
}


/**
 * @brief Same shape as @c func_800BB958 with @c dialogState=6.
 */
s32 func_800BBA3C(Eline *eline) {
    D_800704A8.dialogState = 6;
    D_800704A8.dialogTimer = 0;
    D_800704A8.dialogCount = POP(eline);
    D_800704A8.field_0x11E = POP(eline);
    D_800704A8.field_0x11C = POP(eline);
    D_800704A8.field_0x11A = POP(eline);
    D_800704A8.field_0x118 = D_800704A8.field_0x112;
    D_800704A8.field_0x116 = D_800704A8.field_0x110;
    D_800704A8.field_0x114 = D_800704A8.field_0x10E;
    func_800BB6C8();
    return 2;
}


/**
 * @brief Initialise dialog state 5 + pop 7 halfwords.
 *
 * Like @c func_800BB958 but pops three more halfwords into
 * @c field_0x118 / @c field_0x116 / @c field_0x114 instead of
 * copying from the lower triple.
 *
 * @note Originally split by splat into three parts (BBB20, BBBB4,
 * BBC08); the trailing symbols were removed from
 * @c symbol_addrs.field to merge them back.
 */
s32 func_800BBB20(Eline *eline) {
    D_800704A8.dialogState = 5;
    D_800704A8.dialogTimer = 0;
    D_800704A8.dialogCount = POP(eline);
    D_800704A8.field_0x11E = POP(eline);
    D_800704A8.field_0x11C = POP(eline);
    D_800704A8.field_0x11A = POP(eline);
    D_800704A8.field_0x118 = POP(eline);
    D_800704A8.field_0x116 = POP(eline);
    D_800704A8.field_0x114 = POP(eline);
    func_800BB6C8();
    return 2;
}



/**
 * @brief Same shape as @c func_800BBB20 with @c dialogState=6.
 */
s32 func_800BBC64(Eline *eline) {
    D_800704A8.dialogState = 6;
    D_800704A8.dialogTimer = 0;
    D_800704A8.dialogCount = POP(eline);
    D_800704A8.field_0x11E = POP(eline);
    D_800704A8.field_0x11C = POP(eline);
    D_800704A8.field_0x11A = POP(eline);
    D_800704A8.field_0x118 = POP(eline);
    D_800704A8.field_0x116 = POP(eline);
    D_800704A8.field_0x114 = POP(eline);
    func_800BB6C8();
    return 2;
}

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

/**
 * @brief Dialog ready/idle predicate dispatched on @c D_800DE8D2.
 *
 * When @c D_800DE8D2 == 2 the function reports 2 only if
 * @c D_800704A8.dialogState is currently 0 (idle), otherwise 1.
 * For every other value of @c D_800DE8D2 the function waits for
 * @c dialogState == 3 AND @c dialogTimer == 0xFF before reporting 2.
 *
 * The @c dlg=1; return dlg pattern in the first arm forces gcc to
 * reuse @c v0 for both the loaded dialog state and the return value,
 * matching the target's register allocation.
 *
 * @return 2 when the polled state is ready, 1 while still waiting.
 */
s32 func_800BBDE0(void) {
    u16 dlg;
    volatile SystemState *ss;
    if (D_800DE8D2 == 2) {
        dlg = ((volatile SystemState *)&D_800704A8)->dialogState;
        if (dlg != 0) {
            dlg = 1;
            return dlg;
        }
        return 2;
    }
    ss = &D_800704A8;
    if ((s16)ss->dialogState != 3) {
        return 1;
    }
    if ((s16)ss->dialogTimer == 0xFF) {
        return 2;
    }
    return 1;
}

/**
 * @brief Clear the dialog state and mirror it into @c g_seedState.
 *
 * Writes 0 into @c D_800704A8.dialogState, then copies the freshly
 * cleared value into @c g_seedState->dialogStateMirror. The @c volatile
 * pointer is required to force a real load-after-store rather than
 * letting gcc fold the mirror to a constant 0.
 *
 * @return Always 2 (VM continue).
 */
s32 func_800BBE50(void) {
    volatile SystemState *src = &D_800704A8;
    SeedState *dst = g_seedState;
    src->dialogState = 0;
    dst->dialogStateMirror = src->dialogState;
    return 2;
}

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

/**
 * @brief Register an SFX entry's volume and type via VM stack args.
 *
 * Pops three values from the Eline stack: @c vol (top), @c kind, then
 * @c idx. Forwards @c vol to @c setSfxEntryVolume and remaps the
 * caller-supplied @c kind through a 3-way switch (0→2, 1→3, 2→0) before
 * calling @c setSfxEntityType. Mirrors both into the per-slot SFX entry
 * at @c D_80085300[idx]. Returns 2 (VM continue).
 */
s32 func_800BBEFC(Eline *eline) {
    s32 vol  = POP(eline);
    s32 kind = POP(eline);
    s32 idx  = POP(eline);

    setSfxEntryVolume(idx, vol);

    switch (kind) {
    case 0: kind = 2; break;
    case 1: kind = 3; break;
    case 2: kind = 0; break;
    }

    setSfxEntityType(idx, kind);
    D_80085300[idx].volume = vol;
    D_80085300[idx].type   = kind;
    return 2;
}

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

/**
 * @brief Start an SFX slot if it's not already running on this script.
 *
 * Peeks two stack values: @c val1 (top, the SFX data index) and
 * @c sfxIdx (one below, the playback slot).
 *
 * If the Eline's @c activeMask bit for the current @c scriptGroup is
 * set: returns 5 immediately when the slot bit is already in
 * @c sfxStartMask; otherwise looks up the SFX data via
 * @c func_8003974C(D_800704C0, val1), kicks off playback
 * (@c initSfxPlayback / @c startSfxSlow), promotes the global flag
 * and marks the slot bit in @c sfxStartMask. Returns 1.
 *
 * If the Eline's @c activeMask bit is NOT set: returns 1 when the
 * slot is already running, otherwise pops two stack slots and returns
 * 3 to fall through to the next opcode.
 *
 * The @c do { } while (0) wrapper around the mask read-modify-write is
 * a scheduling barrier — it keeps gcc from interleaving the
 * @c (1 << sfxIdx) constant load with the @c lw of @c g_seedState,
 * matching the target's register allocation.
 */
s32 func_800BC034(Eline *eline) {
    s32 val1   = eline->stack[(s8)eline->stackPtr];
    s32 sfxIdx = eline->stack[(s8)eline->stackPtr - 1];

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        u8 mask;
        if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
            return 5;
        }
        initSfxPlayback(sfxIdx, func_8003974C(D_800704C0, val1));
        startSfxSlow(sfxIdx);
        setSfxGlobalFlag(sfxIdx);
        do {
            mask = g_seedState->sfxStartMask;
            g_seedState->sfxStartMask = mask | (1 << sfxIdx);
        } while (0);
        return 1;
    }
    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 1;
    }
    eline->stackPtr -= 2;
    return 3;
}

/**
 * @brief Write one entry into the @c D_80085300 SFX-entry table.
 *
 * Stores the s32 @c val into @c entry->payload and copies the four
 * input halfwords from @c src into @c entry->rect. The trailing
 * @c volume / @c type fields are left untouched.
 *
 * @param idx  Entry index in @c D_80085300.
 * @param val  s32 payload (typically an SFX data pointer cast to s32).
 * @param src  4-halfword source block, copied into @c entry->rect.
 */
void func_800BC12C(s32 idx, s32 val, u16 *src) {
    SfxEntry *base = D_80085300;
    SfxEntry *entry = base + idx;
    entry->payload = val;
    entry->rect[0] = src[0];
    entry->rect[1] = src[1];
    entry->rect[2] = src[2];
    entry->rect[3] = src[3];
}

/**
 * @brief SFX trigger with sound-data lookup and entry registration.
 *
 * Peeks two stack values: @c val1 (top, used as a 'kind' argument to
 * the sound-data lookup) and @c sfxIdx (one below). If the slot's bit
 * is already set in @c sfxStartMask, return 5 (busy).
 *
 * Otherwise: look up the SFX data via @c func_8003974C(D_800704C0,
 * val1), kick off playback (@c initSfxPlayback / @c startSfxSlow),
 * promote to the global flag, set both @c sfxStartMask and
 * @c sfxActiveMask bits, pop two stack slots, then register the
 * entry into @c D_80085300 via @c func_800BC12C.
 *
 * Returns 3 on success.
 */
s32 func_800BC170(Eline *eline) {
    u8 buf[8];
    s32 val1   = eline->stack[(s8)eline->stackPtr];
    s32 sfxIdx = eline->stack[(s8)eline->stackPtr - 1];
    u8 *data;

    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 5;
    }

    data = func_8003974C(D_800704C0, val1);
    initSfxPlayback(sfxIdx, data);
    startSfxSlow(sfxIdx);
    setSfxGlobalFlag(sfxIdx);

    g_seedState->sfxStartMask  |= (1 << sfxIdx);
    g_seedState->sfxEntryMask  |= (1 << sfxIdx);

    eline->stackPtr -= 2;
    func_800BC12C(sfxIdx, (s32)data, (u16 *)buf);
    return 3;
}

/**
 * @brief Clip a rect to the visible screen area (8,8 → 0x130x0xE0).
 *
 * Adjusts @c r->x so the rect doesn't run off the right edge
 * (@c x+w < 0x130), then clamps @c x >= 8. Same for @c y / @c h
 * against the bottom edge (0xE0) and top edge (8).
 */
void func_800BC258(Rect *r) {
    if (r->x + r->w >= 0x130) {
        r->x = 0x138 - (u16)r->w;
    }
    if (r->x < 8) {
        r->x = 8;
    }
    if (r->y + r->h >= 0xE0) {
        r->y = 0xE0 - (u16)r->h;
    }
    if (r->y < 8) {
        r->y = 8;
    }
}

/**
 * @brief SFX trigger with text-measured rect and entry registration.
 *
 * Variant of @c func_800BC170 that also constructs a clipped on-screen
 * rectangle around a piece of text: peeks @c sfxIdx / @c textIdx /
 * @c rect.x / @c rect.y from the stack, measures the text via
 * @c func_8002E680 to set @c rect.w / @c rect.h (with +0x10/+0x11
 * padding), clips via @c func_800BC258, and calls @c func_8002E064
 * to install the rect for the SFX slot.
 *
 * @return 5 if slot busy, 1 on success (kicks off SFX + entry),
 *         3 once the slot frees up while inactive (pops 4).
 */
s32 func_800BC2E0(Eline *eline) {
    Rect buf;
    s32 sfxIdx;
    s32 textIdx;
    u8 *data;
    s32 dims;

    sfxIdx  = eline->stack[(s8)eline->stackPtr - 3];
    textIdx = eline->stack[(s8)eline->stackPtr - 2];
    buf.x   = (u16)eline->stack[(s8)eline->stackPtr - 1];
    buf.y   = (u16)eline->stack[(s8)eline->stackPtr];

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
            return 5;
        }
        data = func_8003974C(D_800704C0, textIdx);
        initSfxPlayback(sfxIdx, data);
        dims = func_8002E680(data);
        buf.w = (dims & 0xFFFF) + 0x10;
        buf.h = (dims >> 16) + 0x11;
        func_800BC258(&buf);
        func_8002E064(sfxIdx, (s16 *)&buf);
        startSfxSlow(sfxIdx);
        setSfxGlobalFlag(sfxIdx);
        g_seedState->sfxStartMask |= (1 << sfxIdx);
        func_800BC12C(sfxIdx, (s32)data, (u16 *)&buf);
        return 1;
    }
    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 1;
    }
    eline->stackPtr -= 4;
    return 3;
}

/**
 * @brief Unconditional SFX trigger with text-measured rect.
 *
 * Like @c func_800BC2E0 but no @c activeMask gate — always runs once
 * the slot is free. Sets both @c sfxStartMask and @c sfxEntryMask,
 * pops four stack slots, registers the entry, and returns 3.
 *
 * @return 5 if slot busy, 3 otherwise.
 */
s32 func_800BC44C(Eline *eline) {
    Rect buf;
    s32 sfxIdx;
    s32 textIdx;
    u8 *data;
    s32 dims;

    sfxIdx  = eline->stack[(s8)eline->stackPtr - 3];
    textIdx = eline->stack[(s8)eline->stackPtr - 2];
    buf.x   = (u16)eline->stack[(s8)eline->stackPtr - 1];
    buf.y   = (u16)eline->stack[(s8)eline->stackPtr];

    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 5;
    }

    data = func_8003974C(D_800704C0, textIdx);
    initSfxPlayback(sfxIdx, data);
    dims = func_8002E680(data);
    buf.w = (dims & 0xFFFF) + 0x10;
    buf.h = (dims >> 16) + 0x11;
    func_800BC258(&buf);
    func_8002E064(sfxIdx, &buf);
    startSfxSlow(sfxIdx);
    setSfxGlobalFlag(sfxIdx);

    g_seedState->sfxStartMask |= (1 << sfxIdx);
    g_seedState->sfxEntryMask |= (1 << sfxIdx);

    eline->stackPtr -= 4;
    func_800BC12C(sfxIdx, (s32)data, (u16 *)&buf);
    return 3;
}

/**
 * @brief SFX trigger that pops 4 stack slots up front (vs peeking).
 *
 * Variant of @c func_800BC44C that consumes its 4 args with @c POP
 * instead of peeking. Sets only @c sfxStartMask (no @c sfxEntryMask),
 * registers the entry, and returns 2 (vs 3 for the peeking version).
 *
 * @return 5 if slot busy, 2 on success.
 */
s32 func_800BC58C(Eline *eline) {
    Rect buf;
    s32 sfxIdx;
    s32 textIdx;
    u8 *data;
    s32 dims;

    buf.y   = POP(eline);
    buf.x   = POP(eline);
    textIdx = POP(eline);
    sfxIdx  = POP(eline);

    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 5;
    }

    data = func_8003974C(D_800704C0, textIdx);
    initSfxPlayback(sfxIdx, data);
    dims = func_8002E680(data);
    buf.w = (dims & 0xFFFF) + 0x10;
    buf.h = (dims >> 16) + 0x11;
    func_800BC258(&buf);
    func_8002E064(sfxIdx, &buf);
    startSfxSlow(sfxIdx);
    setSfxGlobalFlag(sfxIdx);

    g_seedState->sfxStartMask |= (1 << sfxIdx);
    func_800BC12C(sfxIdx, (s32)data, (u16 *)&buf);
    return 2;
}

/**
 * @brief Field-VM SFX trigger / state-machine handler (6-arg variant).
 *
 * Same shape as @c func_800BC8CC but without the on-screen rect /
 * text-measure setup, so it takes 6 stack params instead of 8.
 *
 * Bit-set: save global flag (@c D_800DE4D8), dispatch
 * @c func_8002D784(sfxIdx, data, paramY..V), kick off SFX, set both
 * @c sfxStartMask and @c sfxActiveMask, clear @c field_0x204.
 *
 * Bit-clear: 2-state machine on @c field_0x204 — state 0 queries
 * remaining duration and fades, state 1 waits for release then pops 6
 * and restores the saved flag.
 *
 * @return 1 working, 3 done, 5 slot busy.
 */
s32 func_800BC6F0(Eline *e) {
    s32 sfxIdx;
    s32 textIdx;
    s32 paramY;
    s32 paramZ;
    s32 paramW;
    s32 paramV;
    u8 *data;
    s32 r;
    s32 state;

    paramV  = e->stack[(s8)e->stackPtr];
    paramW  = e->stack[(s8)e->stackPtr - 1];
    paramZ  = e->stack[(s8)e->stackPtr - 2];
    paramY  = e->stack[(s8)e->stackPtr - 3];
    textIdx = e->stack[(s8)e->stackPtr - 4];
    sfxIdx  = e->stack[(s8)e->stackPtr - 5];

    if ((e->activeMask >> e->scriptGroup) & 1) {
        if ((g_seedState->sfxActiveMask >> sfxIdx) & 1) {
            return 5;
        }
        D_800DE4D8 = getSfxGlobalFlag();
        data = func_8003974C(D_800704C0, textIdx);
        func_8002D784(sfxIdx, data, paramY, paramZ, paramW, paramV);
        startSfxSlow(sfxIdx);
        e->field_0x204 = 0;
        g_seedState->sfxStartMask  |= (1 << sfxIdx);
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
                e->stackPtr -= 6;
                setSfxGlobalFlag(D_800DE4D8);
                return 3;
            }
            break;
        }
    }
    return 1;
}

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


/**
 * @brief Tear down an entry-registered SFX slot.
 *
 * Peeks @c sfxIdx from the top of the stack and decides whether the
 * slot can be torn down.
 *
 * If the entry bit (@c sfxEntryMask) is set: also requires
 * @c sfxStartMask to be set (else return 1). Optionally fades out via
 * @c fadeOutSfxSlow when @c D_80070600 has the @c 0xC0 flag bits,
 * @c getSfxField28 reports something, and the slot is no longer active.
 * Waits for @c getSfxField1C to drop to 0 and @c getSfxField28 to
 * become non-zero, then clears both @c sfxStartMask and @c sfxEntryMask
 * bits, pops one stack slot, and returns 2.
 *
 * If the entry bit is clear: only waits for @c sfxStartMask to clear,
 * then pops one and returns 2.
 *
 * @return 1 while waiting, 2 when torn down.
 */
s32 func_800BCB14(Eline *e) {
    s32 sfxIdx = e->stack[(s8)e->stackPtr];

    if ((g_seedState->sfxEntryMask >> sfxIdx) & 1) {
        if (!((g_seedState->sfxStartMask >> sfxIdx) & 1)) {
            return 1;
        }
        if ((D_80070600 & 0xC0) && getSfxField28(sfxIdx)
            && !((g_seedState->sfxActiveMask >> sfxIdx) & 1)) {
            fadeOutSfxSlow(sfxIdx);
        }
        if (getSfxField1C(sfxIdx)) {
            return 1;
        }
        if (!getSfxField28(sfxIdx)) {
            return 1;
        }
        g_seedState->sfxStartMask &= ~(1 << sfxIdx);
        g_seedState->sfxEntryMask &= ~(1 << sfxIdx);
        e->stackPtr -= 1;
        return 2;
    }
    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 1;
    }
    e->stackPtr -= 1;
    return 2;
}

/**
 * @brief Pop the top stack slot and pass it to @c setSfxGlobalFlag.
 */
s32 func_800BCC6C(Eline *eline) {
    setSfxGlobalFlag(POP(eline));
    return 2;
}

/**
 * @brief Register a pre-built rect entry into @c D_80085300 (no SFX kick).
 *
 * Peeks @c sfxIdx and a 4-halfword rect from the stack. If the slot is
 * busy (@c sfxStartMask set) return 5. Otherwise clip the rect, install
 * it via @c func_8002E064, register an entry with @c data=0 via
 * @c func_800BC12C, pop 5, and return 2.
 */
s32 func_800BCCAC(Eline *e) {
    Rect buf;
    s32 sfxIdx = e->stack[(s8)e->stackPtr - 4];

    if ((g_seedState->sfxStartMask >> sfxIdx) & 1) {
        return 5;
    }

    buf.h = (u16)e->stack[(s8)e->stackPtr];
    buf.w = (u16)e->stack[(s8)e->stackPtr - 1];
    buf.y = (u16)e->stack[(s8)e->stackPtr - 2];
    buf.x = (u16)e->stack[(s8)e->stackPtr - 3];

    func_800BC258(&buf);
    func_8002E064(sfxIdx, &buf);
    e->stackPtr -= 5;
    func_800BC12C(sfxIdx, 0, (u16 *)&buf);
    return 2;
}

/**
 * @brief Tear down (or fade) an SFX entry slot — single-arg variant.
 *
 * Peek @c sfxIdx, then if the SFX is still releasing
 * (@c getSfxField1C != 0), kick a slow fade and return 1. Once it's
 * fully done, clear both @c sfxStartMask and @c sfxEntryMask bits,
 * pop one, and return 2.
 */
s32 func_800BCDA0(Eline *e) {
    s32 sfxIdx = e->stack[(s8)e->stackPtr];

    if (!getSfxField1C(sfxIdx)) {
        g_seedState->sfxStartMask &= ~(1 << sfxIdx);
        g_seedState->sfxEntryMask &= ~(1 << sfxIdx);
        e->stackPtr -= 1;
        return 2;
    }
    fadeOutSfxSlow(sfxIdx);
    return 1;
}

/**
 * @brief Update the @c field4 slot of a @c D_80085398 entry.
 *
 * Pops @c (idx, val) from the Eline stack, calls @c updateAnimEntry
 * to refresh whatever runtime state the helper tracks, then writes
 * @c val into @c D_80085398[idx].field4 and returns 2.
 */
s32 func_800BCE44(Eline *e) {
    s32 val = POP(e);
    s32 idx = POP(e);
    updateAnimEntry(idx, val);
    D_80085398[idx].field4 = val;
    return 2;
}

/**
 * @brief Set up a 7-arg animation entry in the @c D_80085398 table.
 *
 * Pops 7 stack values: 6 halfwords (v1..v6) plus an entry index. The
 * low bit of @c idx selects the bank, @c v4 is passed as the second
 * arg, and the two leading halfwords (@c v5/v6) are passed via a
 * 4-byte stack-allocated @c buf. The remaining halfwords spread
 * across the @c setupAnimEntry register args.
 *
 * After the helper returns, all six values plus a @c flag=1 marker
 * are recorded into the per-slot entry at @c D_80085398[idx].
 */
s32 func_800BCECC(Eline *e) {
    u16 buf[2];
    s32 v1 = POP(e);
    s32 v2 = POP(e);
    s32 v3 = POP(e);
    s32 v4 = POP(e);
    s32 v5 = POP(e);
    s32 v6 = POP(e);
    s32 idx = POP(e);

    buf[0] = v6;
    buf[1] = v5;
    setupAnimEntry(idx & 1, v4, buf, v3, v2, v1);

    D_80085398[idx].flag   = 1;
    D_80085398[idx].fieldE = v6;
    D_80085398[idx].fieldC = v5;
    D_80085398[idx].fieldA = v4;
    D_80085398[idx].field8 = v3;
    D_80085398[idx].field6 = v2;
    D_80085398[idx].field4 = v1;
    return 2;
}

/**
 * @brief Set up an 8-arg animation entry in the @c D_80085398 table.
 *
 * Variant of @c func_800BCECC: pops 8 stack values (7 halfwords plus
 * an entry index), passes them via @c setupAnimEntryFull (the 7th
 * value lands as the 7th register/stack arg), and records all 8 plus
 * a @c flag=2 marker into the per-slot entry at @c D_80085398[idx].
 */
s32 func_800BD024(Eline *e) {
    u16 buf[2];
    s32 v0 = POP(e);
    s32 v1 = POP(e);
    s32 v2 = POP(e);
    s32 v3 = POP(e);
    s32 v4 = POP(e);
    s32 v5 = POP(e);
    s32 v6 = POP(e);
    s32 idx = POP(e);

    buf[0] = v6;
    buf[1] = v5;
    setupAnimEntryFull(idx & 1, v4, buf, v3, v2, v1, v0);

    D_80085398[idx].flag   = 2;
    D_80085398[idx].fieldE = v6;
    D_80085398[idx].fieldC = v5;
    D_80085398[idx].fieldA = v4;
    D_80085398[idx].field8 = v3;
    D_80085398[idx].field6 = v2;
    D_80085398[idx].field4 = v1;
    D_80085398[idx].field2 = v0;
    return 2;
}

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
    D_80085398[val].flag = 0;
    clearAnimEntryActive(val);
    return 2;
}
