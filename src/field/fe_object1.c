#include "common.h"
#include "field.h"
#include "psxsdk/libgte.h"
#include "psxsdk/libgpu.h"
#include "field/fe_object1.h"
#include "field/fe_object10.h"

/** @brief 12-byte path waypoint (64 entries per table, indexed by angle/64). */
typedef struct {
    /* 0x00 */ s16 x;       /**< Position X (fixed-point, << 12 when written). */
    /* 0x02 */ s16 y;       /**< Position Y. */
    /* 0x04 */ s16 z;       /**< Position Z. */
    /* 0x06 */ u16 unk6;    /**< Stored to entity offset 0x1FA. */
    /* 0x08 */ u8  unk8;    /**< Stored to entity offset 0x258. */
    /* 0x09 */ u8  unk9;    /**< Set by func_8009BD50 (recorder) when writing path. */
    /* 0x0A */ u8  padA[2];
} PathEntry;


extern u16 D_8005F118;
extern u16 D_8005F11A;
extern u16 D_8005F144;
extern s16 D_8005F148;
extern u16 D_8005F160;
extern u16 D_8005F162;
extern u8 D_80085388;
extern u8 D_800C32A0[];
extern u8 D_800C3320[];
extern u8 D_800C3520[];
extern u8 D_800C6D90;            /**< PRNG counter advanced 13/step by func_800A2EA4 */
extern u8 D_8005F150;            /**< Outer PRNG counter — D_800C3520 lookup offset, advanced 13/step per 256 calls of func_800A5C9C */
extern u8 D_8005F151;            /**< Inner PRNG counter — D_800C3520 lookup index, advanced 1/call by func_800A5C9C */

extern s32 func_8004D564(s32 a, s32 b);
extern s32 func_80048C50(s32 a);
extern void func_80048F5C(RECT *r, u16 *src);
extern void func_80048EFC(RECT *r, u8 *src);
extern void func_80042634(s32 a);
extern s32 func_8004D524(s32, s32, s32, s32);

extern u16 **D_800D5E9C;         /**< Pointer-to-pointer of u16 count for func_800A29C0's iteration */
extern u16 *D_800C71E4;
extern u16 D_800D3E88[];
extern u8 D_800D5F50[];
extern u8 D_800D61A8[];
extern u8 D_8005F168[];

/**
 * @brief 24-byte draw-point slot holding a 16-bit (x, y, z) position.
 *
 * The fields-only @c x/y/z prefix is what @c func_800A4500 writes;
 * @c pad06 covers the rest of the slot until other helpers populate it.
 */
typedef struct {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
    /* 0x06 */ u8  pad06[0x12];
} DrawPoint;  /* 0x18 = 24 bytes */
extern DrawPoint D_800706A0[];
extern s16 D_8005F122;
extern s16 D_8005F14A;
extern s16 D_8005F100;
extern s16 D_8005F142;
extern u8 D_8005F103;
extern PathEntry D_80070A60[64];
extern PathEntry D_80070760[64];
extern DRAWENV D_80067388[2];   /**< Double-buffered draw environments. */
extern DISPENV D_80067440[2];   /**< Double-buffered display environments. */

/**
 * @brief Initialize the field engine's double-buffered draw/display envs.
 *
 * Sets up two 320x224 draw/display environments at VRAM x=0 and x=512
 * (standard PSX double-buffering), enables dithering, leaves background
 * clearing off, raises the screen window by 8 pixels, and installs the
 * first buffer pair as the live one.
 */
void func_80098314(void) {
    SetDefDrawEnv(&D_80067388[0],   0, 0, 320, 224);
    SetDefDrawEnv(&D_80067388[1], 512, 0, 320, 224);
    SetDefDispEnv(&D_80067440[0], 512, 0, 320, 224);
    SetDefDispEnv(&D_80067440[1],   0, 0, 320, 224);

    D_80067388[0].dtd  = 1;
    D_80067388[1].dtd  = 1;
    D_80067388[0].isbg = 0;
    D_80067388[1].isbg = 0;

    D_80067440[0].screen.y = 8;
    D_80067440[1].screen.y = 8;
    D_80067440[0].screen.h = 224;
    D_80067440[1].screen.h = 224;

    PutDispEnv(&D_80067440[0]);
    PutDrawEnv(&D_80067388[0]);
}

/**
 * @brief Load/refresh the active field map's asset bundle from CD.
 *
 * Either issues a fresh CD read (when @c D_8005F14A is 0 or the current area
 * @c D_8005F14E differs from the cached @c D_8005F100) or restores from the
 * cached pointer @c D_8005F104. Then loads the secondary asset, snapshots
 * pointer-table headers into globals (@c D_800C7208, @c D_800D5EA4 family,
 * etc.), copies script data to the @c 0x801B0000 staging region, and
 * dispatches the field-VM pool setup via @c func_800BFBBC and friends.
 *
 * @return Pointer past the last initialized eline pool entry.
 *
 * @note Decomp at 97.03% match — last 51 instructions of diff are gcc 2.7.2
 *       reg-alloc and instruction-scheduling choices that natural C cannot
 *       force (e.g. target emits a redundant @c addu to recompute the asset
 *       base address into @c a1, while our compile keeps a single base in
 *       @c v0). See @c permuter/func_800983F0/base.c for the source and
 *       https://decomp.me/scratch/SXH2a for the in-browser scratch.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800983F0);

/**
 * Zero 0x40 bytes at D_800704A8+0x1B8 (backwards loop).
 */
void func_80098934(void) {
    s32 i = 0x3F;
    volatile u8 *base = (u8 *)&D_800704A8;
    u8 *ptr = (u8 *)base + 0x3F;
    do {
        *(u8 *)(ptr + 0x1B8) = 0;
        i--;
        ptr--;
    } while (i >= 0);
}

/**
 * @brief Field engine reset / map-transition orchestrator (the big state machine).
 *
 * Runs the per-frame outer init/reset loop for the field engine. The whole
 * function dispatches on @c D_8005F14C (the field load mode — 0=fresh,
 * 1=normal, 2=new-area, 3=movie, 6=transition, 0xA=skip-transition) and on
 * @c D_800704A8.mode (the engine-level state byte that picks one of several
 * exit paths at the end of each iteration: 4=quit, 5/6=copyFramebuffer +
 * flag-reset, 7=sndCmd21 + snapshot, 1=loop back, 3/8/etc.=plain exit).
 *
 * Major phases each pass:
 *   - @c memcpy 8 bytes of the overlay header at @c D_80098000 onto the
 *     stack and call @c InitClearTiles.
 *   - For load modes 0/1/2: reset the @c SystemState entity flags
 *     (@c unk1A6/1A7/1A9/1AE/1B0/1B1) and clear @c fieldStepDelta /
 *     @c unk104 / @c unk106, then call @c func_800A17A4 on the two
 *     16-byte script-VM scratch arrays at @c sys+0x122 and @c sys+0x130
 *     followed by @c func_800A44D8 + @c func_80098934.
 *   - For mode 0: re-init the two @c DRAWENVs and clear them via
 *     @c func_80048DD4.
 *   - For modes 1/2 (with @c sys->unk1A5 == 0 for mode 1): kick a
 *     framebuffer copy and set the post-copy state flags.
 *   - For all modes except 6: snapshot 12 consecutive pointer-table fields
 *     from the freshly-loaded overlay at @c 0x800E1000 into the
 *     @c D_800C7208 / @c D_800C71E8 / @c D_800D5E* globals, then call
 *     @c func_800983F0 to install the eline pool.
 *   - Compute centered screen rectangles into @c D_800C7210 / @c D_800C7214
 *     from @c D_8005F0F8 's bounding-box fields, then derive
 *     @c D_800C71F0 (entry-table start = @c *D_800C7204 + 4) and
 *     @c D_800D5E98 (entry-table end = @c D_800C71F0 + count * 24).
 *   - Dispatch @c func_800BF718 with a mode argument that maps
 *     @c D_8005F14C ∈ {6→2, 0xA→3, 3→0, default→1}.
 *
 * @note Decomp at 97.28% match — last 79 instructions of diff are gcc 2.7.2
 *       reg-alloc and instruction-scheduling cascades (one missing
 *       redundant @c addu in the prologue, state==7 @c lbu/sb reorder,
 *       state==1 jal-delay-slot fill choice). See
 *       @c permuter/func_8009895C/base.c for the source and
 *       https://decomp.me/scratch/rFzLA for the in-browser scratch.
 *
 *       Several semantic bugs were caught during decomp and fixed in the
 *       baseline: the @c D_800704A8.mode = 0 dispatch had inverted
 *       condition; @c func_800BF718 's mode arg mapping had 0xA→2 (wrong,
 *       should be 3) and 3→3 (wrong, should be 0); state==7 was missing
 *       the @c sys->unk120 = @c D_8005F14E save; @c D_800D5E98 was missing
 *       the @c +4 offset for the entry-table-end pointer.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009895C);

void func_80099124(void) {
}

/** @brief Call fadeOutSfxFast for sound channels 0-7, then renderAndUpdateDisplay(1). */
void func_8009912C(void) {
    s16 i = 0;

    do {
        fadeOutSfxFast(i);
        i++;
    } while (i < 8);

    renderAndUpdateDisplay(1);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_80099180);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_80099348);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009A0E8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009A2BC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009A4C0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009A7E8);

/**
 * @brief Clear @c trigger4 and @c unk19D across every @c FieldEntityB in the pool.
 *
 * Walks the entire @c D_8008538C pool (size @c D_800852F8) and zeros each
 * entity's @c trigger4 (offset 0x196) and @c unk19D (offset 0x19D). Called
 * from the @c PCOPYINFO / @c SET script opcodes via @c func_8009A8E0(D_8008538C)
 * after copying entity state — likely a reset of trigger-edge / pending-flag
 * bookkeeping so the new state takes effect without leftover triggers.
 *
 * The loop is written do-while with a top-of-iter @c i++ so the count check
 * uses the post-incremented value (@c v1 < count tests "one more iteration
 * is in range"). The pool count @c D_800852F8 is reloaded each iteration
 * because gcc can't prove the stores through @p e don't alias it.
 */
void func_8009A8E0(FieldEntityB *e) {
    s32 i = 0;
    if (D_800852F8 != 0) {
        do {
            i++;
            e->trigger4 = 0;
            e->unk19D = 0;
            e++;
        } while (i < D_800852F8);
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009A920);

/**
 * @brief Restore an event-entry snapshot into the live @c D_800704A8.
 *
 * Copies the 5 snapshot fields stored in an @c EventEntry slot back into
 * the corresponding live fields of @c D_800704A8, and selects the
 * engine @c mode from the snapshotted @c counter:
 *   - @c counter < 72 → @c mode = 7 (e.g. resume an in-progress event)
 *   - otherwise → @c mode = 1 (e.g. start a fresh interaction)
 *
 * Used when reactivating a queued event after it was paused or saved.
 */
void func_8009AA64(EventEntry *e) {
    if (e->counter < 72) {
        D_800704A8.mode = 7;
    } else {
        D_800704A8.mode = 1;
    }
    D_800704A8.counter = e->counter;
    D_800704A8.position_x = e->position_x;
    D_800704A8.position_y = e->position_y;
    D_800704A8.rotation = e->rotation;
    D_800704A8.anim_state = e->anim_state;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009AAC8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009AC9C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009AEC0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009B4A8);

/**
 * @brief Dispatch entity animation based on slot index and animation parameters.
 *
 * Looks up the entity by slot index, sets animation state, checks
 * screen position thresholds, then dispatches to func_8009B4A8 with
 * the appropriate animation field based on the parameter's field_0A value.
 *
 * @param slotIdx Slot index (0 or 1).
 * @param paramIdx Index into the animation parameter array.
 * @param params Animation parameter array.
 * @param multiplier Speed/direction multiplier for the animation.
 */
void func_8009B74C(s16 slotIdx, u16 paramIdx, AnimParam *params, s16 multiplier) {
    u8 entityIdx;

    entityIdx = D_800704A8.entityIndex[slotIdx];

    if (entityIdx == 0xFF) {
        return;
    }

    D_80085224[D_800704A8.entityIndex[(s16)slotIdx]].field_0x241 = params[paramIdx].field_0B;
    D_80085224[D_800704A8.entityIndex[slotIdx]].field_0x24C = 1;

    if (slotIdx == 1) {
        if (D_8005F160 > D_8005F118) {
            params[paramIdx].field_0A = 2;
        }
    } else {
        if (D_8005F162 > D_8005F11A) {
            params[paramIdx].field_0A = 2;
        }
    }

    switch (params[paramIdx].field_0A) {
    case 0:
        entityIdx = D_800704A8.entityIndex[slotIdx];
        func_8009B4A8(entityIdx, D_80085224[D_800704A8.entityIndex[slotIdx]].field_0x250, 0, (s8)(params[paramIdx].field_09 * multiplier));
        break;
    case 1:
        entityIdx = D_800704A8.entityIndex[slotIdx];
        func_8009B4A8(entityIdx, D_80085224[D_800704A8.entityIndex[slotIdx]].field_0x251, 0, (s8)(params[paramIdx].field_09 * multiplier));
        break;
    case 2:
        entityIdx = D_800704A8.entityIndex[slotIdx];
        func_8009B4A8(entityIdx, D_80085224[D_800704A8.entityIndex[slotIdx]].field_0x24F, 0, (s8)(params[paramIdx].field_09 * multiplier));
        break;
    case 3:
        entityIdx = D_800704A8.entityIndex[slotIdx];
        func_8009B4A8(entityIdx, D_80085224[D_800704A8.entityIndex[slotIdx]].field_0x252, 0, (s8)(params[paramIdx].field_09 * multiplier));
        break;
    case 4:
        entityIdx = D_800704A8.entityIndex[slotIdx];
        func_8009B4A8(entityIdx, D_80085224[D_800704A8.entityIndex[slotIdx]].field_0x253, 0, (s8)(params[paramIdx].field_09 * multiplier));
        break;
    case 5:
        entityIdx = D_800704A8.entityIndex[slotIdx];
        func_8009B4A8(D_800704A8.entityIndex[slotIdx], D_80085224[entityIdx].field_0x254, 0, (s8)(params[paramIdx].field_09 * multiplier));
        break;
    }
}

/**
 * @brief Update path-driven entity positions for slots 1 and 2.
 *
 * For each active slot (entityIndex != 0xFF), looks up a path waypoint by
 * angle (computed as (D_8005F144 - phase) & 0x3F → 0..63 entry) and writes
 * its x/y/z (shifted left 12 for fixed-point), unk6 halfword, and unk8 byte
 * to the entity at offsets 0x190, 0x194, 0x198, 0x1FA, 0x258 respectively.
 *
 * Slot 2 reads from D_80070A60 with phase D_8005F11A; slot 1 reads from
 * D_80070760 with phase D_8005F118.
 */
void func_8009BB18(void) {
    u16 angle;

    if (D_800704A8.entityIndex[2] != 0xFF) {
        angle = (D_8005F144 - D_8005F11A) & 0x3F;
        D_80085224[D_800704A8.entityIndex[2]].posX   = D_80070A60[angle].x << 12;
        D_80085224[D_800704A8.entityIndex[2]].posY   = D_80070A60[angle].y << 12;
        D_80085224[D_800704A8.entityIndex[2]].posZ   = D_80070A60[angle].z << 12;
        D_80085224[D_800704A8.entityIndex[2]].field_0x1FA = D_80070A60[angle].unk6;
        D_80085224[D_800704A8.entityIndex[2]].unk258 = D_80070A60[angle].unk8;
    }
    if (D_800704A8.entityIndex[1] != 0xFF) {
        angle = (D_8005F144 - D_8005F118) & 0x3F;
        D_80085224[D_800704A8.entityIndex[1]].posX   = D_80070760[angle].x << 12;
        D_80085224[D_800704A8.entityIndex[1]].posY   = D_80070760[angle].y << 12;
        D_80085224[D_800704A8.entityIndex[1]].posZ   = D_80070760[angle].z << 12;
        D_80085224[D_800704A8.entityIndex[1]].field_0x1FA = D_80070760[angle].unk6;
        D_80085224[D_800704A8.entityIndex[1]].unk258 = D_80070760[angle].unk8;
    }
}

/**
 * @brief Record entity position into both path tables and advance the path phase.
 *
 * Inverse of @c func_8009BB18: writes the entity's posX/posY/posZ (each
 * divided by 4096 for round-toward-zero fixed-point conversion), unk1FA
 * halfword, and two extra bytes (b9 at offset 9, b8 at offset 8) into the
 * current waypoint slot @c D_8005F144 of BOTH path tables (D_80070A60 and
 * D_80070760).
 *
 * If @p mode == 1, also advances the recorder:
 *   - increments @c D_8005F144 (mod 64),
 *   - nudges @c D_8005F118 by one toward @c D_8005F160 (target phase),
 *   - nudges @c D_8005F11A by one toward @c D_8005F162.
 *
 * Each xyz coordinate uses signed `/ 4096` (target compiles this as
 * `bgez; addiu +0xFFF; sra 12` — the round-toward-zero idiom).
 *
 * @param e   Source entity providing posX/posY/posZ/unk1FA.
 * @param mode If 1, advance D_8005F144 and the phase counters.
 * @param b9  Byte stored at offset 9 of each waypoint.
 * @param b8  Byte stored at offset 8 of each waypoint.
 */
void func_8009BD50(Eline *e, s16 mode, s8 b9, u8 b8) {
    PathEntry *base1 = D_80070760;
    PathEntry *p1 = base1 + D_8005F144;
    PathEntry *base0 = D_80070A60;
    PathEntry *p0 = base0 + D_8005F144;
    s16 v;
    u16 u;

    v = e->posX / 4096;
    p0->x = v;
    p1->x = v;
    v = e->posY / 4096;
    p0->y = v;
    p1->y = v;
    v = e->posZ / 4096;
    p0->z = v;
    p1->z = v;
    u = e->field_0x1FA;
    p0->unk6 = u;
    p1->unk6 = u;
    p0->unk9 = b9;
    p1->unk9 = b9;
    {
        PathEntry *q0 = base0 + D_8005F144;
        PathEntry *q1 = base1 + D_8005F144;
        q0->unk8 = b8;
        q1->unk8 = b8;
    }

    if (mode == 1) {
        D_8005F144++;
        if (D_8005F144 == 64) D_8005F144 = 0;

        if (D_8005F118 != D_8005F160) {
            if (D_8005F160 < D_8005F118) D_8005F118--;
            if (D_8005F118 < D_8005F160) D_8005F118++;
        }
        if (D_8005F11A != D_8005F162) {
            if (D_8005F162 < D_8005F11A) D_8005F11A--;
            if (D_8005F11A < D_8005F162) D_8005F11A++;
        }
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009BEC8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009CEE8);

/**
 * Looks up a halfword from the D_800C32A0 table by index.
 *
 * @param a0 Table index (masked to 8 bits).
 * @return The halfword value at D_800C32A0[a0].
 */
s16 func_8009D234(s32 a0) {
    a0 &= 0xFF;
    return *(s16 *)(D_800C32A0 + a0 * 2);
}

/**
 * Looks up a halfword from the D_800C3320 table by index.
 *
 * @param a0 Table index (masked to 8 bits).
 * @return The halfword value at D_800C3320[a0].
 */
s16 func_8009D254(s32 a0) {
    a0 &= 0xFF;
    return *(s16 *)(D_800C3320 + a0 * 2);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009D274);

/**
 * @brief Scratchpad work context — arg2 of @c func_8009D500.
 *
 * Caller passes @c &scratchpad[0x40] (a temporary buffer in PSX
 * scratchpad memory); the struct describes the slice of that area
 * @c func_8009D500 touches.
 */
typedef struct {
    /* 0x00 */ u8  pad00[0x10];
    /* 0x10 */ s32 unk10;       /**< Passed to @c func_8009DF18 as the @c aux pointer. */
    /* 0x14 */ u8  pad14[0x1C];
    /* 0x30 */ s32 srcX;
    /* 0x34 */ s32 srcY;
    /* 0x38 */ s32 srcZ;        /**< Copied to @c outZ without delta. */
    /* 0x3C */ u8  pad3C[0x04];
    /* 0x40 */ s32 outX;        /**< @c outX = @c srcX + @c dx. */
    /* 0x44 */ s32 outY;        /**< @c outY = @c srcY + @c dy. */
    /* 0x48 */ s32 outZ;
    /* 0x4C */ u8  pad4C[0x04];
    /* 0x50 */ s32 dx;
    /* 0x54 */ s32 dy;
} func_8009D500_arg2;

/**
 * @brief Step a position by (@c dx, @c dy, @c 0) and check collision.
 *
 * Writes the stepped position into @c ctx->outX/outY/outZ, calls
 * @c func_8009DF18 to do the per-axis path/extent computation (its
 * return value is captured into @c *out — clever scheduling puts that
 * store in the @c jal @c func_8009E468 delay slot), then runs the
 * collision query @c func_8009E468 against the computed @c outX/Y/Z.
 *
 * @return @c 4 if @c func_8009E468 reported a hit, @c 0 otherwise.
 */
s32 func_8009D500(s32 selfIdx, s32 arg1, func_8009D500_arg2 *ctx, s32 *out) {
    ctx->outX = ctx->srcX + ctx->dx;
    ctx->outY = ctx->srcY + ctx->dy;
    ctx->outZ = ctx->srcZ;
    *out = func_8009DF18(arg1, (Vec3i *)&ctx->outX, &ctx->dx, &ctx->unk10);
    return func_8009E468((s16)selfIdx, (Vec3i *)&ctx->outX) ? 4 : 0;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009D598);

/**
 * Subtracts two 3-component short vectors, storing result as words.
 *
 * @param a0 Destination word array.
 * @param a1 Source vector A (s16 array).
 * @param a2 Source vector B (s16 array).
 */
void func_8009DED8(u8 *a0, u8 *a1, u8 *a2) {
    *(s32 *)(a0 + 0) = *(s16 *)(a1 + 0) - *(s16 *)(a2 + 0);
    *(s32 *)(a0 + 4) = *(s16 *)(a1 + 2) - *(s16 *)(a2 + 2);
    *(s32 *)(a0 + 8) = *(s16 *)(a1 + 4) - *(s16 *)(a2 + 4);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009DF18);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009E338);

/**
 * @brief Test if @p selfIdx overlaps with any other active entity at world @p pos.
 *
 * Iterates over the @c D_80085224 entity table, skipping @p selfIdx itself
 * and any entity with @c unk218 == -1 (inactive). For each remaining entity,
 * a quick z-axis bounding-band check is applied (|dz| < 0x7E after shifting
 * the entity's @c posZ down by 12 fixed-point bits and subtracting @p pos->z),
 * then a 2D radius overlap test against the average of the two radii.
 *
 * Side effects:
 *   - When @p selfIdx matches the global player slot at @c D_8005F148, any
 *     overlapping entity with @c unk249 == 0 has its @c unk248 byte set to 1.
 *   - Whenever an overlap is found and the other entity's @c field_0x24C is
 *     zero, the function returns 1.
 *
 * @return 1 if any overlap was found, 0 otherwise (also 0 if @p selfIdx is
 *         itself inactive).
 */
s32 func_8009E468(s16 selfIdx, Vec3i *pos) {
    s32 selfRadius;
    s32 found = 0;
    s32 dx, dy, dz;
    s32 distSq, avgRadius;
    s16 i;

    selfRadius = D_80085224[selfIdx].radius;
    if (D_80085224[selfIdx].unk218 != -1) {
        for (i = 0; i < D_80085388; i++) {
            if (i == selfIdx) continue;
            if (D_80085224[i].unk218 == -1) continue;
            dz = (D_80085224[i].posZ >> 12) - pos->z;
            if ((u32)(dz + 0x7E) >= 0xFE) continue;
            dx = (D_80085224[i].posX - pos->x) >> 12;
            dy = (D_80085224[i].posY - pos->y) >> 12;
            avgRadius = (selfRadius + D_80085224[i].radius) >> 1;
            distSq = dx * dx + dy * dy;
            avgRadius *= avgRadius;
            if (distSq >= avgRadius) continue;
            if (selfIdx == D_8005F148) {
                if (D_80085224[i].unk249 == 0) {
                    D_80085224[i].unk248 = 1;
                }
            }
            if (D_80085224[i].field_0x24C == 0) found = 1;
        }
    }
    return found;
}

/**
 * Extracts position data from two entity structures (offsets 0x190/0x194,
 * right-shifted by 12) and calls func_8009A0E8 with them.
 *
 * @param a0 First entity pointer.
 * @param a1 Second entity pointer.
 */
s32 func_8009E604(Eline *a, Eline *b) {
    s32 pos1[4];
    s32 pos2[4];
    s32 result[2];

    pos1[0] = a->posX >> 12;
    pos1[1] = a->posY >> 12;
    pos2[0] = b->posX >> 12;
    pos2[1] = b->posY >> 12;
    return func_8009A0E8(pos1, pos2, result);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009E660);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009ECA4);

/**
 * @brief Asymmetric overlap test between two Eline entities.
 *
 * Checks whether entity @p b is within entity @p a 's talk-trigger area:
 *   - Z-axis separation must satisfy @c -126 <= (b->posZ - a->posZ)/4096 <= 127.
 *   - XY-distance squared must be less than @c (a->talkRadius + b->radius) squared.
 *
 * Asymmetric: @p a contributes the @c talkRadius (0x1F8) while @p b
 * contributes the @c radius (0x1F6).
 *
 * The 2-iteration outer loop is a quirk of the original source: nothing
 * inside the body depends on @c i, and the Z check is loop-invariant,
 * so the body either succeeds on both iterations or fails on both.
 * Preserved here for byte-match with the original binary.
 *
 * @note The variable-reuse pattern (@c r used as a scratch for the
 *       @c dz pre-shift, then reassigned inside the loop; the chained
 *       @c r = (dy = r * r) inside the comparison) is what coaxes
 *       gcc 2.7.2 into the exact register allocation the original
 *       used. Not "natural" C — it's a deliberate trick that survived
 *       to match.
 */
s32 func_8009F74C(Eline *a, Eline *b) {
    s32 dz;
    s16 i;
    s32 dx;
    s32 dy;
    s32 r;
    r = (b->posZ - a->posZ) >> 12;
    dz = r;
    r = a->talkRadius + r;
    for (i = 0; i < 2; i++) {
        if ((dz < (-126)) || (dz > 127)) {
            continue;
        }
        dx = (b->posX - a->posX) >> 12;
        dy = (b->posY - a->posY) >> 12;
        r = b->radius;
        r = a->talkRadius + r;
        if (((dx * dx) + (dy * dy)) >= (r = (dy = r * r))) {
            continue;
        }
        return 1;
    }
    return 0;
}

/**
 * @brief Compute a move/turn vector and record it as a waypoint.
 *
 * Dispatches @c func_8009B4A8 with one of three calling conventions
 * driven by the signed @p sign byte:
 *   - @c sign == 0 → @c (idx, b, 0,  0)  (no move)
 *   - @c sign  > 0 → @c (idx, b, 1,  1)  (forward step)
 *   - @c sign  < 0 → @c (idx, b, 1, -1)  (reverse step)
 *
 * After that, calls @c func_8009BD50 (path recorder) on the Eline
 * @c D_80085224[idx] with the requested @c mode and @c sign, then
 * @c func_8009BB18 to publish the resulting waypoint.
 */
void func_8009F7F4(s16 idx, s8 sign, u8 b, s16 mode) {
    if (sign == 0) {
        func_8009B4A8(idx, b, 0, 0);
    } else if (sign > 0) {
        func_8009B4A8(idx, b, 1, 1);
    } else {
        func_8009B4A8(idx, b, 1, -1);
    }
    func_8009BD50(&D_80085224[idx], mode, sign, 0);
    func_8009BB18();
}

/**
 * @brief Interpolate an Eline's X/Y/Z position via the safe-lerp helper.
 *
 * Looks up @c D_80085224[idx] and runs @c func_800A0E54 three times to
 * compute @c posX / @c posY / @c posZ from the unk1A8/AC/B0 endpoints
 * and field_0x1C0/C4/C8 targets, using field_0x1D8/DA as the total/step.
 *
 * @note Decomp at 58.56% match — gcc 2.7.2 caches the @c Eline pointer
 *       across the 3 calls (loads @c D_80085224 once); target reloads
 *       it after each call. Volatile-pointer cast at site made it
 *       worse. See @c permuter/func_8009F8D0/base.c for the current C.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009F8D0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009F990);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009FE18);

/**
 * @brief Transcode an input list (terminated by sentinel @c 0x7FFF) into
 *        an output list of 16-byte records.
 *
 * Walks the list at @c *D_800D5E90 (input, stride 0x10) and writes one
 * 16-byte record to @p out per non-sentinel entry, returning the
 * advanced output pointer. Each output record gets fixed bytes
 * (@c b3=3, @c b4/b5/b6=0x80, @c b7=0x7C), the input's @c bA/bB at
 * @c bC/bD, and the input's @c h8 halfword at @c hE. Bit 1 of @c b7
 * is then cleared (when input @c kind == 4) or set (otherwise).
 *
 * Called by @c func_800983F0 as part of the chain that lays out
 * draw-prim regions back-to-back in one growing buffer.
 *
 * @note Decomp at 84.86% match — store ordering within the loop body
 *       differs (gcc 2.7.2 places the @c sb of @c bD after the 0x80
 *       trio; target has it before). See @c permuter/func_800A0640/base.c.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A0640);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A06F0);

/**
 * @brief Restore VRAM regions for the dialog window and 16 strip layers.
 *
 * Two-phase StoreImage transfer:
 *   - First, copies a 256x24 region from the front of @p buf to VRAM
 *     at @c (0, 232) (the dialog header / pause overlay strip).
 *   - Then, advances @p buf by @c 0x3000 and StoreImages 16 strips of
 *     @c 832x16 (stride @c 0x6800 in the buffer), each placed at
 *     @c (0, 256 + i*16) — a stack of 16 strips of decoration / VRAM
 *     content.
 *
 * Each transfer is sandwiched by @c func_80048C50(1) polls (GPU-busy
 * waits); @c func_80042634(0) is called once per strip to set up the
 * mode for the upcoming StoreImage.
 *
 * @return Restores VRAM in-place; no return value.
 */
void func_800A0D6C(u8 *buf) {
    RECT rect;
    s16 i;
    rect.x = 0;
    rect.y = 0xE8;
    rect.w = 0x100;
    rect.h = 0x18;
    while (func_80048C50(1) != 0) {}
    func_80048EFC(&rect, buf);
    while (func_80048C50(1) != 0) {}
    buf += 0x3000;
    for (i = 0; i < 16; i++) {
        func_80042634(0);
        rect.x = 0;
        rect.y = i * 16 + 0x100;
        rect.w = 0x340;
        rect.h = 0x10;
        func_80048EFC(&rect, buf);
        buf += 0x6800;
    }
    while (func_80048C50(1) != 0) {}
}

/**
 * @brief Overflow-safe s32 linear interpolation.
 *
 * Computes @c start + ((end - start) * progress) / total without
 * intermediate overflow. The difference @c end - start is checked
 * against signed 20-bit range (@c [-0x7FFFF, 0x7FFFF]):
 *   - When the difference fits, multiplies first then divides — the
 *     precise path that keeps fractional information through the
 *     multiplication.
 *   - When the difference is too large to safely multiply by @p progress
 *     in 32-bit, divides first then multiplies — loses some precision
 *     but avoids overflow.
 *
 * The fit check uses @c (u32)(diff + 0x7FFFF) <= 0xFFFFE — adding the
 * positive bias maps the signed range @c [-0x7FFFF, 0x7FFFF] to the
 * unsigned range @c [0, 0xFFFFE] so a single unsigned compare suffices.
 */
s32 func_800A0E54(s32 start, s32 end, s32 total, s32 progress) {
    s32 diff = end - start;
    if ((u32)(diff + 0x7FFFF) <= 0xFFFFE) {
        start += (diff * progress) / total;
    } else {
        start += (diff / total) * progress;
    }
    return start;
}

/**
 * @brief Sine-eased s32 interpolation between two endpoints.
 *
 * Computes an eased lerp from @p start to @p end using a sin lookup as
 * the easing curve. The phase index for the lookup is derived from
 * @p angle and @p total such that one full sin period spans @p total
 * steps:
 *   - @c (angle << 12) / total scales the angle into a fractional
 *     position of the period.
 *   - @c / 32 narrows that to the sin table's @c 0x100-entry resolution.
 *   - @c - 0x80 shifts the phase so the curve crosses zero at the
 *     midpoint instead of the start.
 *
 * The sin sample is returned in @c [-0x1000, 0x1000]; adding @c 0x1000
 * remaps it to @c [0, 0x2000] and dividing by @c 0x2000 gives a
 * fraction of @c (end - start) to add to @p start.
 *
 * @return Eased value between @p start and @p end at phase
 *         @c angle / total.
 */
s32 func_800A0EB8(s32 start, s32 end, s32 total, s32 angle) {
    s32 idx = ((angle << 12) / total) / 32 - 0x80;
    s32 diff = end - start;
    s16 sin_val = func_8009D254(idx & 0xFF);
    return start + ((sin_val + 0x1000) * diff) / 0x2000;
}

/**
 * @brief Project a 3D point through the current world transform and
 *        return the @c func_80040DE4 projection result.
 *
 * Pushes the GTE matrix stack, installs the current world transform
 * (rotation and translation) from @c D_800C71F8, resets the geometric
 * offset to @c (0, 0), then projects @p v to screen space, writing the
 * resulting on-screen XY into @c *sxy and discarding the @c p and flag
 * outputs into stack locals. Pops the matrix stack via
 * @c func_8003FF88 (return discarded) and returns @c func_80040DE4 's
 * result — the saved value survives @c func_8003FF88 by being copied
 * out of @c v0 in the @c jal delay slot.
 */
s32 func_800A0F34(SVECTOR *v, s32 *sxy) {
    s32 result;
    s32 unk_p, unk_flag;
    func_8003FEE4();
    func_800406A4((u8 *)D_800C71F8);
    func_80040734((u8 *)D_800C71F8);
    func_800408C4(0, 0);
    result = func_80040DE4(v, sxy, &unk_p, &unk_flag);
    func_8003FF88();
    return result;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A0FB8);

/**
 * @brief Advance the sub-mode of each of @c D_800704A8.slots[8] from
 *        @c submode == 0 to either @c 1 or @c 2 based on the slot's
 *        @c mode.
 *
 * For each slot whose @c submode is @c 0 (still in init): zeros
 * @c unk06, snapshots @c q1 / @c q2 into @c savedQ1 / @c savedQ2, and
 * if @c mode is in @c 0..5 dispatches a small jump table:
 *   - @c mode 0,1,2,4,5 → @c submode = 1
 *   - @c mode 3        → @c submode = 2 plus copy @c p1 / @c p2 over @c q1 / @c q2
 *
 * @note Decomp at 64.03% match — @c D_800704A8.slots[i] in C makes gcc
 *       2.7.2 fold the @c +0x20 (slots offset) into the base pointer
 *       and use small per-slot offsets; target keeps the base at
 *       @c &D_800704A8 and uses @c 0x22 / @c 0x28 / etc. as the access
 *       immediates. See @c permuter/func_800A10F4/base.c.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A10F4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A11E0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A1318);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A15C0);

/**
 * Clears fields at offsets 0, 1 (bytes) and 0xA, 0xC (halfwords) in the structure.
 *
 * @param a0 Pointer to the structure.
 */
void func_800A17A4(u8 *a0) {
    *(u8 *)(a0 + 0x0) = 0;
    *(u8 *)(a0 + 0x1) = 0;
    *(u16 *)(a0 + 0xA) = 0;
    *(u16 *)(a0 + 0xC) = 0;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A17B8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A19B8);

/**
 * @brief Restore a 256x16 VRAM region from a saved buffer and clear
 *        the @c 0x8000 transparency bit on every pixel.
 *
 * Sets @c D_800C71E4 to point at the saved-image buffer @c D_800D3E88,
 * then (only when the current event queue's @c unk0E flag is @c 1)
 * uses @c StoreImage (via @c func_80048F5C) to write the buffer back
 * to VRAM at @c (0x100, 0x10) with a @c 256x16 RECT, and masks every
 * pixel's @c 0x8000 transparency bit to leave just the colour bits.
 *
 * The two @c func_80048C50(1) polls are GPU-busy waits sandwiching
 * the transfer so the buffer isn't read while another command is
 * still being issued.
 */
void func_800A1BB8(void) {
    RECT rect;
    u16 *p;
    s32 i;
    D_800C71E4 = D_800D3E88;
    if (D_8005F0F8->unk0E != 1) return;
    rect.x = 0x200;
    rect.y = 0xF0;
    rect.w = 0x100;
    rect.h = 0x10;
    while (func_80048C50(1) != 0) {}
    func_80048F5C(&rect, D_800C71E4);
    while (func_80048C50(1) != 0) {}
    p = D_800C71E4;
    for (i = 0; i < 0x1000; i++) {
        *p &= 0x7FFF;
        p++;
    }
}

/**
 * If D_8005F0F8 byte at offset 0xE is 1, sets up a display region
 * (0x200 x 0xF0 at 0x100, 0x10) and calls LoadImage.
 */
void func_800A1C64(void) {
    u8 *data = (u8 *)D_8005F0F8;

    if (*(u8 *)(data + 0xE) == 1) {
        s16 rect[4];
        rect[0] = 0x200;
        rect[1] = 0xF0;
        rect[2] = 0x100;
        rect[3] = 0x10;
        LoadImage(rect, D_800C71E4);
    }
}

/** @brief Initialize 3 entries in D_800D5F50 and D_800D61A8 arrays to -1. */
void func_800A1CC0(void) {
    s32 i = 0;
    s32 val = -1;
    u8 *a = D_800D5F50;
    u8 *b = D_800D61A8;

    do {
        *(s32 *)b = val;
        *(s32 *)a = val;
        a += 0x70;
        i++;
        b += 0x70;
    } while (i < 3);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A1CFC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A2128);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A222C);

/**
 * @brief Initialize a run of items at @p p; return the pointer past the
 *        last item.
 *
 * Iterates @c **D_800D5E9C items, each iteration writing the fixed
 * trio (@c b3 = 4, @c b7 = 0x22, @c b4/b5/b6 = 0). The buffer count
 * @c **D_800D5E9C is reloaded each iteration (gcc can't prove the
 * stores don't alias the indirect chain). Returns the input pointer
 * advanced past the items written, used by @c func_800983F0 to chain
 * multiple init regions into one growing buffer.
 */
func_800A29C0_arg0 *func_800A29C0(func_800A29C0_arg0 *p) {
    s32 i;
    for (i = 0; i < **D_800D5E9C; i++) {
        p->b3 = 4;
        p->b7 = 0x22;
        p->b4 = 0;
        p->b5 = 0;
        p->b6 = 0;
        p++;
    }
    return p;
}

/**
 * @brief Append one GPU draw-mode prim per entry in the @c D_800D5E9C
 *        list; return the advanced output pointer.
 *
 * For each non-sentinel entry (count from @c **D_800D5E9C), calls
 * @c func_8004D524(0, 1, 0, 0) to get a color value, masks to 9 bits,
 * ORs with the GPU draw-mode command base @c 0xE1000200, and writes
 * one 8-byte prim with @c tag=1 + @c cmd=combined.
 *
 * Used by @c func_800983F0 's draw-prim chain layout.
 */
func_800A2A30_item *func_800A2A30(func_800A2A30_item *p) {
    s32 i;
    for (i = 0; i < **D_800D5E9C; i++) {
        s32 color;
        p->tag = 1;
        color = func_8004D524(0, 1, 0, 0);
        p->cmd = (color & 0x9FF) | 0xE1000200;
        p++;
    }
    return p;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A2AF8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A2D2C);

/**
 * @brief Cheap pseudo-random helper: returns @c (table[counter] * range) >> 8.
 *
 * Advances a one-byte counter @c D_800C6D90 by 13 (a prime stride that
 * walks all 256 positions of the lookup table before repeating), reads
 * a byte from the @c D_800C3520 lookup table, multiplies by @p range,
 * and returns the high 24 bits of the product as a signed s16.
 *
 * Used by @c func_800A303C to seed each new particle's position,
 * velocity, and unk fields with a value in roughly @c [-range/2, range/2].
 *
 * @param range Half-range scaler — the table entry (0..255) is multiplied
 *              by @p range and shifted right 8.
 * @return A signed pseudo-random value derived from the lookup table.
 */
s16 func_800A2EA4(s16 range) {
    D_800C6D90 += 13;
    return ((s32)D_800C3520[D_800C6D90] * range) >> 8;
}

/**
 * Initializes an object by calling a sequence of setup functions.
 *
 * @param a0 Pointer to the script/object structure.
 */
void func_800A2EE0(u8 *a0) {
    func_800A3534(a0);
    func_800A3018(a0);
    func_800A2F48(a0);
    func_800A2F70(a0 + 0x3720);
    func_800A2F70(a0 + 0x4B20);
}

/**
 * Clears 16 bytes at offset 0x190 (backwards loop).
 *
 * @param a0 Unused parameter.
 * @param a1 Pointer to the object structure base.
 */
void func_800A2F28(s32 a0, u8 *a1) {
    s32 i = 0xF;
    a1 += 0xF;
    do {
        *(u8 *)(a1 + 0x190) = 0;
        i--;
        a1--;
    } while (i >= 0);
}

/**
 * @brief Shape of the buffer @c func_800A2F48 sees: an array of 128
 * 32-byte items beginning at offset 0x2739, each item's first 3 bytes
 * being what gets zeroed.
 *
 * @note Named after the function/arg it describes rather than after a
 *       semantic role — we don't know what the original developer
 *       called this view. The same memory is also reached by
 *       @c func_800A303C through the @c Particle overlay (where these
 *       3 bytes are the per-particle @c unk19, @c unk1A, and @c active
 *       fields), but it's not certain that the original C used the same
 *       struct in both functions.
 */
typedef struct {
    /* 0x0000 */ u8 pad0000[0x2739];
    /* 0x2739 */ struct {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 pad[0x1D];
    } items[128];
} func_800A2F48_arg0;

/**
 * @brief Zero the 3 leading bytes of each of the 128 items in the table.
 *
 * Called from @c func_800A2EE0 during the particle-system init chain on
 * the disc-loaded field-map buffer.
 */
void func_800A2F48(func_800A2F48_arg0 *t) {
    s32 i;
    for (i = 0; i < 128; i++) {
        t->items[i].b0 = 0;
        t->items[i].b1 = 0;
        t->items[i].b2 = 0;
    }
}

/**
 * @brief Shape @c func_800A2F70 sees: array of 128 40-byte items, three
 *        fields per item — @c b3 / @c b7 (constant tags) and @c hE (a
 *        @c func_8004D564 -seeded halfword).
 *
 * @note Named after the function/arg. Called from @c func_800A2EE0 twice
 *       (at base + 0x3720 and base + 0x4B20) on two different sub-regions
 *       of the disc-loaded field-map buffer, so the shape describes
 *       "whichever sub-region was passed" rather than any single
 *       canonical struct.
 */
typedef struct {
    /* 0x00 */ u8  pad00[0x3];
    /* 0x03 */ u8  b3;
    /* 0x04 */ u8  pad04[0x3];
    /* 0x07 */ u8  b7;
    /* 0x08 */ u8  pad08[0x6];
    /* 0x0E */ s16 hE;
    /* 0x10 */ u8  pad10[0x18];
} func_800A2F70_arg0;  /* 0x28 = 40 bytes */

/**
 * @brief Seed 128 items with the constant tags 9 / 0x2C and a
 *        per-item @c func_8004D564(0, 0xE8) sample at the @c hE field.
 *
 * Called twice from @c func_800A2EE0 on two distinct sub-regions of the
 * disc-loaded field-map buffer; both regions are arrays of 40-byte
 * items, 128 entries each.
 */
void func_800A2F70(func_800A2F70_arg0 *e) {
    s32 i;
    for (i = 0; i < 128; i++) {
        e->b3 = 9;
        e->b7 = 0x2C;
        e->hE = func_8004D564(0, 0xE8);
        e++;
    }
}

/**
 * @brief Shape of the buffer @c func_800A2FE0 sees: an array of 128
 * 32-byte items beginning at offset 0x2739, each item's third byte
 * being the @c active flag scanned for free slots.
 *
 * @note Named after the function/arg. Same memory layout as
 *       @c func_800A2F48_arg0 (which clears all three leading bytes of
 *       each item); the views weren't unified because we don't know
 *       whether the original C source shared a single typedef.
 */
typedef struct {
    /* 0x0000 */ u8 pad0000[0x2739];
    /* 0x2739 */ struct {
        u8 b0;       /* unk19 in particle terms */
        u8 b1;       /* unk1A */
        u8 active;   /* the alive flag */
        u8 pad[0x1D];
    } items[128];
} func_800A2FE0_arg0;

/**
 * @brief Find the first inactive particle slot (0..127), or @c -1 if none.
 *
 * Scans up to 128 items in @p t for the first one whose @c active flag
 * is zero, returning its index. Called by @c func_800A303C to pick a
 * free slot for a new particle.
 */
s16 func_800A2FE0(func_800A2FE0_arg0 *t) {
    s32 i;
    for (i = 0; i < 128; i++) {
        if (t->items[i].active == 0) return (s16)i;
    }
    return -1;
}

/**
 * @brief Zero @c curCount and @c unk15E across the first 16 emitter slots.
 *
 * Called from @c func_800A2EE0 during particle-system init to reset the
 * active-particle counts (and the small @c unk15E word that travels with
 * them) without touching the static jitter ranges.
 */
void func_800A3018(Emitter *em) {
    s32 i;
    for (i = 0; i < 16; i++) {
        em[i].unk15E = 0;
        em[i].curCount = 0;
    }
}

/**
 * @brief Spawn up to @p count particles for emitter @p emIdx around @p pos.
 *
 * Loops while @p count is positive and the emitter has free slots, calling
 * @c func_800A2FE0 to allocate a particle slot, then seeding its position,
 * velocity, and per-particle counters from the emitter's ranges plus the
 * spawn position. Each lookup uses @c func_800A2EA4 for a signed random
 * value in a half-range about zero.
 *
 * @param emIdx  Emitter index (stride 0x174 within @p sys).
 * @param sys    Particle system buffer.
 * @param pos    Spawn anchor position (3 s16 components: x, y, z).
 * @param count  Maximum particles to spawn this call.
 */
void func_800A303C(s16 emIdx, ParticleSystem *sys, s16 *pos, s16 count) {
    Emitter *em = (Emitter *)sys + emIdx;
    Particle *p;
    s16 slot;

    em->unk14E = 0;

    while (1) {
        if (count <= 0) return;
        if (em->curCount >= em->maxCount) return;

        slot = func_800A2FE0(sys);
        if (slot == -1) return;

        em->curCount++;
        p = (Particle *)&sys->slots[slot];

        p->emitterIdx = emIdx;
        p->active = 1;

        p->unk12 = func_800A2EA4(em->unk162 * 32) + em->unk160 * 32 - em->unk162 * 16;
        p->unk16 = func_800A2EA4(em->unk164 * 2) - em->unk164;
        count--;
        p->posX = pos[0] * 16 + (func_800A2EA4(em->unk166) << 8) - em->unk166 * 128;
        p->posY = pos[1] * 16 + (func_800A2EA4(em->unk168) << 8) - em->unk168 * 128;
        p->posZ = pos[2] * 16 + (func_800A2EA4(em->unk16A & 0x7F) << 8) - (em->unk16A & 0x7F) * 128;
        p->velX = func_800A2EA4(em->unk16C * 32) - em->unk16C * 16;
        p->velY = func_800A2EA4(em->unk16E * 32) - em->unk16E * 16;
        p->velZ = func_800A2EA4(em->unk170 * 32) - em->unk170 * 16;
        p->unk19 = 0;
        p->unk1A = 0;
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A327C);

/**
 * @brief View of the Eline stack region used by @c func_800A3488 — two
 * @c s16 endpoints (@c a, @c b) and a @c num/denom progress ratio.
 *
 * @note Named after the function/arg. The same memory is normally the
 *       Eline bytecode @c stack[]; @c func_800A3488 's caller has
 *       already stashed animation state into specific stack slots
 *       before invoking the interpolation.
 */
typedef struct {
    /* 0x00 */ s16 ax, ay, az;          /**< Endpoint A (at progress @c 0). */
    /* 0x06 */ u8  pad06[0x02];
    /* 0x08 */ s16 bx, by, bz;          /**< Endpoint B (at progress @c denom). */
    /* 0x0E */ u8  pad0E[0xD2];
    /* 0xE0 */ u8  denom;               /**< Step count denominator. */
    /* 0xE1 */ u8  padE1[0x0F];
    /* 0xF0 */ s16 num;                 /**< Current step. */
} func_800A3488_arg0;

/**
 * @brief Linearly interpolate a 3D position between two @c s16 endpoints.
 *
 * Writes @c out->vx/vy/vz with @c a + ((b - a) * num) / denom for each
 * axis, where @c a, @c b, @c num, and @c denom come from specific slots
 * of the actor's bytecode stack region.
 *
 * Used by @c func_800A355C dispatch (mode 2) to compute the per-step
 * spawn position for particle bursts.
 */
void func_800A3488(func_800A3488_arg0 *a, SVECTOR *out) {
    out->vx = a->ax + ((a->bx - a->ax) * a->num) / a->denom;
    out->vy = a->ay + ((a->by - a->ay) * a->num) / a->denom;
    out->vz = a->az + ((a->bz - a->az) * a->num) / a->denom;
}

/**
 * @brief Shape of the buffer @c func_800A3534 sees: an array of 16
 * items beginning at offset 0x1830, each item @c 0xFE bytes with three
 * leading s16 fields that get zeroed.
 *
 * @note Named after the function/arg; we don't know the original name.
 *       Same disc-loaded field-map buffer that the rest of the
 *       @c func_800A2EE0 init chain operates on, but the per-item shape
 *       (stride 0xFE, three s16 fields) doesn't align with any other
 *       struct in this file.
 */
typedef struct {
    /* 0x0000 */ u8 pad0000[0x1830];
    /* 0x1830 */ struct {
        s16 h0;
        s16 h1;
        s16 h2;
        u8  pad[0xF8];   /* pad to 0xFE stride */
    } items[16];
} func_800A3534_arg0;

/**
 * @brief Zero the three leading s16 fields of each of 16 items in the table.
 *
 * Called from @c func_800A2EE0 as the first step of the particle-system
 * init chain on the disc-loaded field-map buffer.
 */
void func_800A3534(func_800A3534_arg0 *t) {
    s32 i;
    for (i = 0; i < 16; i++) {
        t->items[i].h0 = 0;
        t->items[i].h1 = 0;
        t->items[i].h2 = 0;
    }
}


/**
 * @brief Animation slot tick & dispatch — runs the per-frame update for the
 * actor's four animation slots.
 *
 * For each of the 4 slots:
 *   - Skip if slot id is -1 (empty).
 *   - Read a "rate" byte from the source-row table (located at
 *     `&actor->rows[i] + actor->animOffset`); when the slot's tick counter
 *     reaches `rate / 8`, reset the counter and pick a `ratio` value
 *     (`rate` itself if rate < 8, else 1).
 *   - Increment the tick counter.
 *   - Dispatch to func_800A303C with one of three position sources, chosen
 *     by `D_800704A8.slotActive[slot]`:
 *       - kind == 1: select by actor->mode — pass the actor itself
 *         (mode 1), or fill `pos` via func_800A3488 (mode 2) or
 *         func_800A327C (mode 3).
 *       - kind != 1: read entity (kind & 0x7F) from D_80085224, divide
 *         posX/Y/Z by 4096, pass as `pos`.
 *
 * @param actor Field entity (with rows[4]/timers[4]/animOffset/mode).
 * @param slot  Index into D_800704A8.slotActive (0..15).
 * @param a2    Second arg passed through to func_800A303C.
 */
void func_800A355C(FieldActor *actor, s32 slot, s32 a2) {
    SVECTOR pos;
    s32 i;

    for (i = 0; i < 4; i++) {
        u8 srcByte;
        s32 ratio;

        if (actor->rows[i].id == -1) {
            continue;
        }

        srcByte = *((u8 *)&actor->rows[i] + actor->animOffset);
        ratio = 0;
        if (actor->timers[i] >= (s32)((u32)srcByte >> 3)) {
            actor->timers[i] = 0;
            if (*((u8 *)&actor->rows[i] + actor->animOffset) < 8) {
                ratio = *((u8 *)&actor->rows[i] + actor->animOffset);
            } else {
                ratio = 1;
            }
        }
        actor->timers[i] = (u16)actor->timers[i] + 1;

        if (D_800704A8.slotActive[slot] == 1) {
            switch (actor->mode) {
            case 1:
                func_800A303C(actor->rows[i].id, a2, (SVECTOR *)actor, ratio);
                break;
            case 2:
                func_800A3488((Eline *)actor, &pos);
                func_800A303C(actor->rows[i].id, a2, &pos, ratio);
                break;
            case 3:
                func_800A327C((Eline *)actor, &pos);
                func_800A303C(actor->rows[i].id, a2, &pos, ratio);
                break;
            }
        } else {
            pos.vx = (s16)(D_80085224[D_800704A8.slotActive[slot] & 0x7F].posX / 4096);
            pos.vy = (s16)(D_80085224[D_800704A8.slotActive[slot] & 0x7F].posY / 4096);
            pos.vz = (s16)(D_80085224[D_800704A8.slotActive[slot] & 0x7F].posZ / 4096);
            func_800A303C(actor->rows[i].id, a2, &pos, ratio);
        }
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A37A8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A38B4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A39D8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A3FE0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A42EC);

/**
 * Zero 8 bytes of D_8005F168 (backwards loop).
 */
void func_800A44D8(void) {
    s32 i = 7;
    volatile u8 *base = D_8005F168;
    u8 *ptr = (u8 *)base + 7;
    do {
        *ptr = 0;
        i--;
        ptr--;
    } while (i >= 0);
}

/**
 * @brief Install the same draw-point at all 8 slots and mark them active.
 *
 * Iterates the 8-slot @c D_800706A0 draw-point table and writes the
 * 12.4 fixed-point position (@p x, @p y, @p z) into each slot's
 * @c x/y/z fields (after shifting down by 12 to drop the fractional
 * bits), and sets the matching @c D_8005F168[i] flag to 1 to mark the
 * slot as occupied.
 *
 * Called from the @c SETDRAWPOINT script opcode with the source
 * eline's position.
 */
void func_800A4500(s32 x, s32 y, s32 z) {
    s32 i;
    for (i = 0; i < 8; i++) {
        D_8005F168[i] = 1;
        D_800706A0[i].x = x >> 12;
        D_800706A0[i].y = y >> 12;
        D_800706A0[i].z = z >> 12;
    }
}

/**
 * Stores a halfword value to the global D_8005F122.
 *
 * @param a0 The value to store.
 */
void func_800A4550(s16 a0) {
    D_8005F122 = a0;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A455C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A4758);

/**
 * @brief Returns 1 if any of the 8 slot bytes in @c D_8005F168 equals 2.
 *
 * Linear scan over @c D_8005F168[0..7] looking for the value @c 2. Sets
 * a result flag (never early-exits — scans all 8 slots regardless of
 * when the match is found). Used by @c fe_object7 's @c DRAWPOINT
 * dispatcher (case 5) as a predicate.
 *
 * @c D_8005F168 is a 8-slot table whose entries are written elsewhere
 * by the field engine; the value @c 2 here is one of those slot states.
 */
s32 func_800A48CC(void) {
    s32 result = 0;
    s32 i;
    for (i = 0; i < 8; i++) {
        if (D_8005F168[i] == 2) {
            result = 1;
        }
    }
    return result;
}

/**
 * Linear interpolation: a0 + (a1 - a0) * a3 / a2.
 *
 * @param a0 Start value.
 * @param a1 End value.
 * @param a2 Divisor.
 * @param a3 Numerator.
 * @return Interpolated value.
 */
s32 func_800A4910(s32 a0, s32 a1, s32 a2, s32 a3) {
    a1 -= a0;
    return a0 + a1 * a3 / a2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A4934);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A4C14);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5224);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5360);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A553C);

/**
 * @brief Dialog tick that counts the timer DOWN; finalize on expire or
 *        when @c func_800BE274 reports a script gate is open.
 *
 * Decrements @c dialogTimer by @c dialogCount (one count-down step),
 * unconditionally clears the @c unk1A1 flag, then:
 *   - If the new timer is still positive (more frames to wait), polls
 *     @c func_800BE274 — if it returns 0 (gate not yet open), we keep
 *     the dialog state intact and return.
 *   - Otherwise (timer expired @em or gate now open) clears both
 *     @c dialogState and @c dialogTimer so the dialog state machine
 *     can advance.
 *
 * Companion to @c func_800A5700 (which counts the timer up); both are
 * driven from the dialog state machine each frame.
 */
void func_800A5698(void) {
    SystemState *sys = &D_800704A8;
    sys->dialogTimer -= sys->dialogCount;
    sys->unk1A1 = 0;
    if ((s16)*(volatile u16 *)&sys->dialogTimer > 0) {
        if (func_800BE274() == 0) return;
    }
    sys->dialogState = 0;
    sys->dialogTimer = 0;
}

/**
 * @brief Advance the dialog timer by one step and clamp at 0xFF.
 *
 * Adds @c dialogCount to @c dialogTimer (one tick of the dialog scroll
 * accumulator), unconditionally clears the @c unk1A1 flag, then if the
 * advanced timer overflows 8-bit range (>= 256 as signed) clamps it
 * back down to 0xFF.
 *
 * The @c (s16)*(volatile u16 *)&...->dialogTimer cast forces a reload
 * of the just-stored value (without making the canonical struct member
 * volatile) — matches the target's lhu-then-sign-extend pattern instead
 * of letting gcc reuse the post-increment value still in a register.
 */
void func_800A5700(void) {
    SystemState *sys = &D_800704A8;
    sys->dialogTimer += sys->dialogCount;
    sys->unk1A1 = 0;
    if ((s16)*(volatile u16 *)&sys->dialogTimer >= 256) {
        sys->dialogTimer = 0xFF;
    }
}

/**
 * @brief Linear interpolation between two s16 endpoints.
 *
 * Returns @c start + ((end - start) * progress) / total — the standard
 * `start * (1 - progress/total) + end * (progress/total)` lerp evaluated
 * in integer arithmetic, with the difference narrowed to s16 before the
 * multiplication so the product fits in s32 even for large @p progress.
 *
 * @param start    Value at @c progress == 0.
 * @param end      Value at @c progress == total.
 * @param progress Current step (typically @c [0, total]).
 * @param total    Step count denominator.
 */
s16 func_800A5748(s16 start, s16 end, s16 progress, s16 total) {
    s16 diff = end - start;
    return start + (diff * progress) / total;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5788);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5898);

/**
 * If D_8005F14A equals 1, calls resetCdDrive. Then clears D_8005F100 and D_8005F14A.
 *
 * @param a0 Pointer to the script/object structure (unused).
 */
void func_800A59D0(u8 *a0) {

    if (D_8005F14A == 1) {
        resetCdDrive();
    }
    D_8005F100 = 0;
    D_8005F14A = 0;
}

/**
 * Stores a halfword value to the global D_8005F142.
 *
 * @param a0 The value to store.
 */
void func_800A5A14(s16 a0) {
    D_8005F142 = a0;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5A20);

/**
 * @brief Cheap PRNG-style byte generator that mixes a lookup-table read
 *        with a slow-moving outer counter.
 *
 * Two-level counter design:
 *   - @c D_8005F151 is the inner counter, incremented by 1 each call;
 *     it wraps every 256 calls.
 *   - @c D_8005F150 is the outer counter, bumped by 13 only when the
 *     inner counter wraps to 0 (so it advances roughly once per 256 calls).
 *
 * The return is @c D_800C3520[D_8005F151] + @c D_8005F150 truncated to
 * a byte — the lookup-table byte mixed with the slow drift counter.
 *
 * Same @c D_800C3520 lookup table that @c func_800A2EA4 and
 * @c func_800A5CF8 use; this is one of several sibling helpers that
 * produce byte-scale pseudo-randomness for the field engine.
 */
u8 func_800A5C9C(void) {
    D_8005F151++;
    if (D_8005F151 == 0) {
        D_8005F150 += 13;
    }
    return D_800C3520[D_8005F151] + D_8005F150;
}

/**
 * Increments the global byte D_8005F103 and returns the value at
 * D_800C3520[D_8005F103].
 *
 * @return The byte from the D_800C3520 lookup table.
 */
u8 func_800A5CF8(void) {

    D_8005F103++;
    return D_800C3520[D_8005F103];
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5D28);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5FA4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A6100);

/**
 * @brief Per-frame dispatch over 12 entries — call @c func_800A5FA4
 *        with an even/odd flag based on the entry's @c mode.
 *
 * Iterates 12 16-byte entries. For each entry where @c active != @c 0xFF,
 * switches on @c mode (0..5) and calls @c func_800A5FA4(entry, flag)
 * where @c flag = 1 for even modes (0/2/4) and 0 for odd modes (1/3/5).
 *
 * @note Decomp at 71.83% match — gcc 2.7.2 strength-reduces @c items[i]
 *       to an end-pointer comparison while target keeps an explicit
 *       counter; also different s-reg allocation. See
 *       @c permuter/func_800A62EC/base.c for the current C.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A62EC);

/* ============================================================================
 * PsyQ 4.3 boundary — every non-leaf function from here to the end of the
 * file (func_800A63AC .. func_800AA8A0) was originally compiled with the
 * PsyQ 4.3 toolchain (gcc 2.8.0), evidenced by FILLED branch-delay-slot
 * epilogues (`jr ra; addiu sp` instead of PsyQ 4.1's `addiu sp; jr ra; nop`).
 * Confirmed by recompiling the same C with tools/gcc-2.8.0-psx/cc1.
 *
 * fe_object1.c as a whole compiles with PsyQ 4.1, so these functions can't
 * match while they live here. To decomp them in C, this file likely needs
 * to be split at 0x800A63AC into a PsyQ 4.1 part (above) and a PsyQ 4.3
 * part (below, added to PSYQ43_SRCS in the Makefile). Until the split is
 * done, the functions below stay as INCLUDE_ASM stubs.
 * ============================================================================ */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A63AC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A6A80);

/**
 * @brief Reset three field-engine tables.
 *
 * 1. Calls @c func_80047CE4 (@c memset) on each of 64 @c D_800DD6D0
 *    entries (stride 0x30 = 48 bytes), zeroing each in turn.
 * 2. Clears 64 @c s32 entries at @c D_800D6620.
 * 3. Clears 32 @c EntityRenderSlot* pointer entries at @c D_800D9630.
 *
 * The forward for-loops in the scratch source compile to backward
 * @c bgez loops — gcc strength-reduces the index since each iteration
 * is independent.
 *
 * @note Function compiled with PsyQ 4.3 (gcc 2.8.0) in the original
 *       binary — the @c jr-@c ra/@c addiu-@c sp FILLED epilogue is the
 *       4.3 signature. fe_object1.c as a whole is PsyQ 4.1, so this
 *       function and 12 others (0x800A63AC+ scattered to end of file)
 *       stay as INCLUDE_ASM until we either split the file or accept
 *       a per-function toolchain override. The current PsyQ 4.1
 *       compile sits at 95.56%, off by one epilogue word.
 *       See @c permuter/func_800A7194/base.c for the C body.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A7194);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A7224);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A736C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A74B4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A7564);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A8058);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A81AC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A8CDC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A8DAC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A91C8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A9434);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A97E4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800AA46C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800AA5F8);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800AA870);

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800AA8A0);
