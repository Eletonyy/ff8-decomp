#include "common.h"
#include "field.h"
#include "main.h"
#include "psxsdk/libgte.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "field/fe_object1.h"
#include "field/fe_object10.h"



extern u16 D_8005F118;
extern u16 D_8005F11A;
extern u16 D_8005F144;
extern s16 D_8005F148;
extern volatile u8 D_8005F116;   /**< Encounter-disable flag (1 = no random battles); also spun on by the field loader. */
extern u16 D_8005F0FE;           /**< Accumulated battle chance; compared against the encounter RNG roll. */
extern s16 D_8005F120;           /**< Previous battle formation id (avoid immediate repeats). */
extern u8 D_8005F130;            /**< Encounter-pending marker set when a battle triggers. */
extern u16 D_8005F164;           /**< Step accumulator; a battle check runs each time it passes 0x100. */
extern u8 D_80078DF8;            /**< Field movement flags: bit 3 halts encounter steps, bit 2 halves the step rate. */
extern u8 **D_800C71F4;          /**< Field-data section pointer: per-field encounter step-rate byte. */
extern u16 **D_800C720C;         /**< Field-data section pointer: 4-entry battle formation table. */
extern u16 D_8005F160;
extern u16 D_8005F162;
extern u8 D_80085388;
extern u8 D_800C319C[];          /**< Arctangent lookup table (byte per 2*|component| step) for func_8009A0E8. */
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
extern void func_8004D684(void *p);
extern s32 func_8003F4A4(s32 a);                  /* isqrt: integer square root of a */

extern u16 **D_800D5E9C;         /**< Pointer-to-pointer of u16 count for func_800A29C0's iteration */
extern u16 *D_800C71E4;
extern s32 D_800C71FC;           /**< Latched result of @c func_800A0F34 from @c func_800A11E0. */
extern u16 D_800D3E88[];
extern u8 D_800D5F50[];
extern u8 D_800D61A8[];
extern u8 D_8005F168[];
extern volatile s32 D_8005F154;  /**< VSync frame counter (main.c); phase for the @c D_800C3520 perturbation table. */

/**
 * @brief 24-byte draw-point slot holding a 16-bit (x, y, z) position plus the
 *        two derived quad corners built by @c func_800A4758.
 *
 * @c x/y/z is the base position written by @c func_800A4500. @c func_800A4758
 * derives two corner offsets from it: @c field8 / @c fieldA / @c fieldC (base
 * minus a table-perturbed 0x80 bias) and @c field10 / @c field12 / @c field14
 * (base plus fixed 0x40/0x80 offsets).
 */
typedef struct {
    /* 0x00 */ u16 x;
    /* 0x02 */ u16 y;
    /* 0x04 */ u16 z;
    /* 0x06 */ u16 pad06;
    /* 0x08 */ u16 field8;
    /* 0x0A */ u16 fieldA;
    /* 0x0C */ u16 fieldC;
    /* 0x0E */ u16 padE;
    /* 0x10 */ u16 field10;
    /* 0x12 */ u16 field12;
    /* 0x14 */ u16 field14;
    /* 0x16 */ u16 pad16;
} DrawPoint;  /* 0x18 = 24 bytes */
extern DrawPoint D_800706A0[];

/**
 * @brief One 136-byte object slot in the @c D_800C6DA0 table walked by
 *        @c func_800A5224 (8 slots).
 *
 * The two 8-entry vertex arrays @c va / @c vb are seeded by @c func_800A4758
 * from the object's draw-point position (all 8 entries get the same value).
 * @c field86 / @c field87 receive a table-perturbation byte; the trailing tick
 * pair (@c field80 / @c field82) is what @c func_800A5224 later consumes.
 */
typedef struct {
    /* 0x00 */ ObjVertex va[8];
    /* 0x40 */ ObjVertex vb[8];
    /* 0x80 */ s16 field80;   /**< Tick threshold base; slot clears when tick > field80+4. */
    /* 0x82 */ u16 field82;   /**< Per-frame tick counter (incremented while active). */
    /* 0x84 */ u8  pad84[0x02];
    /* 0x86 */ u8  field86;
    /* 0x87 */ u8  field87;
} ObjSlot;  /* 0x88 = 136 bytes */
extern ObjSlot D_800C6DA0[];
extern s16 D_8005F122;
extern s16 D_8005F14A;
extern s16 D_8005F100;
extern s16 D_8005F142;
extern s16 D_800C2568[];         /**< Field id -> streaming-table entry index. */
extern u32 D_800C0904[];         /**< Streaming table: 24-byte (6-word) entries; this symbol
                                      addresses entry 0's size word, with its sector word
                                      in the preceding word. */
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

/** @brief Tag written into a field-file header that carries no script section. */
#define FIELD_HEADER_EMPTY 0x2020
/** @brief Size of the fixed field-file header the script region starts after. */
#define FIELD_HEADER_SIZE 0x5F24

/**
 * @brief CD landing area for the field bundle: a small parameter header
 *        followed by the payload.
 *
 * Each slot is addressed as its own absolute constant rather than through a
 * struct or a @c D_800E1004 -style extern. That is not cosmetic: cc1 emits
 * @c lui/lw with the destination as its own @c %hi temp for a constant
 * address, but allocates a separate @c %hi temp for a symbol, and folds a
 * struct down to one base register plus displacements. Only the form below
 * reproduces the original's two independent address materialisations.
 */
#define FIELD_BUNDLE_BUF     0x800E1000
#define FIELD_BUNDLE_TIM     (FIELD_BUNDLE_BUF + 0x4) /**< Halfword staging buffer for @c func_800A2D2C. */
#define FIELD_BUNDLE_SLOT    (FIELD_BUNDLE_BUF + 0x8) /**< VRAM column slot for @c func_800A2D2C. */
#define FIELD_BUNDLE_STRIPS  (FIELD_BUNDLE_BUF + 0xC) /**< VRAM restore strips for @c func_800A0D6C. */

/** @brief High-RAM staging area the script region is copied into. */
#define FIELD_SCRIPT_STAGE 0x801B0000

/**
 * @brief Upper bound of the field command area handed to @c func_800AA8A0.
 *
 * Load mode 3 stops short of the @c 0x801F0000 mesh-render region; every other
 * mode may run up to the menu image base.
 * @note Purpose inferred from the call site — only the mode-3 split is certain.
 */
#define FIELD_CMD_AREA_END_MODE3 0x801F4000
#define FIELD_CMD_AREA_END       0x801FE800

/**
 * @brief Load/refresh the active field map's asset bundle from CD.
 *
 * Either issues a fresh CD read (when @c D_8005F14A is 0 or the current area
 * @c D_8005F14E differs from the cached @c D_8005F100) or restores from the
 * cached pointer @c D_8005F104. Then loads the secondary asset, snapshots
 * pointer-table headers into globals (@c D_800C7208, @c D_800D5EA4 family,
 * etc.), copies script data to the @c FIELD_SCRIPT_STAGE staging region, and
 * dispatches the field-VM pool setup via @c func_800BFBBC and friends.
 *
 * @return Pointer past the last block laid out after the bundle.
 *
 * @note The streaming table at @c D_800C0900 is three @c {sector,size} pairs
 *       per 24-byte entry, and the two words of a pair must be read as separate
 *       flat-array elements (@c D_800C0900[i*6] / @c [i*6+1]) — a struct field
 *       pair makes gcc share one address register where the original computes
 *       it twice. The @c func_800A1CC0 guard is
 *       @c ((state != 1 && state != 6) || unk0D == 1) — the call fires for
 *       every state outside {1,6}, not only inside them.
 * @note @c ptr is deliberately re-read into itself (@c ptr++/@c ptr--) after
 *       @c buf is copied from it, and is reused for the return value at the
 *       end. Both are load-bearing for the register allocation gcc 2.7.2
 *       produces here: the bump splits the two pointers into separate registers
 *       (without it they share one, and the header pointer is loaded straight
 *       into the callee-saved register instead of @c a0), and the trailing
 *       reuse of @c ptr keeps @c buf from being picked as the shared name for
 *       the pair, which is what puts the @c D_800C7200 address in @c s0.
 */
s32 *func_800983F0(void) {
    s32 stageBase;
    u8 *ptr;
    u8 *buf;
    u32 tag;
    s32 size;
    u8 *heapEnd;

    if (D_8005F14A == 0 || D_8005F14E != D_8005F100) {
        func_80038868(D_800C0900[D_800C2568[D_8005F14E] * 6],
                      D_800C0900[D_800C2568[D_8005F14E] * 6 + 1], (u8 *)FIELD_BUNDLE_BUF,
                      NULL);
        while (func_800393C8() != 0) {}
    } else {
        while (func_800393C8() != 0) {}
        func_80038490(D_8005F104, FIELD_BUNDLE_BUF);
    }

    func_800A0D6C((u8 *)FIELD_BUNDLE_STRIPS);
    while (func_80048C50(1) != 0) {}

    func_800A2D2C(*(s16 **)FIELD_BUNDLE_TIM, *(s32 *)FIELD_BUNDLE_SLOT);

    D_8005F100 = 0;
    D_8005F142 = 0;
    func_80038868(D_800C0908[D_800C2568[D_8005F14E] * 6],
                  D_800C0908[D_800C2568[D_8005F14E] * 6 + 1], (u8 *)FIELD_BUNDLE_BUF, NULL);
    while (func_800393C8() != 0) {}

    D_8005F0F8 = (EventQueue *)*D_800C7208;
    D_800D5EA4 = *D_800C71EC;
    stageBase = FIELD_SCRIPT_STAGE;
    D_800704A8.unk018 = *D_800D5EAC;
    size = *D_800D5E8C - *D_800D5ED4;
    func_80039678(FIELD_SCRIPT_STAGE, (s32)*D_800D5ED4, size);
    D_8005F13C = stageBase + size;

    ptr = *D_800D5E94;
    buf = ptr;
    ptr++;
    ptr--;
    tag = *(s16 *)buf;
    D_800C7200 = ptr;
    if (tag != FIELD_HEADER_EMPTY) {
        func_800A2EE0(ptr);
        buf += FIELD_HEADER_SIZE;
        *D_800D5ED4 = buf;
        if (D_8005F14C != 3 && D_8005F14C != 6 && D_8005F14C != 0xA) {
            func_800A2F28((s32)D_800C7200, (u8 *)&D_800704A8);
        }
    } else {
        D_800C7200 = NULL;
    }

    D_800704B2 = 20;

    if (D_8005F14C == 3) {
        buf = (u8 *)func_800BFBBC((u8 *)FIELD_SCRIPT_STAGE, (FieldEntityB *)0x80090800, (u16 *)*D_800D5ED4, 0);
    } else {
        buf = (u8 *)func_800BFBBC((u8 *)FIELD_SCRIPT_STAGE, (FieldEntityB *)0x80090800, (u16 *)*D_800D5ED4, 1);
    }

    D_800C6D98[0] = buf;
    buf = (u8 *)func_800A0640((TILE *)buf);
    D_800C6D98[1] = buf;
    buf = (u8 *)func_800A0640((TILE *)buf);

    if (D_8005F0F8->unk0E == 1) {
        D_800D5EC8[0] = buf;
        buf = (u8 *)func_800A29C0((func_800A29C0_arg0 *)buf);
        D_800D5EC8[1] = buf;
        buf = (u8 *)func_800A29C0((func_800A29C0_arg0 *)buf);
        D_800D5EB8[0] = buf;
        buf = (u8 *)func_800A2A30((func_800A2A30_item *)buf);
        D_800D5EB8[1] = buf;
        buf = (u8 *)func_800A2A30((func_800A2A30_item *)buf);
    }

    if ((D_8005F14C != 1 && D_8005F14C != 6) || D_8005F0F8->unk0D == 1) {
        func_800A1CC0();
    }

    if (D_8005F14C == 3) {
        heapEnd = (u8 *)FIELD_CMD_AREA_END_MODE3;
    } else {
        heapEnd = (u8 *)FIELD_CMD_AREA_END;
    }

    if (D_8005F0F8->unk0D == 0) {
        buf = (u8 *)func_800AA8A0(buf, buf + 0x20000, D_800C30DC, D_800C311C,
                                  (u8 *)&D_800C0910[D_800C2568[D_8005F14E] * 3], 0, D_800C06A0,
                                  heapEnd);
    } else {
        buf = (u8 *)func_800AA8A0(buf, buf + 0x20000, D_800C30DC, D_800C315C,
                                  (u8 *)&D_800C0910[D_800C2568[D_8005F14E] * 3], 0, D_800C06A0,
                                  heapEnd);
    }

    if (D_8007064D == 0) {
        while (D_8005F116 != 0) {}
    }
    while (D_8005F146 == 4) {}
    while (func_80048C50(1) != 0) {}

    if (D_8005F14C == 3 || D_8005F14C == 0) {
        func_80048BB8(0);
    }

    ptr = buf;
    return (s32 *)ptr;
}

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
 *     @c D_800D5E98 (entry-table end = @c D_800C71F0 + count * 3 vertices).
 *   - Dispatch @c func_800BF718 with a mode argument that maps
 *     @c D_8005F14C ∈ {6→2, 0xA→3, 3→0, default→1}.
 *
 * @note Decomp at 98.78% match; @c permuter/func_8009895C/base.c holds the
 *       source (mirrored to the NAS backup, since @c permuter/ is ignored).
 *       Eight instructions of real diff remain, in two clusters: the prologue
 *       materialises @c &D_800704A8 one @c addu short of the original (which
 *       goes @c lui @c -> @c fp @c -> @c v0 @c -> @c s2, keeping the @c %hi in
 *       its own pseudo), and the @c func_800BF718 argument chain emits its
 *       @c ==3 test before the @c ==6 / @c ==0xA pair instead of after.
 *       The @c ==3 test has to lead in the source: with the original's
 *       @c if @c (x @c == @c 6 @c || @c x @c == @c 0xA) nesting, gcc folds the
 *       inner @c 0 / @c 1 arms into @c xori + @c sltu, which the original does
 *       not have. Everything else — all four loops, the section-pointer
 *       snapshot, and the whole state dispatch — is instruction-exact.
 *       https://decomp.me/scratch/rFzLA is the in-browser scratch.
 *
 *       Several semantic bugs were caught during decomp and fixed in the
 *       baseline: the @c D_800704A8.mode = 0 dispatch had inverted
 *       condition; @c func_800BF718 's mode arg mapping had 0xA→2 (wrong,
 *       should be 3) and 3→3 (wrong, should be 0); state==7 was missing
 *       the @c sys->unk120 = @c D_8005F14E save; @c D_800D5E98 was missing
 *       the @c +4 offset for the entry-table-end pointer. Two more were
 *       found since: the baseline dropped both @c isrgb24 clears on the
 *       @c DISPENV pair (@c D_80067440[0x11] and @c [0x25]) before
 *       @c PutDispEnv, and in state==1 it stored @c D_8005F14E after the
 *       @c sndCmd21 call rather than before — the original loads
 *       @c sys->counter first and lets dbr sink the store into the jal
 *       delay slot, so writing it after the call reads a post-call value.
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

extern void func_800275D4(void);                  /* thread.c: refresh raw controller buffers */
extern s32  getAnimFrameParam(s32 slot, s32 sub); /* per-pad input-frame param (s32 view) */
extern s32  func_80027A58(s32 a, s32 b);          /* per-pad newly-pressed input */
extern s32  func_80027DB4(s32 a, s32 b, s32 c);   /* read an analog axis (b: 2 = X, 3 = Y) */
extern s32  func_80030F10(s32 arg);               /* map pad input word to a button mask */

/**
 * @brief Per-tick controller-input sampling for the field engine's two pad slots.
 *
 * Snapshots the previous held/button state (@c padHeld → @c padHeldPrev,
 * @c unk150 → @c unk154), refreshes the raw pad buffers, then re-reads the held
 * (@c getAnimFrameParam) and pressed (@c func_80027A58) input for slots 0 and 1.
 *
 * When slot 0 has no direction bits latched yet (@c padHeld & 0xF000 == 0) and a
 * pad is present, it converts the two analog axes into direction bits: X read
 * (@c func_80027DB4 axis 2) sets 0x8000 when @c <0x40 or 0x2000 when @c >=0xC1,
 * Y read (axis 3) sets 0x1000 / 0x4000. Each bit is OR'd into @c padHeld always
 * and into @c padPressed only when it was not held last tick (edge detect).
 *
 * Finally derives the held/pressed button masks (@c func_80030F10) into
 * @c unk150 / @c ambientFlags.
 */
void func_80099180(void) {
    s32 r;

    D_800704A8.padHeldPrev = D_800704A8.padHeld;
    D_800704A8.unk154 = D_800704A8.unk150;
    func_800275D4();
    D_800704A8.padHeld = getAnimFrameParam(0, 0);
    D_800704A8.padPressed = func_80027A58(0, 0);
    D_800704A8.field_0x160 = getAnimFrameParam(1, 0);
    D_800704A8.field_0x168 = func_80027A58(1, 0);

    if (!(D_800704A8.padHeld & 0xF000) && func_80027DB4(0, 2, 0) != -1) {
        r = (s16)func_80027DB4(0, 2, 0);
        if (r < 0x40) {
            D_800704A8.padHeld |= 0x8000;
            if (!(D_800704A8.padHeldPrev & 0x8000)) D_800704A8.padPressed |= 0x8000;
        } else if (r >= 0xC1) {
            D_800704A8.padHeld |= 0x2000;
            if (!(D_800704A8.padHeldPrev & 0x2000)) D_800704A8.padPressed |= 0x2000;
        }
        r = (s16)func_80027DB4(0, 3, 0);
        if (r < 0x40) {
            D_800704A8.padHeld |= 0x1000;
            if (!(D_800704A8.padHeldPrev & 0x1000)) D_800704A8.padPressed |= 0x1000;
        } else if (r >= 0xC1) {
            D_800704A8.padHeld |= 0x4000;
            if (!(D_800704A8.padHeldPrev & 0x4000)) D_800704A8.padPressed |= 0x4000;
        }
    }

    D_800704A8.unk150 = func_80030F10(D_800704A8.padHeld);
    D_800704A8.ambientFlags = func_80030F10(D_800704A8.padPressed);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_80099348);

/**
 * @brief Angle (8-bit BAM) and distance between two 2D points.
 *
 * Computes @c dx / @c dy from the two points, stores the squared distance to
 * @c *outDist, replaces it with the true distance via @c func_8003F4A4 (isqrt),
 * then normalizes the deltas to a fixed ~[-128,128] scale and resolves the
 * octant with a magnitude compare (@c |dx| vs @c |dy|) plus the two sign tests.
 * Each octant reads the arctangent table @c D_800C319C at @c 2*|minor| and adds
 * the appropriate quadrant offset; the result is a full-circle angle where
 * 256 == one revolution.
 *
 * @param p0       First point (@c p0[0]=x, @c p0[1]=y).
 * @param p1       Second point (@c p1[0]=x, @c p1[1]=y).
 * @param outDist  Receives the distance from @p p0 to @p p1.
 * @return Angle from @p p0 to @p p1 in the range 0-255.
 *
 * @note The shared @c "return (r + 0x40) & 0xFF" is what lets gcc 2.7.2
 *       cross-jump the eight octant tails into one, matching the target's
 *       merged code. Return type is @c s32 (not @c u8) so the @c &0xFF mask
 *       stays in this function and the caller keeps a plain @c v0 passthrough.
 */
s32 func_8009A0E8(s32 *p0, s32 *p1, s32 *outDist) {
    s32 dx = p1[0] - p0[0];
    s32 dy = p1[1] - p0[1];
    s32 d2 = dx * dx + dy * dy;
    s32 dist;
    s32 r;

    *outDist = d2;
    dist = func_8003F4A4(d2);
    *outDist = dist;

    dx = ((dx << 12) / dist) / 32;
    dy = ((dy << 12) / dist) / 32;

    if (dx * dx > dy * dy) {
        if (dx > 0) {
            if (dy > 0) r = D_800C319C[2 * dy];
            else r = -D_800C319C[-2 * dy];
        } else {
            if (dy > 0) r = -0x80 - D_800C319C[2 * dy];
            else r = D_800C319C[-2 * dy] - 0x80;
        }
    } else {
        if (dy > 0) {
            if (dx > 0) r = 0x40 - D_800C319C[2 * dx];
            else r = D_800C319C[-2 * dx] + 0x40;
        } else {
            if (dx > 0) r = D_800C319C[2 * dx] - 0x40;
            else r = -0x40 - D_800C319C[-2 * dx];
        }
    }
    return (r + 0x40) & 0xFF;
}

/**
 * @brief Project a point onto a 3D line segment and return the squared
 *        distance to the closest point, bounds-checked in X and Y.
 *
 * Computes the fixed-point projection parameter
 * @c t = -256 * dot(P0 - P, dir) / |dir|^2 and writes the closest point
 * @c P0 + t*dir/256 to @p out. The projection is then validated against the
 * segment's X and Y coordinate ranges (either winding accepted; Z is
 * unconstrained): if the closest point falls outside, -1 is returned and the
 * caller treats the segment as missed.
 *
 * @param seg  Line segment (start/end XYZ as consecutive s16 coords).
 * @param p    Query point.
 * @param out  Receives the closest point on the segment's carrier line.
 * @return Squared 3D distance from @p p to the closest point, or -1 when the
 *         projection lies outside the segment's X/Y extent.
 *
 * @note One variable @c d carries the division quotient, the -1 miss value,
 *       and the final squared distance through a single return — and the
 *       bounds checks use bail-out gotos with second-chance labels
 *       (@c xc2 / @c yc2) entered both by branch and fall-through. Both are
 *       original-source structure: restructured single-purpose-variable or
 *       pure if/else forms compile to measurably different code (the
 *       fall-through recheck gets folded away by cse/jump threading).
 */
s32 func_8009A2BC(LineSeg *seg, Vec3i *p, Vec3i *out) {
    s32 dx, dy, dz;
    s32 tx = (seg->x0 - p->x) * (dx = seg->x1 - seg->x0);
    s32 ty = (seg->y0 - p->y) * (dy = seg->y1 - seg->y0);
    s32 tz = (seg->z0 - p->z) * (dz = seg->z1 - seg->z0);
    s32 d = (-((tx + ty + tz) << 8)) / (dx * dx + dy * dy + dz * dz);
    s32 ex, ey, ez;
    s32 ax, ay, ox, oy;

    out->x = ((d * dx) >> 8) + seg->x0;
    out->y = ((d * (seg->y1 - seg->y0)) >> 8) + seg->y0;
    out->z = ((d * (seg->z1 - seg->z0)) >> 8) + seg->z0;

    ax = seg->x0 - (ox = out->x);
    if (ax < 0) goto xc2;
    if (seg->x1 - ox <= 0) goto yaxis;
    if (ax > 0) goto fail;
xc2:
    if (seg->x1 - ox < 0) goto fail;
yaxis:
    ay = seg->y0 - (oy = out->y);
    if (ay < 0) goto yc2;
    if (seg->y1 - oy <= 0) goto sum;
    if (ay > 0) goto fail;
yc2:
    if (seg->y1 - oy < 0) goto fail;
sum:
    ex = out->x - p->x;
    ey = out->y - p->y;
    ez = out->z - p->z;
    d = ex * ex + ey * ey + ez * ez;
    goto done;
fail:
    d = -1;
done:
    return d;
}

/**
 * @brief Per-frame update of the @ref FieldEntityB trigger pool against the
 *        self entity's position.
 *
 * Stages @p self 's position and a secondary query point @p pt (its Z is taken
 * from @p self) into the PSX scratchpad, then for each enabled record
 * (@c activeMarker @c == @c 1) projects the query point onto the record's
 * segment (@c x0..z1) via @ref func_8009A2BC. When the projection lies inside
 * @c self->radius the record is latched in-range (@c trigger4) and its trigger
 * state is refreshed: @c trigger2 on the first in-range frame, @c trigger5 when
 * self and the query point straddle the segment edge (2D cross-product signs
 * differ), @c trigger6 / @c unk19D when self coincides with the projected point
 * or falls within a +/-64 facing window, @c unk19C the facing angle
 * (@ref func_8009A0E8), and @c trigger7 (1/2) from the current pad-hold mode.
 * When out of range, @c trigger3 is raised on the frame the record leaves.
 *
 * @param self    Querying entity.
 * @param records @ref FieldEntityB pool (count @c D_800852F8).
 * @param pt      Secondary query point; its Z is taken from @p self.
 * @return Always 0.
 *
 * @note The scratchpad points are staged as three consecutive VECTORs at
 *       @c getScratchAddr(0) / +0x10 / +0x20; @c queryPt and @c proj are
 *       derived from @c selfPos (not built as fresh constants) so the compiler
 *       shares one scratchpad base register across all three (addu+ori), as in
 *       the original. The cursor inits before the staging (fc first, and fc
 *       re-assigned after it), the empty do/while barrier, and the
 *       @c fc++,rec++ increment order pin the original's register allocation
 *       and the loop-end branch-delay schedule.
 */
s32 func_8009A4C0(Eline *self, FieldEntityB *records, VECTOR *pt) {
    VECTOR *selfPos = (VECTOR *)getScratchAddr(0);
    VECTOR *queryPt;
    VECTOR *proj;
    FieldEntityB *fc;
    FieldEntityB *rec;
    s32 i;
    s32 dist;

    fc = records;
    rec = records;
    selfPos->vx = self->posX >> 12;
    selfPos->vy = self->posY >> 12;
    selfPos->vz = self->posZ >> 12;
    queryPt = (VECTOR *)((u32)selfPos | 0x10);
    proj = (VECTOR *)((u32)selfPos | 0x20);
    queryPt->vx = pt->vx >> 12;
    queryPt->vy = pt->vy >> 12;
    queryPt->vz = self->posZ >> 12;
    fc = records;
    do { } while (0);

    for (i = 0; i < D_800852F8; i++, fc++, rec++) {
        if (fc->activeMarker != 1) {
            continue;
        }
        fc->unk19D = 0;
        dist = func_8009A2BC((LineSeg *)&rec->x0, queryPt, proj);
        if (dist != -1 && dist < self->radius * self->radius) {
            s32 dx;
            s32 dy;
            s32 crossSelf;
            s32 crossPt;

            if (fc->trigger4 == 0) {
                fc->trigger2 = 1;
            }
            fc->trigger4 = 1;
            dx = fc->x1 - fc->x0;
            dy = fc->y1 - fc->y0;
            crossSelf = dx * (selfPos->vy - fc->y0) - (selfPos->vx - fc->x0) * dy;
            crossPt = dx * (queryPt->vy - fc->y0) - (queryPt->vx - fc->x0) * dy;
            if ((crossSelf >= 0 && crossPt < 0) || (crossPt >= 0 && crossSelf < 0)
                || (crossSelf > 0 && crossPt <= 0) || (crossPt > 0 && crossSelf <= 0)) {
                fc->trigger5 = 1;
            }
            if (selfPos->vx == proj->vx && selfPos->vy == proj->vy) {
                fc->trigger6 = 1;
                fc->unk19D = 1;
            } else {
                fc->unk19C = func_8009A0E8((s32 *)selfPos, (s32 *)proj, &dist);
                if (((fc->unk19C - self->unk23F + 0x40) & 0xFF) < 0x80) {
                    fc->trigger6 = 1;
                    fc->unk19D = 1;
                }
            }
            if (fc->unk19D == 1 && ((fc->unk19C - self->unk23F + 0x20) & 0xFF) < 0x40) {
                if ((D_800704A8.unk150 & 0x40) && !(D_800704A8.unk154 & 0x40)) {
                    fc->trigger7 = 1;
                }
                if ((D_800704A8.unk150 & 0x80) && !(D_800704A8.unk154 & 0x80)) {
                    fc->trigger7 = 2;
                }
            }
        } else {
            if (fc->trigger4 == 1) {
                fc->trigger3 = 1;
            }
            fc->trigger4 = 0;
        }
    }

    return 0;
}

/**
 * @brief Sync per-entity @c trigger7 across the FieldEntityB pool from
 *        the global @c unk150 / @c unk154 SFX flag pair.
 *
 * Iterates the @c D_800852F8 entities at @c pool. For each entity that
 * passes the gating tests
 *   - @c activeMarker == 1
 *   - @c eline->msgActive == 0
 *   - @c unk19D == @c activeMarker (so @c unk19D == 1)
 *   - @c entity->unk19C falls within a @c +/-32 window of @c eline->unk23F
 *
 * writes @c trigger7 from the @c D_800704A8.unk150 / @c unk154 pair:
 *   - bit 6 set in @c unk150 and clear in @c unk154 → @c trigger7 = @c unk19D (= 1)
 *   - bit 7 set in @c unk150 and clear in @c unk154 → @c trigger7 = 2
 *
 * The for-loop's @c i++, pool++ in the increment clause (comma
 * operator) keeps gcc 2.7.2 from reordering the increments into the
 * count-reload @c lbu 's load-delay slot — target leaves that slot
 * as a @c nop.
 */
void func_8009A7E8(Eline *e, FieldEntityB *pool) {
    s32 i;
    for (i = 0; i < D_800852F8; i++, pool++) {
        if (pool->activeMarker == 1) {
            if (e->msgActive == 0) {
                if (pool->unk19D == pool->activeMarker) {
                    if ((s32)(((s32)pool->unk19C - (s32)e->unk23F + 0x20) & 0xFF) < 0x40) {
                        if (D_800704A8.unk150 & 0x40) {
                            if (!(D_800704A8.unk154 & 0x40)) {
                                pool->trigger7 = pool->unk19D;
                            }
                        }
                        if (D_800704A8.unk150 & 0x80) {
                            if (!(D_800704A8.unk154 & 0x80)) {
                                pool->trigger7 = 2;
                            }
                        }
                    }
                }
            }
        }
    }
}

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

/**
 * @brief Per-frame proximity check — for each @c FieldEntityB in
 *        @c D_800852F8 entries, run @c func_8009A2BC against the eline's
 *        position and set per-entity trigger bytes based on the hit
 *        distance vs the eline's collision radius.
 *
 * Writes the eline's @c (posX, posY, posZ) @c >> @c 12 to the PSX
 * scratchpad at @c getScratchAddr(2..4), then iterates each
 * @c FieldEntityB and (when active and the eline isn't in a message)
 * calls @c func_8009A2BC. The returned distance is compared against
 * @c radius*radius:
 *  - hit (in range): @c trigger4=1; also @c trigger2=1 when
 *    @c D_8005F14C @c == @c 3
 *  - miss (out of range or @c -1): @c trigger3=1, @c trigger4=0
 *
 * @param eline    Querying entity (position, radius, message state).
 * @param entities @c FieldEntityB pool; walked directly as the loop cursor.
 *
 * @note @c fc is a vestigial second cursor: initialized and stepped in
 *       lockstep but never read. Removing it (or reading through it)
 *       changes the register allocation away from the original — the
 *       original source evidently carried it, so it stays.
 */
void func_8009A920(Eline *eline, FieldEntityB *entities) {
    Vec3i *scratch = (Vec3i *)getScratchAddr(0);
    Vec3i *out = (Vec3i *)getScratchAddr(4);
    FieldEntityB *fc;
    s32 i;
    s32 dist;

    scratch->x = eline->posX >> 12;
    i = 0;
    scratch->y = eline->posY >> 12;
    scratch->z = eline->posZ >> 12;
    if (i < D_800852F8) {
        fc = entities;
        do {
            if (entities->activeMarker == 1 && eline->msgActive == 0) {
                dist = func_8009A2BC((LineSeg *)&entities->x0, scratch, out);
                if (dist != -1 && dist < eline->radius * eline->radius) {
                    if (D_8005F14C == 3) {
                        entities->trigger2 = 1;
                    }
                    entities->trigger4 = 1;
                } else {
                    entities->trigger3 = 1;
                    entities->trigger4 = 0;
                }
            }
            i++;
            fc++;
            entities++;
        } while (i < D_800852F8);
    }
}

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
    D_800704A8.spawnTriIdx = e->spawnTriIdx;
    D_800704A8.anim_state = e->anim_state;
}

/**
 * @brief Scan the 12-entry event queue for trigger segments the query point
 *        crosses, and fire the event restore for each hit.
 *
 * Stages the eline's position (@c >>12) into the scratchpad at
 * @c getScratchAddr(0) and the query point (X/Y from @p pt, Z from the
 * eline) at @c getScratchAddr(4), then for each armed @ref EventEntry
 * (@c counter != 0x7FFF, @c rotation != 0xFFFF): projects the query point
 * onto the entry's trigger segment via @ref func_8009A2BC, and when the
 * squared distance is inside @c eline->radius² and the eline and the query
 * point lie on opposite sides of the segment (2D cross-product signs
 * differ), restores the entry's event snapshot via @ref func_8009AA64.
 *
 * @param eline Querying entity.
 * @param segs  12-entry @ref EventEntry queue (32-byte stride).
 * @param pt    Query point (world fixed-point; only X/Y read).
 *
 * @note @c B and @c C are derived from @c A with @c |0x10 / @c |0x20 so the
 *       compiler shares one scratchpad base register (addu+ori), as in the
 *       original.
 */
void func_8009AAC8(Eline *eline, EventEntry *segs, Vec3i *pt) {
    Vec3i *A = (Vec3i *)getScratchAddr(0);
    Vec3i *B;
    Vec3i *C;
    s32 i;
    s32 dist;
    s32 crossSelf;
    s32 crossPt;

    A->x = eline->posX >> 12;
    B = (Vec3i *)((u32)A | 0x10);
    A->y = eline->posY >> 12;
    C = (Vec3i *)((u32)A | 0x20);
    A->z = eline->posZ >> 12;
    B->x = pt->x >> 12;
    B->y = pt->y >> 12;
    B->z = eline->posZ >> 12;
    for (i = 0; i < 12; i++, segs++) {
        if (segs->counter == 0x7FFF) {
            continue;
        }
        if (segs->spawnTriIdx == 0xFFFF) {
            continue;
        }
        dist = func_8009A2BC((LineSeg *)&segs->x0, B, C);
        if (dist == -1) {
            continue;
        }
        if (dist < eline->radius * eline->radius) {
            crossSelf = (segs->x1 - segs->x0) * (A->y - segs->y0)
                      - (A->x - segs->x0) * (segs->y1 - segs->y0);
            crossPt = (segs->x1 - segs->x0) * (B->y - segs->y0)
                    - (B->x - segs->x0) * (segs->y1 - segs->y0);
            if ((crossSelf >= 0 && crossPt < 0) || (crossPt >= 0 && crossSelf < 0)
                || (crossSelf > 0 && crossPt <= 0) || (crossPt > 0 && crossSelf <= 0)) {
                func_8009AA64(segs);
            }
        }
    }
}

/** @brief 16-byte padded s32 vector — stack twin of the PsyQ VECTOR layout,
 *  private to @c func_8009AC9C (the pad keeps the frame at 0x10 strides). */
typedef struct {
    s32 x, y, z, pad;
} func_8009AC9C_vec;

/**
 * @brief Brute-force walkmesh triangle lookup: find the triangle whose 2D
 *        extent contains (px, py) and whose plane Z is nearest pz.
 *
 * For each triangle in @p list, builds the three edge vectors with
 * @c func_8009DED8 and tests the query point against all three 2D edge
 * cross-products (inside when all are >= 0, computed up front and then
 * &&-chained). For containing triangles the plane Z at the query point is
 * interpolated via @c func_8009E338 and the triangle with the smallest
 * |Z - pz| wins.
 *
 * @param px    Query X (walkmesh units).
 * @param py    Query Y.
 * @param pz    Query Z (used only to rank containing triangles).
 * @param list  Counted triangle list.
 * @return Index of the best containing triangle, as @c s16.
 *
 * @note If no triangle contains the point (or the list is empty) the return
 *       value is an uninitialized stack halfword — the original never
 *       seeds @c bestIdx. Callers are expected to only use the result when
 *       the point is known to be on the mesh.
 * @note Twin loop cursors (@c s16 @c i for the record offset and result,
 *       @c s32 @c n for the count compare), the byte-offset vertex pointers,
 *       and the @c i++, @c n++ increment order are all register-allocation /
 *       scheduling keys.
 */
s16 func_8009AC9C(s16 px, s16 py, s16 pz, TriangleList *list) {
    func_8009AC9C_vec e0;
    func_8009AC9C_vec e1;
    func_8009AC9C_vec e2;
    func_8009AC9C_vec qp;
    u16 bestIdx;
    s32 best;
    s16 i;
    s32 n;
    s32 c0, c1, c2, z;
    SVert *vA;
    SVert *vB;
    SVert *vC;
    u8 *tris;
    s32 ofs;

    i = 0;
    best = 0x7FFFFFFF;
    qp.x = px;
    qp.y = py;
    qp.z = pz;
    tris = (u8 *)list->tris;
    for (n = 0; n < list->count; i++, n++) {
        ofs = (s16)i * 24;
        vB = (SVert *)(tris + (ofs + 8));
        vA = (SVert *)(tris + ofs);
        func_8009DED8((Vec3i *)&e0, vB, vA);
        ofs += 16;
        vC = (SVert *)(tris + ofs);
        func_8009DED8((Vec3i *)&e1, vC, vB);
        func_8009DED8((Vec3i *)&e2, vA, vC);
        c0 = e0.y * (px - vA->sx) - e0.x * (py - vA->sy);
        c1 = e1.y * (px - vA[1].sx) - e1.x * (py - vA[1].sy);
        c2 = e2.y * (px - vA[2].sx) - e2.x * (py - vA[2].sy);
        if (c0 >= 0 && c1 >= 0 && c2 >= 0) {
            z = func_8009E338((Vec3i *)&e0, (Vec3i *)&e1, (Vec3i *)&qp, (Vec3s *)vA);
            if (qp.z < z) {
                z = z - qp.z;
            } else {
                z = qp.z - z;
            }
            if (z < best) {
                best = z;
                bestIdx = i;
            }
        }
    }
    return (s16)bestIdx;
}

/** @brief Scale applied to @c D_800704A8.unk00A when seeding @c Eline::savedChannel. */
#define FIELD_CHANNEL_SCALE 0x4367

/** @brief Sentinel in @c D_800704A8.spawnTriIdx / @c position_x meaning "no override". */
#define SPAWN_UNSET 0x7FFF

/**
 * @brief Place every field entity on the navmesh when a field is entered.
 *
 * For each of the @c D_80085388 entities:
 *  - The player entity (@c D_8005F148, taken from @c D_800704A8.entityIndex[0])
 *    is handled specially. When @c D_800704A8.spawnTriIdx carries a triangle index
 *    (i.e. is not @c SPAWN_UNSET) the entity is moved onto that triangle:
 *    with no X override it is dropped at the triangle centroid, otherwise its
 *    existing X/Y are kept and only Z is re-derived from the triangle plane.
 *    When no triangle is supplied the entity is reset onto triangle 0 with a
 *    default radius and animation set.
 *  - Every other entity keeps its X/Y and has its Z re-derived from the
 *    triangle it stands on.
 *
 * Z comes from @c func_8009E338, which intersects the entity's X/Y against the
 * plane spanned by two triangle edges built by @c func_8009DED8; the result is
 * shifted back into the 12-bit fixed-point the position fields use.
 *
 * A second pass clears every entity's @c headingBase before the movement
 * (@c func_8009E660) and path (@c func_8009BB18) tables are rebuilt.
 *
 * @note The navmesh is indexed as a flat vertex array — triangle @c t owns
 *       @c D_800C71F0[t*3 .. t*3+2] — which is also how @c func_8009DF18 reads
 *       it. A @c Triangle[] view does not reproduce the original's addressing:
 *       gcc then shares the derived triangle pointer between the two corner
 *       arguments and computes the second as @c ptr+8, where the original
 *       scales @c t*3 once and adds the vertex base to each corner.
 * @note @c pos.z is left uninitialised on the player path — only the
 *       non-player path zeroes it. @c func_8009E338 reads just X and Y, so this
 *       is harmless, and the original has the same asymmetry.
 */
void func_8009AEC0(void) {
    Vec3i edge0;
    Vec3i edge1;
    Vec3i pos;
    s16 i;

    D_8005F102 = 0;
    D_800704A8.unk00A = 20;
    D_8005F148 = D_800704A8.entityIndex[0];

    for (i = 0; i < D_80085388; i++) {
        if (i == D_8005F148) {
            if (D_800704A8.spawnTriIdx != SPAWN_UNSET) {
                D_80085224[i].triIdx = D_800704A8.spawnTriIdx;
                if (D_800704A8.position_x == SPAWN_UNSET) {
                    D_80085224[i].posX = ((D_800C71F0[D_80085224[i].triIdx * 3].sx +
                                           D_800C71F0[D_80085224[i].triIdx * 3 + 1].sx +
                                           D_800C71F0[D_80085224[i].triIdx * 3 + 2].sx) / 3) << 12;
                    D_80085224[i].posY = ((D_800C71F0[D_80085224[i].triIdx * 3].sy +
                                           D_800C71F0[D_80085224[i].triIdx * 3 + 1].sy +
                                           D_800C71F0[D_80085224[i].triIdx * 3 + 2].sy) / 3) << 12;
                    D_80085224[i].posZ = ((D_800C71F0[D_80085224[i].triIdx * 3].sz +
                                           D_800C71F0[D_80085224[i].triIdx * 3 + 1].sz +
                                           D_800C71F0[D_80085224[i].triIdx * 3 + 2].sz) / 3) << 12;
                } else {
                    func_8009DED8(&edge0, &D_800C71F0[D_80085224[i].triIdx * 3 + 1],
                                  &D_800C71F0[D_80085224[i].triIdx * 3]);
                    func_8009DED8(&edge1,
                                  &D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 2],
                                  &D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 1]);
                    pos.x = D_80085224[D_8005F148].posX / 4096;
                    pos.y = D_80085224[D_8005F148].posY / 4096;
                    D_80085224[D_8005F148].posZ =
                        func_8009E338(&edge0, &edge1, &pos,
                                      &D_800C71F0[D_80085224[D_8005F148].triIdx * 3]) << 12;
                }
            } else {
                D_80085224[i].field_0x208 = 0x10;
                D_80085224[i].field_0x24F = 0;
                D_80085224[D_8005F148].field_0x250 = 1;
                D_80085224[D_8005F148].field_0x251 = 2;
                /* Same scale func_800B6738 applies to D_800704B2 (there as *69020>>9), so
                   the entity starts exactly at the threshold that picks field_0x251. */
                D_80085224[D_8005F148].savedChannel = ((u32)(D_800704A8.unk00A * 17255)) >> 7;
                D_80085224[D_8005F148].radius = 0x30;
                D_80085224[D_8005F148].triIdx = 0;
                D_80085224[D_8005F148].posX =
                    ((D_800C71F0[D_80085224[D_8005F148].triIdx * 3].sx +
                      D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 1].sx +
                      D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 2].sx) / 3) << 12;
                D_80085224[D_8005F148].posY =
                    ((D_800C71F0[D_80085224[D_8005F148].triIdx * 3].sy +
                      D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 1].sy +
                      D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 2].sy) / 3) << 12;
                D_80085224[D_8005F148].posZ =
                    ((D_800C71F0[D_80085224[D_8005F148].triIdx * 3].sz +
                      D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 1].sz +
                      D_800C71F0[D_80085224[D_8005F148].triIdx * 3 + 2].sz) / 3) << 12;
            }
        } else {
            pos.x = D_80085224[i].posX / 4096;
            pos.y = D_80085224[i].posY / 4096;
            pos.z = 0;
            func_8009DED8(&edge0, &D_800C71F0[D_80085224[i].triIdx * 3 + 1],
                          &D_800C71F0[D_80085224[i].triIdx * 3]);
            func_8009DED8(&edge1, &D_800C71F0[D_80085224[i].triIdx * 3 + 2],
                          &D_800C71F0[D_80085224[i].triIdx * 3 + 1]);
            D_80085224[i].posZ = func_8009E338(&edge0, &edge1, &pos,
                                               &D_800C71F0[D_80085224[i].triIdx * 3]) << 12;
        }
    }

    for (i = 0; i < D_80085388; i++) {
        D_80085224[i].headingBase = 0;
    }

    func_8009E660();
    func_8009BB18();
}

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
void func_8009B74C(s16 slotIdx, u16 paramIdx, PathEntry *params, s16 multiplier) {
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
        D_80085224[D_800704A8.entityIndex[2]].triIdx = D_80070A60[angle].unk6;
        D_80085224[D_800704A8.entityIndex[2]].unk258 = D_80070A60[angle].unk8;
    }
    if (D_800704A8.entityIndex[1] != 0xFF) {
        angle = (D_8005F144 - D_8005F118) & 0x3F;
        D_80085224[D_800704A8.entityIndex[1]].posX   = D_80070760[angle].x << 12;
        D_80085224[D_800704A8.entityIndex[1]].posY   = D_80070760[angle].y << 12;
        D_80085224[D_800704A8.entityIndex[1]].posZ   = D_80070760[angle].z << 12;
        D_80085224[D_800704A8.entityIndex[1]].triIdx = D_80070760[angle].unk6;
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
    u = e->triIdx;
    p0->unk6 = u;
    p1->unk6 = u;
    p0->field_09 = b9;
    p1->field_09 = b9;
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

/**
 * @brief Aim the self entity at whichever entity is most directly "in front" of
 *        it, and stamp that target's delayed-SFX trigger 7 with the pad mode.
 *
 * The self entity is @c D_80085224[D_8005F148]; its position is taken in whole
 * units (fixed-point @c >>12). A @c mode (0/1/2) is derived from the current and
 * previous pad-held bits in @c D_800704A8 (@c unk150 / @c unk154, bits @c 0x80
 * and @c 0x40). If the field is busy (@c D_800704BD != 0) or @c mode is 0, the
 * scan is skipped entirely.
 *
 * Otherwise every entity @c i is scored into @c angleTable: candidates are
 * skipped when they are the self entity, flagged busy (@c field_0x24B), inactive
 * (@c unk218 == -1), share the self's X/Y cell, lie outside a ±0xFF Z band, or
 * fall outside the combined talk/collision radius. For the rest, the score is how
 * far the entity's bearing (via @ref func_8009A0E8) deviates from the self's
 * facing angle (@c field_0x241), folded to 0..0x80. The entity with the smallest
 * deviation below the 0x40 threshold wins and has its @c triggerSfx7 set to
 * @c mode; a tie or no winner leaves everything untouched.
 */
void func_8009CEE8(void) {
    s32 selfPos[3];
    s32 entPos[3];
    s16 angleTable[48];
    s32 dist;
    SystemState *sys;
    s32 i;
    s32 mode;
    s32 diff;
    s16 best;
    s16 bestIdx;

    selfPos[0] = D_80085224[D_8005F148].posX >> 12;
    selfPos[1] = D_80085224[D_8005F148].posY >> 12;
    selfPos[2] = D_80085224[D_8005F148].posZ >> 12;

    sys = &D_800704A8;
    mode = 0;
    if (sys->unk150 & 0x80) {
        mode = ((sys->unk154 & 0x80) == 0) << 1;
    }
    if ((sys->unk150 & 0x40) && !(sys->unk154 & 0x40)) {
        mode = 1;
    }

    if (D_800704BD != 0) return;
    if (mode == 0) return;

    for (i = 0; i < D_80085388; i++) {
        angleTable[i] = 0x100;
        if (i == D_8005F148) continue;
        if (D_80085224[i].field_0x24B != 0) continue;
        if (D_80085224[i].unk218 == -1) continue;
        entPos[0] = D_80085224[i].posX >> 12;
        entPos[1] = D_80085224[i].posY >> 12;
        entPos[2] = D_80085224[i].posZ >> 12;
        if (selfPos[0] == entPos[0] && selfPos[1] == entPos[1]) continue;
        if ((u32)((selfPos[2] - entPos[2]) + 0xFF) >= 0x1FF) continue;
        diff = (D_80085224[D_8005F148].field_0x241 - (func_8009A0E8(selfPos, entPos, &dist) & 0xFF)) & 0xFF;
        angleTable[i] = diff;
        if (diff >= 0x81) {
            angleTable[i] = 0x100 - diff;
        }
        if (dist >= D_80085224[i].talkRadius + D_80085224[D_8005F148].radius) {
            angleTable[i] = 0x100;
        }
    }

    best = 0x40;
    bestIdx = D_8005F148;
    for (i = 0; i < D_80085388; i++) {
        if (D_80085224[i].unk218 == -1) continue;
        if (angleTable[i] < (s16)best) {
            best = (u16)angleTable[i];
            bestIdx = i;
        }
    }

    if (bestIdx != D_8005F148 && best != 0x40) {
        D_80085224[bestIdx].triggerSfx7 = mode;
    }
}

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

/**
 * @brief Walk-toward-destination step: arrival test plus one turn increment.
 *
 * Snapshots the entity position and its destination (@c msgTextPtr /
 * @c msgPosX, which double as the walk target) in whole units and takes the
 * squared XY distance between them.
 *
 * When @p pad is non-zero the entity is treated as still travelling unless the
 * remaining squared distance exceeds @c (radius + pad)^2 + 0x1000, in which
 * case 0 is returned. Arrival — squared distance below @c savedChannel^2 >> 16
 * or below 4 — snaps the position onto the destination and returns 0.
 *
 * Otherwise the bearing to the destination is taken (@c func_8009A0E8, which
 * also rewrites @c dist with the true distance) and reduced by
 * @c headingBase to give the target heading. The turn rate is
 * @c field_0x262, or 0 when the entity is already inside its radius or
 * @c field_0x1DA has left the +/-0x100 band. With no rate the heading snaps;
 * otherwise the heading restarts from @c field_0x241 and is stepped by at most
 * @c rate along the shorter way around the 256-unit circle (the four cases
 * offset the compared angles by 0x100 to keep the window continuous across
 * the wrap), snapping when the target already lies inside the step window.
 *
 * @param self Entity to advance.
 * @param pad  Extra arrival slack added to the radius; 0 disables the test.
 * @return 1 while still travelling, 0 once arrived (or stopped short).
 */
s32 func_8009D274(Eline *self, s16 pad) {
    VECTOR cur;
    VECTOR dst;
    s32 dist;
    s32 dx;
    s32 dy;
    s32 r;
    s32 rr;
    s32 lim;
    s32 delta;
    u16 rate;
    u16 c;

    cur.vx = self->posX >> 12;
    cur.vy = self->posY >> 12;
    dst.vx = self->msgTextPtr >> 12;
    dx = dst.vx - cur.vx;
    dst.vy = self->msgPosX >> 12;
    dy = dst.vy - cur.vy;
    r = self->radius + pad;
    rr = r * r;
    dist = dx * dx + dy * dy;
    lim = rr + 0x1000;
    if (pad != 0) {
        if (lim >= dist) {
            return 0;
        }
    }
    if (dist < (self->savedChannel * self->savedChannel) >> 16 || dist < 4) {
        self->posX = self->msgTextPtr;
        self->posY = self->msgPosX;
        return 0;
    }

    delta = (func_8009A0E8(&cur.vx, &dst.vx, &dist) & 0xFF) - self->headingBase;
    if (dist < self->radius || self->field_0x1DA > 0x100 || self->field_0x1DA < -0x100) {
        rate = 0;
    } else {
        rate = self->field_0x262;
    }
    if (rate == 0) {
        self->unk23F = delta;
    } else {
        self->unk23F = self->field_0x241;
        c = self->unk23F;
        if (c == (u8)delta) {
            self->unk23F = delta;
        } else if ((u16)delta < c) {
            if (c - (u16)delta >= 0x81) {
                delta += 0x100;
                if ((u16)delta < c + rate && c - rate < (u16)delta) {
                    self->unk23F = delta;
                } else {
                    self->unk23F += rate;
                    self->field_0x1DA += rate;
                }
            } else {
                c += 0x100;
                delta += 0x100;
                if ((u16)delta < c + rate && c - rate < (u16)delta) {
                    self->unk23F = delta;
                } else {
                    self->unk23F -= rate;
                    self->field_0x1DA -= rate;
                }
            }
        } else if ((u16)delta - c >= 0x81) {
            c += 0x100;
            if ((u16)delta < c + rate && c - rate < (u16)delta) {
                self->unk23F = delta;
            } else {
                self->unk23F -= rate;
                self->field_0x1DA -= rate;
            }
        } else {
            c += 0x100;
            delta += 0x100;
            if ((u16)delta < c + rate && c - rate < (u16)delta) {
                self->unk23F = delta;
            } else {
                self->unk23F += rate;
                self->field_0x1DA += rate;
            }
        }
    }
    return 1;
}

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
 * @param out Destination edge vector (widened to words).
 * @param a   Source vertex A.
 * @param b   Source vertex B.
 */
void func_8009DED8(Vec3i *out, SVert *a, SVert *b) {
    out->x = a->sx - b->sx;
    out->y = a->sy - b->sy;
    out->z = a->sz - b->sz;
}

/**
 * @brief GTE outer-product (NCLIP) of the query point against one navmesh edge.
 *
 * Two separate asm statements on purpose: the compiler materializes the
 * result address (@c addiu @c vN,sp,K) between the @c nclip and the @c swc2,
 * and the @c "memory" clobber on the first forces the original's per-edge
 * reloads of the query point and mesh pointer. @c nclip has no GAS mnemonic,
 * hence the @c .word encoding (cop2 0x1400006).
 */
#define gte_nclip(out, sxy0, sxy1, sxy2) \
    __asm__ volatile("mtc2 %0, $12\nmtc2 %2, $14\nmtc2 %1, $13\nnop\nnop\n.word 0x4B400006\n" \
        : : "r"(sxy0), "r"(sxy1), "r"(sxy2) : "memory"); \
    __asm__ volatile("swc2 $24, 0(%0)\n" : : "r"(&(out)))

/**
 * @brief Walk the field navmesh from the current triangle toward a stepped
 *        position, following edge adjacency; classify any blocking edge.
 *
 * Rounds @p out 's fixed-point X/Y up toward zero (@c +0xFFF then @c >>12)
 * into a word vector and a packed SXY vertex, then loops: builds the three
 * edge vectors of the current triangle (@ref func_8009DED8), runs GTE NCLIP
 * of the query point against each directed edge, and
 *  - if the point is inside all three edges (all NCLIPs >= 0), stops and
 *    writes @c out->z from the triangle-plane intersection
 *    (@ref func_8009E338), returning 0 or the last edge classification;
 *  - otherwise, for the first failing edge: moves to that edge's neighbor
 *    triangle (@c *pTriIdx updated) when one exists and its
 *    @c D_800704A8.statusBits lock bit is clear, else returns +/-8 by the
 *    sign of the edge direction dotted with the movement delta @p dxy
 *    (which side of the blocking edge the motion crosses).
 *
 * @param pTriIdx In/out current triangle index into @c D_800C71F0.
 * @param out     Stepped position (fixed-point); @c z receives the plane height.
 * @param dxy     Movement delta (dx at [0], dy at [1]).
 * @param aux     Unused.
 * @return 0 when the point settled in a triangle, else +/-8 blocking-edge code.
 */
s32 func_8009DF18(u16 *pTriIdx, Vec3i *out, s32 *dxy, s32 *aux) {
    Vec3i e0;
    Vec3i e1;
    Vec3i e2;
    Vec3i posW;
    SVert posV;
    s32 nc0;
    s32 nc1;
    s32 nc2;
    s32 ret = 0;
    s32 t;
    s32 t2;
    s32 t3;
    s32 t4;
    s16 nb;
    s32 idx3;

    t = out->x;
    if (t < 0) {
        t += 0xFFF;
    }
    t >>= 12;
    posW.x = t;
    t2 = out->y;
    if (t2 < 0) {
        t2 += 0xFFF;
    }
    t2 >>= 12;
    posW.y = t2;
    posW.z = 0;
    t3 = out->x;
    if (t3 < 0) {
        t3 += 0xFFF;
    }
    t3 >>= 12;
    posV.sx = t3;
    t4 = out->y;
    if (t4 < 0) {
        t4 += 0xFFF;
    }
    posV.sy = t4 >> 12;
    posV.sz = 0;

    while (1) {
        func_8009DED8(&e0, &D_800C71F0[*pTriIdx * 3 + 1], &D_800C71F0[*pTriIdx * 3]);
        func_8009DED8(&e1, &D_800C71F0[*pTriIdx * 3 + 2], &D_800C71F0[*pTriIdx * 3 + 1]);
        func_8009DED8(&e2, &D_800C71F0[*pTriIdx * 3], &D_800C71F0[*pTriIdx * 3 + 2]);
        idx3 = *pTriIdx * 3;
        gte_nclip(nc0, *(u32 *)&posV, *(u32 *)&D_800C71F0[idx3 + 1], *(u32 *)&D_800C71F0[idx3]);
        idx3 = *pTriIdx * 3;
        gte_nclip(nc1, *(u32 *)&posV, *(u32 *)&D_800C71F0[idx3 + 2], *(u32 *)&D_800C71F0[idx3 + 1]);
        idx3 = *pTriIdx * 3;
        gte_nclip(nc2, *(u32 *)&posV, *(u32 *)&D_800C71F0[idx3], *(u32 *)&D_800C71F0[idx3 + 2]);
        if (nc0 >= 0 && nc1 >= 0 && nc2 >= 0) {
            break;
        }
        if (nc0 < 0) {
            nb = D_800D5E98[*pTriIdx].neighbor[0];
            if (nb >= 0 && !((D_800704A8.statusBits[nb >> 3] >> (nb - ((nb >> 3) << 3))) & 1)) {
                *pTriIdx = D_800D5E98[*pTriIdx].neighbor[0];
                continue;
            }
            ret = (e0.x * dxy[0] + e0.y * dxy[1] >= 0) ? 8 : -8;
            break;
        } else if (nc1 < 0) {
            nb = D_800D5E98[*pTriIdx].neighbor[1];
            if (nb >= 0 && !((D_800704A8.statusBits[nb >> 3] >> (nb - ((nb >> 3) << 3))) & 1)) {
                *pTriIdx = D_800D5E98[*pTriIdx].neighbor[1];
                continue;
            }
            ret = (e1.x * dxy[0] + e1.y * dxy[1] >= 0) ? 8 : -8;
            break;
        } else if (nc2 < 0) {
            nb = D_800D5E98[*pTriIdx].neighbor[2];
            if (nb >= 0 && !((D_800704A8.statusBits[nb >> 3] >> (nb - ((nb >> 3) << 3))) & 1)) {
                *pTriIdx = D_800D5E98[*pTriIdx].neighbor[2];
                continue;
            }
            ret = (e2.x * dxy[0] + e2.y * dxy[1] >= 0) ? 8 : -8;
            break;
        }
    }

    out->z = func_8009E338(&e0, &e1, &posW, (Vec3s *)&D_800C71F0[*pTriIdx * 3]);
    return ret;
}

/**
 * @brief Plane-cross intersection — compute @c (cross_xyz @c · (a3 @c -
 *        @c a2_partial)) @c / @c cross_z, where @c cross @c = @c a1 @c
 *        × @c a0.
 *
 * Builds the cross product @c a1 @c × @c a0 (stored to a stack array
 * @c sp[3]), then overwrites @c a0 with @c a3's sign-extended values.
 * The return value is the scalar projection of @c (a3 - a2_partial)
 * along the cross-product axis divided by @c cross_z. Note: @c a2's
 * @c z component is intentionally not subtracted.
 *
 * @param a0 Direction vector A (s32 x,y,z) — overwritten with @c a3.
 * @param a1 Direction vector B (s32 x,y,z).
 * @param a2 Reference point (s32 x,y,z) — only @c .x and @c .y used.
 * @param a3 Target point (s16 x,y,z).
 * @return The intersection parameter @c (s32).
 *
 * The trailing block caches @c sp[0..2] into local @c s32 vars (t0,
 * t1, t2) so gcc 2.7.2 loads each only once — without the cache, gcc
 * reloads them from the stack for each of the 5 uses.
 */
s32 func_8009E338(Vec3i *a0, Vec3i *a1, Vec3i *a2, Vec3s *a3) {
    s32 sp[3];

    sp[0] = -a0->y * a1->z + a1->y * a0->z;
    sp[1] = -a0->z * a1->x + a0->x * a1->z;
    sp[2] = -a0->x * a1->y + a1->x * a0->y;

    a0->x = a3->x;
    a0->y = a3->y;
    a0->z = a3->z;

    {
        s32 t0 = sp[0];
        s32 t1 = sp[1];
        s32 t2 = sp[2];
        return (t0 * a0->x + t1 * a0->y + t2 * a0->z
                - t0 * a2->x - t1 * a2->y) / t2;
    }
}

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

/**
 * @brief Reset both follower path tables to the player's position on field entry.
 *
 * @c D_80070A60 and @c D_80070760 are the 64-entry breadcrumb rings the two
 * party followers walk along: @c D_8009B74C writes the player's current
 * waypoint at cursor @c D_8005F144 each step, and each follower samples the
 * ring at @c (cursor @c - @c lag) @c & @c 0x3F. Entering a field leaves the ring
 * with no history, so it is refilled here. Every waypoint is marked walkable
 * (@c field_09 / @c unk8 @c = @c 1) and the two follower lag distances are reset
 * to their defaults, current (@c D_8005F118 / @c D_8005F11A) and target
 * (@c D_8005F160 / @c D_8005F162) alike.
 *
 * On a fresh load (@c D_8005F14C @c == @c 0) there is no heading to trail along,
 * so every waypoint just gets the player's position, triangle and facing — the
 * followers stand on top of the player until real movement fills the ring.
 *
 * Otherwise the trail is synthesised backwards from the player. Slot 0 holds the
 * exact position and slots 63..1 each step one trail spacing further back along
 * the player's facing, the step being @c sin / @c cos (@ref func_8009D234 /
 * @ref func_8009D254) scaled by @c SystemState::unk00A. Each stepped point is
 * snapped onto the navmesh by @ref func_8009AC9C and its Z read off that
 * triangle's plane (@ref func_8009DED8 twice, then @ref func_8009E338); if the
 * plane Z lands further than @c 0x136 from the stepped Z the point is off the
 * walkable surface and the slot falls back to the player's own X/Y.
 *
 * @note Slot 0 only receives X and Y here — its Z, triangle and facing keep
 *       whatever the ring already held.
 */
void func_8009E660(void) {
    Vec3i edge0;
    Vec3i edge1;
    Vec3i pos;
    s32 x;
    s32 y;
    s32 z;
    s16 px;
    s16 py;
    s32 pz;
    s32 planeZ;
    s16 i;

    D_8005F144 = 0;
    D_8005F118 = 15;
    D_8005F11A = 30;
    D_8005F160 = 15;
    D_8005F162 = 30;

    for (i = 0; i < 64; i++) {
        D_80070760[i].field_09 = D_80070A60[i].field_09 = 1;
        D_80070760[i].unk8 = D_80070A60[i].unk8 = 1;
    }

    if (D_8005F14C == 0) {
        for (i = 0; i < 64; i++) {
            D_80070760[i].x = D_80070A60[i].x = D_80085224[D_8005F148].posX / 4096;
            D_80070760[i].y = D_80070A60[i].y = D_80085224[D_8005F148].posY / 4096;
            D_80070760[i].z = D_80070A60[i].z = D_80085224[D_8005F148].posZ / 4096;
            D_80070760[i].unk6 = D_80070A60[i].unk6 = D_80085224[D_8005F148].triIdx;
            D_80070760[i].field_0A = D_80070A60[i].field_0A = 0;
            D_80070760[i].field_0B = D_80070A60[i].field_0B = D_80085224[D_8005F148].field_0x241;
        }
    } else {
        x = D_80085224[D_8005F148].posX;
        y = D_80085224[D_8005F148].posY;
        z = D_80085224[D_8005F148].posZ;
        D_80070760[0].x = D_80070A60[0].x = D_80085224[D_8005F148].posX / 4096;
        D_80070760[0].y = D_80070A60[0].y = D_80085224[D_8005F148].posY / 4096;

        for (i = 0; i < 63; i++) {
            pos.x = px = D_80070760[63 - i].x = D_80070A60[63 - i].x = x / 4096;
            pos.y = py = D_80070760[63 - i].y = D_80070A60[63 - i].y = y / 4096;
            pz = z / 4096;
            D_80070760[63 - i].unk6 = D_80070A60[63 - i].unk6 =
                func_8009AC9C(px, py, pz, *D_800C7204);
            func_8009DED8(&edge0, &D_800C71F0[D_80070760[63 - i].unk6 * 3 + 1],
                          &D_800C71F0[D_80070760[63 - i].unk6 * 3]);
            func_8009DED8(&edge1, &D_800C71F0[D_80070760[63 - i].unk6 * 3 + 2],
                          &D_800C71F0[D_80070760[63 - i].unk6 * 3 + 1]);
            planeZ = func_8009E338(&edge0, &edge1, &pos,
                                   (Vec3s *)&D_800C71F0[D_80070760[63 - i].unk6 * 3]);
            if (planeZ < pz + 0x136 && pz - 0x136 < planeZ) {
                D_80070760[63 - i].z = D_80070A60[63 - i].z = planeZ;
            } else {
                D_80070760[63 - i].x = D_80070A60[63 - i].x =
                    D_80085224[D_8005F148].posX / 4096;
                D_80070760[63 - i].y = D_80070A60[63 - i].y =
                    D_80085224[D_8005F148].posY / 4096;
                D_80070760[63 - i].z = D_80070A60[63 - i].z = z / 4096;
            }
            D_80070760[63 - i].field_0A = D_80070A60[63 - i].field_0A = 0;
            D_80070760[63 - i].field_0B = D_80070A60[63 - i].field_0B =
                D_80085224[D_8005F148].field_0x241;
            /* FIELD_CHANNEL_SCALE * 4 >> 9 is the same trail spacing func_800B6738
               applies to D_800704B2; written *4 >> 9 because that is the shift pair
               the original emits. */
            x -= func_8009D234(D_80085224[D_8005F148].field_0x241) *
                 ((D_800704A8.unk00A * (FIELD_CHANNEL_SCALE * 4)) >> 9) / 256;
            y -= -(func_8009D254(D_80085224[D_8005F148].field_0x241) *
                   ((D_800704A8.unk00A * (FIELD_CHANNEL_SCALE * 4)) >> 9)) / 256;
        }
    }
}

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
 * @note Uses unsubscripted @c D_80085224 to defeat gcc 2.7.2's CSE
 *       of the global pointer load (target reloads after each call).
 */
void func_8009F8D0(s16 idx) {
    D_80085224[idx].posX = func_800A0E54(D_80085224[idx].unk1A8,
                                         D_80085224[idx].field_0x1C0,
                                         (s16)D_80085224[idx].field_0x1D8,
                                         (s16)D_80085224[idx].field_0x1DA);
    D_80085224[idx].posY = func_800A0E54(D_80085224[idx].unk1AC,
                                         D_80085224[idx].field_0x1C4,
                                         (s16)D_80085224[idx].field_0x1D8,
                                         (s16)D_80085224[idx].field_0x1DA);
    D_80085224[idx].posZ = func_800A0E54(D_80085224[idx].unk1B0,
                                         D_80085224[idx].field_0x1C8,
                                         (s16)D_80085224[idx].field_0x1D8,
                                         (s16)D_80085224[idx].field_0x1DA);
}

/**
 * @brief Apply one frame of directional input to the player entity.
 *
 * Only the player (@c D_8005F148) moves, and only while @c D_800704BD is
 * clear; any other entity index — or a set @c D_800704BD — falls through to
 * the passive path at the end.
 *
 * The two direction bit-pairs are handled as a pair of opposite steps whose
 * meaning flips with the entity's message state: with @c windowId @c == @c 1
 * the @c 0xC000 bits step one way and @c 0x3000 the other, and without it the
 * assignment is reversed. Either way the step seeds both path tables at the
 * current write cursor @c D_8005F144 (@c field_0A @c = @c 4 marks the entry a
 * walk step, @c field_0B carries the entity's heading), runs the step through
 * @c func_8009F7F4 / @c func_8009F8D0, and replays both tables into the anim
 * system at their phase offsets (@c func_8009B74C). The two directions then
 * differ only in how they age @c field_0x1DA: one counts it down and reports
 * message state 5 at zero, the other counts it up and reports state 4 on
 * reaching @c field_0x1D8. With no direction bits the step is issued neutral.
 *
 * The passive path just ages @c field_0x1DA toward @c field_0x1D8 (reporting
 * state 2 once there) while advancing the entity's render-slot motion counter
 * @c unk52 by @c field_0x208, wrapping it at @c unk0C.
 *
 * @param idx   Entity index.
 * @param flags Input bits; @c 0x3000 and @c 0xC000 select the two directions.
 *
 * @note The two @c field_0A / @c field_0B stores are chained assignments: the
 *       plain two-statement form lets gcc merge the four otherwise identical
 *       table-seeding blocks, which collapses the shared step tails.
 */
void func_8009F990(s16 idx, s32 flags) {
    s32 p;
    u8 b;

    if (idx == D_8005F148 && D_800704BD == 0) {
        if (D_80085224[idx].windowId == 1) {
            if (flags & 0xC000) {
                p = D_8005F144;
                D_80070760[p].field_0A = D_80070A60[p].field_0A = 4;
                p = D_8005F144;
                D_80070760[p].field_0B = D_80070A60[p].field_0B = D_80085224[idx].field_0x241;
                func_8009F7F4(idx, -1, D_80085224[idx].field_0x253, 1);
                func_8009F8D0(idx);
                func_8009B74C(2, (D_8005F144 - D_8005F11A) & 0x3F, D_80070A60, 1);
                func_8009B74C(1, (D_8005F144 - D_8005F118) & 0x3F, D_80070760, 1);
                D_80085224[idx].field_0x1DA--;
                if (D_80085224[idx].field_0x1DA < 0) {
                    D_80085224[idx].field_0x1DA = 0;
                    D_80085224[idx].msgState = 5;
                }
            } else if (flags & 0x3000) {
                p = D_8005F144;
                D_80070760[p].field_0A = D_80070A60[p].field_0A = 4;
                p = D_8005F144;
                D_80070760[p].field_0B = D_80070A60[p].field_0B = D_80085224[idx].field_0x241;
                func_8009F7F4(idx, 1, D_80085224[idx].field_0x253, 1);
                func_8009F8D0(idx);
                func_8009B74C(2, (D_8005F144 - D_8005F11A) & 0x3F, D_80070A60, 1);
                func_8009B74C(1, (D_8005F144 - D_8005F118) & 0x3F, D_80070760, 1);
                D_80085224[idx].field_0x1DA++;
                if (D_80085224[idx].field_0x1DA == D_80085224[idx].field_0x1D8) {
                    D_80085224[idx].field_0x1DA = 0;
                    D_80085224[idx].msgState = 4;
                }
            } else {
                func_8009F7F4(idx, 0, D_80085224[idx].field_0x253, 0);
            }
        } else {
            if (flags & 0x3000) {
                p = D_8005F144;
                D_80070760[p].field_0A = D_80070A60[p].field_0A = 4;
                p = D_8005F144;
                D_80070760[p].field_0B = D_80070A60[p].field_0B = D_80085224[idx].field_0x241;
                func_8009F7F4(idx, 1, D_80085224[idx].field_0x253, 1);
                func_8009F8D0(idx);
                func_8009B74C(2, (D_8005F144 - D_8005F11A) & 0x3F, D_80070A60, 1);
                func_8009B74C(1, (D_8005F144 - D_8005F118) & 0x3F, D_80070760, 1);
                D_80085224[idx].field_0x1DA--;
                if (D_80085224[idx].field_0x1DA < 0) {
                    D_80085224[idx].field_0x1DA = 0;
                    D_80085224[idx].msgState = 5;
                }
            } else if (flags & 0xC000) {
                p = D_8005F144;
                D_80070760[p].field_0A = D_80070A60[p].field_0A = 4;
                p = D_8005F144;
                D_80070760[p].field_0B = D_80070A60[p].field_0B = D_80085224[idx].field_0x241;
                func_8009F7F4(idx, -1, D_80085224[idx].field_0x253, 1);
                func_8009F8D0(idx);
                func_8009B74C(2, (D_8005F144 - D_8005F11A) & 0x3F, D_80070A60, 1);
                func_8009B74C(1, (D_8005F144 - D_8005F118) & 0x3F, D_80070760, 1);
                D_80085224[idx].field_0x1DA++;
                if (D_80085224[idx].field_0x1DA == D_80085224[idx].field_0x1D8) {
                    D_80085224[idx].field_0x1DA = 0;
                    D_80085224[idx].msgState = 4;
                }
            } else {
                func_8009F7F4(idx, 0, D_80085224[idx].field_0x253, 0);
            }
        }
    } else {
        if (D_80085224[idx].field_0x1DA == D_80085224[idx].field_0x1D8) {
            D_80085224[idx].msgState = 2;
        } else {
            D_80085224[idx].field_0x1DA++;
            D_800D9630[idx]->unk52 += D_80085224[idx].field_0x208;
            if (D_800D9630[idx]->unk0C - 1 < D_800D9630[idx]->unk52) {
                D_800D9630[idx]->unk52 = 0;
            }
            func_8009F8D0(idx);
        }
    }
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_8009FE18);

/**
 * @brief Transcode the script entry list at @c D_800D5E90->entries into
 *        a buffer of @c TILE primitives.
 *
 * Walks the list of @ref ScriptEntry records (stride @c 0x10, terminated
 * by @c terminator == @c 0x7FFF) and writes one @c TILE per non-terminator
 * entry, returning the advanced @c prim pointer. Each tile is fixed at
 * @c count=3, gray @c 0x80 color, and base code @c 0x7C. The entry's
 * @c wLo/wHi bytes populate the two bytes of @c TILE::w, and @c h is
 * the @c TILE::h halfword. Bit 1 of @c code (semi-translucency) is
 * cleared when @c kind == @c 4 (opaque) and set otherwise.
 *
 * Called by @c func_800983F0 as part of the chain that lays out
 * draw-prim regions back-to-back in one growing buffer.
 */
TILE *func_800A0640(TILE *prim) {
    ScriptEntry *e = D_800D5E90->entries;
    while (1) {
        if (e->terminator == 0x7FFF) break;
        setlen(prim, 3);
        prim->code = 0x7C;
        ((u8 *)&prim->w)[0] = e->wLo;
        ((u8 *)&prim->w)[1] = e->wHi;
        prim->h = e->h;
        prim->b0 = 0x80;
        prim->g0 = 0x80;
        prim->r0 = 0x80;
        if (e->kind == 4) {
            prim->code &= ~0x02;
        } else {
            prim->code |= 0x02;
        }
        prim++;
        e++;
    }
    return prim;
}

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
    SetRotMatrix((u8 *)D_800C71F8);
    SetTransMatrix((u8 *)D_800C71F8);
    SetGeomOffset(0, 0);
    result = func_80040DE4(v, sxy, &unk_p, &unk_flag);
    func_8003FF88();
    return result;
}

/**
 * @brief 2D position clamp — clamp @c out->(x,y) to a rect defined by
 *        @c D_8005F0F8->rect_a[a], shrunk by half the extents of
 *        @c D_8005F0F8->rect_b[b].
 *
 * Computes four "half" values from rect_b's (f0,f2) and (f4,f6) field
 * pairs (signed average with round-toward-zero via @c (x + (x>>31)) >> 1),
 * then clamps @c out->x and @c out->y to the rect_a bounds @c (f4,f6) and
 * @c (f2,f0) minus/plus the appropriate half.
 *
 * @param out  Output position (s16 x, s16 y).
 * @param a    @c rect_a[] index (one of 2 in caller).
 * @param b    @c rect_b[] index (always 0 in caller).
 *
 * @note The first and last `half` expressions are inlined (not assigned
 *       to the @c half local) to match the target's register allocation
 *       cascade — when inlined, gcc reuses the @c a*8 register slot
 *       (a1) for the final half value; when assigned to @c half, gcc
 *       picks a3, leaving a1 holding stale @c a*8 and the function
 *       grows by 4 register-field bit changes.
 */
void func_800A0FB8(Vec2s *out, s16 a, s16 b) {
    s32 half;

    if (D_8005F0F8->rect_a[a].f4 - (D_8005F0F8->rect_b[b].f4 - D_8005F0F8->rect_b[b].f6) / 2 < out->x) {
        out->x = D_8005F0F8->rect_a[a].f4 - (D_8005F0F8->rect_b[b].f4 - D_8005F0F8->rect_b[b].f6) / 2;
    }
    half = (D_8005F0F8->rect_b[b].f4 - D_8005F0F8->rect_b[b].f6) / 2;
    if (out->x < D_8005F0F8->rect_a[a].f6 + half) {
        out->x = D_8005F0F8->rect_a[a].f6 + half;
    }
    half = (D_8005F0F8->rect_b[b].f2 - D_8005F0F8->rect_b[b].f0) / 2;
    if (D_8005F0F8->rect_a[a].f2 - half < out->y) {
        out->y = D_8005F0F8->rect_a[a].f2 - half;
    }
    if (out->y < D_8005F0F8->rect_a[a].f0 + (D_8005F0F8->rect_b[b].f2 - D_8005F0F8->rect_b[b].f0) / 2) {
        out->y = D_8005F0F8->rect_a[a].f0 + (D_8005F0F8->rect_b[b].f2 - D_8005F0F8->rect_b[b].f0) / 2;
    }
}

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
 * @note Decomp at 91.27% match — semantics and structure match; remaining
 *       diff is gcc 2.7.2 hoisting the constant @c 1 (submode init) to a
 *       prologue temp and operand-order on the @c addu of base+stride.
 *       See @c permuter/func_800A10F4/base.c.
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A10F4);

/**
 * @brief Build an @c SVECTOR from the active slot's entity position and
 *        project it through @c func_800A0F34, then call @c func_800A0FB8
 *        with a flag selected by the active-slot index.
 *
 * Reads @c D_800704A8.slots[unk1A6].param to pick an @ref Eline entity,
 * fills @c svec.{vx,vy,vz} from its @c posX/posY/posZ shifted right by
 * @c 12, biasing @c vz by @c D_8005F0F8->baseZ. The projection result is
 * latched to @c D_800C71FC. The trailing @c func_800A0FB8 clamp call gets
 * flag @c 0 when @c unk1A6 @c == @c 0 and flag @c 1 otherwise.
 *
 * @param arg0 Screen-space position, written by @c func_800A0F34 and then
 *             clamped in place by @c func_800A0FB8.
 */
void func_800A11E0(Vec2s *arg0) {
    SVECTOR svec;

    svec.vx = D_80085224[D_800704A8.slots[D_800704A8.unk1A6].param].posX >> 12;
    svec.vy = D_80085224[D_800704A8.slots[D_800704A8.unk1A6].param].posY >> 12;
    svec.vz = (D_80085224[D_800704A8.slots[D_800704A8.unk1A6].param].posZ >> 12) +
              D_8005F0F8->baseZ;
    D_800C71FC = func_800A0F34(&svec, (s32 *)arg0);
    if (D_800704A8.unk1A6 == 0) {
        func_800A0FB8(arg0, 0, 0);
    } else {
        func_800A0FB8(arg0, 1, 0);
    }
}

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

/**
 * @brief Advance one interpolating oscillator / wobble driver.
 *
 * Ticks an @ref Oscillator one step, dispatching on @c mode / @c phase:
 *  - @b mode==1 (continuous): on @c phase==0 it seeds the first target
 *    (@c end = @c (s16)(D_800C3520[tableIdx] * amplitude) / 256) and returns;
 *    otherwise, once @c angle passes @c total it starts a new cycle — the new
 *    @c end takes the @b opposite sign of the previous one (so the value
 *    oscillates) — then interpolates.
 *  - @b otherwise: on @c phase==1 it snaps @c start to the previous @c end,
 *    clears @c end, and drops to @c phase==0; on @c phase!=1 it runs until
 *    @c angle reaches @c total, then latches @c output to @c 0.
 *
 * Each non-terminal path interpolates via @c func_800A0EB8(start, end, total,
 * angle), stores the result in @c output, and advances @c angle. Every started
 * cycle also advances @c tableIdx into the @c D_800C3520 waveform table.
 *
 * @param osc The oscillator to advance.
 *
 * @note The empty @c do/while(0) after the target update is a scheduling
 *       barrier: it stops gcc from hoisting the @c tableIdx reload into the
 *       divide, keeping the round-toward-zero @c /256 in @c $v0 to match.
 *       @c delta is @c s32 so the @c (s16) truncation materialises before the
 *       negate (rather than gcc folding @c -(s16)x into @c (s16)(-x)).
 */
void func_800A17B8(Oscillator *osc) {
    if (osc->mode == 1) {
        if (osc->phase == 0) {
            s16 delta;
            osc->angle = 0;
            osc->start = 0;
            delta = (s16)(D_800C3520[osc->tableIdx] * osc->amplitude);
            osc->end = delta / 256;
            osc->phase = 1;
            osc->tableIdx++;
            return;
        }
        if (osc->total < osc->angle) {
            s32 delta;
            s16 old = osc->end;
            osc->angle = 0;
            osc->start = old;
            if (old < 0) {
                delta = (s16)(D_800C3520[osc->tableIdx] * osc->amplitude);
            } else {
                delta = -(s16)(D_800C3520[osc->tableIdx] * osc->amplitude);
            }
            osc->end = delta / 256;
            do {} while (0);
            osc->tableIdx++;
        }
        osc->output = func_800A0EB8(osc->start, osc->end, osc->total, osc->angle);
        osc->angle++;
    } else {
        if (osc->phase == 1) {
            if (osc->total < osc->angle) {
                s16 old = osc->end;
                osc->angle = 0;
                osc->end = 0;
                osc->phase = 0;
                osc->start = old;
                osc->tableIdx++;
            }
            osc->output = func_800A0EB8(osc->start, osc->end, osc->total, osc->angle);
            osc->angle++;
        } else if (osc->total <= osc->angle) {
            osc->output = 0;
        } else {
            osc->output = func_800A0EB8(osc->start, osc->end, osc->total, osc->angle);
            osc->angle++;
        }
    }
}

/**
 * @brief Per-frame field-object vertex transform + clip (hand-written asm).
 *
 * Reads clip bounds from the scratchpad (@c 0x1F800100 / @c 0x1F800104 =
 * min/max X,Y as s16 pairs), walks a stride-0x10 vertex array (@c a3)
 * until @c x == @c 0x7FFF, applies a per-object translate
 * (@c 0x1F800080[idx], idx = byte at @c a3+0xC) with wraparound by the
 * modulus at @c 0x1F800110, writes the clipped X/Y to @c a1+8 / @c a1+0xA,
 * and links GPU primitives via the @c 0xFFFFFF / @c 0x03000000 /
 * @c 0xFF000000 address masks (@c a0 / @c a2, including a @c 0xE1000000
 * draw-mode packet at @c a2).
 *
 * @note Hand-written assembly (own translation unit). Forensics from the
 *       C+asm-macro reconstruction campaign (permuter/func_800A19B8/base.c,
 *       82.93%% plateau, full structural parity): (1) its @c li constants
 *       are ori-form (ASPSX expand_li) while all 166 li sites in the rest
 *       of fe_object1.c are addiu-form (--aspsx-version=2.67) — assembled
 *       with different settings than this CU; (2) trapping @c add / @c sub
 *       / @c addi cluster in the wrap/window-test/advance idioms while
 *       addresses use @c addu (the classic hand-asm signed-math/address
 *       convention); (3) the tag-build @c or operand order is
 *       anti-canonical for gcc combine; (4) two branch delay slots are
 *       left as @c nop where dbr had eligible fillers; (5) the register
 *       assignment (mask=t3 allocated before hotter x=t4) is unreachable
 *       by gcc's allocno priority order. Both project compilers emit only
 *       @c addu / @c subu / @c addiu for C arithmetic (re-verified
 *       2026-07-29). @c INCLUDE_ASM builds to a 100%% byte match and is
 *       the faithful source form; the base.c reconstruction documents the
 *       semantics and the residual classes.
 */
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

/**
 * @brief Per-frame turn/aim update for every @ref Eline entity, then flush.
 *
 * For each of the @c D_80085388 entities: publishes the render-slot angle
 * vector (0, 0, @c field_0x241<<4 + 0x400) via @ref func_800A736C and the
 * world position (@c pos>>12 plus @c posOfs) via @ref func_800A7224. When
 * @c turnMode is 1 and a turn is not in progress (@c turnTick == 0), it
 * queries the target height (@ref func_800A8DAC op 0x1E), computes the XY
 * bearing from the entity (with offsets and height) to @c turnTgt via
 * @ref func_8009A0E8 and stores it rate-clamped (+/- @c turnYawRate around
 * the current @c field_0x241 heading) into @c turnYawDst, then likewise the
 * elevation bearing into @c turnPitchDst (clamped by @c turnPitchRate).
 * While @c turnTick < @c turnLen it unwraps both destinations into the
 * +/-0x80 window around their committed angles, advances @c turnTick,
 * interpolates yaw/pitch via @ref func_800A0EB8, emits render op 0x13 with
 * the angle vector, commits Dst->Cur when the turn completes, and emits op
 * 0x25; otherwise, idle entities (@c turnMode == 0) emit ops 0x15 and 0x16.
 * Entities also emit op 0x25 whenever @ref func_800BE274 reports active.
 * Finally @ref func_800A63AC flushes with @p arg1.
 *
 * @param ents Eline entity array (@c D_80085224).
 * @param arg1 Pass-through context for the @ref func_800A63AC flush.
 */
void func_800A1CFC(Eline *ents, u8 *arg1) {
    Vec3i pB;       /* sp+0x10: bearing arg B */
    Vec3i pA;       /* sp+0x20: bearing arg A */
    Vec3s v30;      /* sp+0x30: world-position vector */
    Vec3s v38;      /* sp+0x38: angle vector */
    s16 buf[4];     /* sp+0x40: func_800A8DAC output (buf[2] = target height) */
    s32 dist;       /* sp+0x48: horizontal distance from the first bearing */
    s32 i;
    Eline *ent;
    /* Separate clamp variables per axis: sharing one diff/cmp pair across
       both clamps changes the allocno densities and rotates a0/v1. */
    s32 diff;
    s32 diffB;
    s32 cmp;
    s32 cmpB;
    s32 d2;
    s32 d2B;
    s32 v;

    for (i = 0, ent = ents; i < D_80085388; ent++, i++) {
        v38.z = (ent->field_0x241 << 4) + 0x400;
        v38.x = 0;
        v38.y = 0;
        func_800A736C(i, (u16 *)&v38, 0);
        v30.x = (u16)ent->posOfsX + (ent->posX >> 12);
        v30.y = (u16)ent->posOfsY + (ent->posY >> 12);
        v30.z = (u16)ent->posOfsZ + (ent->posZ >> 12);
        func_800A7224(i, (u16 *)&v30, 0);
        if (ent->turnMode == 1 && ent->turnTick == 0) {
            func_800A8DAC(i, 0x1E, D_800C71F8, buf);
            pB.x = ent->turnTgtX;
            pB.y = ent->turnTgtY;
            pB.z = ent->turnTgtZ;
            pA.x = (ent->posX >> 12) + ent->posOfsX;
            pA.y = (ent->posY >> 12) + ent->posOfsY;
            pA.z = buf[2] + (ent->posZ >> 12) + ent->posOfsZ;
            v = func_8009A0E8((s32 *)&pA, (s32 *)&pB, &dist);
            v &= 0xFF;
            v -= ent->field_0x241;
            diff = (u8)v;
            cmp = ent->turnYawDst = diff;
            if (cmp < 0x80) {
                if (ent->turnYawRate < cmp) {
                    ent->turnYawDst = ent->turnYawRate;
                }
            } else {
                d2 = diff - 0x100;
                ent->turnYawDst = d2;
                if (d2 < -ent->turnYawRate) {
                    ent->turnYawDst = -ent->turnYawRate;
                }
            }
            pB.y = 0;
            pB.x = ent->turnTgtZ;
            pB.z = 0;
            pA.y = dist;
            pA.x = buf[2] + (ent->posZ >> 12) + ent->posOfsZ;
            pA.z = 0;
            v = func_8009A0E8((s32 *)&pA, (s32 *)&pB, &dist);
            diffB = v & 0xFF;
            cmpB = ent->turnPitchDst = diffB;
            if (cmpB < 0x80) {
                if (ent->turnPitchRate < cmpB) {
                    ent->turnPitchDst = ent->turnPitchRate;
                }
            } else {
                d2B = diffB - 0x100;
                ent->turnPitchDst = d2B;
                if (d2B < -ent->turnPitchRate) {
                    ent->turnPitchDst = -ent->turnPitchRate;
                }
            }
        }
        if (ent->turnTick < ent->turnLen) {
            if (ent->turnPitchDst > ent->turnPitchCur) {
                if (ent->turnPitchDst - ent->turnPitchCur > 0x80) {
                    ent->turnPitchDst = (u16)ent->turnPitchDst - 0x100;
                }
            } else {
                if (ent->turnPitchCur - ent->turnPitchDst > 0x80) {
                    ent->turnPitchDst = (u16)ent->turnPitchDst + 0x100;
                }
            }
            if (ent->turnYawDst > ent->turnYawCur) {
                if (ent->turnYawDst - ent->turnYawCur > 0x80) {
                    ent->turnYawDst = (u16)ent->turnYawDst - 0x100;
                }
            } else {
                if (ent->turnYawCur - ent->turnYawDst > 0x80) {
                    ent->turnYawDst = (u16)ent->turnYawDst + 0x100;
                }
            }
            ent->turnTick++;
            v = func_800A0EB8(ent->turnYawCur, ent->turnYawDst, ent->turnLen, ent->turnTick);
            v38.z = -((v & 0xFF) << 4);
            v = func_800A0EB8(ent->turnPitchCur, ent->turnPitchDst, ent->turnLen, ent->turnTick);
            v &= 0xFF;
            v38.y = v << 4;
            v38.x = 0;
            func_800A97E4(i, 0x13, (s32)&v38, 0);
            if (ent->turnTick == ent->turnLen) {
                ent->turnPitchCur = ent->turnPitchDst;
                ent->turnRollCur = ent->turnRollDst;
                ent->turnYawCur = ent->turnYawDst;
            }
            func_800A97E4(i, 0x25, 0, 0);
        } else {
            if (ent->turnMode == 0) {
                func_800A97E4(i, 0x15, 0, 0);
                func_800A97E4(i, 0x16, 0, 0);
            }
        }
        if (func_800BE274() != 0) {
            func_800A97E4(i, 0x25, 0, 0);
        }
    }
    func_800A63AC(arg1, D_800C71F8, 0);
}

/**
 * @brief Shape @c func_800A2128 sees: a buffer with a 128-entry
 *        28-byte item table at offset 0x4000 and a 16-entry 8-byte
 *        prim table immediately after.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x7];
    /* 0x07 */ u8 b7;
    /* 0x08 */ u8 pad08[0x4];
    /* 0x0C */ u8 bC;
    /* 0x0D */ u8 bD;
    /* 0x0E */ u8 bE;
    /* 0x0F */ u8 pad0F[0x5];
    /* 0x14 */ u8 b14;
    /* 0x15 */ u8 b15;
    /* 0x16 */ u8 b16;
    /* 0x17 */ u8 pad17[0x5];
} func_800A2128_item1;  /* 0x1C = 28 bytes */

typedef struct {
    /* 0x00 */ u8  pad03[0x3];
    /* 0x03 */ u8  tag;     /**< Always written as @c 1. */
    /* 0x04 */ s32 cmd;     /**< @c 0xE1000200 | (color & 0x9FF). */
} func_800A2128_item2;  /* 8 bytes */

typedef struct {
    /* 0x0000 */ u8 pad0000[0x4000];
    /* 0x4000 */ func_800A2128_item1 items1[128];
    /* 0x4E00 */ func_800A2128_item2 items2[16];
} func_800A2128_arg0;

/**
 * @brief Reset 128 entries in @c items1[] and emit 16 GPU draw-mode
 *        prims into @c items2[].
 *
 * Loop 1: for each of 128 28-byte items, call @c func_8004D684(item)
 *         (which clears/inits it), then clear @c bC/bD/bE and
 *         @c b14/b15/b16 and set bit 1 of @c b7.
 *
 * Loop 2: for each of 16 8-byte items, write @c tag = 1 and
 *         @c cmd = @c 0xE1000200 | (color & 0x9FF), where @c color
 *         comes from @c func_8004D524(0, 2, 0, 0).
 *
 * @note The two @c i[t->items1] / @c i[t->items2] uses (instead of
 *       @c t->items1[i] / @c t->items2[i]) are the trick that swaps
 *       the @c addu operand order to match the target — see
 *       @c pattern_i_arr_to_swap_addu in project memory.
 */
void func_800A2128(func_800A2128_arg0 *t) {
    s16 i;
    for (i = 0; i < 128; i++) {
        func_8004D684(&i[t->items1]);
        i[t->items1].bC = 0;
        i[t->items1].bD = 0;
        i[t->items1].bE = 0;
        i[t->items1].b14 = 0;
        i[t->items1].b15 = 0;
        i[t->items1].b16 = 0;
        i[t->items1].b7 |= 2;
    }
    for (i = 0; i < 16; i++) {
        s32 color;
        i[t->items2].tag = 1;
        color = func_8004D524(0, 2, 0, 0);
        t->items2[i].cmd = (color & 0x9FF) | 0xE1000200;
    }
}

/**
 * @brief Draws every field entity's blob shadow as a flat-shaded 8-triangle fan.
 *
 * Builds a unit octagon once per call — @c func_8009D234 / @c func_8009D254 sampled
 * at the eight 32-step headings give the cos/sin pair for each ring point — then
 * walks the @ref Eline pool. An entity casts a shadow only when it is not flagged
 * out (@c flags bit 3), is active (@c unk218 @c != @c -1) and is in the state
 * @c unk258 @c == @c 1.
 *
 * The fan centre is the entity's position dropped to integer world units, and ring
 * point @c k sits at that centre offset along octagon direction @c k, scaled by the
 * entity's own @c shadowRadius[k]. Because the eight radii are independent the
 * shadow need not be circular — @c SHADEFORM sets them individually, @c SHADESET
 * makes them uniform. The nine points are projected with one @c func_80040DE4 (whose
 * return gives the OTZ) plus three @c RTPT batches, and when the centre is in depth
 * range the fan is emitted as eight @ref POLY_G3 triangles, every one flat-shaded in
 * @c shadowLevel, followed by the slot's tpage command.
 *
 * @param ot   Ordering table to link the shadows into.
 * @param m    Camera matrix loaded into the GTE before projecting.
 * @param prim Triangle arena; advanced eight prims per shadow drawn.
 * @param tp   Tpage commands; advanced one per shadow drawn.
 * @param ents The @ref Eline pool (@c D_80085388 entries).
 *
 * @note The scratchpad holds the octagon tables and the nine working points:
 *       cos at @c getScratchAddr(2), sin at @c getScratchAddr(6), points at
 *       @c getScratchAddr(10).
 * @note @c sxy is two words wide because that is the slot the original reserves for
 *       @c func_80040DE4's screen-XY output; only the first word is meaningful.
 * @note @c rad is advanced at the end of the ring loop rather than indexed: that is
 *       what makes gcc give it an induction variable of its own alongside the two
 *       octagon tables, which is how the original walks all three.
 */
void func_800A222C(u32 *ot, MATRIX *m, POLY_G3 *prim, DR_TPAGE *tp, Eline *ents) {
    s16 *cosTbl = (s16 *)getScratchAddr(2);
    s16 *sinTbl = (s16 *)getScratchAddr(6);
    SVECTOR *pt = (SVECTOR *)getScratchAddr(10);
    s32 sxy[2];
    s32 p;
    s32 flag;
    s32 otz;
    s32 i;
    s32 k;
    u8 *rad;
    u8 c;

    for (i = 0; i < 8; i++) {
        cosTbl[i] = func_8009D234(i * 32);
        sinTbl[i] = func_8009D254(i * 32);
    }

    func_8003FEE4();
    SetRotMatrix(m);
    SetTransMatrix(m);

    for (i = 0; i < D_80085388; ents++, i++) {
        if (ents->flags & 8) {
            continue;
        }
        if (ents->unk218 == -1) {
            continue;
        }
        if (ents->unk258 != 1) {
            continue;
        }

        pt[0].vx = ents->posX / 4096;
        pt[0].vy = ents->posY / 4096;
        pt[0].vz = ents->posZ / 4096;

        rad = &ents->shadowRadius[0];
        for (k = 0; k < 8; k++) {
            pt[k + 1].vx = pt[0].vx + cosTbl[k] * rad[0] / 512;
            pt[k + 1].vy = pt[0].vy + sinTbl[k] * rad[0] / 512;
            pt[k + 1].vz = pt[0].vz;
            rad++;
        }

        otz = func_80040DE4(&pt[0], sxy, &p, &flag);
        gte_ldv3(&pt[0], &pt[1], &pt[2]);
        gte_RTPT();
        gte_stsxy3(&pt[0], &pt[1], &pt[2]);
        gte_ldv3(&pt[3], &pt[4], &pt[5]);
        gte_RTPT();
        gte_stsxy3(&pt[3], &pt[4], &pt[5]);
        gte_ldv3(&pt[6], &pt[7], &pt[8]);
        gte_RTPT();
        gte_stsxy3(&pt[6], &pt[7], &pt[8]);

        if (otz < 0xFFF) {
            c = ents->shadowLevel;

            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[1].vx, pt[1].vy, pt[2].vx, pt[2].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[2].vx, pt[2].vy, pt[3].vx, pt[3].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[3].vx, pt[3].vy, pt[4].vx, pt[4].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[4].vx, pt[4].vy, pt[5].vx, pt[5].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[5].vx, pt[5].vy, pt[6].vx, pt[6].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[6].vx, pt[6].vy, pt[7].vx, pt[7].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[7].vx, pt[7].vy, pt[8].vx, pt[8].vy);
            addPrim(&ot[otz], prim);
            prim++;
            setRGB0(prim, c, c, c);
            setXY3(prim, pt[0].vx, pt[0].vy, pt[8].vx, pt[8].vy, pt[1].vx, pt[1].vy);
            addPrim(&ot[otz], prim);
            prim++;

            addPrim(&ot[otz], tp);
            tp++;
        }
    }

    func_8003FF88();
}

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

/** @brief Signed/unsigned halfword view of the split-image header word. */
typedef union {
    s16 s;
    u16 u;
} func_800A2D2C_half;

/**
 * @brief Upload a split field-graphic to VRAM in three blits.
 *
 * @p buf begins with a halfword header: the split row (0x2020 — ASCII
 * spaces — means "empty buffer, skip"). The upload sequence, each part
 * preceded by a busy-wait on @ref func_80048C50 and a @ref func_80042634
 * reset:
 *  1. a 256x1 strip at (0, 0xE8) from @c buf+2 (the palette row),
 *  2. a 64-wide column at (slot*64, header+0x100) of height
 *     (0x100-header)/2 from @c buf+0x102,
 *  3. the remaining half directly below it, from the matching offset
 *     within @p buf.
 *
 * @param buf  Halfword staging buffer (header, palette row, then pixel rows).
 * @param slot VRAM column slot; x = @p slot * 64.
 *
 * @note The volatile-cast re-read of the header in part 3 keeps gcc from
 *       folding the unsigned reload into the earlier signed load — the
 *       original emits both.
 */
void func_800A2D2C(s16 *buf, s32 slot) {
    RECT rect;
    func_800A2D2C_half *hp;
    s32 y0;
    s32 h2;

    hp = (func_800A2D2C_half *)buf;
    if (buf[0] != 0x2020) {
        while (func_80048C50(1) != 0) {}
        func_80042634(0);
        rect.x = 0;
        rect.y = 0xE8;
        rect.w = 0x100;
        rect.h = 1;
        func_80048EFC(&rect, (u8 *)(buf + 2));
        while (func_80048C50(1) != 0) {}
        func_80042634(0);
        rect.x = slot << 6;
        rect.y = hp->u + 0x100;
        rect.w = 0x40;
        rect.h = (0x100 - hp->s) / 2;
        func_80048EFC(&rect, (u8 *)(buf + 0x102));
        while (func_80048C50(1) != 0) {}
        func_80042634(0);
        rect.x = slot << 6;
        y0 = ((volatile func_800A2D2C_half *)hp)->u;
        h2 = (0x100 - hp->s) / 2;
        y0 += 0x100;
        rect.y = h2 + y0;
        rect.w = 0x40;
        rect.h = (0x100 - hp->s) / 2;
        func_80048EFC(&rect, (u8 *)(buf + ((((0x100 - hp->s) / 2) << 6) + 0x102)));
    }
}

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

/**
 * @brief View of the Eline stack region used by @c func_800A327C — three
 * @c s16 control points (@c a, @c b, @c c) and a @c num/denom progress ratio.
 *
 * @note Named after the function/arg, mirroring @ref func_800A3488_arg0
 *       (the two-point linear variant): same memory, one more control point.
 */
typedef struct {
    /* 0x00 */ s16 ax, ay, az;          /**< Control point A (at progress @c 0). */
    /* 0x06 */ u8  pad06[0x02];
    /* 0x08 */ s16 bx, by, bz;          /**< Control point B (curve pull). */
    /* 0x0E */ u8  pad0E[0x02];
    /* 0x10 */ s16 cx, cy, cz;          /**< Control point C (at progress @c denom). */
    /* 0x16 */ u8  pad16[0xE0 - 0x16];
    /* 0xE0 */ u8  denom;               /**< Step count denominator. */
    /* 0xE1 */ u8  padE1[0x0F];
    /* 0xF0 */ s16 num;                 /**< Current step. */
} func_800A327C_arg0;

/** @brief Intermediate lerp point of @c func_800A327C (u16 components). */
typedef struct {
    u16 x, y, z, pad;
} func_800A327C_mid;

/**
 * @brief Quadratic-Bezier interpolate a 3D position across three @c s16
 *        control points (de Casteljau evaluation).
 *
 * Computes the two edge midpoints @c m1 = A + (B-A)*num/denom and
 * @c m2 = B + (C-B)*num/denom, then writes @c out->vx/vy/vz with the
 * second-stage lerp @c m1 + (m2-m1)*num/denom.
 *
 * Used by @c func_800A355C dispatch (mode 3) to compute the per-step spawn
 * position for particle bursts along a curved path.
 *
 * @note The midpoint components are @c u16 with @c (s16) casts at the
 *       second stage — this mixed-width view reproduces the original's
 *       lhu/lh load pairing and shift re-extensions.
 */
void func_800A327C(func_800A327C_arg0 *a, SVECTOR *out) {
    func_800A327C_mid m1;
    func_800A327C_mid m2;

    m1.x = ((a->bx - a->ax) * a->num) / a->denom + (u16)a->ax;
    m1.y = ((a->by - a->ay) * a->num) / a->denom + (u16)a->ay;
    m1.z = ((a->bz - a->az) * a->num) / a->denom + (u16)a->az;
    m2.x = ((a->cx - a->bx) * a->num) / a->denom + (u16)a->bx;
    m2.y = ((a->cy - a->by) * a->num) / a->denom + (u16)a->by;
    m2.z = ((a->cz - a->bz) * a->num) / a->denom + (u16)a->bz;

    out->vx = m1.x + ((s16)m2.x - (s16)m1.x) * a->num / a->denom;
    out->vy = m1.y + ((s16)m2.y - (s16)m1.y) * a->num / a->denom;
    out->vz = m1.z + ((s16)m2.z - (s16)m1.z) * a->num / a->denom;
}

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

/**
 * @brief Per-frame animation tick for all 16 slots of a field subscene buffer.
 *
 * Walks the 16 @ref FieldSubsceneSlot entries of @p buf (stride @c 0xFE). For
 * each slot marked active in @c D_800704A8.slotActive[i]:
 *  - if the per-frame counter @c h1 has reached @c table[h2], reset @c h1,
 *    advance the table cursor @c h2, and if the next table entry is @c 0 wrap
 *    @c h2 and @c h0 back to 0;
 *  - dispatch the visual update via @c func_800A355C (the slot's @c subscene is
 *    the @ref FieldActor argument), then advance @c h0 and @c h1.
 * Inactive slots have all three state halfwords (@c h0 / @c h1 / @c h2) cleared.
 *
 * @param arg0 Unused.
 * @param arg1 Unused.
 * @param buf  Subscene buffer (from the @c D_800C7200 table).
 *
 * @note @c pos is declared but unused: the original reserves an 8-byte stack
 *       slot here (gcc 2.7.2 keeps an unused struct local), matching the frame.
 */
void func_800A37A8(void *arg0, s32 arg1, FieldSubsceneBuffer *buf) {
    s32 i;
    SVECTOR pos;

    for (i = 0; i < 16; i++) {
        if (D_800704A8.slotActive[i] != 0) {
            if (buf->slots[i].h1 >= buf->slots[i].table[buf->slots[i].h2]) {
                buf->slots[i].h1 = 0;
                buf->slots[i].h2++;
                if (buf->slots[i].table[buf->slots[i].h2] == 0) {
                    buf->slots[i].h2 = 0;
                    buf->slots[i].h0 = 0;
                }
            }
            func_800A355C((FieldActor *)&buf->slots[i], i, (s32)buf);
            buf->slots[i].h0++;
            buf->slots[i].h1++;
        } else {
            buf->slots[i].h0 = 0;
            buf->slots[i].h1 = 0;
            buf->slots[i].h2 = 0;
        }
    }
}

/**
 * @brief Advance a 3-axis position+angle lerp accumulator by one tick.
 *
 * Updates @p out 's @c posX/posY/posZ (@c s32) and @c angle (@c u16)
 * accumulators by adding (per-axis) @c (startSnapshot @c + @c origin @c +
 * @c (target @c - @c origin) @c * @c stepProgress @c / @c stepTotal),
 * where the angle term is then @c <<4. Bails early when
 * @p in->stepTotal @c == @c 0 to avoid a divide-by-zero.
 *
 * Caching @c in->angle into an @c s32 local prevents gcc from doing
 * lhu+sll/sra for that field; caching @c out->angleStart into an
 * @c s32 local makes gcc pick @c lh over @c lhu for it. Together
 * these match the target's exact instruction selection.
 */
void func_800A38B4(MoveAccum *out, MoveStep *in, MoveStep *target) {
    s32 a;
    s32 s;
    if (in->stepTotal != 0) {
        a = in->angle;
        s = out->angleStart;
        out->angle += (s + a + (target->angle - a) * out->stepProgress / in->stepTotal) << 4;
        out->posX  += out->xStart + in->x + (target->x - in->x) * out->stepProgress / in->stepTotal;
        out->posY  += out->yStart + in->y + (target->y - in->y) * out->stepProgress / in->stepTotal;
        out->posZ  += out->zStart + in->z + (target->z - in->z) * out->stepProgress / in->stepTotal;
    }
}

/**
 * @brief Emit one field sprite for a movement accumulator and link it into the OT.
 *
 * Advances @p acc one tick along its current waypoint pair (@c func_800A38B4),
 * projects the accumulated position through the GTE, and — when the projected
 * depth lands inside the ordering table — builds a @ref POLY_FT4 for the sprite
 * and chains it in.
 *
 * The quad is a screen-space billboard: RTPS gives the centre, the sprite's
 * half-extents are lerped between the current and next waypoint, scaled by
 * @c FieldView::spriteScale / OTZ so it shrinks with distance, then rotated by
 * @c acc->angle through two @c mvmva passes to give the two corner offsets.
 * Texture coordinates, colour and the semi-transparency bits of the tpage word
 * all come from the current waypoint; colour is lerped like the extents.
 *
 * Finally the tick counter advances, rolling over to the next waypoint once it
 * reaches @c MoveStep::stepTotal.
 *
 * @param acc Accumulator to advance and draw.
 * @param rec Movement command @p acc is playing.
 * @param buf Field bundle header; its @c primCursor is the prim arena cursor.
 * @param ot  Ordering table to link the quad into.
 *
 * @note The scratchpad slots are separate @c getScratchAddr locals rather than
 *       one struct: the original materialises each address independently and
 *       spills two of them, which a single base pointer does not reproduce.
 * @note @c func_80041C74 is the main binary's @c RotMatrix_gte and
 *       @c func_80040534 its @c TransMatrix; the field overlay links both by
 *       address, so they keep their @c func_ names here.
 */
void func_800A39D8(MoveAccum *acc, MoveRecord *rec, FieldSubsceneBuffer *buf, u32 *ot) {
    SVECTOR *pos = (SVECTOR *)getScratchAddr(5);
    SVECTOR *rot = (SVECTOR *)getScratchAddr(7);
    SVECTOR *corner = (SVECTOR *)getScratchAddr(9);
    MacVec *mac = (MacVec *)getScratchAddr(11);
    VECTOR *trans = (VECTOR *)getScratchAddr(15);
    MATRIX *m = (MATRIX *)getScratchAddr(19);
    MoveStep *step;
    MoveStep *next;
    s32 otz;
    s16 spriteX, spriteY;
    s32 dx, dy;
    s32 scale;
    u32 tp;
    u32 halfW, halfH;

    step = &rec->steps[acc->stepIndex];
    next = &rec->steps[acc->stepIndex + 1];
    func_800A38B4(acc, step, next);

    pos->vx = acc->posX / 16;
    pos->vy = acc->posY / 16;
    pos->vz = acc->posZ / 16;

    if (step->mode == 4) {
        buf->primCursor->code &= ~2;
        buf->primCursor->tpage = (D_8005F0F8->tpageX & 0xF) | 0x10;
    } else {
        buf->primCursor->code |= 2;
        tp = (((step->mode & 3) << 5) | 0x10);
        tp |= D_8005F0F8->tpageX & 0xF;
        buf->primCursor->tpage = tp;
    }

    gte_ldv0(pos);
    gte_RTPS();
    gte_stsxy(&buf->primCursor->x0);
    gte_stszotz(&otz);

    otz += rec->zBias;
    if (otz > 0 && otz < 0x1000) {
        dx = (next->spriteX - step->spriteX) * acc->stepProgress / step->stepTotal;
        dy = (next->spriteY - step->spriteY) * acc->stepProgress / step->stepTotal;
        scale = (D_800C71F8->spriteScale << 14) / otz;
        spriteX = step->spriteX + dx;
        spriteY = step->spriteY + dy;

        if ((rec->flags & 0xFF80) == 0) {
            halfW = (u32)(spriteX * scale) >> 14;
            halfH = (u32)(spriteY * scale) >> 14;
        } else {
            halfW = (u32)(spriteX * scale) >> 13;
            halfH = (u32)(spriteY * scale) >> 13;
        }

        rot->vy = 0;
        rot->vx = 0;
        rot->vz = acc->angle;
        func_80041C74(rot, m);

        trans->vz = 0;
        trans->vy = 0;
        trans->vx = 0;
        func_80040534(m, trans);

        gte_SetRotMatrix(m);
        gte_SetTransMatrix(m);

        corner->vx = halfW;
        corner->vy = -halfH;
        corner->vz = 0;
        gte_ldv0(corner);
        gte_mvmva(1, 0, 0, 0, 0);

        buf->primCursor->u0 = buf->primCursor->u2 = step->u;
        buf->primCursor->v0 = buf->primCursor->v1 = step->v;
        gte_stlvnl(mac);

        buf->primCursor->x1 = buf->primCursor->x0 + mac->x;
        buf->primCursor->y1 = buf->primCursor->y0 + mac->y;
        buf->primCursor->x2 = buf->primCursor->x0 - mac->x;
        buf->primCursor->y2 = buf->primCursor->y0 - mac->y;

        corner->vy = halfH;
        gte_ldv0(corner);
        gte_mvmva(1, 0, 0, 0, 0);

        buf->primCursor->u1 = buf->primCursor->u3 = step->u + step->w - 1;
        buf->primCursor->v2 = buf->primCursor->v3 = step->v + step->h - 1;
        gte_stlvnl(mac);

        buf->primCursor->x3 = buf->primCursor->x0 + mac->x;
        buf->primCursor->y3 = buf->primCursor->y0 + mac->y;
        buf->primCursor->x0 = buf->primCursor->x0 - mac->x;
        buf->primCursor->y0 = buf->primCursor->y0 - mac->y;

        buf->primCursor->r0 = step->r + (next->r - step->r) * acc->stepProgress / step->stepTotal;
        buf->primCursor->g0 = step->g + (next->g - step->g) * acc->stepProgress / step->stepTotal;
        buf->primCursor->b0 = step->b + (next->b - step->b) * acc->stepProgress / step->stepTotal;

        addPrim(&ot[otz], buf->primCursor);
        buf->primCursor = (POLY_FT4 *)((u8 *)buf->primCursor + 0x28);
    }

    acc->stepProgress++;
    if (acc->stepProgress >= step->stepTotal) {
        acc->stepProgress = 0;
        acc->stepIndex++;
    }
}

/**
 * @brief Fast-forward an entire field animation buffer to completion.
 *
 * Runs the per-tick work of @c func_800A37A8 and @c func_800A38B4 in a loop
 * until every subscene slot has played out: the tick count is the largest
 * @c frameCount across the 16 slots, and a slot is dropped (its
 * @c D_800704A8.slotActive entry cleared) once the tick passes its own count.
 *
 * Each tick advances two things. Every still-active slot steps its animation
 * table — resetting the cursor to the start when the table byte runs out —
 * and is dispatched through @c func_800A355C. Every running accumulator is
 * lerped one step by @c func_800A38B4 toward the next waypoint of its command;
 * a waypoint with a zero tick count ends the command (clearing @c active and
 * releasing one user of the record), and reaching a waypoint's tick count
 * advances to the next one.
 *
 * The @c slotActive flags are saved on entry and restored on exit, so the
 * caller's live slot set survives the fast-forward.
 *
 * @param buf Animation buffer to run to completion.
 */
void func_800A3FE0(FieldSubsceneBuffer *buf) {
    u8 saved[16];
    s32 i;
    s32 j;
    s32 maxFrames;

    for (i = 0; i < 16; i++) {
        saved[i] = D_800704A8.slotActive[i];
    }

    maxFrames = 0;
    for (i = 0; i < 16; i++) {
        if (maxFrames < buf->slots[i].frameCount) {
            maxFrames = buf->slots[i].frameCount;
        }
    }

    for (i = 0; i < maxFrames; i++) {
        for (j = 0; j < 16; j++) {
            if (buf->slots[j].frameCount < i) {
                D_800704A8.slotActive[j] = 0;
            }
            if (D_800704A8.slotActive[j] != 0) {
                if (buf->slots[j].h1 >= buf->slots[j].table[buf->slots[j].h2]) {
                    buf->slots[j].h1 = 0;
                    buf->slots[j].h2++;
                    if (buf->slots[j].table[buf->slots[j].h2] == 0) {
                        buf->slots[j].h2 = 0;
                        buf->slots[j].h0 = 0;
                    }
                }
                func_800A355C(&buf->slots[j], j, buf);
                buf->slots[j].h0++;
                buf->slots[j].h1++;
            }
        }
        for (j = 0; j < 128; j++) {
            if (buf->entries[j].active == 1) {
                func_800A38B4(&buf->entries[j],
                              &buf->records[buf->entries[j].cmdIndex].steps[buf->entries[j].stepIndex],
                              &buf->records[buf->entries[j].cmdIndex].steps[buf->entries[j].stepIndex + 1]);
                if (buf->records[buf->entries[j].cmdIndex].steps[buf->entries[j].stepIndex].stepTotal == 0) {
                    buf->entries[j].active = 0;
                    buf->records[buf->entries[j].cmdIndex].activeCount--;
                }
                buf->entries[j].stepProgress++;
                if (buf->entries[j].stepProgress >=
                    buf->records[buf->entries[j].cmdIndex].steps[buf->entries[j].stepIndex].stepTotal) {
                    buf->entries[j].stepProgress = 0;
                    buf->entries[j].stepIndex++;
                }
            }
        }
    }

    for (i = 0; i < 16; i++) {
        D_800704A8.slotActive[i] = saved[i];
    }
}

/**
 * @brief Initialise the field-object GPU primitive packets and seed the
 *        8-object shimmer state.
 *
 * Runs three passes:
 *  1. **40 gouraud quads** (@p polys): stamp each @ref POLY_G4 with a length
 *     of @c 8 words and code @c 0x3A (gouraud four-point, semi-transparent).
 *  2. **32 draw-mode packets** (@p tpages): stamp each @ref DR_TPAGE with a
 *     length of @c 1 word and a GP0(E1h) draw-mode command whose low bits come
 *     from @c func_8004D524() (texture page / semi-transparency selection).
 *  3. **8 shimmer objects**: for each @ref ObjSlot / @ref DrawPoint pair, sample
 *     a perturbation byte from @c D_800C3520 at the current VSync phase
 *     (@c D_8005F154 @c + slot), offset the draw-point corners by it, mirror the
 *     base @c (x,y,z) into all 8 vertices of both corner buffers, and reset the
 *     per-object tick (@c field80 @c = @c 0x10 @c + slot*2) and @c field82.
 *
 * @param polys  Array of 40 @ref POLY_G4 quad primitives to initialise.
 * @param tpages Array of 32 @ref DR_TPAGE draw-mode packets to initialise.
 *
 * @note The @c P_TAG length byte sits 4 bytes before the @c code byte; both
 *       passes write the length through the same walking @c code cursor
 *       (@c c[-4] / @c w[-1]) so gcc keeps the loop induction variable anchored
 *       at the code field, matching the original's cursor.
 */
void func_800A42EC(POLY_G4 *polys, DR_TPAGE *tpages) {
    s32 i, j;
    POLY_G4 *p;
    DR_TPAGE *q;

    i = 0;
    p = polys;
    do {
        u8 *c = &p->code;
        c[-4] = 8;
        *c = 0x3A;
        p++;
    } while (++i < 40);

    i = 0;
    q = tpages;
    do {
        u32 *w = &q->code[0];
        ((u8 *)w)[-1] = 1;
        *w = (func_8004D524(0, 1, 0, 0) & 0x9FF) | 0xE1000200;
        q++;
    } while (++i < 32);

    for (i = 0; i < 8; i++) {
        D_800C6DA0[i].field86 = D_800C3520[(D_8005F154 + i) & 0xFF];
        D_800C6DA0[i].field87 = D_800C3520[(D_8005F154 + i) & 0xFF];
        D_800706A0[i].field8 = (D_800706A0[i].x + D_800C3520[(D_8005F154 + i) & 0xFF]) - 0x80;
        D_800706A0[i].fieldA = (D_800706A0[i].y + D_800C3520[(D_8005F154 + i + 8) & 0xFF]) - 0x80;
        D_800706A0[i].fieldC = D_800706A0[i].z + 0x40;
        D_800706A0[i].field10 = D_800706A0[i].x;
        D_800706A0[i].field12 = D_800706A0[i].y;
        D_800706A0[i].field14 = D_800706A0[i].z + 0x80;

        for (j = 0; j < 8; j++) {
            D_800C6DA0[i].va[j].x = D_800C6DA0[i].vb[j].x = D_800706A0[i].x;
            D_800C6DA0[i].va[j].y = D_800C6DA0[i].vb[j].y = D_800706A0[i].y;
            D_800C6DA0[i].va[j].z = D_800C6DA0[i].vb[j].z = D_800706A0[i].z;
        }

        D_800C6DA0[i].field80 = 0x10 + i * 2;
        D_800C6DA0[i].field82 = 0;
    }
}

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

/**
 * @brief Spawn the 8 shimmer objects aimed at an entity.
 *
 * Computes the facing angle from draw-point 0 (@c D_800706A0[0]) to entity
 * @p entityIdx 's world position (@ref Eline @c posX / @c posY, right-shifted
 * out of 12-bit fixed point) via @c func_8009A0E8, then arms all 8 object slots:
 *  - marks each slot flag @c D_8005F168[i] @c = @c 2 ("spawned"; later detected
 *    by the @c ==2 scan);
 *  - stores the facing angle @c ±0x40 into @c field86 / @c field87;
 *  - seeds the draw-point corners with a table perturbation scaled ×4 and
 *    biased by @c -0x200 (@c field8 / @c fieldA), a rising Z offset
 *    @c 1000 @c + slot*128 (@c fieldC), and the entity position for
 *    @c field10 / @c field12 / @c field14 (Z @c + @c 0xB4);
 *  - mirrors the base @c (x,y,z) into all 8 vertices of both corner buffers;
 *  - resets the per-object tick (@c field80 @c = @c 0x18 @c + slot*2) and
 *    @c field82.
 *
 * @param entityIdx Index into the @ref Eline entity array (@c D_80085224) that
 *                  the shimmer objects are aimed at.
 */
void func_800A455C(s16 entityIdx) {
    s32 i, j;
    s32 angle;
    s32 objPos[3];
    s32 entityPos[3];
    s32 dist[2];

    objPos[0] = (s16)D_800706A0[0].x;
    objPos[1] = (s16)D_800706A0[0].y;
    objPos[2] = 0;
    entityPos[0] = D_80085224[entityIdx].posX >> 12;
    entityPos[1] = D_80085224[entityIdx].posY >> 12;
    entityPos[2] = 0;
    angle = func_8009A0E8(objPos, entityPos, dist);

    for (i = 0; i < 8; i++) {
        D_8005F168[i] = 2;
        D_800C6DA0[i].field86 = angle + 0x40;
        D_800C6DA0[i].field87 = angle - 0x40;
        D_800706A0[i].field8 = (D_800706A0[i].x + D_800C3520[(D_8005F154 + i) & 0xFF] * 4) - 0x200;
        D_800706A0[i].fieldA = (D_800706A0[i].y + D_800C3520[(D_8005F154 + i + 8) & 0xFF] * 4) - 0x200;
        D_800706A0[i].fieldC = D_800706A0[i].z + (1000 + i * 128);
        D_800706A0[i].field10 = D_80085224[entityIdx].posX >> 12;
        D_800706A0[i].field12 = D_80085224[entityIdx].posY >> 12;
        D_800706A0[i].field14 = (D_80085224[entityIdx].posZ >> 12) + 0xB4;

        for (j = 0; j < 8; j++) {
            D_800C6DA0[i].va[j].x = D_800C6DA0[i].vb[j].x = D_800706A0[i].x;
            D_800C6DA0[i].va[j].y = D_800C6DA0[i].vb[j].y = D_800706A0[i].y;
            D_800C6DA0[i].va[j].z = D_800C6DA0[i].vb[j].z = D_800706A0[i].z;
        }

        D_800C6DA0[i].field80 = 0x18 + i * 2;
        D_800C6DA0[i].field82 = 0;
    }
}

/**
 * @brief Seed the 8 active object slots' draw geometry from their base points.
 *
 * Walks the 8 @ref ObjSlot / @ref DrawPoint pairs. For each slot not yet marked
 * ready (@c D_8005F168[i] == 0) it marks it, then:
 *  - stamps a perturbation byte (@c D_800C3520 indexed by the VSync phase
 *    @c D_8005F154 + slot) into @c field86 / @c field87;
 *  - derives the draw-point's two corner offsets: @c field8/A/C = base minus a
 *    table-perturbed 0x80 bias (X and Y perturbed by the table at phases +i and
 *    +i+8; Z a flat +0x40), and @c field10/12/14 = base plus 0/0/0x80;
 *  - replicates the base (x, y, z) into all 8 entries of both vertex arrays
 *    @c va / @c vb;
 *  - resets the slot's tick pair (@c field80 = 0x10 + 2*slot, @c field82 = 0).
 *
 * @note The unused @c frameSlot local reserves the 0x20 stack frame the
 *       original build allocated (its scratch was register-allocated away).
 */
void func_800A4758(void) {
    struct { s32 w[8]; } frameSlot;
    s32 i, j;

    for (i = 0; i < 8; i++) {
        if (D_8005F168[i] == 0) {
            D_8005F168[i] = 1;
            D_800C6DA0[i].field86 = D_800C3520[(D_8005F154 + i) & 0xFF];
            D_800C6DA0[i].field87 = D_800C3520[(D_8005F154 + i) & 0xFF];
            D_800706A0[i].field8 = (D_800706A0[i].x + D_800C3520[(D_8005F154 + i) & 0xFF]) - 0x80;
            D_800706A0[i].fieldA = (D_800706A0[i].y + D_800C3520[(D_8005F154 + i + 8) & 0xFF]) - 0x80;
            D_800706A0[i].fieldC = D_800706A0[i].z + 0x40;
            D_800706A0[i].field10 = D_800706A0[i].x;
            D_800706A0[i].field12 = D_800706A0[i].y;
            D_800706A0[i].field14 = D_800706A0[i].z + 0x80;
            for (j = 0; j < 8; j++) {
                D_800C6DA0[i].va[j].x = D_800C6DA0[i].vb[j].x = D_800706A0[i].x;
                D_800C6DA0[i].va[j].y = D_800C6DA0[i].vb[j].y = D_800706A0[i].y;
                D_800C6DA0[i].va[j].z = D_800C6DA0[i].vb[j].z = D_800706A0[i].z;
            }
            D_800C6DA0[i].field80 = 0x10 + i * 2;
            D_800C6DA0[i].field82 = 0;
        }
    }
}

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

/**
 * @brief Rebuild one shimmer object's two corner vertices for this tick.
 *
 * Re-seeds the slot pool (@c func_800A4758), then stages the object's current
 * mid-point in the scratchpad: once the tick counter @c field82 has passed
 * @c field80 the draw-point's settled position (@c field10 / @c field12 /
 * @c field14) is used directly, otherwise the point is interpolated twice with
 * @c func_800A4910 — base to first corner and first corner to second, both at
 * @c field82 / @c field80 — and the two results interpolated again.
 *
 * The staged mid-point is then spread into the slot's two corner arrays at
 * index @c field82 & 7: a cosine/sine pair sampled at @c field86 (scaled by
 * @c >> 8) is scaled by the cosine of the tick angle @c (field82 * 8) & 0xF8
 * and applied as @c >> 12, added on one corner and subtracted on the other, so
 * the pair straddles the mid-point along the tick direction. Z is copied
 * unchanged to both.
 *
 * @param slot Object slot to update.
 * @param dp   Draw point holding the object's base and corner positions.
 */
void func_800A4934(ObjSlot *slot, DrawPoint *dp) {
    ObjVertex *mid = (ObjVertex *)getScratchAddr(0);
    ObjVertex *end = (ObjVertex *)getScratchAddr(2);
    s32 idx;

    func_800A4758();
    idx = slot->field82 & 7;
    if (slot->field80 < (s16)slot->field82) {
        mid->x = dp->field10;
        mid->y = dp->field12;
        mid->z = dp->field14;
    } else {
        mid->x = func_800A4910((s16)dp->x, (s16)dp->field8, slot->field80, (s16)slot->field82);
        mid->y = func_800A4910((s16)dp->y, (s16)dp->fieldA, slot->field80, (s16)slot->field82);
        mid->z = func_800A4910((s16)dp->z, (s16)dp->fieldC, slot->field80, (s16)slot->field82);
        end->x = func_800A4910((s16)dp->field8, (s16)dp->field10, slot->field80, (s16)slot->field82);
        end->y = func_800A4910((s16)dp->fieldA, (s16)dp->field12, slot->field80, (s16)slot->field82);
        end->z = func_800A4910((s16)dp->fieldC, (s16)dp->field14, slot->field80, (s16)slot->field82);
        mid->x = func_800A4910((s16)mid->x, (s16)end->x, slot->field80, (s16)slot->field82);
        mid->y = func_800A4910((s16)mid->y, (s16)end->y, slot->field80, (s16)slot->field82);
        mid->z = func_800A4910((s16)mid->z, (s16)end->z, slot->field80, (s16)slot->field82);
    }

    slot->vb[idx].x = mid->x - ((func_8009D234(slot->field86) >> 8) *
                                func_8009D234(((u8)slot->field82 << 3) & 0xF8) >> 12);
    slot->vb[idx].y = mid->y + ((func_8009D254(slot->field86) >> 8) *
                                func_8009D234(((u8)slot->field82 << 3) & 0xF8) >> 12);
    slot->vb[idx].z = mid->z;
    slot->va[idx].x = mid->x + ((func_8009D234(slot->field86) >> 8) *
                                func_8009D234(((u8)slot->field82 << 3) & 0xF8) >> 12);
    slot->va[idx].y = mid->y - ((func_8009D254(slot->field86) >> 8) *
                                func_8009D234(((u8)slot->field82 << 3) & 0xF8) >> 12);
    slot->va[idx].z = mid->z;
}

/**
 * @brief Draws one shimmer object's four-segment trailing ribbon.
 *
 * The slot keeps two 8-entry ring buffers of corner vertices, @c va and @c vb
 * (the left and right edge of the ribbon), with @c field82 as the write cursor.
 * Starting at the newest entry this walks six entries back through the ring and
 * projects the twelve corners in four @c func_80040E14 calls, three points per
 * call, producing a zigzag @c va[i], @c vb[i], @c va[i-1], @c vb[i-1], ... that
 * reads as a ribbon when stroked.
 *
 * Those twelve points are stroked as five overlapping four-point @c LINE_G4
 * strips, each sharing its first two points with the previous one. Points a
 * call does not write are copied over from the neighbouring strip, so only the
 * projections cost GTE time.
 *
 * Each call's OTZ links that block's strips into the ordering table at their
 * own depth, with a tpage command of their own, so the ribbon sorts correctly
 * against the rest of the scene even when it spans a large depth range. A
 * strip whose OTZ is beyond @c 0x1000 is simply not linked.
 *
 * The head strip is white; the rest fade along the trail through the five RGB
 * triples of @c D_800C3720, selected by @c D_80070657.
 *
 * @param slot  Shimmer object slot holding the two corner ring buffers.
 * @param ot    Ordering table to link the strips into.
 * @param line0 The slot's five @c LINE_G4 strips.
 * @param tp    The slot's four tpage commands, one per block.
 *
 * @note @c line3 and @c line4 are linked as @c &line2[1] / @c &line3[1] rather
 *       than by name: cse rewrites a bare strip pointer used as an address into
 *       whichever equivalent base it has already hashed, and only these
 *       spellings keep the previous strip as that base.
 * @note The @c line0 bump pair is load-bearing. gcc 2.7.2 double-counts
 *       @c reg_live_length for a parameter that is live-in and never
 *       reassigned, which halves its priority in @c allocno_compare and costs
 *       @c line0 the first callee-saved register. A second assignment restores
 *       the true count; the pair folds away before the prologue is emitted.
 */
void func_800A4C14(ObjSlot *slot, u32 *ot, LINE_G4 *line0, DR_TPAGE *tp) {
    LINE_G4 *line1;
    LINE_G4 *line2;
    LINE_G4 *line3;
    LINE_G4 *line4;
    s32 pal;
    s32 otz;
    s32 i;
    s32 p;
    s32 flag;

    line0++;
    line0--;
    i = slot->field82 & 7;
    pal = D_80070657 * 16;

    otz = func_80040E14(&slot->va[i], &slot->vb[i], &slot->va[(i - 1) & 7],
                        (s32 *)&line0->x0, (s32 *)&line0->x1, (s32 *)&line0->x2, &p, &flag);

    line1 = &line0[1];
    line2 = &line0[2];
    line3 = &line0[3];
    line4 = &line0[4];
    if (otz < 0x1000) {
        line0->r1 = line0->g1 = line0->b1 = 0x80;
        line0->r0 = line0->g0 = line0->b0 = 0x80;
        addPrim(&ot[otz], line0);
        addPrim(&ot[otz], tp);
        tp++;
    }

    line1->x0 = line0->x2;
    line1->y0 = line0->y2;

    otz = func_80040E14(&slot->vb[(i - 1) & 7], &slot->va[(i - 2) & 7], &slot->vb[(i - 2) & 7],
                        (s32 *)&line1->x1, (s32 *)&line1->x2, (s32 *)&line1->x3, &p, &flag);
    if (otz < 0x1000) {
        line0->r2 = line0->r3 = line1->r0 = line1->r1 = D_800C3720[pal];
        line0->g2 = line0->g3 = line1->g0 = line1->g1 = D_800C3720[pal + 1];
        line0->b2 = line0->b3 = line1->b0 = line1->b1 = D_800C3720[pal + 2];
        line1->r2 = line1->r3 = line2->r0 = line2->r1 = D_800C3720[pal + 3];
        line1->g2 = line1->g3 = line2->g0 = line2->g1 = D_800C3720[pal + 4];
        line1->b2 = line1->b3 = line2->b0 = line2->b1 = D_800C3720[pal + 5];
        addPrim(&ot[otz], line1);
        addPrim(&ot[otz], line2);
        addPrim(&ot[otz], tp);
        tp++;
    }

    line0->x3 = line1->x1;
    line0->y3 = line1->y1;
    line2->x0 = line1->x2;
    line2->y0 = line1->y2;
    line2->x1 = line1->x3;
    line2->y1 = line1->y3;

    otz = func_80040E14(&slot->va[(i - 3) & 7], &slot->vb[(i - 3) & 7], &slot->va[(i - 4) & 7],
                        (s32 *)&line3->x0, (s32 *)&line3->x1, (s32 *)&line3->x2, &p, &flag);
    if (otz < 0x1000) {
        line2->r2 = line2->r3 = line3->r0 = line3->r1 = D_800C3726[pal];
        line2->g2 = line2->g3 = line3->g0 = line3->g1 = D_800C3726[pal + 1];
        line2->b2 = line2->b3 = line3->b0 = line3->b1 = D_800C3726[pal + 2];
        addPrim(&ot[otz], &line2[1]);
        addPrim(&ot[otz], tp);
        tp++;
    }

    line2->x2 = line3->x0;
    line2->y2 = line3->y0;
    line2->x3 = line3->x1;
    line2->y3 = line3->y1;
    line4->x0 = line3->x2;
    line4->y0 = line3->y2;

    otz = func_80040E14(&slot->vb[(i - 4) & 7], &slot->va[(i - 5) & 7], &slot->vb[(i - 5) & 7],
                        (s32 *)&line4->x1, (s32 *)&line4->x2, (s32 *)&line4->x3, &p, &flag);
    if (otz < 0x1000) {
        line3->r2 = line3->r3 = line4->r0 = line4->r1 = D_800C3729[pal];
        line3->g2 = line3->g3 = line4->g0 = line4->g1 = D_800C3729[pal + 1];
        line3->b2 = line3->b3 = line4->b0 = line4->b1 = D_800C3729[pal + 2];
        line4->r2 = line4->r3 = D_800C3729[pal + 3];
        line4->g2 = line4->g3 = D_800C3729[pal + 4];
        line4->b2 = line4->b3 = D_800C3729[pal + 5];
        addPrim(&ot[otz], &line3[1]);
        addPrim(&ot[otz], tp);
        tp++;
    }

    line3->x3 = line4->x1;
    line3->y3 = line4->y1;
}

/**
 * @brief 8-iteration script-dispatch loop with per-slot flag-driven
 *        callbacks and a tick counter that auto-clears.
 *
 * Sets the GTE rotation/translation matrix from @p m (guarded by
 * @c func_8003FEE4 / @c func_8003FF88), then iterates @c i in @c [0,8) over
 * four parallel arrays: @c D_800C6DA0 (@ref ObjSlot, stride 0x88),
 * @c D_800706A0 (@ref DrawPoint, stride 0x18), @p tpages (stride 0x20), and
 * @p prims (stride 0xB4). When the per-slot flag @c D_8005F168[i] is
 * non-zero, runs @c func_800A4934 on the slot, then conditionally
 * @c func_800A4C14 (when the flag is @c 2, or it is @c 1 and
 * @c D_8005F122 @c == @c 1), then bumps @c ObjSlot.field82 and clears the
 * flag once it exceeds @c field80 + 4.
 *
 * @param m      GTE rotation/translation matrix.
 * @param ot     Ordering table the ribbons are linked into.
 * @param prims  Per-slot ribbon strips.
 * @param tpages Per-slot ribbon tpage commands.
 */
void func_800A5224(MATRIX *m, u32 *ot, FieldRibbonPrims *prims,
                   FieldRibbonTPages *tpages) {
    s32 i;

    func_8003FEE4();
    SetRotMatrix(m);
    SetTransMatrix(m);
    for (i = 0; i < 8; i++) {
        if (D_8005F168[i] != 0) {
            u8 flag;
            s16 tick;
            func_800A4934(&D_800C6DA0[i], &D_800706A0[i]);
            flag = D_8005F168[i];
            if (flag == 2 || (flag == 1 && D_8005F122 == flag)) {
                func_800A4C14(&D_800C6DA0[i], ot, prims[i].lines, tpages[i].tpages);
            }
            tick = D_800C6DA0[i].field82 + 1;
            D_800C6DA0[i].field82 = tick;
            if (tick > D_800C6DA0[i].field80 + 4) {
                D_8005F168[i] = 0;
            }
        }
    }
    func_8003FF88();
}

/**
 * @brief Reset the ordering table and link the double-buffer's clear-tile
 *        primitives, tinted (@p r, @p g, @p b) and faded by @c dialogTimer.
 *
 * The brightness-scaled twin of @ref func_800A553C: it clears the active
 * buffer's ordering table via @c ClearOTagR, then writes each fill-color
 * component scaled by the current dialog fade level
 * (@c dialogTimer, 0..256) into that buffer's clear @ref TILE, and finally
 * prepends both the tile and its preceding @ref DR_MODE primitive (12 bytes
 * before the tile) to the caller-supplied ordering table @p ot.
 *
 * @param ot Ordering-table slot to link the two clear primitives into.
 * @param r  Fill red   — scaled to @c r*dialogTimer/256, low byte to TILE @c r0.
 * @param g  Fill green — scaled to @c g*dialogTimer/256, low byte to TILE @c g0.
 * @param b  Fill blue  — scaled to @c b*dialogTimer/256, low byte to TILE @c b0.
 *
 * @note @c g_bufferIndex is @c volatile, so every subscript re-reads it; the
 *       @c dialogTimer read is likewise forced through a @c volatile pointer to
 *       reload each component (its fade value can change between frames).
 *       The first store caches the tile index in @c idx so gcc emits the index
 *       before the shared @c g_clearTiles base, matching the original schedule
 *       in the multiply's delay window; @c g0 / @c b0 recompute it inline (each
 *       is its own @c volatile @c g_bufferIndex read).
 */
void func_800A5360(u32 *ot, s16 r, s16 g, s16 b) {
    ClearOTagR(&g_orderingTablePtrs[(s16)g_bufferIndex], 1);
    {
        volatile SystemState *sys = &D_800704A8;
        s32 idx = (s16)g_bufferIndex * 2;
        g_clearTiles[idx].r0 = (s16)sys->dialogTimer * r / 256;
        g_clearTiles[(s16)g_bufferIndex * 2].g0 = (s16)sys->dialogTimer * g / 256;
        g_clearTiles[(s16)g_bufferIndex * 2].b0 = (s16)sys->dialogTimer * b / 256;
    }
    addPrim(ot, &g_clearTiles[(s16)g_bufferIndex * 2]);
    addPrim(ot, (DR_MODE *)&g_clearTiles[(s16)g_bufferIndex * 2] - 1);
}

/**
 * @brief Reset the current frame's ordering table and link the double-buffer's
 *        clear-tile primitives, tinted (@p r, @p g, @p b), into @p ot.
 *
 * A generalized sibling of @c BuildPrimList: it clears the active buffer's
 * ordering table via @c ClearOTagR, writes the fill color into that buffer's
 * clear @ref TILE, then prepends both the tile and its preceding @ref DR_MODE
 * primitive (12 bytes before the tile) to the caller-supplied ordering table
 * @p ot. @c g_bufferIndex is @c volatile, so each subscript re-reads it.
 *
 * @param ot Ordering-table slot to link the two clear primitives into.
 * @param r  Fill red   — low byte stored to TILE @c r0.
 * @param g  Fill green — low byte stored to TILE @c g0.
 * @param b  Fill blue  — low byte stored to TILE @c b0.
 *
 * @note @p r / @p g / @p b are @c s16 because the sole caller (@c func_800A5788)
 *       feeds them the signed fade-lerp results; only the low byte reaches each
 *       TILE color component.
 */
void func_800A553C(u32 *ot, s16 r, s16 g, s16 b) {
    ClearOTagR(&g_orderingTablePtrs[(s16)g_bufferIndex], 1);
    g_clearTiles[(s16)g_bufferIndex * 2].r0 = r;
    g_clearTiles[(s16)g_bufferIndex * 2].g0 = g;
    g_clearTiles[(s16)g_bufferIndex * 2].b0 = b;
    addPrim(ot, &g_clearTiles[(s16)g_bufferIndex * 2]);
    addPrim(ot, (DR_MODE *)&g_clearTiles[(s16)g_bufferIndex * 2] - 1);
}

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

/**
 * @brief Companion of @c func_800A5700 — advances the dialog-pos animation
 *        and dispatches the per-frame visual update.
 *
 * Increments @c dialogTimer; once it catches up to @c dialogCount, snapshots
 * the destination triple (@c field_0x11A/11C/11E) into the current triple
 * (@c field_0x114/116/118) and resets @c dialogTimer to @c dialogCount (the
 * snapshot is what subsequent ticks lerp back from). Each tick then runs the
 * three-axis safe-lerp via @c func_800A5748 and stores the result back to
 * @c field_0x10E/110/112, finally calling @c func_800A553C with the new
 * (x, y, z) tuple.
 *
 * @param a0 Opaque pointer forwarded as @c func_800A553C's first arg.
 */
void func_800A5788(s32 a0) {
    SystemState *sys = &D_800704A8;

    sys->unk1A1 = 0;
    sys->dialogTimer++;
    if ((s16)*(volatile u16 *)&sys->dialogTimer >= (s16)*(volatile u16 *)&sys->dialogCount) {
        sys->field_0x114 = sys->field_0x11A;
        sys->field_0x116 = sys->field_0x11C;
        sys->field_0x118 = sys->field_0x11E;
        sys->dialogTimer = *(volatile u16 *)&sys->dialogCount;
    }
    sys->field_0x10E = func_800A5748((s16)sys->field_0x114, (s16)sys->field_0x11A,
                                     (s16)*(volatile u16 *)&sys->dialogTimer,
                                     (s16)*(volatile u16 *)&sys->dialogCount);
    sys->field_0x110 = func_800A5748((s16)sys->field_0x116, (s16)sys->field_0x11C,
                                     (s16)*(volatile u16 *)&sys->dialogTimer,
                                     (s16)*(volatile u16 *)&sys->dialogCount);
    func_800A553C(a0, (s16)sys->field_0x10E, (s16)sys->field_0x110,
                  (s16)(sys->field_0x112 = func_800A5748((s16)sys->field_0x118,
                                                         (s16)sys->field_0x11E,
                                                         (s16)*(volatile u16 *)&sys->dialogTimer,
                                                         (s16)*(volatile u16 *)&sys->dialogCount)));
}

/**
 * @brief Dialog-state dispatcher — runs the per-state handler for the
 *        current @c D_800704A8.dialogState (@c 0..8).
 *
 * Behavior per state:
 *  - @c 0: clear @c unk1A1, @c field_0x114/116/118 (reset)
 *  - @c 1: no-op
 *  - @c 2/3: rate-2 timer tick + setup/teardown, then dispatch via
 *            @c func_800A5360 with the @c field_0x10E/110/112 triplet
 *  - @c 4: latch the global @c D_80070649 sentinel to @c 1
 *  - @c 5/6: rate-1/2 timer tick, then @c func_800A5788 (per-frame anim)
 *  - @c 7/8: clear @c unk1A1, rate-1/2 timer tick, dispatch via
 *            @c func_800A553C with the @c field_0x10E/110/112 triplet
 *
 * @note Stays INCLUDE_ASM because gcc-2.7.2-cdk always emits `.align 3`
 *       (8-byte) before switch jtbls, but jtbl_8009806C is at the
 *       4-byte aligned offset 0x6C in the original binary — a `switch`
 *       decomp would introduce a 4-byte padding gap that cascades
 *       through the rest of field.bin. A computed-goto rewrite matches
 *       byte-perfect but reads as transliterated assembly; keeping the
 *       INCLUDE_ASM until either maspsx grows `.align` translation or
 *       a cleaner C structure is found. Source preserved below for
 *       reference.
 *
 * @verbatim
 * void func_800A5898(s32 a0) {
 *     SystemState *sys = &D_800704A8;
 *     switch ((s16)*(volatile u16 *)&sys->dialogState) {
 *         case 0:
 *             D_800704A8.unk1A1 = 0;
 *             D_800704A8.field_0x114 = 0;
 *             D_800704A8.field_0x116 = 0;
 *             D_800704A8.field_0x118 = 0;
 *             break;
 *         case 1:
 *             break;
 *         case 2:
 *             func_800127F8(2);
 *             func_800A5698();
 *             func_800A5360(a0, D_800704A8.field_0x10E, D_800704A8.field_0x110, D_800704A8.field_0x112);
 *             break;
 *         case 3:
 *             func_800127F8(2);
 *             func_800A5700();
 *             func_800A5360(a0, D_800704A8.field_0x10E, D_800704A8.field_0x110, D_800704A8.field_0x112);
 *             break;
 *         case 4:
 *             D_80070649 = 1;
 *             break;
 *         case 7:
 *             D_800704A8.unk1A1 = 0;
 *             func_800127F8(1);
 *             func_800A553C(a0, D_800704A8.field_0x10E, D_800704A8.field_0x110, D_800704A8.field_0x112);
 *             break;
 *         case 8:
 *             D_800704A8.unk1A1 = 0;
 *             func_800127F8(2);
 *             func_800A553C(a0, D_800704A8.field_0x10E, D_800704A8.field_0x110, D_800704A8.field_0x112);
 *             break;
 *         case 5:
 *             func_800127F8(1);
 *             func_800A5788(a0);
 *             break;
 *         case 6:
 *             func_800127F8(2);
 *             func_800A5788(a0);
 *             break;
 *     }
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5898);

/**
 * If D_8005F14A equals 1, calls resetCdDrive. Then clears D_8005F100 and D_8005F14A.
 *
 * @note K&R declarator: the body ignores its argument and callers vary between
 *       passing none (@c func_800A5A20, fe_object5) and one.
 */
void func_800A59D0() {

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

/** @brief Words per streaming-table entry (24-byte stride). */
#define FIELD_STREAM_STRIDE 6

/**
 * @brief Per-frame background preload of the field nearest the player.
 *
 * Snapshots the player position into the scratchpad (whole units), then — while
 * the map is not suppressed by @c D_800704BD — scans the 12 event-queue entries
 * for the armed one (@c counter != @c 0x7FFF) whose trigger-segment start is
 * nearest in XY, recording its @c counter (the destination field id) in
 * @c D_8005F142.
 *
 * The streaming half then runs unless the movie subsystem is busy or the engine
 * is in mode 3 (both abort through @c func_800A59D0), and unless a load is
 * already pending. The destination pointer @c D_8005F104 is floored to
 * @c 0x801A0000, and when the nearest field differs from the one already loaded
 * (@c D_8005F100) and its data still fits below @c 0x801FE000, the previous load
 * is cancelled and a new @c cdRead is issued: field ids from @c 0x49 up are
 * looked up through @c D_800C2568 into the streaming table, lower ids use the
 * fixed descriptor @c D_800974D0. @c D_8005F14A is left at 1 to mark the read
 * in flight.
 *
 * @param self    Player entity, read for its 20.12 world position.
 * @param entries Event-queue entry array (12 slots).
 */
void func_800A5A20(Eline *self, EventEntry *entries) {
    Vec3i *scratch = (Vec3i *)getScratchAddr(0);
    s32 best;
    s32 i;
    s32 dx;
    s32 dy;
    s32 d;
    s32 idx;
    u16 id;

    if (D_8005F14A == 1) {
        if (func_800393C8() == 0) {
            D_8005F14A = 2;
        }
    }
    best = 0x7FFFFFFF;
    scratch->x = self->posX >> 12;
    scratch->y = self->posY >> 12;
    scratch->z = self->posZ >> 12;

    if (D_800704BD == 0) {
        for (i = 0; i < 12; i++, entries++) {
            id = entries->counter;
            if (id != 0x7FFF) {
                dx = entries->x0 - scratch->x;
                dy = entries->y0 - scratch->y;
                d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                    D_8005F142 = id;
                }
            }
        }
    }

    if (func_800BE264() != 0 || D_800704A8.mode == 3) {
        func_800A59D0();
        return;
    }
    if (func_800393C8() != 0 && D_8005F14A == 0) {
        return;
    }
    if ((u32)D_8005F104 <= 0x8019FFFF) {
        D_8005F104 = 0x801A0000;
    }
    if (D_8005F100 == D_8005F142) {
        return;
    }
    if (D_800C0904[D_800C2568[D_8005F142] * FIELD_STREAM_STRIDE] < 0x801FE000 - D_8005F104) {
        func_800A59D0();
        D_8005F100 = D_8005F142;
        if (D_8005F142 >= 0x49) {
            idx = D_800C2568[D_8005F142];
            cdRead(D_800C0904[idx * FIELD_STREAM_STRIDE - 1],
                   D_800C0904[idx * FIELD_STREAM_STRIDE], (u8 *)D_8005F104, NULL);
        } else {
            cdRead(D_800974D0[0].sector, D_800974D0[0].size, (u8 *)D_8005F104, NULL);
        }
        D_8005F14A = 1;
    }
}

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
s32 func_800A5C9C(void) {
    D_8005F151++;
    if (D_8005F151 == 0) {
        D_8005F150 += 13;
    }
    return (u8)(D_800C3520[D_8005F151] + D_8005F150);
}

/**
 * Increments the global byte D_8005F103 and returns the value at
 * D_800C3520[D_8005F103].
 *
 * @return The byte from the D_800C3520 lookup table.
 */
s32 func_800A5CF8(void) {

    D_8005F103++;
    return D_800C3520[D_8005F103];
}

/**
 * @brief Per-step random-encounter accumulator and battle trigger.
 *
 * Runs the classic FF8 field encounter formula. Bails when: the engine mode
 * is 1 or 7, @c func_800BE274() reports activity, @c g_fieldVars->fieldCF is
 * set, the dialog state is 2/3/4, encounters are disabled (@c D_8005F116),
 * or movement flag bit 3 is set. Otherwise adds the field's step-rate byte
 * (halved when movement flag bit 2 is set) to the step accumulator
 * @c D_8005F164; when it passes 0x100 the accumulator wraps (@c &= 0xFF) and
 * the player entity's danger halfword (offset 0x1FE, read as unsigned)
 * divided by 1348 is added to the battle chance @c D_8005F0FE. A random
 * byte below the battle chance triggers an encounter: engine mode 3,
 * chance reset, @c D_8005F130 set, and a formation picked from the field's
 * 4-entry table with thresholds 0x80/0xC0/0xF0 — preferring the first
 * bucket whose formation differs from the previous one (@c D_8005F120),
 * falling back to entry 3 unconditionally.
 *
 * @note The three dialog-state reads go through a volatile cast — the
 *       original re-reads the halfword for each compare.
 * @note The first formation store writes @c D_800704A8.counter through the
 *       struct; the others use the alias symbol @c D_800704AA (same word,
 *       0x800704AA) — both spellings exist in the original.
 * @note @c savedChannel (0x1FE) is read here as the per-entity danger value
 *       via a @c (u16) view; the message code uses the same slot as its
 *       saved-channel word.
 */
void func_800A5D28(void) {
    u8 *rate;
    s32 r;
    u16 *fm;

    if (D_800704A8.mode == 1) {
        return;
    }
    if (D_800704A8.mode == 7) {
        return;
    }
    r = func_800BE274();
    if (r != 0) {
        return;
    }
    if (g_fieldVars->fieldCF != 0) {
        return;
    }
    if ((s16)*(volatile u16 *)&D_800704A8.dialogState == 4) {
        return;
    }
    if ((s16)*(volatile u16 *)&D_800704A8.dialogState == 3) {
        return;
    }
    if ((s16)*(volatile u16 *)&D_800704A8.dialogState == 2) {
        return;
    }
    if (D_8005F116 == 1) {
        return;
    }
    if (D_80078DF8 & 8) {
        return;
    }
    rate = *D_800C71F4;
    if (D_80078DF8 & 4) {
        D_8005F164 += *rate >> 1;
    } else {
        D_8005F164 += *rate;
    }
    if (D_8005F164 < 0x101) {
        return;
    }
    D_8005F164 &= 0xFF;
    D_8005F0FE += (s16)(u16)D_80085224[D_8005F148].savedChannel / 1348;
    if ((u8)func_800A5C9C() < D_8005F0FE) {
        D_800704A8.mode = 3;
        D_8005F0FE = 0;
        D_8005F130 = 1;
        r = func_800A5CF8();
        fm = *D_800C720C;
        if ((u8)r < 0x80 && D_8005F120 != (s16)fm[0]) {
            D_800704A8.counter = fm[0];
        } else if ((u8)r < 0xC0 && D_8005F120 != (s16)fm[1]) {
            D_800704AA = fm[1];
        } else if ((u8)r < 0xF0 && D_8005F120 != (s16)fm[2]) {
            D_800704AA = fm[2];
        } else {
            D_800704AA = fm[3];
        }
        D_8005F120 = D_800704AA;
    }
}

/**
 * @brief Per-selector dispatcher — sets or clears a @ref FieldEntityC
 *        trigger byte indexed by @c entry->status, gated by a per-selector
 *        flag in @c D_80070628[0..5].
 *
 * @param entry  Per-frame entry (16-byte stride; @c status field at 0xC).
 * @param sel    Selector @c 0..5 (any other value is a no-op returning 0).
 * @return @c 1 when the gate passes (state-change occurred); @c 0 otherwise.
 *
 * Behavior per selector:
 *  - Even (@c 0/2/4): if the target's @c activeMarker is set AND the
 *    flag is currently @c 0, latch the flag to @c 1 and (when
 *    @c status < D_80085228) set the target's @c trigger6.
 *  - Odd (@c 1/3/5): if the target's @c activeMarker is set AND the
 *    flag is currently @c 1, clear the flag and (when
 *    @c status < D_80085228) clear the target's @c trigger7.
 *
 * @note Stays INCLUDE_ASM because of a register-allocation difference:
 *       the original masks @c sel into @c v1 (`andi v1, a1, 0xFF`) and
 *       keeps the masked value in @c v1 through the case bodies, while
 *       gcc with this C body allocates the mask in @c a1 in-place
 *       (`andi a1, a1, 0xFF`), which cascades through 10 bytes of
 *       register-field bits in the function. Source preserved below
 *       for reference; the alignment is not the issue here (jtbl_80098090
 *       at 0x90 is naturally 8-byte aligned), only the reg-alloc.
 *
 * @verbatim
 * s32 func_800A5FA4(func_800A62EC_entry *entry, s32 sel) {
 *     u8 *flag_arr = D_80070628;
 *     s32 result = 0;
 *     u32 s = sel & 0xFF;
 *
 *     if (s < 6) {
 *         switch (s) {
 *             case 0:
 *             case 2:
 *             case 4:
 *                 if (D_80085384[entry->status].activeMarker == 1 && flag_arr[s] == 0) {
 *                     flag_arr[s] = 1;
 *                     result = 1;
 *                     if (entry->status < D_80085228) {
 *                         D_80085384[entry->status].trigger6 = 1;
 *                     }
 *                 }
 *                 break;
 *             case 1:
 *             case 3:
 *             case 5:
 *                 if (D_80085384[entry->status].activeMarker == 1 && flag_arr[s] == 1) {
 *                     flag_arr[s] = 0;
 *                     result = 1;
 *                     if (entry->status < D_80085228) {
 *                         D_80085384[entry->status].trigger7 = 0;
 *                     }
 *                 }
 *                 break;
 *         }
 *     }
 *     return result;
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object1", func_800A5FA4);

/**
 * @brief Scan the 12-entry eline segment table and fire per-segment triggers
 *        based on proximity, facing angle, and edge orientation to @p eline.
 *
 * Stages @p eline 's world position (@c posX/Y/Z >> 12) into the scratchpad
 * at @c getScratchAddr(0), then for each non-empty segment (@c marker != 0xFF):
 *  - Runs @c func_8009A2BC (which projects the segment and returns a squared
 *    distance, also writing the projected point to @c getScratchAddr(8)).
 *  - If the point is within @c eline->radius²: the segment fires
 *    (@c func_800A5FA4 with the segment @c type) when either the projected
 *    point coincides with @p eline, or the facing angle from @c func_8009A0E8
 *    lies within a @c +/-64 window of @c eline->unk23F.
 *  - Otherwise (out of range): segments with @c type >= 4 are gated by a
 *    cross-product orientation test against the segment edge, then
 *    @c type 2/4 fire with flag 1 and @c type 3/5 fire with flag 0.
 *
 * @param eline The querying eline entity.
 * @param segs  The 12-entry, 16-byte-stride segment table.
 *
 * @note The empty @c do{}while(0) is a scheduling barrier: it keeps gcc 2.7.2
 *       from reordering the @c posY store ahead of the @c posX store while
 *       staging the scratchpad, matching the original prologue schedule.
 */
void func_800A6100(Eline *eline, FieldLineTrigger *segs) {
    s32 *p = getScratchAddr(0);
    s32 *q;
    FieldLineTrigger *seg;
    s32 i;
    s32 dist;

    seg = segs;
    q = getScratchAddr(8);
    p[0] = eline->posX >> 12;
    do { } while (0);
    p[1] = eline->posY >> 12;
    p[2] = eline->posZ >> 12;

    for (i = 0; i < 12; i++, seg++) {
        if (seg->marker == 0xFF) {
            continue;
        }
        dist = func_8009A2BC(seg, p, q);
        if (dist != -1 && dist < eline->radius * eline->radius) {
            if (p[0] == q[0] && p[1] == q[1]) {
                func_800A5FA4(seg, seg->type);
            } else if ((((func_8009A0E8(p, q, &dist) & 0xFF) - eline->unk23F + 0x40) & 0xFF) < 0x80) {
                func_800A5FA4(seg, seg->type);
            }
        } else {
            if (seg->type >= 4) {
                s32 dx = seg->x1 - seg->x0;
                s32 dy = seg->y1 - seg->y0;
                if (dx * (p[1] - seg->y0) - dy * (p[0] - seg->x0) > 0) {
                    continue;
                }
            }
            if (seg->type == 2 || seg->type == 4) {
                func_800A5FA4(seg, 1);
            }
            if (seg->type == 3 || seg->type == 5) {
                func_800A5FA4(seg, 0);
            }
        }
    }
}

/**
 * @brief Per-frame dispatch over 12 entries — call @c func_800A5FA4
 *        with an even/odd flag based on the entry's @c mode.
 *
 * Iterates 12 16-byte entries. For each entry where @c active != @c 0xFF,
 * switches on @c mode (0..5) and calls @c func_800A5FA4(entry, flag)
 * where @c flag = 1 for even modes (0/2/4) and 0 for odd modes (1/3/5).
 *
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x0C];
    /* 0x0C */ u8 status;       /**< @c 0xFF means slot is unused (skip). */
    /* 0x0D */ u8 padD;
    /* 0x0E */ u8 mode;         /**< Even (0/2/4) → flag=1, odd (1/3/5) → flag=0. */
    /* 0x0F */ u8 padF;
} func_800A62EC_entry;  /* 0x10 = 16 bytes */

void func_800A62EC(func_800A62EC_entry *entries) {
    s32 i;
    func_800A62EC_entry *p;
    p = entries;
    i = 0;
    do {
        if (p->status != 0xFF) {
            switch (p->mode) {
                case 0:
                case 2:
                case 4:
                    func_800A5FA4(p, 1);
                    break;
                case 1:
                case 3:
                case 5:
                    func_800A5FA4(p, 0);
                    break;
            }
        }
        p++;
        i++;
    } while (i < 12);
}

/* PsyQ 4.3 island (func_800A63AC..func_800AA8A0) lives in fe_object1b.c. */
