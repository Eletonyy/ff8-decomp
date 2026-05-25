#include "common.h"
#include "sound.h"
#include "psxsdk/libapi.h"
#include "psxsdk/libgpu.h"

extern u8 D_800780D8[];
extern u8 D_800C4DCC[];
extern u8 D_800C4FD3;
extern u8 D_800C4FD4;
extern u8 D_800C4FD5;
extern u8 D_800C4FD6;
extern u8 D_800C4FD7;
extern u8 D_800D23D8[];

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_800997E8);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_80099B48);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_80099C84);

extern SeqEntry D_800C4FD8[];

/**
 * @brief Kick off the SFX for @c D_800C4FD8[idx] and mark it active.
 *
 * Dispatches on the entry's @c bankHandle:
 *  - non-zero: @c sndPlayBankSfx(bankHandle, @c field04, @c field08,
 *    @c field0C).
 *  - zero:     @c sndPlaySfx(field10, @c field04, @c field08,
 *    @c field0C).
 *
 * Then sets @c field12 = @c 1 to mark the entry as running.
 *
 * @param idx Index into @c D_800C4FD8.
 */
void func_80099EDC(s32 idx) {
    if (D_800C4FD8[idx].bankHandle != 0) {
        sndPlayBankSfx(D_800C4FD8[idx].bankHandle,
                       D_800C4FD8[idx].field04,
                       D_800C4FD8[idx].field08,
                       D_800C4FD8[idx].field0C);
    } else {
        sndPlaySfx(D_800C4FD8[idx].field10,
                   D_800C4FD8[idx].field04,
                   D_800C4FD8[idx].field08,
                   D_800C4FD8[idx].field0C);
    }
    D_800C4FD8[idx].field12 = 1;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_80099F78);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A4DC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A638);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A7C0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A954);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009AD3C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009AEE4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B358);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B550);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B748);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B840);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B954);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009BFA0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C070);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C1A4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C294);

/**
 * @brief Two-stage @c LoadImage descriptor — pair of @c RECT / @c data
 *        slots gated by the low nibble of @c flag. Shared between
 *        @c func_8009C478 and @c func_8009C5FC.
 */
typedef struct {
    RECT     rect1;
    u32 *    data1;
    RECT     rect2;
    u32 *    data2;
    u8       flag;
} ImageDesc;

extern void func_8009CA34(s32 *src, ImageDesc *desc);
extern RECT D_800C8698;

/**
 * @brief Variant of @c func_8009C5FC that overrides the first
 *        @c RECT's @c (x, y) with caller-supplied coordinates.
 *
 * Same two-stage @c LoadImage flow as @c func_8009C5FC, but the first
 * blit's top-left corner is replaced by @p x, @p y (the @c w / @c h
 * still come from the descriptor). The second blit (when bit 3 of
 * @c desc.flag is set) uses @c desc.rect2 verbatim.
 *
 * Streams to the scratch @c D_800C8698 (separate from
 * @c func_8009C5FC's @c D_800C8640).
 *
 * @param src Source buffer fed to @c func_8009CA34.
 * @param x   Override X coordinate for the first blit.
 * @param y   Override Y coordinate for the first blit.
 */
void func_8009C478(s32 *src, s32 x, s32 y) {
    ImageDesc desc;
    func_8009CA34(src, &desc);
    D_800C8698.x = x;
    D_800C8698.y = y;
    D_800C8698.w = desc.rect1.w;
    D_800C8698.h = desc.rect1.h;
    LoadImage(&D_800C8698, desc.data1);
    if ((desc.flag >> 3) & 1) {
        D_800C8698.x = desc.rect2.x;
        D_800C8698.y = desc.rect2.y;
        D_800C8698.w = desc.rect2.w;
        D_800C8698.h = desc.rect2.h;
        LoadImage(&D_800C8698, desc.data2);
    }
}

/**
 * @brief Fatal-error thunk that invokes the PSX SDK SystemError with category 0x57.
 *
 * Forwards @p rc to SystemError with a fixed category code. Callers pass a
 * line-number-like token so the fault site can be identified post-mortem.
 *
 * @note Category values in use elsewhere: field → 0x53, battle
 *       → 0x59, world → 0x57. They happen to be ASCII ('S','Y','W')
 *       but the mapping to subsystem isn't obvious — purpose uncertain.
 *
 * @param rc Diagnostic code (typically a unique line/site identifier).
 */
void func_8009C528(s32 rc) {
    SystemError(0x57, rc);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C54C);

extern RECT D_800C8640;

/**
 * @brief Pull a two-image descriptor from @p src and stream both TIMs
 *        through @c LoadImage, gated by bit 3 of the descriptor flag.
 *
 * Steps:
 *  1. @c func_8009CA34(@p src, @c &desc) — parse a streaming-image
 *     header into @c desc.
 *  2. Copy @c desc.rect1 into the scratch @c D_800C8640 and
 *     @c LoadImage(@c &D_800C8640, @c desc.data1).
 *  3. If bit 3 of @c desc.flag is set, copy @c desc.rect2 and
 *     @c LoadImage the second payload too.
 *
 * The scratch @c D_800C8640 is re-used between both loads — the GPU
 * blit is synchronous from this routine's POV, so the RECT can be
 * overwritten right after the call returns.
 *
 * @param src Source buffer fed to @c func_8009CA34.
 */
void func_8009C5FC(s32 *src) {
    ImageDesc desc;
    func_8009CA34(src, &desc);
    D_800C8640.x = desc.rect1.x;
    D_800C8640.y = desc.rect1.y;
    D_800C8640.w = desc.rect1.w;
    D_800C8640.h = desc.rect1.h;
    LoadImage(&D_800C8640, desc.data1);
    if ((desc.flag >> 3) & 1) {
        D_800C8640.x = desc.rect2.x;
        D_800C8640.y = desc.rect2.y;
        D_800C8640.w = desc.rect2.w;
        D_800C8640.h = desc.rect2.h;
        LoadImage(&D_800C8640, desc.data2);
    }
}

extern void sndSeqSetTempoAlt(s32 tempo);
extern void sndSetMasterVolume(s32 vol);
extern void sndCmdF1(void);

/**
 * @brief Silence the audio subsystem: zero tempo, zero master volume, send cmd F1.
 *
 * Likely used when leaving a map or suspending sound playback. The @c sndCmdF1
 * call appears in similar "stop audio" sequences elsewhere in the codebase.
 */
void func_8009C69C(void) {
    sndSeqSetTempoAlt(0);
    sndSetMasterVolume(0);
    sndCmdF1();
}

extern void sndSetChannelVolume(s32 channel, s32 vol);

/**
 * @brief Set a sound channel's volume, narrowing the volume arg to signed 8 bits.
 *
 * Forwards to sndSetChannelVolume with @p vol sign-extended from s8 to s32
 * (the canonical `sll 24 / sra 24` pair). Useful when the caller only has
 * an s8 volume value that needs to be promoted into the s32 SDK signature.
 *
 * @param channel Sound channel index.
 * @param vol Volume level treated as signed 8-bit.
 */
void func_8009C6CC(s32 channel, s8 vol) {
    sndSetChannelVolume(channel, vol);
}

extern SeqEntry D_800C4FD8[];
extern void sndSeqStartPan(s32 a0, s32 a1, s32 a2, s32 a3);

/**
 * @brief Start a sound sequence with pan for entry @p idx of the table.
 *
 * Looks up the sequence-table entry at @c D_800C4FD8[idx] and dispatches
 * @c sndSeqStartPan with its @c field10 and @c field04, plus the caller's
 * @p a1 and @p pan (sign-extended from s8).
 *
 * @param idx Index into the sequence table.
 * @param a1 Sound parameter forwarded as the third arg.
 * @param pan Signed 8-bit pan value (sign-extended to s32 for the ABI).
 */
void func_8009C6F0(s32 idx, s32 a1, s8 pan) {
    sndSeqStartPan(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04, a1, pan);
}

/**
 * @brief Stop the sound sequence associated with table entry @p idx.
 *
 * Dispatches @c sndCmd21(entry->field10, entry->field04) then clears the
 * entry's runtime-state halfword at +0x12.
 *
 * @param idx Index into the @c D_800C4FD8 sequence table.
 */
void func_8009C738(s32 idx) {
    sndCmd21(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04);
    D_800C4FD8[idx].field12 = 0;
}

extern void sndSeqPlayPan7bit(s32 a0, s32 a1, s32 a2, s32 a3);

/**
 * @brief Start a panned sound sequence with a 7-bit-clamped pan value.
 *
 * Clamps @p pan into [0, 0x7F] (7-bit range) before calling
 * @c sndSeqPlayPan7bit with the sequence-table entry's @c field10 and
 * @c field04, plus the forwarded @p arg.
 *
 * @param idx Index into the @c D_800C4FD8 sequence table.
 * @param arg Forwarded as the third arg of sndSeqPlayPan7bit.
 * @param pan Pan value; clamped to [0, 0x7F].
 */
void func_8009C780(s32 idx, s32 arg, s32 pan) {
    if (pan >= 0x80) pan = 0x7F;
    if (pan < 0) pan = 0;
    sndSeqPlayPan7bit(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04, arg, pan);
}

/** Sets bit 0x20 on two related flag bytes. */
void func_8009C7DC(void) {
    D_800780D8[0x108] |= 0x20;
    D_800D23D8[0x66] |= 0x20;
}

/** Clears bit 0x20 on two related flag bytes. */
void func_8009C808(void) {
    D_800780D8[0x108] &= ~0x20;
    D_800D23D8[0x66] &= ~0x20;
}

extern s32 D_800C4D94;
extern void fadeOutSfxSlow(s32 idx);

/**
 * @brief Fade out SFX slot tracked by D_800C4D94 (if any) and clear the slot.
 *
 * If D_800C4D94 holds a non-negative value (an active slot handle), calls
 * @c fadeOutSfxSlow(1) to fade channel 1, then resets the tracker to -1
 * (inactive). A no-op when already inactive.
 */
void func_8009C834(void) {
    if (D_800C4D94 >= 0) {
        fadeOutSfxSlow(1);
        D_800C4D94 = -1;
    }
}

/** Advances index and returns difference from table lookup. */
u8 func_8009C870(void) {
    u8 idx;
    D_800C4FD6++;
    idx = D_800C4FD6;
    if (idx == 0) {
        D_800C4FD5 += 0xD;
    }
    return (D_800C4DCC[D_800C4FD6] - D_800C4FD5) & 0xFF;
}

/** Sets two related byte values. */
void func_8009C8CC(s32 val) {
    D_800C4FD6 = val;
    D_800C4FD5 = val;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C8E0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009CA34);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009CAE0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009CB70);

void func_8009CC34(void) {
}

/** @brief Advance D_800C4FD4 index, add 0xD to D_800C4FD3 on wraparound, return table diff. */
s32 func_8009CC3C(void) {
    D_800C4FD4++;
    if ((u8)D_800C4FD4 == 0) {
        D_800C4FD3 += 0xD;
    }
    return (u8)(D_800C4DCC[D_800C4FD4] - D_800C4FD3);
}

/** @brief Increment D_800C4FD7 index, return D_800C4DCC table value at new index. */
s32 func_8009CC98(void) {
    D_800C4FD7++;
    return D_800C4DCC[D_800C4FD7];
}

/** Sets two related byte values. */
void func_8009CCC8(s32 val) {
    D_800C4FD4 = val;
    D_800C4FD3 = val;
}

/** Sets a single byte value. */
void func_8009CCDC(s32 val) {
    D_800C4FD7 = val;
}
