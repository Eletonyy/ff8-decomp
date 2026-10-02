#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"
#include "gamestate.h"
#include "thread.h"
#include "btl_transition.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 224

/** The strips are a 128-pixel band of the saved picture, starting 32 pixels in. */
#define STRIP_COUNT 128
#define STRIP_LEFT 32

/** Step at which the strips stop and the wipe to black starts. */
#define WIPE_START 48
/** Step at which the wipe has finished and the battle takes over. */
#define TRANSITION_END 80

/** Where in VRAM the picture on screen when the battle started is kept. */
#define SNAPSHOT_X 384
#define SNAPSHOT_Y 256

/** Sets the bit of the status register that lets a thread use the GTE. */
#define STATUS_GTE_ENABLE 0x40000000

/** Primitive codes with the semi-transparency bit set, as they sit in the top byte of the colour word. */
#define CODE_FT4_BLENDED (0x2E << 24)
#define CODE_G4_BLENDED (0x3A << 24)

/** Ordering table entries, in the order they are drawn. */
enum {
    LAYER_GLOW = 3,
    LAYER_STRIPS = 4,
    LAYER_WIPE = 5,
    LAYER_COPY = 6
};

/**
 * @brief Write a primitive's length byte.
 *
 * This is inline assembly because the original was: the target loads the
 * length with @c ori where every constant the compiler loads itself is an
 * @c addiu, and it reloads a spilled pointer before loading the constant,
 * which the compiler only does when both belong to one statement.
 */
#define setPrimLen(p, n) \
    __asm__ volatile ("ori $2, $0, " #n "; sb $2, 3(%0)" : : "r"(p) : "$2")

/** One ordering table per display buffer. */
typedef u32 TransitionOt[8];
/** One primitive buffer per display buffer. */
typedef u8 TransitionPrims[0x8000];

/** @brief One vertical strip of the saved picture. Positions, speeds and accelerations are 16.16 fixed point. */
typedef struct {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 vx;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 vz;
    /* 0x18 */ s32 ax;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 az;
    /* 0x24 */ s16 brightness; /**< Added to the step count to give the strip's brightness. */
    /* 0x26 */ s16 unk26;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} TransitionStrip;

/** @brief One horizontal bar of the wipe to black. */
typedef struct {
    /* 0x00 */ s32 length; /**< 16.16 fixed point. */
    /* 0x04 */ s32 speed;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s16 r; /**< How much of each colour the bar takes away, per unit of strength. */
    /* 0x0E */ s16 g;
    /* 0x10 */ s16 b;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} TransitionBar;

/** @brief State of the normal battle transition. */
typedef struct {
    /* 0x00 */ s16 step;
    /* 0x04 */ TransitionStrip *strips;
    /* 0x08 */ TransitionBar *bars;
} TransitionState;

/** @brief Scratch block the transition code shares. */
typedef struct {
    /* 0x00 */ u8 pad00[0x10];
    /* 0x10 */ SVECTOR angle;
    /* 0x18 */ u8 pad18[0x08];
    /* 0x20 */ MATRIX rot;
    /* 0x40 */ u8 pad40[0x20];
    /* 0x60 */ VECTOR trans;
    /* 0x70 */ u8 pad70[0x10];
    /* 0x80 */ void *primPtr;
    /* 0x84 */ u8 pad84[0x1C];
    /* 0xA0 */ SVECTOR vertex;
    /* 0xA8 */ u8 padA8[0x48];
    /* 0xF0 */ s32 arg0; /**< Arguments and result of func_80026ADC; also where RotTransPers puts its outputs. */
    /* 0xF4 */ void *arg1;
    /* 0xF8 */ s32 arg2;
    /* 0xFC */ s32 result;
} TransitionWork;

#define TRANSITION_STATE ((TransitionState *)0x801F0000)
#define TRANSITION_WORK ((TransitionWork *)0x801F5000)
#define TRANSITION_OTS ((TransitionOt *)0x801F6000)
#define TRANSITION_PRIMS ((TransitionPrims *)0x801DE000)

extern s8 D_8005F17D;
extern u8 D_8005F17C;
extern u8 D_8005F180;
extern s32 D_80052914[];
extern DR_MOVE D_80082CA0[];
extern DRAWENV D_80082CD0[];
extern DISPENV D_80082D90[];
extern RECT D_8005EC2C;
extern RECT D_8005EC34;

INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80023D60);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80024064);


/**
 * @brief Draw one step of the normal battle transition.
 *
 * The transition runs in its own thread while the battle loads, one step every
 * second VSync. The step count goes up before it is used, so it runs from 1 to 80:
 * - Steps 1 to 47 redraw a band of the saved picture as 128 one-pixel strips
 *   that fly apart and brighten, between four glowing quads.
 * - Steps 48 to 80 take the picture away to black with 224 bars that grow in
 *   from one side.
 * - Step 80 also clears @c g_renderMode, closes the thread and blanks the display.
 *
 * @c D_8005F180 mirrors the whole thing left to right.
 *
 * @note The first strip's left edge gets its texture coordinates twice: once
 *       under the mirror test and then again unconditionally. The original is
 *       missing an @c else there.
 */
void func_800242C8(void) {
    /* Never used. They and unused below only hold their places in the stack frame. */
    s32 sxy, depth, flag;
    /* The order of these is the order of the registers and stack slots: the
     * first eight get $s0 to $s7 and the rest live on the stack. */
    register s32 r;
    register s32 q;
    register s32 m;
    register DR_MODE *scratch; /* one pointer for the angle, a RECT and the draw modes: all three sit in $s3 */
    register POLY_FT4 *prim;
    register POLY_FT4 *next;
    register s32 i;
    register TransitionStrip *strip;
    register TransitionState *state;
    register TransitionBar *bar;
    register POLY_G4 *wipe;
    register POLY_G4 *glow;
    register s32 unused;
    register TransitionWork *work;
    register u32 *ot;

    if (D_8005F17D != 0) {
        return;
    }
    D_8005F17D = -1;
    D_8005F17C = 0;

    work = TRANSITION_WORK;
    state = TRANSITION_STATE;

    state->step++;
    ot = TRANSITION_OTS[state->step & 1];

    r = (state->step + 1) & 1;
    PutDispEnv(&D_80082D90[r]);
    PutDrawEnv(&D_80082CD0[r]);

    if (state->step < TRANSITION_END - 2) {
        DrawOTag(ot);
    } else {
        SetDispMask(0);
    }

    ot = TRANSITION_OTS[(state->step + 1) & 1];
    ClearOTag(ot, 8);

    r = getInterruptStatus();
    r |= STATUS_GTE_ENABLE;
    func_80026FD4(r);

    scratch = (DR_MODE *)&work->angle;
    if (D_8005F180 == 0) {
        ((SVECTOR *)scratch)->vy = state->step * 4;
        work->trans.vx = -(SCREEN_WIDTH / 2);
    } else {
        ((SVECTOR *)scratch)->vy = 0x800 - state->step * 4;
        work->trans.vx = SCREEN_WIDTH / 2;
    }
    ((SVECTOR *)scratch)->vx = 0;
    ((SVECTOR *)scratch)->vz = 0;
    RotMatrix(&work->angle, &work->rot);
    SetRotMatrix(&work->rot);
    work->trans.vy = -(SCREEN_HEIGHT / 2);
    work->trans.vz = 512;
    gte_SetTransVector(&work->trans);

    /* Copy the buffer that was just drawn back over the saved picture. The
     * label is not jumped to: it is what puts a nop here, as in the original. */
    if (state->step < 2) {
    } else {
        if (state->step < WIPE_START) {
            r = state->step;
            r &= 1;
            if (r == 0) {
                scratch = (DR_MODE *)&D_8005EC2C;
            } else {
                scratch = (DR_MODE *)&D_8005EC34;
            }
            SetDrawMove(&D_80082CA0[state->step & 1], (RECT *)scratch, SNAPSHOT_X, SNAPSHOT_Y);
            AddPrim(ot + LAYER_COPY, &D_80082CA0[state->step & 1]);
        }
    copy_done:;
    }

    work->primPtr = TRANSITION_PRIMS[state->step & 1];
    work->arg0 = (s32)ot;
    work->arg1 = work->primPtr;
    work->arg2 = 0x808080;
    func_80026ADC();
    work->primPtr = (void *)work->result;

    if (state->step < WIPE_START) {
        prim = work->primPtr;
        strip = state->strips;
        work->trans.vx = 0;
        work->trans.vy = 0;
        *(s32 *)&work->vertex.vx = 0;
        work->vertex.vz = strip->z >> 16;
        RotTransPers(&work->vertex, &work->arg0, &work->result, &work->result);
        *(s32 *)&prim->x0 = work->arg0;
        work->vertex.vy += SCREEN_HEIGHT;
        RotTransPers(&work->vertex, &work->arg0, &work->result, &work->result);
        *(s32 *)&prim->x2 = work->arg0;
        if (D_8005F180 == 0) {
            *(s16 *)&prim->u0 = 0;
            *(s16 *)&prim->u2 = SCREEN_HEIGHT << 8;
        }
        *(s16 *)&prim->u0 = 192;
        *(s16 *)&prim->u2 = (SCREEN_HEIGHT << 8) | 192;

        for (i = 0; i < STRIP_COUNT; i++) {
            next = prim + 1;
            setPrimLen(prim, 9);
            if (D_8005F180 == 0) {
                prim->tpage = getTPage(2, 3, SNAPSHOT_X, SNAPSHOT_Y);
            } else {
                prim->tpage = getTPage(2, 3, SNAPSHOT_X + 128, SNAPSHOT_Y);
            }
            r = state->step + strip->brightness;
            if (r >= 0xFF) r = 0xFF;
            if (r < 0) r = 0;
            r |= r << 8;
            r |= r << 8;
            *(u32 *)&prim->r0 = r | CODE_FT4_BLENDED;

            strip->vx += strip->ax;
            strip->x += strip->vx;
            strip->vz += strip->az;
            strip->z += strip->vz;

            work->vertex.vx = (strip->x >> 16) + i + STRIP_LEFT;
            r = strip->y >> 16;
            work->vertex.vy = r;
            work->vertex.vz = strip->z >> 16;
            RotTransPers(&work->vertex, &work->arg0, &work->result, &work->result);
            *(s32 *)&prim->x1 = *(s32 *)&next->x0 = work->arg0;
            work->vertex.vy += SCREEN_HEIGHT;
            RotTransPers(&work->vertex, &work->arg0, &work->result, &work->result);
            *(s32 *)&prim->x3 = *(s32 *)&next->x2 = work->arg0;

            if (D_8005F180 == 0) {
                prim->u1 = prim->u3 = next->u0 = next->u2 = i + STRIP_LEFT;
            } else {
                /* 160 - i, written as the byte it becomes. */
                prim->u1 = prim->u3 = next->u0 = next->u2 = -96 - i;
            }
            prim->v0 = prim->v1 = 0;
            prim->v2 = prim->v3 = SCREEN_HEIGHT;
            AddPrim(ot + LAYER_STRIPS, prim);
            prim++;
            strip++;
        }
        work->primPtr = prim;

        /* The glow: two quads from the top and bottom edges, then two from the middle. */
        glow = work->primPtr;
        i = state->step / 2;
        setPrimLen(&glow[0], 8);
        setPrimLen(&glow[1], 8);

        r = state->step;
        q = (32 - r) / 2;
        if (q < 4) q = 4;
        q |= (q << 16) | (q << 8);
        m = state->step % 3;
        m = D_80052914[m];
        *(u32 *)&glow[0].r0 = *(u32 *)&glow[1].r0 = (q + m) | CODE_G4_BLENDED;
        *(u32 *)&glow[0].r2 = *(u32 *)&glow[1].r2 = q + m;
        *(u32 *)&glow[0].r1 = *(u32 *)&glow[1].r1 = m;
        *(u32 *)&glow[0].r3 = *(u32 *)&glow[1].r3 = m;

        q = 32 - r;
        if (q < 4) q = 4;
        m = r;
        if (r > 32) m = 32;
        q = q * (q + 1) / 4;
        m = m * (m + 1) / 4;
        glow[0].y0 = glow[0].y1 = 0;
        glow[0].y2 = glow[0].y3 = m;
        glow[0].x0 = glow[0].x2 = STRIP_LEFT - i;
        glow[0].x1 = glow[0].x3 = q + STRIP_LEFT;
        glow[1].y0 = glow[1].y1 = SCREEN_HEIGHT - m;
        glow[1].y2 = glow[1].y3 = SCREEN_HEIGHT;
        glow[1].x0 = glow[1].x2 = STRIP_LEFT - i;
        glow[1].x1 = glow[1].x3 = q + STRIP_LEFT;

        setPrimLen(&glow[2], 8);
        setPrimLen(&glow[3], 8);

        r = state->step;
        q = (32 - r) / 2;
        if (q < 4) q = 4;
        q |= (q << 16) | (q << 8);
        m = state->step % 3;
        m = D_80052914[m];
        *(u32 *)&glow[2].r0 = *(u32 *)&glow[3].r0 = (q + m) | CODE_G4_BLENDED;
        *(u32 *)&glow[2].r2 = *(u32 *)&glow[3].r2 = q + m;
        *(u32 *)&glow[2].r1 = *(u32 *)&glow[3].r1 = m;
        *(u32 *)&glow[2].r3 = *(u32 *)&glow[3].r3 = m;

        q = 32 - r;
        if (q < 4) q = 4;
        m = r;
        if (r > 32) m = 32;
        q = q * (q + 1) / 8;
        m = m * (m + 1) / 8;
        glow[2].y0 = glow[2].y1 = SCREEN_HEIGHT / 2;
        glow[2].y2 = glow[2].y3 = m + SCREEN_HEIGHT / 2;
        glow[2].x0 = glow[2].x2 = STRIP_LEFT - i;
        glow[2].x1 = glow[2].x3 = q + STRIP_LEFT;
        glow[3].y0 = glow[3].y1 = SCREEN_HEIGHT / 2 - m;
        glow[3].y2 = glow[3].y3 = SCREEN_HEIGHT / 2;
        glow[3].x0 = glow[3].x2 = STRIP_LEFT - i;
        glow[3].x1 = glow[3].x3 = q + STRIP_LEFT;

        if (D_8005F180 != 0) {
            glow[0].x0 = SCREEN_WIDTH - glow[0].x0;
            glow[0].x1 = SCREEN_WIDTH - glow[0].x1;
            glow[0].x2 = SCREEN_WIDTH - glow[0].x2;
            glow[0].x3 = SCREEN_WIDTH - glow[0].x3;
            glow[1].x0 = SCREEN_WIDTH - glow[1].x0;
            glow[1].x1 = SCREEN_WIDTH - glow[1].x1;
            glow[1].x2 = SCREEN_WIDTH - glow[1].x2;
            glow[1].x3 = SCREEN_WIDTH - glow[1].x3;
            glow[2].x0 = SCREEN_WIDTH - glow[2].x0;
            glow[2].x1 = SCREEN_WIDTH - glow[2].x1;
            glow[2].x2 = SCREEN_WIDTH - glow[2].x2;
            glow[2].x3 = SCREEN_WIDTH - glow[2].x3;
            glow[3].x0 = SCREEN_WIDTH - glow[3].x0;
            glow[3].x1 = SCREEN_WIDTH - glow[3].x1;
            glow[3].x2 = SCREEN_WIDTH - glow[3].x2;
            glow[3].x3 = SCREEN_WIDTH - glow[3].x3;
        }
        AddPrim(ot + LAYER_GLOW, &glow[0]);
        AddPrim(ot + LAYER_GLOW, &glow[1]);
        AddPrim(ot + LAYER_GLOW, &glow[2]);
        AddPrim(ot + LAYER_GLOW, &glow[3]);

        /* The glow is added to the picture. The tag is stored as one word, length 1. */
        glow += 4;
        scratch = (DR_MODE *)glow;
        scratch->tag = 0x01000000;
        scratch->code[0] = _get_mode(0, 0, getTPage(0, 1, 0, 0));
        scratch->code[1] = 0;
        AddPrim(ot + LAYER_GLOW, scratch);
        work->primPtr = scratch + 1;
    }
/* Not jumped to either; it accounts for the nop between the two halves. */
wipe_start:

    if (state->step < WIPE_START) {
    } else {
        bar = state->bars;
        wipe = work->primPtr;
        /* The strength of the wipe, kept in the translation's unused x. */
        if (state->step >= 64) {
            work->trans.vx = state->step - 63;
        } else {
            work->trans.vx = 1;
        }

        i = 0;
        while (i < SCREEN_HEIGHT) {
            setPrimLen(wipe, 8);
            wipe->y0 = wipe->y1 = i;
            r = bar->length >> 16;
            /* Compared unsigned, so a negative length stops the bar too. */
            if ((u32)r >= SCREEN_WIDTH * 2) {
                bar->speed = 0;
                bar->unk08 = 0;
                r = SCREEN_WIDTH * 2;
            }
            if (D_8005F180 == 0) {
                wipe->x0 = 0;
                wipe->x1 = r;
            } else {
                wipe->x0 = SCREEN_WIDTH;
                r = SCREEN_WIDTH - r;
                wipe->x1 = r;
            }
            wipe->y2 = wipe->y3 = wipe->y0 + 2;
            wipe->x2 = wipe->x0;
            wipe->x3 = wipe->x1;
            *(u32 *)&wipe->r0 = CODE_G4_BLENDED | 0xFFFFFF;

            q = work->trans.vx * bar->r;
            if (q > 0xFF) q = 0xFF;
            r = q;
            q = work->trans.vx * bar->g;
            if (q > 0xFF) q = 0xFF;
            r |= q << 8;
            q = work->trans.vx * bar->b;
            if (q > 0xFF) q = 0xFF;
            r |= q << 16;
            *(u32 *)&wipe->r1 = r;
            *(u32 *)&wipe->r2 = *(u32 *)&wipe->r0 & 0xFFFFFF;
            *(u32 *)&wipe->r3 = *(u32 *)&wipe->r1;
            bar->length += bar->speed;
            AddPrim(ot + LAYER_WIPE, wipe);
            wipe++;
            bar++;
            /* Each bar is taken away from the picture. */
            scratch = (DR_MODE *)wipe;
            scratch->tag = 0x01000000;
            scratch->code[0] = _get_mode(0, 0, getTPage(0, 2, 0, 0));
            scratch->code[1] = 0;
            AddPrim(ot + LAYER_WIPE, scratch);
            scratch++;
            wipe = (POLY_G4 *)scratch;
            i++;
        }
        work->primPtr = wipe;
        if (state->step >= TRANSITION_END) {
            g_renderMode = 0;
            func_80026E20();
            SetDispMask(0);
        }
    /* Another label nothing jumps to, kept for its nop. */
    done:;
    }

    DrawSync(0);
    D_8005F17D = 0;
}
