#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"
#include "psxsdk/libetc.h"
#include "gamestate.h"
#include "main.h"
#include "thread.h"
#include "btl_transition.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 224

/** 128 quads cover the picture's left 160 pixels: the first is the 32-pixel margin, the other 127 are one pixel wide. */
#define STRIP_COUNT 128
#define STRIP_LEFT 32

/** Strip and bar records in the tables: more than the draw step uses (STRIP_COUNT strips, one bar per line). */
#define STRIP_TABLE_SIZE 168
#define BAR_TABLE_SIZE 232

/** Step at which the strips stop and the wipe to black starts. */
#define WIPE_START 48
/** Step at which the wipe has finished and the battle takes over. */
#define TRANSITION_END 80

/** Where in VRAM the picture on screen when the battle started is kept. */
#define SNAPSHOT_X 384
#define SNAPSHOT_Y 256

/** Sets the bit of the status register that lets a thread use the GTE. */
#define STATUS_GTE_ENABLE 0x40000000

/** Distance of the projection plane. The strips start on it, so they first project at their own size. */
#define SCREEN_DISTANCE 512
/** Half a turn, in the GTE's 4096-per-revolution angle units. */
#define HALF_TURN (ONE / 2)
/** Colour word the saved picture is drawn with: its own brightness. */
#define SNAPSHOT_COLOR 0x808080
/** Step at which the wipe's strength starts to grow. */
#define WIPE_RAMP_START 64

/** Primitive codes with the semi-transparency bit set, as they sit in the top byte of the colour word. */
#define CODE_FT4_BLENDED (0x2E << 24)
#define CODE_G4_BLENDED (0x3A << 24)
#define CODE_GT4_BLENDED (0x3E << 24)

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
#define TRANSITION_OT_SIZE 8
typedef u32 TransitionOt[TRANSITION_OT_SIZE];
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
    /* 0x28 */ s16 unk28;
    /* 0x2A */ s16 unk2A;
    /* 0x2C */ s32 unk2C;
} TransitionStrip;

/** @brief One horizontal bar of the wipe to black. */
typedef struct {
    /* 0x00 */ s32 length; /**< 16.16 fixed point. */
    /* 0x04 */ s32 speed;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s16 r; /**< How much of each colour the bar's leading edge takes away, per unit of strength; its root takes away everything. */
    /* 0x0E */ s16 g;
    /* 0x10 */ s16 b;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} TransitionBar;

/** @brief State of the normal battle transition. The strip table starts right after it. */
typedef struct {
    /* 0x00 */ s16 step;
    /* 0x02 */ s16 unk02; /**< Set to the display buffer in use when the transition starts; both set-ups clear it again. */
    /* 0x04 */ TransitionStrip *strips;
    /* 0x08 */ TransitionBar *bars;
    /* 0x0C */ s32 unk0C;
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

/** @brief Screen vertex (8 bytes). Packed x,y screen coordinates from GTE. */
typedef struct {
    s32 xy; /**< Packed x:16, y:16 screen coordinates. */
    s32 pad;
} ScreenVert;

/** @brief POLY_GT4: gouraud-textured 4-point polygon (52 bytes). */
typedef struct {
    /* 0x00 */ u8 tag[3]; /**< P_TAG address (24 bits). */
    /* 0x03 */ u8 len; /**< P_TAG word count. */
    /* 0x04 */ s32 color0; /**< Vertex 0 color (R,G,B,code). */
    /* 0x08 */ s32 vert0; /**< Vertex 0 screen XY. */
    /* 0x0C */ u16 uv0; /**< Vertex 0 UV coordinates. */
    /* 0x0E */ u16 clut; /**< CLUT id. */
    /* 0x10 */ s32 color1; /**< Vertex 1 color. */
    /* 0x14 */ s32 vert1; /**< Vertex 1 screen XY. */
    /* 0x18 */ u16 uv1; /**< Vertex 1 UV coordinates. */
    /* 0x1A */ u16 tpage; /**< Texture page id. */
    /* 0x1C */ s32 color2; /**< Vertex 2 color. */
    /* 0x20 */ s32 vert2; /**< Vertex 2 screen XY. */
    /* 0x24 */ u16 uv2; /**< Vertex 2 UV coordinates. */
    /* 0x26 */ u16 pad26;
    /* 0x28 */ s32 color3; /**< Vertex 3 color. */
    /* 0x2C */ s32 vert3; /**< Vertex 3 screen XY. */
    /* 0x30 */ u16 uv3; /**< Vertex 3 UV coordinates. */
    /* 0x32 */ u16 pad32;
} PolyGT4; /* 0x34 = 52 bytes */

/** @brief Mesh render context for GTE transformation and GPU primitive generation. */
typedef struct {
    /* 0x00 */ s32 pad00;
    /* 0x04 */ PolyGT4 *primPtr; /**< Current primitive write pointer. */
    /* 0x08 */ ScreenVert *vertices; /**< Vertex position array. */
    /* 0x0C */ SVECTOR *points; /**< The grid's points, before projection. */
    /* 0x10 */ s32 *otBase; /**< Ordering table base pointer. */
} MeshRenderCtx;

#define GRID_SIZE 8 /**< Quads per row/column in the mesh grid. */
#define GRID_VERTS 9 /**< Vertices per row (GRID_SIZE + 1). */

/* The transitions' working memory sits at fixed addresses, as in the original:
 * the target loads them as literals (lui/ori, no relocation), so a symbol would
 * not match. The two transitions share the region. */
#define TRANSITION_STATE ((TransitionState *)0x801F0000)
/** Bytes cleared at TRANSITION_STATE: the state and both tables. */
#define TRANSITION_STATE_SIZE 0x4000
#define TRANSITION_WORK ((TransitionWork *)0x801F5000)
#define TRANSITION_OTS ((TransitionOt *)0x801F6000)
#define TRANSITION_PRIMS ((TransitionPrims *)0x801DE000)
/** One-entry ordering table for copying the picture on screen; the packets start 16 bytes in. */
#define SNAPSHOT_COPY_OT ((u32 *)0x801DC000)
#define MESH_RENDER_CTX ((MeshRenderCtx *)0x801F0000)
#define MESH_INPUT_VERTS ((SVECTOR *)0x801F0400)
#define MESH_SCREEN_VERTS ((ScreenVert *)0x801F1000)

extern s8 D_8005F17D;
extern u8 D_8005F17C;
extern u8 D_8005F17F;
extern u8 D_8005F180;
extern s32 D_80052914[];
extern DR_MOVE D_80082CA0[];
extern DRAWENV D_80082CD0[];
extern DISPENV D_80082D90[];
extern RECT D_8005EC24;
extern RECT D_8005EC2C;
extern RECT D_8005EC34;
extern u8 g_meshIntensityA[]; /**< Intensity table A (indexed by col, then row). */
extern u8 g_meshIntensityB[]; /**< Intensity table B (indexed by col, then row). */
extern u8 g_meshUCoord[]; /**< U coord per column. */
extern u8 g_meshVCoord[]; /**< V coord per column. */
extern u8 g_meshUPage[]; /**< U page offset per row (shifted <<8). */
extern u8 g_meshVPage[]; /**< V page offset per row (shifted <<8). */
extern u8 g_meshTpage[]; /**< Texture page ID per column. */
extern MATRIX g_meshBaseMatrix;

static void func_80024064(void);
static void normalTransitionTick(void);
static void transformMeshVertices(MeshRenderCtx *mesh);
static PolyGT4 *renderMeshGrid(ScreenVert *vertices, PolyGT4 *primBuf, s32 *ot, s32 intensity, s32 perVertex);
static void renderMeshPanel(MeshRenderCtx *mesh, MATRIX *matrix, s32 intensity, s32 tx, s32 ty);
static void renderScaledMesh(MeshRenderCtx *mesh, s32 *ot, s32 scale, s32 intensity);
static void initMeshRenderer(void);
void func_80026ADC(void); /**< Draw the saved picture; arguments and result go through the transition's scratch block. */
void func_80026CA0(void); /**< Random number below the transition scratch block's first argument, returned in its result word. */
void func_80026E20(void); /**< Close the battle transition's thread. */
void func_80026E70(void); /**< Open the battle transition's thread. */

/**
 * @brief Start the screen transition that plays while a battle loads.
 *
 * Sets up two drawing buffers side by side and a third display area for the
 * saved picture, copies the picture on screen into that area, shows it and
 * copies it into both buffers. Then sets up the chosen transition, opens the
 * thread that draws it and hands the VSync callback over to it.
 *
 * @param boss Non-zero for a boss battle, which gets the boss transition
 * instead of the normal one.
 */
void func_80023D60(register s32 boss) {
    /* The caller passes an int it does not mask, so the parameter is s32. The
     * original keeps it in a register and copies it to a byte on the stack: a
     * plain s32 parameter would be stored to its argument slot instead. */
    u8 type = boss;
    /* Never read: the original reserves 0x30 bytes of frame it does not touch. */
    u8 unused[0x30];
    register u32 *ot;
    register TransitionState *state;
    register DR_MOVE *move;
    register DR_STP *stp;

    g_renderMode = RENDER_IDLE;
    state = TRANSITION_STATE;
    state->unk02 = g_bufferIndex & 1;
    VSync(0);

    SetDefDrawEnv(&D_80082CD0[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&D_80082CD0[1], SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&D_80082D90[0], SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&D_80082D90[1], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&D_80082D90[2], SNAPSHOT_X, SNAPSHOT_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    SetGeomScreen(SCREEN_DISTANCE);
    /* Show 224 of the 240 lines, centred. */
    D_80082D90[0].screen.y = D_80082D90[1].screen.y = D_80082D90[2].screen.y = 8;
    D_80082D90[0].screen.h = D_80082D90[1].screen.h = D_80082D90[2].screen.h = SCREEN_HEIGHT;
    D_8005F17F = type;

    /* Copy the picture on screen to the snapshot area. */
    ot = SNAPSHOT_COPY_OT;
    ClearOTag(ot, 1);
    stp = (DR_STP *)(ot + 4);
    /* libgpu.h declares SetDrawStp with a u32 pointer where the SDK's takes a DR_STP. */
    SetDrawStp((u32 *)stp, 0);
    AddPrim(ot, stp);
    stp++;
    move = (DR_MOVE *)stp;
    SetDrawMove(move, &((DISPENV *)D_8005F138)->disp, SNAPSHOT_X, SNAPSHOT_Y);
    AddPrim(ot, move);
    move++;
    stp = (DR_STP *)move;
    SetDrawStp((u32 *)stp, 1);
    AddPrim(ot, stp);
    stp++;
    DrawOTag(ot);
    DrawSync(0);
    VSync(0);

    /* Show the snapshot, and start both buffers from it. */
    PutDispEnv(&D_80082D90[2]);
    MoveImage(&D_8005EC24, 0, 0);
    MoveImage(&D_8005EC24, SCREEN_WIDTH, 0);
    if (D_8005F17F == 0) {
        func_80024064();
    } else {
        initMeshRenderer();
    }
    DrawSync(0);
    D_8005F17C = 0;
    D_8005F17D = 0;
    func_80026E70();
    g_renderMode = RENDER_BATTLE;
}


/**
 * @brief Set up the normal battle transition.
 *
 * Picks at random which side the transition runs from, clears the state
 * block and both ordering tables, and fills in the strip and bar tables that
 * normalTransitionTick animates: the strips start spread out by their index, and
 * each bar of the wipe gets a random start, speed and strength.
 */
static void func_80024064(void) {
    /* Never read: the original reserves 0x18 bytes of frame it does not touch. */
    u8 unused[0x18];
    register u32 *p;
    register s32 i;
    register TransitionStrip *strip;
    register TransitionState *state;
    register TransitionBar *bar;
    register TransitionWork *work;
    register TransitionOt *ot;

    work = TRANSITION_WORK;
    work->arg0 = 256;
    func_80026CA0();
    D_8005F180 = work->result & 1;

    /* Clear the state block, which holds the strip and bar tables too. */
    p = (u32 *)TRANSITION_STATE;
    for (i = 0; i < TRANSITION_STATE_SIZE / 4; i++) {
        *p++ = 0;
    }

    state = TRANSITION_STATE;
    ot = TRANSITION_OTS;
    ClearOTag(ot[0], TRANSITION_OT_SIZE);
    ClearOTag(ot[1], TRANSITION_OT_SIZE);

    strip = (TransitionStrip *)(state + 1);
    state->strips = strip;
    for (i = 0; i < STRIP_TABLE_SIZE; i++) {
        /* Multiplies, not shifts: the target copies i before shifting it. */
        strip->x = i * 0x8000;
        strip->vx = i * 0x2000;
        strip->ax = i * 0x100;
        if (D_8005F180 == 0) {
            strip->vz = -0x10000;
            strip->az = -(i * 64);
        } else {
            strip->vz = 0x10000;
            strip->az = i * 64;
        }
        /* Not 64 - i: the target negates first. */
        strip->brightness = -i + 64;
        strip->unk26 = 0;
        strip->unk28 = i * 16;
        strip->unk2A = i * 8;
        strip++;
    }

    /* The bars follow the strips. */
    bar = (TransitionBar *)strip;
    state->bars = bar;
    for (i = 0; i < BAR_TABLE_SIZE; i++) {
        work->arg0 = 48;
        func_80026CA0();
        bar->length = work->result << 16;
        work->arg0 = 48;
        func_80026CA0();
        bar->speed = (work->result << 16) + (20 << 16);
        bar->unk08 = 0;
        work->arg0 = 16;
        func_80026CA0();
        bar->r = work->result + 16;
        bar->g = 16;
        bar->b = 16;
        bar++;
    }
}


/**
 * @brief Draw one step of the normal battle transition.
 *
 * The transition runs in its own thread while the battle loads, one step every
 * second VSync. The step count goes up before it is used, so it runs from 1 to 80:
 * - Every step's list starts with the saved picture. Steps 1 to 47 add four
 * glowing quads over it and, on top of those, 128 quarter-strength strips of
 * the picture's left 160 pixels that fly apart and brighten; from step 2 the
 * result is also copied back over the saved picture, so the next step builds
 * on it.
 * - Steps 48 to 80 take the picture away to black with 224 bars that grow in
 * from one side.
 * - From step 78 the list is no longer drawn and the display is blanked.
 * - Step 80 also clears @c g_renderMode and closes the thread.
 *
 * @c D_8005F180 mirrors the whole thing left to right.
 *
 * @note The first quad (the 32-pixel margin) gets its left edge's texture
 * coordinates twice: once under the mirror test and then again
 * unconditionally. The original is missing an @c else there.
 */
static void normalTransitionTick(void) {
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
    ClearOTag(ot, TRANSITION_OT_SIZE);

    r = getInterruptStatus();
    r |= STATUS_GTE_ENABLE;
    func_80026FD4(r);

    scratch = (DR_MODE *)&work->angle;
    if (D_8005F180 == 0) {
        ((SVECTOR *)scratch)->vy = state->step * 4;
        work->trans.vx = -(SCREEN_WIDTH / 2);
    } else {
        ((SVECTOR *)scratch)->vy = HALF_TURN - state->step * 4;
        work->trans.vx = SCREEN_WIDTH / 2;
    }
    ((SVECTOR *)scratch)->vx = 0;
    ((SVECTOR *)scratch)->vz = 0;
    RotMatrix(&work->angle, &work->rot);
    SetRotMatrix(&work->rot);
    work->trans.vy = -(SCREEN_HEIGHT / 2);
    work->trans.vz = SCREEN_DISTANCE;
    gte_SetTransVector(&work->trans);

    /* Last in the list: once it has been drawn, copy its frame back over the
     * saved picture, so the next step builds on this one. The label is not
     * jumped to: it is what puts a nop here, as in the original. */
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
    work->arg2 = SNAPSHOT_COLOR;
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
                setTPage(prim, 2, 3, SNAPSHOT_X, SNAPSHOT_Y);
            } else {
                setTPage(prim, 2, 3, SNAPSHOT_X + 128, SNAPSHOT_Y);
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
                prim->u1 = prim->u3 = next->u0 = next->u2 = STRIP_LEFT + STRIP_COUNT - i;
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
        /* The strength of the wipe, kept in the translation's x, which is free once the GTE has loaded it. */
        if (state->step >= WIPE_RAMP_START) {
            work->trans.vx = state->step - (WIPE_RAMP_START - 1);
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
            g_renderMode = RENDER_IDLE;
            func_80026E20();
            SetDispMask(0);
        }
    /* Another label nothing jumps to, kept for its nop. */
    done:;
    }

    DrawSync(0);
    D_8005F17D = 0;
}


/**
 * @brief Project the 81 grid points through the GTE into screen positions.
 * @param mesh Mesh state holding the grid's points and where their screen positions go.
 */
static void transformMeshVertices(MeshRenderCtx *mesh) {
    register SVECTOR *points = mesh->points;
    register ScreenVert *vertices = mesh->vertices;
    register s32 i = 0;
    s32 screenXY;
    s32 flag;
    s32 interpZ;

    while (i < GRID_VERTS * GRID_VERTS) {
        RotTransPers(points, &vertices->xy, &screenXY, &screenXY);
        points++;
        vertices++;
        i++;
    }
}


/**
 * @brief Render an 8x8 grid of gouraud-textured quads.
 *
 * Generates 64 POLY_GT4 GPU primitives from a 9x9 vertex grid,
 * applying per-vertex or uniform lighting from intensity tables.
 *
 * @param vertices 9x9 vertex position grid (stride 8 bytes per vertex).
 * @param primBuf Output primitive buffer.
 * @param ot GPU ordering table to insert primitives into.
 * @param intensity Brightness scale factor (8.8 fixed point).
 * @param perVertex If nonzero, use per-vertex shading; else uniform gray.
 * @return Pointer past the last written primitive.
 */
static PolyGT4 *renderMeshGrid(ScreenVert *vertices, PolyGT4 *primBuf, s32 *ot,
                               s32 intensity, s32 perVertex) {
    register PolyGT4 *prim = primBuf;
    register s32 r;
    register s32 minVal;
    register s32 uvHiU;
    register s32 uvHiV;
    register s32 col;
    register s32 row;
    register ScreenVert *mesh;
    char pad;

    mesh = vertices;
    row = 0;

    while (row < GRID_SIZE) {
        col = 0;
        while (col < GRID_SIZE) {
            setPrimLen(prim, 12);
            prim->tpage = g_meshTpage[col] | getTPage(2, 1, 0, 0);

            if (perVertex) {
                /* Vertex 0: min(tableA[col], tableA[row]) * intensity */
                r = g_meshIntensityA[col];
                minVal = g_meshIntensityA[row];
                if (minVal < r) r = minVal;
                r = (r * intensity) / 256;
                r &= 0xFF;
                r = (r | (r << 8)) | (r << 16);
                prim->color0 = r | CODE_GT4_BLENDED;

                /* Vertex 1: min(tableB[col], tableA[row]) * intensity */
                r = g_meshIntensityB[col];
                if (minVal < r) r = minVal;
                r = (r * intensity) / 256;
                r &= 0xFF;
                r = (r | (r << 8)) | (r << 16);
                prim->color1 = r;

                /* Vertex 2: min(tableA[col], tableB[row]) * intensity */
                r = g_meshIntensityA[col];
                minVal = g_meshIntensityB[row];
                if (minVal < r) r = minVal;
                r = (r * intensity) / 256;
                r &= 0xFF;
                r = (r | (r << 8)) | (r << 16);
                prim->color2 = r;

                /* Vertex 3: min(tableB[col], tableB[row]) * intensity */
                r = g_meshIntensityB[col];
                if (minVal < r) r = minVal;
                r = (r * intensity) / 256;
                r &= 0xFF;
                r = (r | (r << 8)) | (r << 16);
                prim->color3 = r;
            } else {
                /* Uniform gray */
                r = (intensity / 2) & 0xFF;
                r = (r | (r << 8)) | (r << 16);
                r = r | CODE_GT4_BLENDED;
                prim->color0 = prim->color1 = prim->color2 = prim->color3 = r;
            }

            /* Copy vertex positions from the 9-wide grid */
            prim->vert0 = mesh[0].xy;
            prim->vert1 = mesh[1].xy;
            prim->vert2 = mesh[GRID_VERTS].xy;
            prim->vert3 = mesh[GRID_VERTS + 1].xy;

            /* UV coordinates from lookup tables */
            r = g_meshUCoord[col];
            minVal = g_meshVCoord[col];
            uvHiU = g_meshUPage[row] << 8;
            uvHiV = g_meshVPage[row] << 8;

            prim->uv0 = r | uvHiU;
            prim->uv1 = minVal | uvHiU;
            prim->uv2 = r | uvHiV;
            prim->uv3 = minVal | uvHiV;

            AddPrim(ot, prim);
            prim++;
            mesh++;
            col++;
        }
        mesh++;
        row++;
    }
    return prim;
}


/**
 * @brief Set up GTE matrices and render a mesh grid.
 *
 * Applies rotation/translation matrix, transforms vertices through
 * the GTE, then renders an 8x8 textured quad grid via renderMeshGrid.
 *
 * @param mesh Mesh render context with vertices, primitives, and OT.
 * @param matrix Rotation matrix to apply (translation set from tx/ty params).
 * @param intensity Brightness scale factor.
 * @param tx Translation X (stored to matrix->t[0]).
 * @param ty Translation Y (stored to matrix->t[1], from stack).
 */
static void renderMeshPanel(MeshRenderCtx *mesh, MATRIX *matrix, s32 intensity,
                            s32 tx, s32 ty) {
    register s32 *otPtr;
    s32 unused1;
    s32 unused2;
    s32 unused3;

    matrix->t[0] = tx;
    matrix->t[1] = ty;
    SetRotMatrix(matrix);
    SetTransMatrix(matrix);
    transformMeshVertices(mesh);
    otPtr = mesh->otBase;
    otPtr += 4;
    mesh->primPtr = renderMeshGrid(mesh->vertices, mesh->primPtr, otPtr, intensity, 1);
}


/**
 * @brief Copy global rotation matrix, apply uniform scale, and render mesh.
 *
 * Copies the 32-byte global rotation matrix g_meshBaseMatrix into a local copy,
 * applies a uniform scale factor, sets the GTE matrices, transforms vertices,
 * and renders the mesh grid with no per-vertex shading.
 *
 * @param mesh Mesh render context.
 * @param ot GPU ordering table.
 * @param scale Uniform scale factor (applied to all 3 axes).
 * @param intensity Brightness scale factor.
 */
static void renderScaledMesh(MeshRenderCtx *mesh, s32 *ot, s32 scale, s32 intensity) {
    register s32 i;
    register MATRIX *dst;
    register MATRIX *src;
    s32 scaleVec[3];
    MATRIX localMatrix;
    s32 unused1;
    s32 unused2;

    src = &g_meshBaseMatrix;
    dst = &localMatrix;
    i = 0;
    /* The MATRIX's 32 bytes; sizeof would make the compare unsigned. */
    while (i < 32) {
        *(s32 *)((u8 *)dst + i) = *(s32 *)((u8 *)src + i);
        i += 4;
    }

    scaleVec[0] = scaleVec[1] = scaleVec[2] = scale;
    /* A VECTOR without its pad word: the frame needs the 12-byte array, and ScaleMatrix never reads the pad. */
    ScaleMatrix(dst, (VECTOR *)scaleVec);
    SetRotMatrix(dst);
    SetTransMatrix(dst);
    transformMeshVertices(mesh);
    mesh->primPtr = renderMeshGrid(mesh->vertices, mesh->primPtr, ot, intensity, 0);
}


INCLUDE_ASM("asm/nonmatchings/btl_transition", renderFlatMesh);


/**
 * @brief Initialize 9x9 vertex grid and ordering tables for mesh rendering.
 *
 * Sets up the mesh render context at MESH_RENDER_CTX with buffer pointers,
 * clears both ordering tables, and fills in the 9x9 grid of points the mesh
 * is projected from: 40 apart across and 27 down, centred on the origin.
 */
static void initMeshRenderer(void) {
    register MeshRenderCtx *ctx;
    register TransitionOt *ot;
    register s32 col;
    register s32 row;
    register SVECTOR *point;
    s32 unused1;
    s32 unused2;
    s32 unused3;
    s32 unused4;

    ctx = MESH_RENDER_CTX;
    ctx->pad00 = 0;
    ctx->points = MESH_INPUT_VERTS;
    ctx->vertices = MESH_SCREEN_VERTS;
    ot = TRANSITION_OTS;
    ClearOTag(ot[0], TRANSITION_OT_SIZE);
    ClearOTag(ot[1], TRANSITION_OT_SIZE);
    point = ctx->points;
    row = 0;
    while (row < GRID_VERTS) {
        col = 0;
        while (col < GRID_VERTS) {
            point->vx = col * 40 - 160;
            point->vy = row * 27 - 108;
            /* vz and the pad cleared with one word store, as the target does. */
            *(s32 *)&point->vz = 0;
            point++;
            col++;
        }
        row++;
    }
}


INCLUDE_ASM("asm/nonmatchings/btl_transition", bossTransitionTick);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026ADC);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026CA0);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026CF0);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026D10);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026D8C);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026E20);


INCLUDE_ASM("asm/nonmatchings/btl_transition", func_80026E70);
