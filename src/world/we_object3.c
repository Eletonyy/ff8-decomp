#include "common.h"
#include "psxsdk/libgpu.h"
#include "world.h"
#include "world/we_object1.h"
#include "world/we_object3.h"
#include "world/we_object4.h"
#include "world/we_object9.h"   /* func_800BC5E0 */
#include "thread.h"

/* ---- private to renderWorldMapFrame -------------------------------------
 * Direction bits of D_800D2278 (declared in we_object1.h); only this unit
 * tests them, so they stay here rather than in the shared header. Named to
 * match the field side (see FIELD_PAD_XLOW in field.h).
 */
#define WORLD_PAD_XLOW   0x8000  /**< Stick/d-pad X-low (left). */
#define WORLD_PAD_XHIGH  0x2000  /**< Stick/d-pad X-high (right). */

/** Analog axes sampled per frame: X, Y and the second stick's pair, per controller. */
#define WORLD_PAD_AXES   8
/** @c D_800D2278 alternates between two banks so the previous frame stays readable. */
#define WORLD_PAD_BANKS  2
/** @c D_800D2278 slot holding the buttons that went down this frame. */
#define WORLD_PAD_PRESSED 2
/** Frames an unchanged input must persist before it auto-repeats. */
#define WORLD_PAD_REPEAT_DELAY 0x1F
/** Repeats emitted before the sub-counter resets and a fresh press is reported. */
#define WORLD_PAD_REPEAT_STEPS 3

/* ---- world render callback (func_800A47A4) ------------------------------ */

/** @c D_800C4D60 packs a slot pair: the low byte is the armed code / low slot,
    the high byte its partner. Hence the @c (u8) casts (low byte, emitting
    @c lbu) and the @c >>8 / @c >>9 reads below — each is a view of the pair
    that the @c u16 type cannot express on its own. */
#define WORLD_CODE_NONE     0xFFFF  /**< Both bytes 0xFF: nothing armed. */
/** Bit 0 of @c D_800D241E selects the alternate (biased) code table. */
#define WORLD_FLAG_ALTCODE  0x1
/** Added to a code when the alternate table is selected. */
#define WORLD_CODE_ALT_BIAS 0xC
/** Stride and length of one message record in the @c D_800C9E58 table. */
#define WORLD_MSG_LEN       37
/** Length of one per-object message record (@c D_800C97E0 / @c D_800C97E4). */
#define WORLD_OBJMSG_LEN    18
/** Frames to wait before retrying a stalled or failed stream read. */
#define WORLD_STREAM_RETRY  0x78
/** @c WorldFlags::opParam value that keeps an armed code on map 0x32. */
#define WORLD_OPPARAM_KEEP  5
/** World-map cell grid; a cell index is @c row * WORLD_GRID_W + column, and both
    axes wrap (the map is a torus). */
#define WORLD_GRID_W        0x80
#define WORLD_GRID_H        0x60
/** Fixed RAM staging buffer for streamed records. It has no symbol in the map,
    so the address is spelled out; the @c Tim cast at the upload call is a real
    reinterpretation of that buffer, not a type fix. */
#define WORLD_STAGE_ADDR    0x801E8000
/** GTE-scratchpad slot where the caller stages the projected probe point for
    the descriptor hit tests (scratchpad RAM has no symbol in the map). */
#define WORLD_PROBE_POINT   ((VECTOR *)0x1F8002D0)

/** Countdown to the next stream retry; @c D_800C53AC caches the last tick. */
extern s32 D_800C53AC;
extern s32 D_800C53B0;
/** Staged per-object message address (the buffer itself is in the header). */
extern s32 D_800D34E8;
/** World-engine mode flags; bit 0 picks the alternate code table. */
extern u16 D_800D241E;

/* None of the declarations below are referenced outside this unit either. */

/**
 * @brief One placed world-map sprite produced by @c placeWorldSpriteFan (0x2C stride).
 *
 * @c pos is the final world position; @c cell receives the @c worldPosToCell
 * projection; @c cellId/flag are the projected grid-cell id and a fixed marker.
 * @note Field purpose partly uncertain — named from the access pattern.
 */
typedef struct {
    VECTOR   pos;        /* 0x00 */
    SVECTOR  cell;       /* 0x10 — worldPosToCell output */
    u8       pad18[0x4]; /* 0x18 */
    CmdDesc *cmd;        /* 0x1C — installed as the current descriptor D_800C4D64 */
    s16      cellId;     /* 0x20 — worldPosToCell return */
    s16      flag;       /* 0x22 — one of WORLD_SPRITE_* below */
    s16      angle;      /* 0x24 — heading used to bias the camera track */
    s16      code;       /* 0x26 — packed type | flag<<8 of the installed descriptor */
    u16      unk28;      /* 0x28 — published to D_800C4D48; its readers are still asm */
    u8       pad2A[0x2]; /* 0x2A */
} WorldSprite;           /* 0x2C */

/** @c WorldSprite::flag states. */
#define WORLD_SPRITE_FREE    0  /**< Slot unused. */
#define WORLD_SPRITE_PLACED  2  /**< Given a position this pass. */
#define WORLD_SPRITE_CLAIMED 3  /**< Matched a glyph this frame. */

/** @brief Probe vector: three unsigned halfword components plus padding.
 *
 * Unsigned because the cell deltas wrap modulo 0x10000 as they are added
 * (@c func_800A1678 reads them with @c lhu), so overflow is intended.
 */
typedef struct {
    u16 vx, vy, vz, pad;
} ProbeVec;              /* 0x08 */

/** The six probe-corner offsets, used as two batches of three. */
typedef struct {
    ProbeVec v[6];
} CellCorners;           /* 0x30 */

/** @brief One cell's projection scratch, laid out so a single walking pointer
 *         reaches the delta, the corners and the probes.
 *
 * @c func_800A1678 indexes this as @c ProbeVec[10] from @c &delta: element 0 is
 * the delta, 1..6 the corners, 7..9 the probes.
 */
typedef struct {
    ProbeVec    delta;   /* 0x00 */
    CellCorners c;       /* 0x08 */
    ProbeVec    probe[3];/* 0x38 */
} ProbeFrame;            /* 0x50 */

/** Projected screen position as the GTE writes it (unsigned compare against the
    screen width, so not @c DVECTOR). */
typedef struct {
    u16 sx, sy;
} ScreenXY;              /* 0x04 */

/** Sprites in one pool record: an anchor plus the four spread around it. */
#define WORLD_FAN_SPRITES 5

/** A pool record: five sprites placed together as one fan. */
typedef struct {
    WorldSprite sprite[WORLD_FAN_SPRITES];
} WorldSpriteRec;        /* 0xDC */

/** One of the two alternating pool banks of eight records. */
typedef struct {
    WorldSpriteRec rec[8];
} WorldSpriteBank;       /* 0x6E0 */

extern SVECTOR          D_800CA038;   /**< Reference offset fed to the pool placer. */
/** Six probe-corner offsets, used as two batches of three by @c func_800A1678.
    It stays an extern rather than a file-scope @c const because the struct copy
    tests the source alignment at run time, which only happens when the compiler
    cannot see the object's alignment. */
extern CellCorners      D_800980EC;
extern WorldSpriteBank  D_800D2508[];  /**< Two alternating banks of eight records;
                                            index 2 is the end address (= D_800D2BE8). */
extern WorldSpriteBank  D_800D2BE8[]; /**< End marker: D_800D2508 + one bank. */
extern WorldSpriteRec  *D_800D2478[10];/**< Records queued for this frame. */
extern WorldSpriteRec   D_800D32C8[]; /**< End of both banks: D_800D2508 + 16 records. */
extern GlyphHeader     *D_800D24A0;   /**< Glyph currently being dispatched, or NULL. */
extern GlyphHeader     *D_800C9740;   /**< Glyph used when worldObjectById finds no object. */
extern s32              D_800C53A4;   /**< Active pool bank (0 or 1). */
extern s32              D_800C53A8;   /**< Cleared when a record is placed, bumped by the
                                           fallback; the pass loop stops below 4. */
extern s32              D_800C9720;   /**< Third argument to the glyph renderers, which
                                           are still asm. */
extern s32              D_800C972C;   /**< Accumulated glyph-entry count. */
extern s32              D_800C96D8[WORLD_PAD_AXES]; /**< Analog axes sampled this frame;
                                           element 0 is pad 0's X, compared against 0x7F
                                           +- 0x2D/0x2E to pick a bank. */
extern s32              D_800D2240[WORLD_PAD_AXES]; /**< Previous frame's copy of
                                           @c D_800C96D8, tested the same way. */
extern s32              D_800C9724;   /**< Frames elapsed this tick; drives the repeat timers. */
extern s32              D_800C9ED0;   /**< Pad 0's raw frame parameter. */
extern s32              D_800C9ED4;   /**< Pad 1's raw frame parameter, packed into the high half. */
extern s32              D_800D2470;   /**< Repeat delay accumulator. */
extern s32              D_800D2474;   /**< Repeat step counter. */

/* getAnimFrameParam returns u16 in thread.c, but every caller uses the result as
   s32 with no widening mask; thread.h documents that those callers keep their own
   declaration. func_80030F10 has no owning translation unit yet. */
extern s32              getAnimFrameParam(s32 slot, s32 sub);
extern s32              func_80030F10(s32 arg);
extern s16              D_800C9772;   /**< Receives the low half of the camera-follow
                                           reference D_800C9870. */

/* The matched caller passes (&D_800C9868, &D_800C9838); the function itself
   takes no parameters (it reads D_800D23C0), so keep the unprototyped form
   for the call-site ABI. */
static void             func_800A1F10();
extern s32              func_800A4420(WorldSpriteRec *rec, SVECTOR *ref, SVECTOR *out, s32 size);
static WorldObject     *worldObjectById(s16 id, WorldObject *head);
/* All defined below; the render callback is the first caller in the file. */
static void             buildViewportCellList(WorldPos *cam, WorldObject *out, MATRIX *m);
static void            func_800A6030(WorldObject **pp);
static WorldObject    *func_800A60B4(s32 key, WorldObject *head);
static void            func_800A6188(Tim *tim, u8 tableIdx);
static void            drainPendingObjects(void);
static s32              func_800A2920(GlyphHeader *glyph, WorldSprite *st, s16 key, CmdDesc *end);
extern WorldSpriteRec  *func_800A26E8(GlyphHeader *p, WorldSpriteRec *rec, s32 v);
extern s32              func_800A3C9C(WorldSpriteRec *rec, s32 mode);
extern s32              func_800A5B48(void);
extern s32              func_800B21EC(WorldSpriteRec *rec, s32 mode, s32 c, s32 d);
/* Both below are declared in we_object10.h, which this unit cannot include:
 * its func_800B0010 prototype is (void) to serve a no-argument caller there,
 * conflicting with the true (u32 kind) definition in we_object6.c that
 * func_800A2D50 relies on. */
extern void            *func_80047CE4(void *dst, s32 c, u32 n);
extern s32              func_800B0010(u32 kind);
static void             func_800A1678(s32 col, s32 mode);
static void             func_800A0388(void);
static void             func_800A39BC(WorldSprite *out, s16 h);
static void             placeWorldSpriteFan(WorldSprite *out, VECTOR *v, SVECTOR *angles,
                                            s32 arg3, s32 arg4, VECTOR *origin);
static void             func_800A581C(void);
static s32              func_800A6254(WorldObject *head);
static void             func_800A62E0(s16 val, u16 *coarse, u16 *fine);
static void             func_800A6358(void);
static void             func_800A63F0(void);
static void             initWorldDoubleBuffer(void);
static void             setupWorldRenderParams(void);
extern void             func_800ACDC4(GlyphHeader *p, BattleSceneCtx *ctx, s32 arg);
extern void             func_800BF80C(GlyphHeader *p, s32 *ot, s32 arg);
extern void             func_800BF2E8(GlyphHeader *p, s32 *ot, s32 arg);
extern void             func_800BF20C(CmdDesc *p, s32 kind, s32 arg);
extern void             func_800BFBFC(s32 kind);


INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A01DC);

/**
 * @brief Sample both controllers for the frame and derive the newly-pressed mask.
 *
 * Flips @c D_800C4D04 so this frame's buttons land in the other half of
 * @c D_800D2278, leaving last frame's readable alongside. Both pads' button
 * masks are packed into one word (pad 1 in the high half), the eight analog
 * axes are copied to @c D_800D2240 before being resampled, and
 * @c D_800D2278[WORLD_PAD_PRESSED] receives the buttons that went down this
 * frame.
 *
 * When the input is unchanged that mask would be empty, so an auto-repeat runs:
 * @c D_800D2470 accumulates until @c WORLD_PAD_REPEAT_DELAY, after which
 * @c D_800D2474 counts @c WORLD_PAD_REPEAT_STEPS repeats and then reports the
 * held buttons as if freshly pressed.
 *
 * @note The pressed expression is written out per branch rather than cached in a
 *       local: the stores to the repeat counters sit between the compare and the
 *       later branches, and re-reading is what keeps those loads there. Caching
 *       it lets gcc fold the equal case to a constant zero.
 */
static void func_800A0388(void) {
    s32 idx;

    D_800C4D04 = D_800C4D04 == 0;
    func_800275D4();
    D_800C9ED0 = getAnimFrameParam(0, 0);
    D_800C9ED4 = getAnimFrameParam(1, 0);
    D_800D2278[D_800C4D04] = func_80030F10(D_800C9ED0) | (D_800C9ED4 << 16);

    D_800D2240[0] = D_800C96D8[0];
    D_800D2240[1] = D_800C96D8[1];
    D_800D2240[2] = D_800C96D8[2];
    D_800D2240[3] = D_800C96D8[3];
    D_800D2240[4] = D_800C96D8[4];
    D_800D2240[5] = D_800C96D8[5];
    D_800D2240[6] = D_800C96D8[6];
    D_800D2240[7] = D_800C96D8[7];

    D_800C96D8[0] = func_80027DB4(0, PAD_AXIS_X, 0);
    D_800C96D8[1] = func_80027DB4(0, PAD_AXIS_Y, 0);
    D_800C96D8[2] = func_80027DB4(0, PAD_AXIS_X2, 0);
    D_800C96D8[3] = func_80027DB4(0, PAD_AXIS_Y2, 0);
    D_800C96D8[4] = func_80027DB4(1, PAD_AXIS_X, 0);
    D_800C96D8[5] = func_80027DB4(1, PAD_AXIS_Y, 0);
    D_800C96D8[6] = func_80027DB4(1, PAD_AXIS_X2, 0);
    D_800C96D8[7] = func_80027DB4(1, PAD_AXIS_Y2, 0);

    idx = D_800C4D04;

    if (D_800D2278[idx] == D_800D2278[(idx + 1) % WORLD_PAD_BANKS]) {
        D_800D2470 += D_800C9724;
        if (D_800D2470 >= WORLD_PAD_REPEAT_DELAY) {
            if (D_800D2474 >= WORLD_PAD_REPEAT_STEPS) {
                D_800D2474 = 0;
                D_800D2278[WORLD_PAD_PRESSED] = D_800D2278[idx];
            } else {
                D_800D2474 += D_800C9724;
                D_800D2278[WORLD_PAD_PRESSED] = D_800D2278[idx] & (D_800D2278[idx] ^ D_800D2278[(idx + 1) % WORLD_PAD_BANKS]);
            }
        } else {
            D_800D2278[WORLD_PAD_PRESSED] = D_800D2278[idx] & (D_800D2278[idx] ^ D_800D2278[(idx + 1) % WORLD_PAD_BANKS]);
        }
    } else {
        D_800D2278[WORLD_PAD_PRESSED] = D_800D2278[idx] & (D_800D2278[idx] ^ D_800D2278[(idx + 1) % WORLD_PAD_BANKS]);
        D_800D2470 = 0;
    }
}

/**
 * @brief World-map frame driver: rebuild the sprite pool, then draw every
 *        pending world object.
 *
 * Runs once per world-map frame, in four stages:
 *
 *  1. Refresh the camera basis from @c D_800C9868 and derive @c coord, the
 *     packed tile index -- the fine part from the camera's @c x in the low 7
 *     bits, the coarse part from its @c y shifted above them -- that the
 *     GTE translation vector is programmed from. Both pool banks are then
 *     re-placed by @c func_800A4420, whose per-record size argument depends on
 *     the current scene mode.
 *  2. Unlink each queued record's object from the active list @c D_800D2284
 *     onto a local list, resolve it to its world section, and dispatch its
 *     glyph (@c func_800BF20C, then @c func_800BFBFC and one of
 *     @c func_800BF80C / @c func_800BF2E8, or @c func_800ACDC4 when the glyph
 *     id is out of range).
 *  3. Walk both banks looking for records whose five sprites are all free,
 *     place a fresh fan into them and track the camera toward the result. As
 *     each record is taken, the outgoing descriptor's flag bit is mirrored
 *     into the slot state and the record's own descriptor becomes current.
 *  4. Splice the local list back onto the front of @c D_800D2284 and publish
 *     the focused object's section record to @c D_800C97A0.
 *
 * @note The two @c goto statements in stage 3 leave both the record scan and the
 *       pass loop and skip the no-record-placed fallback — the one thing C has
 *       no other spelling for. A @c placed flag instead costs 11 instructions
 *       and no longer matches, so the jump is what the original wrote.
 *
 * @note The clear of @c newHead after the splice is a dead store the compiler
 *       removes, but it is load-bearing at compile time: it ends cse's
 *       @c D_800D2284 == @c newHead equivalence so the walk re-reads the global.
 */
void renderWorldMapFrame(void) {
    SVECTOR probe;
    s32 tr[3];
    WorldObject *newHead;
    WorldObject *newTail;
    s32 coord;
    s32 modeBias;
    s32 mode;
    s32 xm;
    s32 xo;
    s32 bank;
    s16 small;
    SVECTOR *ref;
    s32 pass;
    s32 z;
    s32 i;
    WorldSpriteRec *rec;
    /* The record the pool scan matched. The object walk below borrows it as
       scratch for the section it is about to walk, because sharing this
       variable's live range is what the allocation needs: a separate variable
       moves five instructions, and borrowing sec instead moves two. */
    WorldSpriteRec *found;
    WorldSection *sec;
    WorldSpriteRec **slot;
    WorldSpriteRec **slotEnd;
    WorldSpriteRec *slotRec;
    WorldSprite *sprite;
    WorldObject *node;
    WorldObject *prev;
    WorldObject *link;
    WorldObject *obj;
    WorldSection *section;
    GlyphHeader *hdr;
    /* One past the last glyph entry -- the sibling glyphAt uses the same
       expression as a loop end. The two walks need separate variables:
       sharing one moves 271 instructions. */
    CmdDesc *entryEnd;
    CmdDesc *entryEnd2;
    s32 size;
    s32 v;
    u32 vyu;
    s32 vy;
    /* The splice test compares against these rather than against NULL: a
       register-to-register compare, not an immediate one, is what the
       allocation needs (constants here move 51 instructions). */
    WorldObject *emptyHead;
    WorldObject *emptyTail;
    s16 t;
    s16 key;
    s16 h;
    u16 keep;
    CmdDesc *cmdp;
    mode = D_800C4D38;
    emptyHead = NULL;
    emptyTail = NULL;
    newHead = emptyHead;
    newTail = emptyTail;
    func_800A1F10(&D_800C9868, &D_800C9838);
    xo = D_800C9868.x + 0x60000;
    {
        s32 rnd = xo;
        if (xo < 0) {
            rnd = 0x3FFFF + xo;
        }
        rnd = (rnd >> 18) << 18;
        xm = xo - rnd;
    }
    coord = (xm / 0x800) + ((((D_800C9868.y + 0x48000) % 0x30000) / 0x800) << 7);
    ref = &D_800CA038;
    modeBias = mode - 0x20;
    for (bank = 0; bank < 2; bank++) {
        probe.vx = 0;
        probe.vy = 0;
        probe.vz = 0;
        rec = D_800D2508[bank].rec;
        if (rec < D_800D2508[bank + 1].rec) {
            small = (modeBias >= 0) && (modeBias < 9);
            do {
                if (mode != 0x32) {
                    size = 0x40;
                    if (((mode != 0x30) && (!small)) && (mode != 0x84)) {
                        size = 0x20;
                    }
                } else {
                    size = 0x100;
                }
                func_800A4420(rec, ref, &probe, size);
                /* The temp and the two full stores are load-bearing; probe.vy += 0xA0
                   in place moves 845 instructions. */
                vy = probe.vy;
                if (bank != 0) {
                    probe.vy = vy + 0xA0;
                } else {
                    probe.vy = vy - 0xA0;
                }
                rec++;
            } while (rec < D_800D2508[bank + 1].rec);
        }
    }

    SetRotMatrix(&D_800C9838);
    D_800D2478[0] = D_800D2508[0].rec;
    slot = D_800D2478;
    slotEnd = &D_800D2478[1];
    while (slot < slotEnd) {
        slotRec = *(slot++);
        rec = slotRec;
        node = D_800D2284;
        key = rec->sprite[0].cellId;
        prev = NULL;
        if (node == NULL) {
            break;
        }
        while (node != NULL) {
            if (node->id == key) {
                break;
            }
            prev = node;
            node = node->next;
        }

        if (node == NULL) {
            break;
        }
        if (prev != NULL) {
            prev->next = node->next;
        } else {
            link = node->next;
            D_800D2284 = link;
        }
        node->next = NULL;
        if (newHead == NULL) {
            newTail = node;
            newHead = node;
        }
        if (newTail == NULL) {
            func_8009C528(0x82);
        }
        newTail->next = node;
        newTail = node;
        obj = worldObjectById(node->key, D_800CA030);
        if (obj != NULL) {
            section = &D_800C4D5C[obj->sectionIdx];
            /* offsets[] holds a byte offset from the section base, so the u32 * turns
               the >> 2 into a word index; idx[offsets] == offsets[idx], and the
               index-first form matches the addu operand order (as in func_800A5EC4). */
            hdr = (GlyphHeader *) (((u32 *) section) + (node->slot.b.idx[section->offsets] >> 2));
            entryEnd = &hdr->entries[hdr->count];
            setWorldMapTransVector(coord, node->id);
            if (hdr->id < 0x5B) {
                D_800D24A0 = hdr;
                func_800BF20C(entryEnd, hdr->id, D_800C4D30);
            } else {
                D_800D24A0 = NULL;
            }
            /* i is set before pass on purpose: folding it into the for header costs
               four instructions here. */
            i = 0;
            pass = D_800C53A4;
            for (; i < 2; i++) {
                found = func_800A26E8(hdr, D_800D2508[pass].rec, key);
                if (found != NULL) {
                    WorldSpriteRec **q = slot;
                    while (q < slotEnd) {
                        if ((*q)->sprite[0].cellId ==
                            found->sprite[0].cellId) {
                            break;
                        }
                        q++;
                    }

                    if (q >= slotEnd) {
                        /* D_800D2478 is bounded by the next symbol: &D_800D24A0 is
                           &D_800D2478[10], and naming D_800D2478 there changes the
                           relocation and stops matching. */
                        if (slotEnd < ((WorldSpriteRec **) (&D_800D24A0))) {
                            *(slotEnd++) = found;
                        } else {
                            func_8009C528(0x83);
                        }
                    }
                }
                pass = !pass;
            }

            gte_SetRotMatrix(&D_800C9838);
            rec = D_800D2508[0].rec;
            /* 16 records = both banks, the same address as D_800D32C8 below; the
               loop needs it spelled this way (D_800D32C8 here moves 717). */
            if (rec < (D_800D2508[0].rec + 16)) {
                do {
                    for (sprite = rec->sprite; sprite < rec[1].sprite; sprite++) {
                        if ((sprite->cellId == key) && (sprite->flag == WORLD_SPRITE_PLACED)) {
                            sprite->flag = WORLD_SPRITE_CLAIMED;
                        }
                    }

                    rec++;
                } while (rec < D_800D32C8);
            }
            if (D_800D24A0 != NULL) {
                gte_sttr(tr);
                gte_ldtr(0, 0, 0);
                func_800BFBFC(hdr->id);
                if (((mode == 0x32) && (D_800D23D8[0] != 5)) && (D_800D23D8[0] != 6)) {
                    D_800C972C += hdr->count;
                    func_800BF80C(hdr, D_800D244C->primList, D_800C9720);
                } else {
                    func_800BF2E8(hdr, D_800D244C->primList, D_800C9720);
                }
                gte_SetTransVector(tr);
            } else {
                func_800ACDC4(hdr, D_800D244C, D_800C9720);
            }
        }
    }

    if (((((u32) D_800C4D38) < 0xA) || (D_800C4D38 == 0x80)) || (D_800C4D38 == 0x31)) {
        s32 a = D_800D2278[D_800C4D04];
        s32 b = D_800D2278[(D_800C4D04 + 1) % 2];
        s32 chg = a & (a ^ b);
        if (chg & WORLD_PAD_XLOW) {
            D_800C53A4 = 0;
        } else if (chg & WORLD_PAD_XHIGH) {
            D_800C53A4 = 1;
        } else if ((D_800C96D8[0] != (-1)) && (D_800D2240[0] != (-1))) {
            s32 d = D_800D2240[0] - 0x7F;
            s32 c = D_800C96D8[0] - 0x7F;
            if ((c < (-0x2D)) && (d >= (-0x2D))) {
                D_800C53A4 = 0;
            } else if ((c >= 0x2E) && (d < 0x2E)) {
                D_800C53A4 = 1;
            }
        }
    } else {
        s32 a = D_800D2278[D_800C4D04];
        if (a & WORLD_PAD_XLOW) {
            D_800C53A4 = 0;
        } else if (a & WORLD_PAD_XHIGH) {
            D_800C53A4 = 1;
        } else if (D_800C96D8[0] != (-1)) {
            s32 c = D_800C96D8[0] - 0x7F;
            if (c < (-0x2D)) {
                D_800C53A4 = 0;
            } else if (c >= 0x2E) {
                D_800C53A4 = 1;
            }
        }
    }
    pass = D_800C53A4;
    D_800C4D48 = 0;
    D_800C9E38.vx = 0;
    D_800C9E38.vy = 0;
    D_800C9E38.vz = 0;
    for (i = 0; i < 2; i++) {
        rec = D_800D2508[pass].rec;
        if (rec < D_800D2508[pass + 1].rec) {
            /* D_800D2BE8 is the same address as D_800D2508[pass + 1].rec, but the
               relocation has to name that symbol to match. */
            do {
                if (((((rec->sprite[0].flag == WORLD_SPRITE_FREE) && (rec->sprite[1].flag == WORLD_SPRITE_FREE)) &&
                      (rec->sprite[2].flag == WORLD_SPRITE_FREE)) && (rec->sprite[3].flag == WORLD_SPRITE_FREE)) &&
                    (rec->sprite[4].flag == WORLD_SPRITE_FREE)) {
                    D_800C53A8 = 0;
                    h = func_800A3C9C(rec, mode);
                    for (v = 0; v < WORLD_FAN_SPRITES; v++) {
                        rec->sprite[v].cell.vy = h;
                        rec->sprite[v].pos.vy = h;
                    }

                    v = func_800B21EC(rec, mode, 0, 0);
                    D_800C4D78 = v;
                    gte_SetRotMatrix(&D_800C9838);
                    gte_SetTransMatrix(&D_800C9838);
                    /* (u32)(x - 1) >= 2 is the two-sided test for x outside 1..2. */
                    if (((v >= 0) && (((u32) (D_800D23D8[0] - 1)) >= 2)) &&
                        (D_800D23D8[0] != 0xD)) {
                        D_800C4D7C = v;
                        D_800C9770[0].vy = D_800C9868.z;
                        if (D_800C4D38 == 0x32) {
                            D_800C4D40 = 0;
                        }
                        goto after_pass;
                    }
                    v = func_800B21EC(rec, mode, 0xC8, 0);
                    D_800C4D7C = v;
                    if ((mode == 0x30) && (D_800D23D8[0] != 8)) {
                        /* The (u16)/(s16) casts below pick the load width and signedness the
                           heading comparison needs; each one changes the emitted code. */
                        s16 cur = D_800C9868.z;
                        s32 want = (u16) rec->sprite[0].angle;
                        s32 back = ((u16) rec->sprite[0].angle) - 0x80;
                        u16 lo = rec->sprite[0].angle - 0x100;
                        keep = rec->sprite[0].pos.vy;
                        if (((u32) (rec->sprite[0].cmd->type - 0x21)) < 2) {
                            lo = want + 0x148;
                            if (((s16) (want + 0xC8)) > ((s16) cur)) {
                                cur += 0xE;
                            }
                            if (((s16) lo) < ((s16) cur)) {
                                cur -= 0xE;
                            }
                        } else {
                            if (((s16) lo) > ((s16) cur)) {
                                cur += 0xE;
                            }
                            if (((s16) back) < ((s16) cur)) {
                                cur -= 0xE;
                            }
                        }
                        h = cur;
                        for (v = 0; v < 5; v++) {
                            rec->sprite[v].cell.vy = h;
                            rec->sprite[v].pos.vy = h;
                        }

                        v = func_800B21EC(rec, mode, 0, 0);
                        if (((v >= 0) && (((u32) (D_800D23D8[0] - 1)) >= 2)) &&
                            (D_800D23D8[0] != 0xD)) {
                            h = keep;
                            for (v = 0; v < 5; v++) {
                                rec->sprite[v].cell.vy = h;
                                rec->sprite[v].pos.vy = h;
                            }

                        }
                    }
                    if (D_800C4DC8 == 0) {
                        cmdp = rec->sprite[0].cmd;
                        if ((D_800D23D8[0] == 0) || (D_800D23D8[0] == 0xD)) {
                            t = (D_800C9868.z = h);
                            D_800C9770[0].vy = t;
                            D_800C9770[0].vx +=
                                ((u16) rec->sprite[0].pos.vx) - ((u16) D_800C9868.x);
                            vyu = (u16) D_800C9868.y;
                            D_800C9770[0].vz += vyu + rec->sprite[0].pos.vz;
                            D_800C9868.x = rec->sprite[0].pos.vx;
                            D_800C9868.y = -rec->sprite[0].pos.vz;
                        }
                        D_800C4D74 = D_800C4D64;
                        if (D_800C4D64 != NULL) {
                            /* Branchless mirror of CmdDesc flag 0x08 into the slot's 0x100 bit. */
                            D_800D226C->unk6C =
                                (D_800D226C->unk6C & (~SLOT_FLAG_CMD_MIRROR)) |
                                ((D_800C4D64->flag << 5) & SLOT_FLAG_CMD_MIRROR);
                        }
                        cmdp = rec->sprite[0].cmd;
                        D_800C4D64 = cmdp;
                        if (mode == 0x31) {
                            D_800C4D68 = cmdp;
                        }
                        D_800D212C = rec->sprite[0].cellId;
                        D_800C97F4 = rec->sprite[0].angle;
                        D_800C4D48 = rec->sprite[0].unk28;
                        D_800C9E38 = D_800CA038;
                    }
                    D_800C53A4 = pass;
                    goto after_pass;
                }
                rec++;
            } while (rec < D_800D2BE8[pass].rec);
        }
        if (D_800C53A8 < 4) {
            break;
        }
        pass = !pass;
    }

    {
        rec = D_800D2508[D_800C53A4].rec;
        z = func_800B21EC(rec, mode, 0, 0);
        D_800C4D78 = z;
        gte_SetRotMatrix(&D_800C9838);
        gte_SetTransMatrix(&D_800C9838);
        if (((z >= 0) && (((u32) (D_800D23D8[0] - 1)) >= 2)) && (D_800D23D8[0] != 0xD)) {
            D_800C4D7C = z;
            D_800C9772 = D_800C9870.half;
            if (D_800C4D38 == 0x32) {
                D_800C4D40 = 0;
            }
        } else {
            D_800C4D7C = func_800B21EC(rec, mode, 0xC8, 0);
            D_800C53A8++;
        }
    }
    after_pass:
    func_800A1678(coord, mode);

    for (node = D_800D2284; node != NULL; node = node->next) {
        obj = worldObjectById(node->key, D_800CA030);
        if (obj != NULL) {
            section = &D_800C4D5C[obj->sectionIdx];
            found = (WorldSpriteRec *) section;   /* borrowed as scratch */
            /* index-first subscript and word scaling, as above. */
            hdr = (GlyphHeader *) (((u32 *) found) + (node->slot.b.idx[section->offsets] >> 2));
        } else {
            hdr = D_800C9740;
        }
        setWorldMapTransVector(coord, node->id);
        entryEnd2 = &hdr->entries[hdr->count];
        if (hdr->id < 0x5B) {
            D_800D24A0 = hdr;
            func_800BF20C(entryEnd2, hdr->id, D_800C4D30);
            gte_sttr(tr);
            gte_ldtr(0, 0, 0);
            func_800BFBFC(hdr->id);
            if (((mode == 0x32) && (D_800D23D8[0] != 5)) && (D_800D23D8[0] != 6)) {
                func_800BF80C(hdr, D_800D244C->primList, D_800C9720);
            } else {
                func_800BF2E8(hdr, D_800D244C->primList, D_800C9720);
            }
            gte_SetTransVector(tr);
        } else {
            D_800D24A0 = NULL;
            func_800ACDC4(hdr, D_800D244C, D_800C9720);
        }
    }

    if ((newHead != emptyHead) && (newTail != emptyTail)) {
        link = D_800D2284;
        D_800D2284 = newHead;
        newTail->next = link;
        newHead = NULL;
        if (newTail == NULL) {
            return;
        }
        for (node = D_800D2284; node != NULL; node = node->next) {
            if (node->id == D_800D212C) {
                break;
            }
        }

        if (node != NULL) {
            obj = worldObjectById(node->key, D_800CA030);
            if (obj != NULL) {
                sec = &D_800C4D5C[obj->sectionIdx];
                /* index-first subscript and word scaling, as above. */
                D_800C97A0 = ((u32 *) sec) + (node->slot.b.idx[sec->offsets] >> 2);
            }
        }
    }
}

/**
 * @brief Program the GTE translation vector (TR, control regs 5/6/7) for world-map rendering.
 *
 * Each coordinate packs a coarse tile index (@c /128) and a fine sub-tile offset
 * (@c %128). The view offset is the delta from @p coord0 to @p coord1, taken
 * separately on the sub-tile axis (X) and the tile axis (Z), each wrapped into a
 * half-range so the world map scrolls seamlessly across the wrap seam:
 *  - @c off.vx = wrap(coord1%128 - coord0%128) into [-64,64] (via ±128), scaled *2048
 *  - @c off.vy = 0
 *  - @c off.vz = -wrap(coord1/128 - coord0/128) into [-48,48] (via ±96), scaled *2048
 * The offset is added to the camera base @c D_800DB0E8 and loaded via @c gte_ldtr.
 *
 * @note Handwritten GTE routine. The block-2 conditions re-evaluate the tile delta
 *       @c (coord1/128 - coord0/128) (rather than reusing @c d) to match the original
 *       codegen, which keeps both dividends live through the signed-division rounding.
 *
 * @param coord0 Reference world-map coordinate.
 * @param coord1 Current world-map coordinate; the view tracks the delta to @p coord0.
 */
void setWorldMapTransVector(s16 coord0, s16 coord1) {
    SVECTOR off;
    s32 d;

    d = (coord1 % 128) - (coord0 % 128);
    if (d < 65) {
        if (d < -64) {
            off.vx = (d + 128) * 2048;
        } else {
            off.vx = d * 2048;
        }
    } else {
        off.vx = (d - 128) * 2048;
    }
    off.vy = 0;

    d = (coord1 / 128) - (coord0 / 128);
    if (((coord1 / 128) - (coord0 / 128)) < 49) {
        if (((coord1 / 128) - (coord0 / 128)) < -48) {
            off.vz = -(d + 96) * 2048;
        } else {
            off.vz = -d * 2048;
        }
    } else {
        s32 t = d - 96;
        off.vz = -t * 2048;
    }

    gte_ldtr(D_800DB0E8.vx + off.vx, D_800DB0E8.vy + off.vy, D_800DB0E8.vz + off.vz);
}

/* Wrap a signed cell delta into the shorter way round the WORLD_GRID_W x
   WORLD_GRID_H torus. The scratch @c t is assigned by the macro so the argument is
   evaluated once for the tests; the in-range arm deliberately re-reads @c d instead
   of @c t -- that asymmetry is what the original compiles to, and using @c t there
   costs two instructions. */
#define WRAP_COL(d, t) (((t) = (d)) < WORLD_GRID_W / 2 + 1 ? ((t) < -(WORLD_GRID_W / 2) ? (t) + WORLD_GRID_W : (d)) : (t) - WORLD_GRID_W)
#define WRAP_ROW(d, t) (((t) = (d)) < WORLD_GRID_H / 2 + 1 ? ((t) < -(WORLD_GRID_H / 2) ? (t) + WORLD_GRID_H : (d)) : (t) - WORLD_GRID_H)
/** Absolute value; re-evaluates its argument, so the wrap above runs three times
    per axis exactly as the original does. */
#define CELL_ABS(x) ((x) > 0 ? (x) : 0 - (x))
/** Column/row distance from the camera cell @p c to node @p n. The @c (s16) cast is
    load-bearing: it truncates the column to 16 bits (the @c sll/sra pair) before the
    subtraction. @c ROW_DELTA caches the node id in @c w, which the vertical delta
    below reuses -- that shared variable is what places the id in its register. */
#define COL_DELTA(c, n) ((c) % WORLD_GRID_W - (s16)((n)->id % WORLD_GRID_W))
/** The same column distance measured the other way, from node @p n back to the
    camera cell @p c; this is the direction the per-node delta is built in. Since
    @c id is already @c s16 the cast cannot change the value -- it exists purely
    to force the truncation, and dropping it costs the @c sll/sra pair. */
#define COL_DELTA_FROM(n, c) ((s16)((n)->id % WORLD_GRID_W) - (c) % WORLD_GRID_W)
#define ROW_DELTA(c, n) ((c) / WORLD_GRID_W - (w = (n)->id) / WORLD_GRID_W)

/** Cell size in world units; a wrapped cell delta scales up by this. */
#define CELL_UNITS_SHIFT 11
/** Index of the corner block and the probe block within a @c ProbeFrame, counted
    in @c ProbeVec units from @c &frame.delta. Indexing off that one base is what
    makes the compiler derive the two pointers the original walks. */
#define FRAME_CORNER 1
#define FRAME_PROBE  7
/** First corner of the batch-0 half of @c CellCorners; batch 1 starts at 0. */
#define CORNER_BATCH0 3
/** A projected corner counts as on screen below this X. */
#define SCREEN_X_MAX 0x141
/** Depth cut: map 0x32 keeps cells twice as far out as the rest. */
#define DEPTH_CUT_WIDE   0x2000
#define DEPTH_CUT_NORMAL 0x1801

/**
 * @brief Drop world objects whose cell has scrolled out of view.
 *
 * Walks the active object list @c D_800D2284 and keeps each node that is either
 * near the camera cell @p col or still projects on screen; the rest are unlinked.
 *
 * A node within @p lim cells of the camera on both axes (torus-wrapped, so the
 * short way round) is kept outright. Otherwise its eight probe corners are
 * projected in two batches of three through @c rtpt: batch 0 uses corners 3..5
 * for the node itself, and batch 1 rebuilds the delta for @c node->next and uses
 * corners 0..2. A node survives if any projected corner lands within screen X
 * and nearer than the depth cut (wide on map 0x32, normal elsewhere).
 *
 * @c lim is 3 for the wide maps (0x32 and 0x40..0x42) and 1 elsewhere.
 *
 * @param col  Camera cell index (row * 0x80 + column).
 * @param mode World dispatch code, selecting @c lim and the depth cut.
 */
static void func_800A1678(s32 col, s32 mode) {
    ProbeFrame f;
    s32 otz;
    ScreenXY *sp;
    WorldObject *node;
    WorldObject *prev;
    s32 lim;
    s32 d;
    s32 tz;
    ProbeVec *pp;
    ProbeVec *q;
    s32 j;
    s32 batch;
    s32 w;
    s32 nz;

    f.c = D_800980EC;
    if (mode == 0x32 || (mode >= 0x40 && mode < 0x43)) {
        lim = 3;
    } else {
        lim = 1;
    }
    gte_SetRotMatrix(&D_800C9838);
    gte_SetTransVector(D_800C9838.t);

    d = COL_DELTA_FROM(D_800D2284, col);
    d = WRAP_COL(d, tz) << CELL_UNITS_SHIFT;
    f.delta.vx = d;
    f.delta.vy = 0;
    w = D_800D2284->id;
    d = w / WORLD_GRID_W - col / WORLD_GRID_W;
    nz = 0 - WRAP_ROW(d, tz);
    f.delta.vz = nz << CELL_UNITS_SHIFT;
    /* probe[j] = corner[j] + delta */
    pp = &f.delta;
    for (j = 0; j < 3; j++) {
        pp[j + FRAME_PROBE].vx = pp[j + FRAME_CORNER].vx + f.delta.vx;
        pp[j + FRAME_PROBE].vy = pp[j + FRAME_CORNER].vy + f.delta.vy;
        pp[j + FRAME_PROBE].vz = pp[j + FRAME_CORNER].vz + f.delta.vz;
    }

    prev = NULL;
    node = D_800D2284;
    while (node != NULL) {
        if (CELL_ABS(WRAP_COL(COL_DELTA(col, node), tz)) <= lim) {
            if (CELL_ABS(WRAP_ROW(ROW_DELTA(col, node), tz)) <= lim) {
                batch = 0;
                goto keep;
            }
        }
        {
            ScreenXY screen[3];

            for (batch = 0; batch < 2; batch++) {
                gte_ldv3c(f.probe);
                gte_rtpt();
                if (batch != 0) {
                    d = COL_DELTA_FROM(node->next, col);
                    d = WRAP_COL(d, tz) << CELL_UNITS_SHIFT;
                    f.delta.vx = d;
                    f.delta.vy = 0;
                    w = node->next->id;
                    d = w / WORLD_GRID_W - col / WORLD_GRID_W;
                    nz = 0 - WRAP_ROW(d, tz);
                    f.delta.vz = nz << CELL_UNITS_SHIFT;
                    q = f.c.v;
                } else {
                    q = &f.c.v[CORNER_BATCH0];
                }
                pp = &f.delta;
                for (j = 0; j < 3; j++) {
                    pp[j + FRAME_PROBE].vx = q[j].vx + f.delta.vx;
                    pp[j + FRAME_PROBE].vy = q[j].vy + f.delta.vy;
                    pp[j + FRAME_PROBE].vz = q[j].vz + f.delta.vz;
                }
                gte_stsxy3c(screen);
                gte_stszotz(&otz);
                for (j = 0, sp = screen; j < 3; j++, sp++) {
                    if (sp->sx < SCREEN_X_MAX) {
                        if (otz < (mode == 0x32 ? DEPTH_CUT_WIDE : DEPTH_CUT_NORMAL)) {
                            goto keep;
                        }
                    }
                }
            }
            if (prev != NULL) {
                node = (prev->next = node->next);
            } else {
                node = (D_800D2284 = node->next);
            }
            continue;
        }
    keep:
        prev = node;
        node = node->next;
        if (batch == 0) {
            d = COL_DELTA_FROM(node, col);
            d = WRAP_COL(d, tz) << CELL_UNITS_SHIFT;
            f.delta.vx = d;
            f.delta.vy = 0;
            w = node->id;
            d = w / WORLD_GRID_W - col / WORLD_GRID_W;
            /* dead: recomputing the column delta here anchors the register
               allocation the original used for the vertical delta. */
            tz = COL_DELTA(col, node);
            nz = 0 - WRAP_ROW(d, tz);
            f.delta.vz = nz << CELL_UNITS_SHIFT;
            pp = &f.delta;
            for (j = 0; j < 3; j++) {
                pp[j + FRAME_PROBE].vx = pp[j + FRAME_CORNER].vx + f.delta.vx;
                pp[j + FRAME_PROBE].vy = pp[j + FRAME_CORNER].vy + f.delta.vy;
                pp[j + FRAME_PROBE].vz = pp[j + FRAME_CORNER].vz + f.delta.vz;
            }
        }
    }
}

/**
 * @brief Build the list of world-map cells covered by the camera's visibility box.
 *
 * Reads the composed world position @c D_800D23C0 and forms a symmetric box
 * of half-size 0x27FF around it (the +0x5D801/+0x627FF and +0x45801/+0x4A7FF
 * corner offsets are +1.5 wraps -/+ 0x27FF, which keeps the coordinates
 * positive before the wrap). Corner coordinates wrap on the world torus
 * (0x40000 x 0x30000 units, 0x800-unit cells, so a 128 x 96 cell grid) and
 * are packed as cell indices (col + row * 128). The wrapped column/row deltas
 * between the two corners give the box extent, walked inclusively per axis.
 *
 * For every covered cell one @c WorldObject record is appended to the pool at
 * @c D_800C9888: @c id = cell index, @c key = 32x24 sector key
 * ((row / 4) * 0x20 + col / 4), @c slot.b.idx = 4x4 sub-cell index
 * ((row %% 4) * 4 + col %% 4). The list is then null-terminated and published
 * as the active world-object list head @c D_800D2284.
 */
static void func_800A1F10(void)
{
    WorldObject *rec;
    s32 x;
    s32 c1;
    s32 c2;
    s32 col;
    s32 row;
    s32 ncols;
    s32 nrows;
    s32 i;
    s32 j;
    s32 d;

    rec = D_800C9888;
    x = D_800D23C0.x;

    c1 = ((x + 0x5D801) % 0x40000) / 0x800 + (((D_800D23C0.y + 0x45801) % 0x30000) / 0x800) * 0x80;
    c2 = ((x + 0x627FF) % 0x40000) / 0x800 + (((D_800D23C0.y + 0x4A7FF) % 0x30000) / 0x800) * 0x80;

    d = c2 % 0x80 - c1 % 0x80;
    if (d < 0x41) {
        if (d < -0x40) {
            d += 0x80;
        }
    } else {
        d -= 0x80;
    }
    if (d > 0) {
        d = c2 % 0x80 - c1 % 0x80;
        if (d < 0x41) {
            if (d < -0x40) {
                d += 0x80;
            }
        } else {
            d -= 0x80;
        }
        ncols = d;
    } else {
        d = c2 % 0x80 - c1 % 0x80;
        if (d < 0x41) {
            if (d < -0x40) {
                d += 0x80;
            }
        } else {
            d -= 0x80;
        }
        ncols = -d;
    }
    d = c2 / 0x80 - c1 / 0x80;
    if (d < 0x31) {
        if (d < -0x30) {
            d += 0x60;
        }
    } else {
        d -= 0x60;
    }
    if (d > 0) {
        d = c2 / 0x80 - c1 / 0x80;
        if (d < 0x31) {
            if (d < -0x30) {
                d += 0x60;
            }
        } else {
            d -= 0x60;
        }
        nrows = d;
    } else {
        d = c2 / 0x80 - c1 / 0x80;
        if (d < 0x31) {
            if (d < -0x30) {
                d += 0x60;
            }
        } else {
            d -= 0x60;
        }
        nrows = -d;
    }

    row = c1 / 0x80;
    for (i = 0; i <= nrows; i++) {
        col = c1 % 0x80;
        for (j = 0; j <= ncols; j++) {
            while (col >= 0x80) {
                col -= 0x80;
            }
            while (row >= 0x60) {
                row -= 0x60;
            }
            while (col < 0) {
                col += 0x80;
            }
            while (row < 0) {
                row += 0x60;
            }
            rec->id = row * 0x80 + col;
            rec->key = (row / 4) * 0x20 + col / 4;
            rec->slot.b.idx = (row % 4) * 4 + col % 4;
            rec->next = rec + 1;
            rec++;
            col++;
        }
        row++;
    }
    (rec - 1)->next = 0;
    D_800D2284 = D_800C9888;
}

/**
 * @brief Initialize the world's two double-buffered graphics contexts.
 *
 * Sets up the draw/display environments for both @c BattleSceneCtx buffers
 * (the sentinel @c D_800CA040 and its successor at @c +0x4070) at VRAM x=0 /
 * x=384, sized by @c D_800C97EA x @c D_800C97E8. Then, for each buffer, clears
 * the 0x1000-entry ordering table, patches the display screen region to the
 * NTSC active area (@c y=8, @c h=224), and enables dithering. Finally installs
 * the first buffer as the active scene context (@c D_800D244C).
 */
static void initWorldDoubleBuffer(void) {
    s32 i;

    SetDefDrawEnv(&(&D_800CA040)[1].drawEnv, 0, 0, D_800C97EA, D_800C97E8);
    SetDefDrawEnv(&(&D_800CA040)[0].drawEnv, 384, 0, D_800C97EA, D_800C97E8);
    SetDefDispEnv(&(&D_800CA040)[1].disp, 384, 0, D_800C97EA, D_800C97E8);
    SetDefDispEnv(&(&D_800CA040)[0].disp, 0, 0, D_800C97EA, D_800C97E8);

    for (i = 0; i < 2; i++) {
        ClearOTagR((u32 *)(&D_800CA040)[i].primList, 0x1000);
        (&D_800CA040)[i].disp.screen.y = 8;
        (&D_800CA040)[i].disp.screen.h = 224;
        (&D_800CA040)[i].drawEnv.dtd = 1;
    }

    D_800D244C = &D_800CA040;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A246C);

/**
 * @brief Program the GTE for world-map rendering: screen offset, back color, color matrix.
 *
 * Sets the GTE projection screen offset to half the worldmap screen dimensions
 * (@c D_800C97EA, @c D_800C97E8), copies the background (ambient) color
 * @c D_800C53F8 into the active back-color cache @c D_800DB0E0 and programs it
 * via @c SetBackColor, then copies the lighting color matrix @c D_800C5428 into
 * @c D_800DA8B0 and loads it via @c SetColorMatrix.
 */
static void setupWorldRenderParams(void) {
    ResetGraph(3);
    SetGeomOffset((s16)D_800C97EA / 2, (s16)D_800C97E8 / 2);

    D_800DB0E0.r = D_800C53F8.r;
    D_800DB0E0.g = D_800C53F8.g;
    D_800DB0E0.b = D_800C53F8.b;
    SetBackColor(D_800DB0E0.r, D_800DB0E0.g, D_800DB0E0.b);

    D_800DA8B0 = D_800C5428;
    SetColorMatrix(&D_800DA8B0);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A26E8);

/**
 * @brief Pick the best command descriptor for a sprite and install it.
 *
 * The scoring sibling of @c glyphAt, driven by the projected probe point the
 * caller stages in the GTE scratchpad. Two-phase search, both phases hit-testing
 * with @c func_800BF024:
 *
 *  1. The descriptor cache @c D_800D24A8, filtered to entries resolved for
 *     @p key. For dispatch codes 0x30/0x32 the lowest hit angle wins (code
 *     0x30 additionally requires type 0x1E..0x22 while the camera-follow
 *     reference @c D_800C9870 is active); for any other code the first hit is
 *     taken outright. A cache hit that fails the visibility gate
 *     (@c func_800A45D8) still ends the search — it only reports no pick.
 *  2. Only when the cache had no hit at all: the glyph's own @c entries[].
 *     Candidates must pass a mode filter (codes 0x30/0x32 as above, low types
 *     always accepted, otherwise the hit angle must lie within 0xC8 of the
 *     sprite's current @c angle). The best candidate — smallest hit angle, or
 *     smallest deviation from the sprite's angle for plain codes — is inserted
 *     at the head of the cache (older entries shift down, the oldest drops
 *     out), then the visibility gate decides whether the pick stands.
 *
 * The winning descriptor, its hit angle and its packed low code half are
 * written to @p st (@c cmd / @c angle / @c code) even when the gate rejected
 * the pick (then @c cmd is NULL).
 *
 * @param glyph Object glyph header owning the candidate @c entries[].
 * @param st    Sprite receiving the pick.
 * @param key   Grid-cell key the cache entries must have been resolved for.
 * @param end   Descriptor-table end bound, passed through to the hit test.
 * @return 1 when a descriptor was picked and passed the visibility gate, else 0.
 */
static s32 func_800A2920(GlyphHeader *glyph, WorldSprite *st, s16 key, CmdDesc *end) {
    FeaEntry40C0 *e;
    int ok;
    s32 code;
    CmdDesc *g;
    FeaEntry40C0 *slot;
    s32 found;
    CmdDesc *best;
    s32 bestAngle;
    s32 bestMetric;
    s32 mode;
    s32 prevAngle;
    s32 packed;
    s16 want;
    s32 metric;
    AngleSlot res1;
    AngleSlot res2;
    VECTOR *point;

    point = WORLD_PROBE_POINT;
    found = 0;
    best = NULL;
    bestAngle = 0x7FFFFFFF;
    mode = D_800C4D38;
    prevAngle = st->angle;
    want = key;
    /* regalloc: `ok` must be a real assignment — folding the test into the
       `if` costs the frame layout — and `found = 1` must precede `best = ...`
       in both arms or best/prevAngle swap s5/s6. The (s16) casts on the gate's
       mode argument are load-bearing: the original truncates the dispatch
       code at each call (the sll/sra pair). */
    for (e = D_800D24A8; e < &D_800D24A8[12]; e++) {
        ok = e->val != NULL;
        if (!ok) {
            continue;
        }
        if (want != e->hval) {
            continue;
        }
        if (!func_800BF024(e->val, point, &res1, end)) {
            continue;
        }
        if (mode == 0x30) {
            if (!(e->val->type >= 0x1E && e->val->type < 0x23)) {
                if (D_800C9870.word > 0) {
                    continue;
                }
            }
        }
        if (found == 0) {
            found = -1;
        }
        if (mode == 0x30 || mode == 0x32) {
            code = e->val->type | e->val->flag << 8 | e->val->param << 16;
            if (res1.word < bestAngle) {
                packed = code;
                bestAngle = res1.word;
                if (func_800A45D8(code, (s16)mode)) {
                    found = 1;
                    best = e->val;
                }
            }
        } else {
            packed = e->val->type | e->val->flag << 8 | e->val->param << 16;
            bestAngle = res1.word;
            if (func_800A45D8(packed, (s16)mode)) {
                found = 1;
                best = e->val;
            }
            break;
        }
    }

    bestMetric = 0x7FFFFFFF;
    if (found == 0) {
        g = glyph->entries;
        for (; g < &glyph->entries[glyph->count]; g++) {
            if (!func_800BF024(g, point, &res2, end)) {
                continue;
            }
            if (mode == 0x32
                || (mode == 0x30 && ((g->type >= 0x1E && g->type < 0x23) || D_800C9868.z <= 0))
                || g->type < 6
                || D_800C4D64->type < 6
                || (mode != 0x30 && mode != 0x32
                    && (res2.word - prevAngle > 0 ? res2.word - prevAngle < 0xC8
                                                  : prevAngle - res2.word < 0xC8))) {
                code = g->type | g->flag << 8 | g->param << 16;
                if (mode != 0x30 && mode != 0x32) {
                    metric = res2.word - prevAngle;
                    if (metric <= 0) {
                        metric = prevAngle - res2.word;
                    }
                } else {
                    metric = res2.word;
                }
                if (metric < bestMetric) {
                    found++;
                    bestMetric = metric;
                    best = g;
                    packed = code;
                    bestAngle = res2.word;
                }
            }
        }

        if (found != 0) {
            if (best != NULL) {
                for (slot = &D_800D24A8[10]; !(slot < D_800D24A8); slot--) {
                    slot[1] = *slot;
                }
                slot++;
                slot->val = best;
                slot->hval = key;
            }
            if (!func_800A45D8(packed, (s16)mode)) {
                best = NULL;
                found = 0;
            }
        }
    }
    st->code = packed;
    st->angle = bestAngle;
    st->cmd = best;
    return found > 0;
}

/**
 * @brief Probe up to 8 rotations for a free placement of a 5-sprite fan
 *        around the camera, and report where it landed.
 *
 * Gate: the active command descriptor @c D_800C4D64 must enable the current
 * dispatch code @c D_800C4D38 (bit chosen by code range 0x20..0x28 / 0x84,
 * 0x30, 0x31, 0x32; everything else passes; @c D_800C4D20 == 0 force-passes
 * the byte gates). The fan size is 0x40 for codes 0x30 / 0x20..0x28 / 0x84,
 * 0x100 for 0x32, else 0x20.
 *
 * Each pass rotates the probe angle @c yang.vy by +0x200 (starting at
 * @c angles->vy + 0x400), transforms @c ang by it, and places sprite 0 at the
 * camera @c D_800C9868 plus that offset with sprites 1..4 spread by
 * @c size in +-X / +-Z. Every sprite is projected with @c worldPosToCell and
 * tagged @c WORLD_SPRITE_PLACED. Any glyph found under a sprite re-runs the command
 * gates for that glyph and checks its angle word against the camera angle
 * @c D_800C97F4 (within 0xC8); a failed gate abandons the whole pass (not
 * just that sprite). Glyph hits overwrite the sprite's angle fields, and a
 * hit on sprite 0 records the full angle word for the caller.
 *
 * The second phase re-places the fan from the camera-relative position
 * (@c func_800BC544) spun by the scene angle bias (@c func_800B0010 /
 * @c func_800BC5E0 / @c func_800B5ADC spread points), then asks
 * @c func_800B21EC to accept the record: negative means placed — the probe
 * writes sprite 0's position (Y = the recorded angle word) to @p hitPos and
 * returns 1. Otherwise the accept code is written to @p hitCell and the next
 * pass runs.
 *
 * @note Dead code as far as static analysis shows: the only callers are
 * @c func_8009C294 and the asm @c func_800B4AA0, and neither is referenced by
 * any jal, address word, or C call site in the executable or any overlay.
 * Together with @c D_800C9778 and several functions only @c func_800B4AA0
 * calls, they form an unreferenced island — likely a feature dropped before
 * ship. Dispatch from runtime-loaded data cannot be fully ruled out.
 *
 * @param code    Command/dispatch code being placed (same domain as
 *                @c D_800C4D38).
 * @param angZ    Z rotation for the probe offset vector.
 * @param angles  Base angles; @c angles->vy seeds the probe rotation.
 * @param hitPos  Out: sprite-0 world position on success (may be NULL).
 * @param arg4    Unused.
 * @param hitCell Out: last non-negative accept code (may be NULL).
 * @return 1 when a placement was accepted, else 0 after 8 passes.
 */
s32 func_800A2D50(s32 code, s32 angZ, SVECTOR *angles, VECTOR *hitPos, s32 arg4, s32 *hitCell) {
    WorldSpriteRec rec;
    s32 camY;
    s32 two;
    u16 angBase;
    s32 d;
    SVECTOR ang;
    SVECTOR yang;
    VECTOR camPos;
    MATRIX m;
    VECTOR xf;
    MATRIX m2;
    VECTOR xf2;
    SVECTOR pts[WORLD_FAN_SPRITES - 1];
    SVECTOR spin;
    AngleSlot slot;
    s32 pass;
    s32 saved;
    SVECTOR *pang;
    MATRIX *pm;
    WorldSprite *base;
    WorldSprite *e;
    WorldSprite *w;
    WorldSprite *p;
    s32 size;
    s32 i;
    s32 j;
    s32 best;
    s32 ctx;
    s32 ret;
    u32 packed;
    s32 ok;
    u16 mode;
    s16 kind;
    u16 op;
    u16 gmode;
    u16 ang16;

    func_80047CE4(&ang, 0, 8);
    ang.vz = angZ;
    func_80047CE4(&yang, 0, 8);

    packed = D_800C4D64->type | (D_800C4D64->flag << 8) | (D_800C4D64->param << 16);
    mode = D_800C4D38;
    if (D_800C4D20 == 0) {
        packed |= 0xFFFFFF00;
    }
    if (((u16)(mode - 0x20) < 9) || ((s16)mode == 0x84)) {
        ok = (packed >> 16) & CMDPAR_MODE_20_28;
    } else if ((s16)mode == 0x30) {
        ok = (packed >> 16) & CMDPAR_MODE_30;
    } else if ((s16)mode == 0x31) {
        ok = (packed >> 16) & CMDPAR_MODE_31;
    } else if ((s16)mode == 0x32) {
        ok = (packed >> 8) & CMDFLAG_MODE_32;
    } else {
        ok = 1;
    }
    if (ok == 0) {
        return 0;
    }

    if (code != 0x32) {
        size = 0x40;
        if ((code != 0x30) && ((u32)(code - 0x20) >= 9) && (code != 0x84)) {
            size = 0x20;
        }
    } else {
        size = 0x100;
    }

    pass = 0;
    pm = &m;
    yang.vy = angles->vy + 0x400;

    do {
        best = -1;
        /* base and pang are the loop's two single-set invariants: written here
         * so loop.c hoists them in this order, which is what fills the size
         * dispatch's delay slots with the sprite base address. */
        base = rec.sprite;
        pang = &ang;
        RotMatrix(&yang, pm);
        gte_SetRotMatrix(pm);
        m.t[2] = 0;
        m.t[1] = 0;
        m.t[0] = 0;
        gte_SetTransMatrix(pm);
        e = base;
        gte_ldv0(pang);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&xf);

        for (j = 0; j < WORLD_FAN_SPRITES; j++, e++) {
            /* Plain 2, not WORLD_SPRITE_PLACED: this same variable is the
               index compared below, and holding it in a callee-saved
               register is what lets the flag store share the constant. */
            two = 2;
            e->pos.vx = D_800C9868.x + xf.vx;
            e->pos.vy = D_800C9868.z + xf.vy;
            camY = D_800C9868.y;
            e->pos.vz = xf.vz - camY;
            if (j != 0) {
                if (j == 1) {
                    e->pos.vx -= size;
                } else if (j == two) {
                    e->pos.vz += size;
                } else if (j == 3) {
                    e->pos.vx += size;
                } else {
                    e->pos.vz -= size;
                }
            }
            e->cellId = worldPosToCell(&e->pos, &e->cell);
            e->flag = WORLD_SPRITE_PLACED;
            e->unk28 = yang.vy;
        }

        kind = code;
        for (p = rec.sprite; p < &rec.sprite[WORLD_FAN_SPRITES]; p++) {
            /* the sprite's pos + cell pair coincides with the GlyphQuery layout */
            CmdDesc *g = glyphAt((GlyphQuery *)p, &slot);
            if (g != NULL) {
                packed = g->type | (g->flag << 8) | (g->param << 16);
                op = code;
                if (D_800C4D20 == 0) {
                    ok = 1;
                } else if ((op < 0xA) || (kind == 0x80)) {
                    ok = (packed >> 16) & CMDPAR_VIS_00_09;
                } else if (((u16)(code - 0x20) < 9) || (kind == 0x84)) {
                    ok = (packed >> 16) & CMDPAR_VIS_20_28;
                } else if (kind == 0x30) {
                    ok = (packed >> 16) & CMDPAR_VIS_30;
                } else if (kind == 0x31) {
                    ok = (packed >> 16) & CMDPAR_VIS_31;
                } else {
                    ok = 1;
                }
                if (ok == 0) {
                    goto nextPass; /* a failed gate abandons the whole pass */
                }
                gmode = D_800C4D38;
                if (D_800C4D20 == 0) {
                    packed |= 0xFFFFFF00;
                }
                if (((u16)(gmode - 0x20) < 9) || ((s16)gmode == 0x84)) {
                    ok = (packed >> 16) & CMDPAR_MODE_20_28;
                } else if ((s16)gmode == 0x30) {
                    ok = (packed >> 16) & CMDPAR_MODE_30;
                } else if ((s16)gmode == 0x31) {
                    ok = (packed >> 16) & CMDPAR_MODE_31;
                } else if ((s16)gmode == 0x32) {
                    ok = (packed >> 8) & CMDFLAG_MODE_32;
                } else {
                    ok = 1;
                }
                if (ok == 0) {
                    goto nextPass;
                }
                d = slot.word - D_800C97F4;
                if (d > 0) {
                    ok = d < 0xC8;
                } else {
                    ok = (D_800C97F4 - (s32)slot.word) < 0xC8;
                }
                if (ok == 0) {
                    goto nextPass;
                }
                ang16 = slot.half;
                p->angle = ang16;
                p->cell.vy = ang16;
                p->pos.vy = (s16)ang16; /* sign-extended on purpose */
                if (p == base) {
                    saved = slot.word;
                }
            }
        }

        func_800BC544((VECTOR *)&D_800C9868, &camPos);
        camPos.vy = saved;
        ctx = func_800B0010(code);
        angBase = angles->vy;
        RotMatrix(&yang, &m2);
        gte_SetRotMatrix(&m2);
        m2.t[2] = 0;
        m2.t[1] = 0;
        m2.t[0] = 0;
        gte_SetTransMatrix(&m2);
        w = base;
        gte_ldv0(pang);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&xf2);

        func_800B5ADC(ctx, pts, 0, 0);
        ret = func_800BC5E0(ctx);
        spin.vx = 0;
        spin.vz = 0;
        spin.vy = angBase + ret;
        RotMatrix(&spin, &m2);
        gte_SetRotMatrix(&m2);
        gte_SetTransVector(&xf2);

        /* Sprite 0 mixes the array and pointer spellings — this reproduces the
         * original's interleaved sp-constant / register addressing, and the
         * literal &rec.sprite[1] re-init (rather than w++) is what lets the
         * loop below walk from a constant while w itself stays a copy of base. */
        rec.sprite[0].pos.vx = camPos.vx + xf2.vx;
        w->pos.vy = camPos.vy + xf2.vy;
        w->pos.vz = camPos.vz + xf2.vz;
        w->cellId = worldPosToCell(&w->pos, &rec.sprite[0].cell);
        w->flag = WORLD_SPRITE_PLACED;
        w = &rec.sprite[1];

        for (i = 1; i < WORLD_FAN_SPRITES; i++, w++) {
            gte_ldv0(&pts[i - 1]);
            gte_mvmva(1, 0, 0, 0, 0);
            gte_stlvnl(&xf2);
            w->pos.vx = camPos.vx + xf2.vx;
            w->pos.vy = camPos.vy + xf2.vy;
            w->pos.vz = camPos.vz + xf2.vz;
            w->cellId = worldPosToCell(&w->pos, &w->cell);
            w->flag = WORLD_SPRITE_PLACED;
        }

        best = func_800B21EC(&rec, code, 0, 1);
        if (best < 0) {
            if (hitPos != NULL) {
                *hitPos = rec.sprite[0].pos;
                hitPos->vy = saved;
            }
            gte_SetRotMatrix(&D_800C9838);
            gte_SetTransMatrix(&D_800C9838);
            return 1;
        }
    nextPass:
        yang.vy += 0x200;
        if (hitCell != NULL) {
            if (best >= 0) {
                *hitCell = best;
            }
        }
        pass++;
    } while (pass < 8);

    gte_SetRotMatrix(&D_800C9838);
    gte_SetTransMatrix(&D_800C9838);
    return 0;
}

/**
 * @brief Place a five-sprite fan around @p slot and hand it to the emitter.
 *
 * Sprite 0 sits exactly on the slot's own position: the fan is built by
 * transforming the zero vector through an identity rotation, so the offset it
 * yields is (0,0,0). Sprites 1..4 are spread around it — @c func_800B5ADC
 * supplies four offsets for the scene, and each is rotated by the slot's
 * heading (@c vec.vy) biased by @c func_800BC5E0 before being added to the
 * slot position. Every sprite gets its grid cell from @c worldPosToCell and is
 * marked placed (@c flag @c = @c 2). The GTE is left holding @c D_800C9838
 * again, which is what the rest of the world renderer expects.
 *
 * @param kind   Dispatch code; selects the scene for the spread and the bias.
 * @param slot   Slot whose @c position anchors the fan.
 * @param angles Slot heading; only @c vy is read, as an unsigned angle.
 * @param flag   Passed through to @c func_800B21EC.
 * @return @c func_800B21EC's result — negative when the fan was rejected.
 *
 * @note @c base and @c pang are single-set loop invariants written before the
 *       rotation so loop.c hoists them in this order; @c e is a copy of
 *       @c base rather than @c rec.sprite directly. @c pa carries the spread
 *       array into the call and is then rebound, which stops cse sharing the
 *       array's address with the loop's base and keeps that base out of the
 *       registers live across these three calls. Both idioms are the ones
 *       @c func_800A2D50 uses for the same fan-building sequence.
 */
s32 func_800A358C(s32 kind, SlotEntry *slot, SVECTOR *angles, s32 flag) {
    WorldSpriteRec rec;
    SVECTOR ang;
    SVECTOR yang;
    MATRIX m;
    VECTOR xf;
    SVECTOR pts[WORLD_FAN_SPRITES - 1];
    SVECTOR spin;
    SVECTOR *pang;
    WorldSprite *base;
    WorldSprite *e;
    SVECTOR *pa;
    s32 ctx;
    u16 angBase;
    s32 ret;
    s32 i;

    func_80047CE4(&ang, 0, sizeof(ang));
    func_80047CE4(&yang, 0, sizeof(yang));
    ctx = func_800B0010(kind);
    angBase = angles->vy;
    base = rec.sprite;
    pang = &ang;
    RotMatrix(&yang, &m);
    gte_SetRotMatrix(&m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    gte_SetTransMatrix(&m);
    e = base;
    gte_ldv0(pang);
    gte_mvmva(1, 0, 0, 0, 0);
    gte_stlvnl(&xf);

    pa = pts;
    func_800B5ADC(ctx, pa, 0, 0);
    pa = NULL;
    ret = func_800BC5E0(ctx);
    spin.vx = 0;
    spin.vz = 0;
    spin.vy = angBase + ret;
    RotMatrix(&spin, &m);
    gte_SetRotMatrix(&m);
    gte_SetTransVector(&xf);

    e->pos.vx = slot->position.vx + xf.vx;
    e->pos.vy = slot->position.vy + xf.vy;
    e->pos.vz = slot->position.vz + xf.vz;
    e->cellId = worldPosToCell(&e->pos, &e->cell);
    e->flag = WORLD_SPRITE_PLACED;
    e++;

    for (i = 1; i < WORLD_FAN_SPRITES; i++, e++) {
        gte_ldv0(&pts[i - 1]);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&xf);
        e->pos.vx = slot->position.vx + xf.vx;
        e->pos.vy = slot->position.vy + xf.vy;
        e->pos.vz = slot->position.vz + xf.vz;
        e->cellId = worldPosToCell(&e->pos, &e->cell);
        e->flag = WORLD_SPRITE_PLACED;
    }

    ret = func_800B21EC(&rec, kind, 0, flag);
    gte_SetRotMatrix(&D_800C9838);
    gte_SetTransMatrix(&D_800C9838);
    return ret;
}

/**
 * @brief Resolve the command descriptor (glyph) for the projected world query @p v.
 *
 * Fetches the object glyph header for the query's cell key (@c v->buf.angle)
 * via @c func_800A5EC4, then searches two tables for the descriptor whose
 * region contains the projected point @c v->buf.proj (tested by
 * @c func_800BF024): first the @c D_800D24A8 cell cache, then the header's own
 * @c entries[]. On a hit the test's result word is copied to @p out (when
 * non-NULL) and the matching @c CmdDesc is returned; otherwise NULL.
 *
 * @param v   Projected world query (position + projection + cell key).
 * @param out Optional slot to receive the hit's result word.
 * @return The matching command descriptor, or NULL.
 */
CmdDesc *glyphAt(GlyphQuery *v, AngleSlot *out) {
    GlyphHeader *hdr;
    FeaEntry40C0 *e = &D_800D24A8[0];
    CmdDesc *g;
    AngleSlot res1, res2;

    hdr = (GlyphHeader *)func_800A5EC4(v->buf.angle);
    if (hdr == NULL) {
        return NULL;
    }

    for (; e < &D_800D24A8[12]; e++) {
        if (e->val != 0 && v->buf.angle == e->hval) {
            if (func_800BF024(e->val, &v->buf.proj, &res1, &hdr->entries[hdr->count])) {
                if (out != NULL) {
                    *out = res1;
                }
                return e->val;
            }
        }
    }

    for (g = &hdr->entries[0]; g < &hdr->entries[hdr->count]; g++) {
        if (func_800BF024(g, &v->buf.proj, &res2, &hdr->entries[hdr->count])) {
            if (out != NULL) {
                *out = res2;
            }
            return g;
        }
    }

    return NULL;
}

/**
 * @brief Re-place an existing sprite fan around its own anchor, then flatten it.
 *
 * Reads the fan's own first sprite for both inputs: @c unk28 becomes the Y angle
 * of the primary rotation, and @c pos is the anchor the five sprites are spread
 * around. The spread itself is built exactly as @c placeWorldSpriteFan does —
 * @c func_800B5ADC supplies four offsets for the current map's scene and
 * @c func_800BC5E0 a per-scene angle bias applied on top of @c D_800C977A, the
 * camera-follow heading. Every sprite is re-projected with @c worldPosToCell and
 * marked placed. A final pass overwrites all five heights with @p h, which is
 * what settles the fan onto the terrain.
 *
 * @param out Fan of @c WORLD_FAN_SPRITES sprites; sprite 0 supplies the inputs.
 * @param h   Height forced into every sprite's @c pos.vy afterwards.
 *
 * @note @c pv and @c pa are the fan idiom shared with @c placeWorldSpriteFan and
 *       @c func_800A358C: @c pv carries the zero vector's address to @c gte_ldv0,
 *       and @c pa carries the spread array into the call and is then rebound, so
 *       cse cannot share that array's address with the loop's base and strand it
 *       in a register across the calls. The self-truncating @c ctx assignment is
 *       load-bearing too — it emits the @c sll/sra pair after the scene lookup,
 *       and splitting it into two variables loses the match.
 */
static void func_800A39BC(WorldSprite *out, s16 h) {
    SVECTOR v;
    SVECTOR rot;
    SVECTOR ang;
    VECTOR pos;
    MATRIX m;
    VECTOR xf;
    SVECTOR pts[WORLD_FAN_SPRITES - 1];
    SVECTOR yang;
    WorldSprite *e;
    s32 ctx;
    u16 angBase;
    s32 ret;
    SVECTOR *pa;
    SVECTOR *pv;
    s32 i;
    s32 j;

    func_80047CE4(&v, 0, sizeof(v));
    func_80047CE4(&ang, 0, sizeof(ang));
    ang.vy = out->unk28;
    rot = ang;
    pos = out->pos;
    ctx = func_800B0010(D_800C4D38);
    angBase = D_800C977A;
    ctx = (s16)ctx;
    pv = &v;
    RotMatrix(&rot, &m);
    gte_SetRotMatrix(&m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    gte_SetTransMatrix(&m);
    gte_ldv0(pv);
    gte_mvmva(1, 0, 0, 0, 0);
    gte_stlvnl(&xf);

    pa = pts;
    func_800B5ADC(ctx, pa, 0, 0);
    pa = NULL;
    ret = func_800BC5E0(ctx);
    yang.vx = 0;
    yang.vz = 0;
    yang.vy = angBase + ret;
    RotMatrix(&yang, &m);
    gte_SetRotMatrix(&m);
    gte_SetTransVector(&xf);

    e = out;
    e->pos.vx = pos.vx + xf.vx;
    e->pos.vy = pos.vy + xf.vy;
    e->pos.vz = pos.vz + xf.vz;
    e->cellId = worldPosToCell(&e->pos, &e->cell);
    e->flag = WORLD_SPRITE_PLACED;
    e++;

    for (i = 1; i < WORLD_FAN_SPRITES; i++) {
        gte_ldv0(&pts[i - 1]);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&xf);
        e->pos.vx = pos.vx + xf.vx;
        e->pos.vy = pos.vy + xf.vy;
        e->pos.vz = pos.vz + xf.vz;
        e->cellId = worldPosToCell(&e->pos, &e->cell);
        e->flag = WORLD_SPRITE_PLACED;
        e++;
    }

    for (j = 0; j < WORLD_FAN_SPRITES; j++) {
        out[j].pos.vy = h;
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A3C9C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A3EE4);


/** Clears an array of 12 entries. */
void func_800A40C0(void) {
    FeaEntry40C0 *ptr = D_800D24A8;
    FeaEntry40C0 *end = ptr + 12;
    while (ptr < end) {
        ptr->val = 0;
        ptr->hval = 0;
        ptr++;
    }
}

/* Dual view of a world coordinate: full position (@c word) and its
 * low-16-bit angle component (@c half). */
typedef union {
    s32 word;
    u16 half;
} WorldCoord;

typedef struct {
    WorldCoord x, y, z;
} WorldVec;

/**
 * @brief Project a world position to a grid-cell index, optionally emitting its angle triple.
 *
 * When @p out is non-NULL, copies @p pos's three low-16-bit angle components into
 * @p out — the X/Y components masked to 11 bits, the Z component additionally
 * wrapped into the @c [-0x800, 0] half-revolution range.
 *
 * Always returns a grid-cell index derived from @p pos's X/Z world coordinates:
 * the X coordinate (biased by @c 0x60000) folds modulo @c 0x40000 into one of
 * @c 0x80 columns, and the Z coordinate (taken as @c 0x48000 - z) folds modulo
 * @c 0x30000 into one of @c 0x60 rows, combined as @c col + row * 0x80.
 *
 * @param pos World position; each coordinate is read both as a full word and as
 *            a low-u16 angle component.
 * @param out Optional destination for the angle triple (s16 x/y/z); may be NULL.
 * @return    Grid-cell index @c col + row * 0x80.
 */
s32 worldPosToCell(VECTOR *pos, SVECTOR *out) {
    WorldVec *p = (WorldVec *)pos;

    if (out != NULL) {
        out->vx = p->x.half & 0x7FF;
        out->vy = p->y.half;
        {
            s32 t = p->z.half & 0x7FF;
            out->vz = t;
            if (t != 0) out->vz = t - 0x800;
        }
        if (out->vz < -0x7FF) out->vz = (u16)out->vz + 0x800;
    }

    return ((p->x.word + 0x60000) % 0x40000) / 0x800
         + ((0x48000 - p->z.word) % 0x30000) / 0x800 * 0x80;
}

/**
 * @brief Place a fan of 5 world-map sprites around @p origin, oriented by @p angles.
 *
 * Builds a rotation matrix from @p angles and transforms @p v through it to get a
 * base offset; then asks @c func_800B5ADC for up to 4 spread vectors and
 * @c func_800BC5E0 for a per-scene angle bias, builds a second (Y-axis) rotation
 * from @c arg4 + that bias whose translation is the base offset, and emits one
 * @ref WorldSprite per source vector at @c origin + transformedOffset. Each sprite
 * is projected with @c worldPosToCell (filling @c cell / @c cellId) and tagged
 * @c WORLD_SPRITE_PLACED. The first sprite uses @p v's offset directly; the remaining four
 * use the spread vectors transformed by the second matrix.
 *
 * @param out    Output array of 5 @ref WorldSprite entries.
 * @param v      Base offset vector, transformed by the @p angles matrix.
 * @param angles Rotation angles for the primary matrix.
 * @param arg3   Scene context id passed to @c func_800B5ADC / @c func_800BC5E0.
 * @param arg4   Base Y-axis angle for the secondary (spread) rotation.
 * @param origin World-space origin the sprites are placed relative to.
 */
static void placeWorldSpriteFan(WorldSprite *out, VECTOR *v, SVECTOR *angles, s32 arg3, s32 arg4,
                   VECTOR *origin) {
    MATRIX m;
    VECTOR xf;
    SVECTOR pts[WORLD_FAN_SPRITES - 1];
    SVECTOR yang;
    WorldSprite *e;
    s32 i;
    s32 ret;

    RotMatrix(angles, &m);
    gte_SetRotMatrix(&m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    gte_SetTransMatrix(&m);
    e = out;
    gte_ldv0(v);
    gte_mvmva(1, 0, 0, 0, 0);
    gte_stlvnl(&xf);

    func_800B5ADC(arg3, pts, 0, 0);
    ret = func_800BC5E0(arg3);

    yang.vx = 0;
    yang.vz = 0;
    yang.vy = arg4 + ret;
    RotMatrix(&yang, &m);
    gte_SetRotMatrix(&m);
    gte_SetTransVector(&xf);

    e->pos.vx = origin->vx + xf.vx;
    e->pos.vy = origin->vy + xf.vy;
    e->pos.vz = origin->vz + xf.vz;
    e->cellId = worldPosToCell(&e->pos, &e->cell);
    e->flag = WORLD_SPRITE_PLACED;
    e++;

    for (i = 1; i < 5; i++) {
        gte_ldv0(&pts[i - 1]);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&xf);
        e->pos.vx = origin->vx + xf.vx;
        e->pos.vy = origin->vy + xf.vy;
        e->pos.vz = origin->vz + xf.vz;
        e->cellId = worldPosToCell(&e->pos, &e->cell);
        e->flag = WORLD_SPRITE_PLACED;
        e++;
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A4420);

/**
 * @brief Tag-based flag lookup — larger sibling of @c func_800A4670.
 *
 * Returns 1 when @c D_800C4D20 is zero. Otherwise picks a bit from the
 * upper bytes of @p a based on @p b's low 16 bits:
 *   - 0x00..0x09, or 0x80: bit 7 of @c (a >> 16)
 *   - 0x20..0x28, or 0x84: bit 6 of @c (a >> 16)
 *   - 0x30: bit 5 of @c (a >> 16)
 *   - 0x31: bit 4 of @c (a >> 16)
 *   - otherwise: 1
 *
 * @note Purpose uncertain — likely command/slot flag dispatch.
 *
 * @param a Source word containing the flag bits in its upper bytes.
 * @param b Tag selector (low 16 bits examined).
 * @return The extracted bit (0 or the mask value), or 1 as a default.
 */
s32 func_800A45D8(u32 a, s32 b) {
    u16 op = (u16)b;
    if (D_800C4D20 == 0) return 1;
    if (op < 0xA || (s16)b == 0x80) return (a >> 16) & CMDPAR_VIS_00_09;
    if ((u16)(b - 0x20) < 9 || (s16)b == 0x84) return (a >> 16) & CMDPAR_VIS_20_28;
    if ((s16)b == 0x30) return (a >> 16) & CMDPAR_VIS_30;
    if ((s16)b == 0x31) return (a >> 16) & CMDPAR_VIS_31;
    return 1;
}

/**
 * @brief Extract one of several bit flags from @p a based on @p b's tag.
 *
 * Uses @p b as a selector to pick which bit of @p a's upper bytes to return.
 * When @c D_800C4D20 is zero the upper byte of @p a is forced to 0xFF before
 * extraction. The selector ranges are:
 *   - 0x20..0x28, or 0x84: bit 4 of @c (a >> 16)
 *   - 0x30: bit 2 of @c (a >> 16)
 *   - 0x31: bit 1 of @c (a >> 16)
 *   - 0x32: bit 7 of @c (a >> 8)
 *   - otherwise: 1
 *
 * @note Purpose uncertain — appears to be a tag-based flag lookup on
 *       command/slot data.
 *
 * @param a Source word containing the flag bits.
 * @param b Tag selector (low 16 bits examined).
 * @return The extracted bit (0 or the masked value), or 1 as a default.
 */
s32 func_800A4670(u32 a, s32 b) {
    if (D_800C4D20 == 0) {
        a |= 0xFFFFFF00;
    }
    if ((u16)(b - 0x20) < 9 || (s16)b == 0x84) return (a >> 16) & CMDPAR_MODE_20_28;
    if ((s16)b == 0x30) return (a >> 16) & CMDPAR_MODE_30;
    if ((s16)b == 0x31) return (a >> 16) & CMDPAR_MODE_31;
    if ((s16)b == 0x32) return (a >> 8) & CMDFLAG_MODE_32;
    return 1;
}

/**
 * @brief Signed delta of (a mod 128) and (b mod 128), wrapped into [-64, 64].
 *
 * Computes the difference of @p a and @p b after each is reduced modulo 128
 * (signed truncation toward zero), then wraps the difference into a
 * ~[-64, 64] band by adding or subtracting 128. Looks like 256-step angle
 * arithmetic — 128 maps to a half-turn — used to find the shortest signed
 * distance between two angle-like values.
 *
 * @param a First value.
 * @param b Second value.
 * @return Wrapped signed delta in approx [-64, 64].
 */
s32 func_800A4700(s32 a, s32 b) {
    s32 d = (a % 128) - (b % 128);
    if (d < 65) {
        if (d < -64) {
            d += 128;
        }
    } else {
        d -= 128;
    }
    return d;
}

/**
 * @brief Signed delta between two /128-scaled inputs, wrapped into (-48, 48].
 *
 * Divides @p a and @p b by 128 (signed truncation toward zero), subtracts,
 * and wraps the result into a ~[-48, 48] band by adding or subtracting 96.
 * Looks like modular-angle arithmetic: @c 192 maps to a full circle and
 * @c 96 to a half turn, so this computes "shortest signed distance"
 * between two angle-like values.
 *
 * @param a First value (numerator scale 128).
 * @param b Second value (numerator scale 128).
 * @return Wrapped signed delta in approx [-48, 48].
 */
s32 func_800A475C(s32 a, s32 b) {
    s32 d = a / 128 - b / 128;
    if (d < 49) {
        if (d < -48) {
            d += 96;
        }
    } else {
        d -= 96;
    }
    return d;
}

/**
 * @brief World render callback: advance the streamer and keep the world-map
 *        object lists and message state in step with it.
 *
 * Installed by @c setupWorldRender through @c func_800C3DB0, which invokes it
 * once per frame through a stored pointer. Rebuilds the viewport cell list,
 * recycles finished objects, then dispatches on the streaming controller's
 * tick (@c func_800C4558):
 *
 *  - **tick != 0, countdown still running, tick != -1** — a read is in
 *    progress and nothing is due: hand every pending object back to the free
 *    list @c D_800D3318 (clearing its @c D_800D34A0 slot) once
 *    @c func_800A6254 reports the list idle.
 *  - **tick != 0 otherwise** — the countdown expired or the read failed
 *    (@c -1). Reset the reader, then either resend the pending message
 *    (@c D_800D34E8), or, when no object is pending and no message is armed,
 *    resend the message selected by @p D_800C4D60's code. Either way the
 *    countdown is rearmed to @c WORLD_STREAM_RETRY.
 *  - **tick == 0** — the reader is idle. Drain pending objects; else claim a
 *    free @c D_800C5398 slot and either stage its message or publish the slot
 *    pair into @c D_800C4D60; else stage the section-pointer list
 *    @c D_800D34F0 for the queued objects; else ask @c func_800A610C for the
 *    object under the cursor and publish it as a slot pair.
 *
 * @return 2 while either object list is non-empty, 1 when a code is armed in
 *         @c D_800C4D60, else 0.
 *
 * @note Statically unreachable in the retail build — see the dead-island note
 * on @c setupWorldRender. Reproduced for byte-exactness only.
 */
s32 func_800A47A4(void) {
    s32 buf[2];
    WorldObject *p;
    WorldObject *nx;
    u32 *dst;
    s32 ret;
    s32 tick;
    s32 sel;
    s32 selIdx;
    s32 pair;
    s32 which;
    s32 lo;
    u32 code16;
    s8 code;
    s32 i;

    buildViewportCellList(&D_800C9868, D_800C9EF0, &D_800C9838);
    func_800A6030(&D_800D34E0);
    ret = 0;
    if (D_800D34E4 == NULL) {
        if (D_800D34E0 == NULL) {
            registerNewWorldObjects();
        }
    }
    tick = func_800C4558(1, buf);
    D_800C53AC = tick;

    if (tick != 0) {
        D_800C53B0--;
        if ((D_800C53B0 <= 0) || (tick == -1)) {
            func_800C4450();
            if (D_800D34E4 != NULL) {
                if (D_800C4D60 == WORLD_CODE_NONE) {
                    while (func_800C4480(D_800D34E8, WORLD_OBJMSG_LEN, D_800D34F0) == 0) {
                        ;
                    }
                    func_800C4558(1, buf);
                    goto rearm;
                }
            } else {
                if (D_800C4D60 == WORLD_CODE_NONE) {
                    goto rearm;
                }
            }
            /* Both arms above fall through to this join, so the reload and
               re-test of D_800D34E4 here are real. A `do {} while (0)` with
               `break` expresses the same flow but emits loop notes that change
               the delay-slot scheduling, so the label is load-bearing. */
            if (D_800D34E4 == NULL) {
                code16 = D_800C4D60;
                if ((code16 & 0xFF) != WORLD_SLOT_NONE) {
                    if ((D_800D241E & WORLD_FLAG_ALTCODE) &&
                        (((code16 & 0xFFFF) == 0x504) || ((code16 & 0xFFFF) == 0x706))) {
                        code = (u8)D_800C4D60 + WORLD_CODE_ALT_BIAS;
                    } else {
                        code = (u8)D_800C4D60;
                    }
                } else {
                    if ((D_800D241E & WORLD_FLAG_ALTCODE) &&
                        (((code16 & 0xFFFF) == 0x5FF) || ((code16 & 0xFFFF) == 0x7FF))) {
                        code = ((code16 & 0xFFFF) >> 8) + WORLD_CODE_ALT_BIAS;
                    } else {
                        code = D_800C4D60 >> 8;
                    }
                }
                D_800D34F0[0] = WORLD_STAGE_ADDR;
                D_800D34F0[1] = 0;
                while (func_800C4480(D_800C9E58 + code * WORLD_MSG_LEN, WORLD_MSG_LEN,
                                     D_800D34F0) == 0) {
                    ;
                }
                func_800C4558(1, buf);
            }
        rearm:
            D_800C53B0 = WORLD_STREAM_RETRY;
        } else {
            p = D_800D34E4;
            if (p != NULL) {
                if (D_800C4D60 == WORLD_CODE_NONE) {
                    if (func_800A6254(p) == 0) {
                        func_800C4450();
                        p = D_800D34E4;
                        if (p != NULL) {
                            do {
                                D_800D34A0[p->sectionIdx] = 0;
                                nx = D_800D34E4->next;
                                p->next = D_800D3318;
                                D_800D3318 = p;
                                p = nx;
                                D_800D34E4 = p;
                            } while (p != NULL);
                        }
                    }
                }
            }
        }
    } else {
        if (D_800D34E4 != NULL) {
            drainPendingObjects();
        } else if (D_800C4D60 != WORLD_CODE_NONE) {
            if (D_800C5398[0] == WORLD_SLOT_NONE) {
                sel = 0;
            } else if (D_800C5398[2] == WORLD_SLOT_NONE) {
                sel = 2;
            } else {
                sel = -1;
            }
            selIdx = sel & 0xFF;
            if (selIdx == WORLD_SLOT_NONE) {
                D_800C4D60 = WORLD_CODE_NONE;
            } else if ((u8)D_800C4D60 != WORLD_SLOT_NONE) {
                func_800A6188((Tim *)WORLD_STAGE_ADDR, selIdx);
                if ((D_800D241E & WORLD_FLAG_ALTCODE) &&
                    ((D_800C4D60 == 0x504) || (D_800C4D60 == 0x706))) {
                    code = (D_800C4D60 >> 8) + WORLD_CODE_ALT_BIAS;
                } else {
                    code = D_800C4D60 >> 8;
                }
                D_800D34F0[0] = WORLD_STAGE_ADDR;
                D_800D34F0[1] = 0;
                while (func_800C4480(D_800C9E58 + code * WORLD_MSG_LEN, WORLD_MSG_LEN,
                                     D_800D34F0) == 0) {
                    ;
                }
                func_800C4558(1, buf);
                D_800C53B0 = WORLD_STREAM_RETRY;
                D_800C4D60 |= WORLD_SLOT_NONE;
            } else {
                func_800A6188((Tim *)WORLD_STAGE_ADDR, sel + 1);
                /* Chained so both slots take one loaded copy of the code; a
                   temp on its own line swaps the value and address registers. */
                D_800C5398[selIdx + 1] = pair = D_800C4D60 >> 9;
                D_800C5398[selIdx] = pair;
                D_800C4D60 = WORLD_CODE_NONE;
            }
        } else if (D_800D34E0 != NULL) {
            which = func_800A5B48();
            p = D_800D34E4;
            if (p != NULL) {
                dst = D_800D34F0;
                do {
                    /* the staging list is words, so the section pointer is stored raw */
                    *dst = (u32)&D_800C4D5C[p->sectionIdx];
                    p = p->next;
                    dst++;
                } while (p != NULL);
                *dst = 0;
                p = D_800D34E4;
                if (which == 0) {
                    D_800D34E8 = D_800C97E0 + p->key * WORLD_OBJMSG_LEN;
                } else {
                    D_800D34E8 = D_800C97E4 + p->slot.row * WORLD_OBJMSG_LEN;
                }
                while (func_800C4480(D_800D34E8, WORLD_OBJMSG_LEN, D_800D34F0) == 0) {
                    ;
                }
                func_800C4558(1, buf);
                D_800C53B0 = WORLD_STREAM_RETRY;
            }
        } else {
            tick = func_800A610C(D_800CA030);
            /* The cast on this side only: spelling both tests `& 0xFF` lets cse
               fold them into one AND, which drops the copy the target keeps. */
            if ((tick & 0xFF) != WORLD_SLOT_NONE) {
                lo = ((u8)tick) * 2;
                D_800C4D60 = ((lo + 1) << 8) | lo;
            } else {
                D_800C4D60 = WORLD_CODE_NONE;
            }
            if (D_800C4D38 == 0x32) {
                if (((WorldFlags *)D_800D23D8)->opParam != WORLD_OPPARAM_KEEP) {
                    D_800C4D60 = WORLD_CODE_NONE;
                }
            }
            if (D_800C4D60 != WORLD_CODE_NONE) {
                for (i = 0; i < WORLD_SLOT_COUNT; i++) {
                    if (D_800C5398[i] != WORLD_SLOT_NONE) {
                        if (func_800A60B4(D_800C5398[i], D_800CA030) == NULL) {
                            D_800C5398[i] = WORLD_SLOT_NONE;
                        }
                    }
                }
                if ((D_800C5398[0] == WORLD_SLOT_NONE) || (D_800C5398[2] == WORLD_SLOT_NONE)) {
                    if ((D_800D241E & WORLD_FLAG_ALTCODE) &&
                        ((D_800C4D60 == 0x504) || (D_800C4D60 == 0x706))) {
                        code = (u8)D_800C4D60 + WORLD_CODE_ALT_BIAS;
                    } else {
                        code = (u8)D_800C4D60;
                    }
                    D_800D34F0[0] = WORLD_STAGE_ADDR;
                    D_800D34F0[1] = 0;
                    while (func_800C4480(D_800C9E58 + code * WORLD_MSG_LEN, WORLD_MSG_LEN,
                                         D_800D34F0) == 0) {
                        ;
                    }
                    func_800C4558(1, buf);
                    D_800C53B0 = WORLD_STREAM_RETRY;
                } else {
                    D_800C4D60 = WORLD_CODE_NONE;
                }
            }
        }
    }

    if ((D_800D34E0 != NULL) || (D_800D34E4 != NULL)) {
        ret = 2;
    } else if (D_800C4D60 != WORLD_CODE_NONE) {
        ret = 1;
    }
    return ret;
}

/**
 * @brief Build a linked list of the world-map grid cells inside the camera
 *        viewport, writing it into @p out.
 *
 * Projects the two opposite corners of a fixed 0x2FFF-radius box around the
 * camera position @c D_800D23C0 to packed cell coordinates via
 * @c func_800A5E40, then walks every cell in the spanned rectangle. Columns
 * (the low @c %32 axis) wrap modulo 32 and rows (the @c /32 axis) wrap modulo
 * 24, so the enumeration covers a torus-shaped region across the world-map
 * grid edges. Each visited cell is written to a fresh @ref WorldObject node
 * (@c id = @c row*32+col), chained through @c next at a 0xC-byte stride, and
 * the final node's @c next is cleared to terminate the list.
 *
 * @param cam Unused (the incoming register is overwritten immediately).
 * @param out Output buffer; receives the 0xC-byte node list, NULL-terminated.
 * @param m   Unused; the sole caller passes the world matrix.
 */
static void buildViewportCellList(WorldPos *cam, WorldObject *out, MATRIX *m) {
    s32 r1, r2;
    s32 row, col;
    s32 colSpan, rowSpan;
    s32 i, j;

    r1 = func_800A5E40(D_800D23C0.x - 0x2FFF, D_800D23C0.y - 0x2FFF);
    r2 = func_800A5E40(D_800D23C0.x + 0x2FFF, D_800D23C0.y + 0x2FFF);

    {
        s32 f1 = r1 % 32;
        s32 f2 = r2 % 32;
        if (r2 % 32 < r1 % 32) {
            s32 lo = f2 + 32;
            colSpan = lo - f1;
        } else {
            colSpan = f2 - f1;
        }
    }
    {
        s32 c1 = r1 / 32;
        s32 c2 = r2 / 32;
        if (r2 / 32 < r1 / 32) {
            s32 lo = c2 + 24;
            rowSpan = lo - c1;
        } else {
            rowSpan = c2 - c1;
        }
    }

    row = r1 / 32;
    for (i = 0; i <= rowSpan; i++) {
        col = r1 % 32;
        for (j = 0; j <= colSpan; j++) {
            if (col >= 32) col -= 32;
            if (row >= 24) row -= 24;
            if (col < 0) col += 32;
            if (row < 0) row += 24;
            out->id = row * 32 + col;
            out->next = out + 1;
            out++;
            col++;
        }
        row++;
    }
    out[-1].next = 0;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A50A0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A568C);


/**
 * @brief Initialise the world-engine subsystem's object pools.
 *
 * Builds the 16-entry free list at @c D_800D3320 (linked via @c next),
 * publishes its head at @c D_800D3318, zeros the various object-list
 * roots (@c D_800D34E0, @c D_800D34E4, @c D_800D2284, @c D_800CA030)
 * and the 16-entry @c D_800D34A0 u32 slot table, then resets each of
 * the 16 entries in both @c D_800D33E0 and @c D_800C9EF0 to
 * @c { .next = NULL, .id = -1 }. Finally sets @c D_800C4D60 to its
 * sentinel value 0xFFFF and zeros @c D_800D34F0.
 */
static void func_800A581C(void) {
    s32 i;

    for (i = 14; i >= 0; i--) {
        D_800D3320[i].next = &D_800D3320[i + 1];
    }
    D_800D3320[15].next = 0;
    D_800D3318 = &D_800D3320[0];
    D_800D34E0 = 0;
    D_800D34E4 = 0;
    D_800D2284 = 0;
    D_800CA030 = 0;

    for (i = 15; i >= 0; i--) {
        D_800D34A0[i] = 0;
    }

    for (i = 0; i < 16; i++) {
        D_800C9EF0[i].next = 0;
        D_800C9EF0[i].id = -1;
        D_800D33E0[i].next = 0;
        D_800D33E0[i].id = -1;
    }

    D_800C4D60 = 0xFFFF;
    D_800D34F0[0] = 0;
}

/**
 * @brief Register world objects from the master list that aren't yet tracked.
 *
 * Walks the master object list @c D_800C9EF0 and, for each entry whose @c id is
 * not already present in any of the three tracking lists (@c D_800CA030, the
 * active list @c D_800D34E0, or @c D_800D34E4), pops a node from the free pool
 * @c D_800D3318, stamps it with that @c id, and pushes it onto the active list
 * @c D_800D34E0. If the free pool is exhausted, @c func_8009C528(0x6E) raises a
 * system error.
 *
 * @note Each search re-reads @c node->id (re-loaded per list) and uses a
 *       found-pointer walk; objects already tracked anywhere are skipped.
 */
void registerNewWorldObjects(void) {
    WorldObject *node;
    WorldObject *p;
    WorldObject *found;
    WorldObject *freeNode;
    s16 id;

    for (node = D_800C9EF0; node != NULL; node = node->next) {
        id = node->id;
        for (p = D_800CA030; p != NULL; p = p->next) {
            if (id == p->id) { found = p; goto tracked0; }
        }
        found = NULL;
    tracked0:
        if (found != NULL) continue;

        id = node->id;
        for (p = D_800D34E0; p != NULL; p = p->next) {
            if (id == p->id) { found = p; goto tracked1; }
        }
        found = NULL;
    tracked1:
        if (found != NULL) continue;

        id = node->id;
        for (p = D_800D34E4; p != NULL; p = p->next) {
            if (id == p->id) { found = p; goto tracked2; }
        }
        found = NULL;
    tracked2:
        if (found != NULL) continue;

        freeNode = D_800D3318;
        if (freeNode == NULL) {
            func_8009C528(0x6E);
        }
        D_800D3318 = freeNode->next;
        freeNode->id = node->id;
        freeNode->next = D_800D34E0;
        D_800D34E0 = freeNode;
    }
}

/**
 * @brief Drain the pending @c WorldObject list at @c D_800D34E4.
 *
 * For each node on the @c D_800D34E4 list: look it up in the @c D_800C9EF0[16]
 * id table; if its @c id is present, append a copy (id + sectionIdx) to the
 * @c D_800CA030 active list — which grows in place through the @c D_800D33E0[16]
 * pool via @c tail->next = tail + 1 — otherwise clear @c D_800D34A0 for the
 * node's section. Either way the node is then pushed onto the @c D_800D3318
 * free list. Iterates until the pending list is empty.
 *
 * @note The match needs the search to leave its result in a @c found flag that
 *       is set only at the two loop exits (1 on a hit, 0 on exhaustion), which
 *       requires the @c goto over the @c found=0 — a plain @c break cannot skip
 *       it.
 */
static void drainPendingObjects(void) {
    WorldObject *node;
    WorldObject *entry;

    node = D_800D34E4;
    if (node == 0) {
        return;
    }

    do {
        WorldObject *search;
        s32 found;

        D_800D34E4 = node->next;
        search = &D_800C9EF0[0];
        while (search != 0) {
            if (node->id == search->id) {
                found = 1;
                goto matched;
            }
            search = search->next;
        }
        found = 0;
    matched:
        if (found) {
            if (D_800CA030 != 0) {
                entry = D_800CA030;
                while (entry->next != 0) {
                    entry = entry->next;
                }
                entry->next = entry + 1;
                entry += 1;
            } else {
                entry = &D_800D33E0[0];
                D_800CA030 = &D_800D33E0[0];
            }
            entry->id = node->id;
            entry->sectionIdx = node->sectionIdx;
            entry->next = 0;
        } else {
            D_800D34A0[node->sectionIdx] = 0;
        }

        node->next = D_800D3318;
        D_800D3318 = node;

        node = D_800D34E4;
    } while (node != 0);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object3", func_800A5B48);


/**
 * @brief Draw a translucent dark-gray fullscreen quad and wait for the GPU.
 *
 * Sets up @c D_800D3300 as a 24-byte POLY_F4 covering the viewport
 * (0,0)-(width,height) with RGB(8,8,8) and code 0x2A (semi-transparent
 * 4-vertex polygon). Submits via @c DrawPrim, then @c DrawSync(0) blocks
 * until the GPU finishes. Likely a screen-darken pass for fades.
 */
void func_800A5D10(void) {
    POLY_F4 *prim = &D_800D3300;
    s16 w = D_800C97EA;
    s16 h = D_800C97E8;

    setlen(prim, 5);
    setcode(prim, 0x2A);

    prim->x0 = 0;
    prim->y0 = 0;
    prim->y1 = 0;
    prim->x2 = 0;

    prim->r0 = 8;
    prim->g0 = 8;
    prim->b0 = 8;

    prim->x1 = w;
    prim->y2 = h;
    prim->x3 = w;
    prim->y3 = h;

    DrawPrim(prim);
    DrawSync(0);
}

/**
 * @brief Linear search a world-object list for a node with matching @p id.
 *
 * @param id Signed 16-bit id to match.
 * @param head Head of the linked list (may be NULL).
 * @return Matching WorldObject, or NULL if none found.
 */
static WorldObject *worldObjectById(s16 id, WorldObject *head) {
    while (head != NULL) {
        if (id == head->id) return head;
        head = head->next;
    }
    return NULL;
}

/**
 * @brief Compute a 2D tile index from world coordinates (128-wide grid).
 *
 * Like @c func_800A5E40 but finer-grained: divides by 0x800 (2048) instead
 * of 0x2000 and the Y multiplier is 128 (bit 7) instead of 32. Maps
 * (x, y) world coords to a linear tile index on a 128-column grid.
 *
 * @note The ugly pointer-alias expression @c ((*(new_var2 = &rnd)) >> 18)
 *       and the split @c rnd assignment are load-bearing — they coerce
 *       gcc's register allocator to keep the rounded quotient in @c a1
 *       throughout the signed-mod calculation, matching the target.
 *       Cleaner forms (inline cast, single-line rnd expression) drop the
 *       match rate by 1–5 instructions.
 *
 * @param x World X coordinate.
 * @param y World Y coordinate.
 * @return Linear tile index on a 128-column grid.
 */
s32 func_800A5DC8(s32 x, s32 y) {
    s32 xm;
    s32 *new_var2;
    s32 ym;
    s32 new_var;
    x += 0x60000;
    {
        s32 rnd = x;
        if (x < 0) {
            rnd = 0x3FFFF + x;
        }
        new_var = x;
        rnd = ((*(new_var2 = &rnd)) >> 18) << 18;
        xm = new_var - rnd;
    }
    ym = ((y + 0x48000) % 0x30000) >> 11;
    return (xm >> 11) + (ym << 7);
}

/**
 * @brief Compute a tile index from 2D world coordinates.
 *
 * Offsets @p x and @p y to be positive, wraps them into 0x40000 × 0x30000
 * ranges, divides by the tile size (0x2000), and linearises:
 *   - X tile = ((x + 0x60000) mod 0x40000) / 0x2000   — range [0, 32)
 *   - Y tile = ((y + 0x48000) mod 0x30000) / 0x2000   — range [0, 24)
 *   - index = X + Y × 32
 *
 * @note The declaration of @c ym is load-bearing despite being "unused" —
 *       removing it changes gcc's register allocation and breaks the match.
 *       The inline recomputation of the @c y expression in the return is
 *       similarly intentional; gcc folds the duplicate into a single
 *       computation at runtime.
 *
 * @param x World X coordinate.
 * @param y World Y coordinate.
 * @return Linear tile index.
 */
s32 func_800A5E40(s32 x, s32 y) {
    s32 xo = x + 0x60000;
    s32 xm = xo % 0x40000;
    s32 ym = (y + 0x48000) % 0x30000;
    s32 xtile = xm / 0x2000;
    return xtile + ((((y + 0x48000) % 0x30000) / 0x2000) * 32);
}

/**
 * @brief Resolve a world id to a pointer into its data section.
 *
 * @c func_800A62E0 splits @p id into a coarse key and a fine index. The coarse
 * key is matched against the @c id of each WorldObject in the @c D_800CA030
 * list; the matching node's @c sectionIdx selects a WorldSection in the
 * @c D_800C4D5C region table, and the section's @c offsets[fine] (low 2 flag
 * bits stripped by the @c >>2 word index) gives the target's location within
 * that section.
 *
 * @param id World id to resolve.
 * @return Pointer into the matched section, or NULL if no node matches.
 */
u32 *func_800A5EC4(s16 id) {
    s16 objId;
    u16 coarse;
    u16 fine;
    WorldObject *node;
    WorldObject *found;
    s32 idx;
    u32 *result;

    result = NULL;
    func_800A62E0(id, &coarse, &fine);
    objId = coarse;
    for (node = D_800CA030; node != NULL; node = node->next) {
        if (objId == node->id) {
            found = node;
            goto done;
        }
    }
    found = NULL;
done:
    if (found != NULL) {
        WorldSection *section = &D_800C4D5C[found->sectionIdx];
        idx = (s16)fine;
        /* idx[offsets] == offsets[idx]; index-first form matches the addu operand order */
        result = (u32 *)section + (idx[section->offsets] >> 2);
    }
    return result;
}

/**
 * @brief Set up a draw environment for screen @p screenIdx and submit it.
 *
 * Initialises a DRAWENV for a viewport at (screenIdx * width, 0) of size
 * (width × height) where width = @c D_800C97EA and height = @c D_800C97E8.
 * Patches @c tpage = 0x40 (default GPU texture page) and @c dfe = 1
 * (enable drawing to display area), then pushes it via @c PutDrawEnv.
 *
 * @param screenIdx Horizontal viewport index (multiplied by width).
 */
void func_800A5F78(s32 screenIdx) {
    DRAWENV env;
    SetDefDrawEnv(&env, screenIdx * D_800C97EA, 0, D_800C97EA, D_800C97E8);
    env.dfe = 1;
    env.tpage = 0x40;
    PutDrawEnv(&env);
}

/**
 * @brief Set up a display environment for screen @p screenIdx and submit it.
 *
 * Mirror of @c func_800A5F78 but for @c DISPENV (the VRAM-readout side
 * of the GPU pair). Patches the visible screen region to @c y=8,
 * @c h=224 (NTSC full-frame minus 16 overscan lines) before pushing.
 *
 * @param screenIdx Horizontal viewport index (multiplied by width).
 */
void func_800A5FD4(s32 screenIdx) {
    DISPENV env;
    SetDefDispEnv(&env, screenIdx * D_800C97EA, 0, D_800C97EA, D_800C97E8);
    env.screen.y = 8;
    env.screen.h = 0xE0;
    PutDispEnv(&env);
}

/**
 * @brief Walk a @c WorldObject list, moving every node whose @c id is
 *        not present in the @c D_800C9EF0 lookup table onto the front
 *        of the @c D_800D3318 free list.
 *
 * For each @c WorldObject in the list rooted at @c *pp, scan the
 * 16-entry @c D_800C9EF0 table (linked via @c next) for an entry with
 * a matching @c id. When no match is found the node is removed from
 * the source list (the predecessor's @c next pointer is patched via
 * the @c Node** indirection) and prepended to @c D_800D3318. When a
 * match is found the node is kept and the walker advances by
 * re-pointing @p pp at the node's @c next.
 *
 * The @c Node @c ** indirection (rather than a separate @c prev
 * pointer) is the key to byte-matching this routine: it lets gcc keep
 * the same register as both the head-pointer and the predecessor's
 * @c next pointer across iterations.
 *
 * @param pp Address of the head pointer for the list to filter.
 */
static void func_800A6030(WorldObject **pp) {
    WorldObject *curr;
    WorldObject *search;
    s32 found;

    curr = *pp;
    if (curr == NULL) {
        return;
    }

    do {
        search = &D_800C9EF0[0];
        found = 0;

        while (search != NULL) {
            if (curr->id == search->id) {
                found = 1;
                break;
            }
            search = search->next;
            found = 0;
        }

        if (!found) {
            *pp = curr->next;
            curr->next = D_800D3318;
            D_800D3318 = curr;
        } else {
            pp = &curr->next;
        }
        curr = *pp;
    } while (curr != NULL);
}

/**
 * @brief Walk a WorldObject list and return the first node whose section
 *        key (first byte of @c D_800C4D5C[sectionIdx]) matches @p key.
 *
 * @param key Key byte to match (low 8 bits of @p key are used).
 * @param head Head of the WorldObject list (may be NULL).
 * @return Matching WorldObject, or NULL if none found.
 */
static WorldObject *func_800A60B4(s32 key, WorldObject *head) {
    if (head) {
        WorldSection *base = D_800C4D5C;
        key &= 0xFF;
        do {
            WorldSection *section = &base[head->sectionIdx];
            if (key == section->key) return head;
            head = head->next;
        } while (head != 0);
    }
    return NULL;
}

/**
 * @brief Walk a WorldObject list and return the first section-byte that
 *        isn't @c WORLD_SLOT_NONE, @c D_800C5398[0], or @c D_800C5398[2];
 *        else @c WORLD_SLOT_NONE.
 *
 * For each node in @p head, reads the first byte of its world-section
 * (@c D_800C4D5C[sectionIdx]). A byte is "interesting" when it differs from
 * all three skip values, at which point it's returned. If no interesting
 * byte is found (or @p head is NULL), returns 0xFF.
 *
 * @note @c D_800C5398[0] is cached once outside the loop; @c D_800C5398[2]
 *       is re-read each iteration (gcc emits it inside the loop because
 *       it's the third byte read — CSE doesn't hoist it).
 *
 * @param head Head of the WorldObject list (may be NULL).
 * @return An interesting section byte, or @c WORLD_SLOT_NONE if none found.
 */
s32 func_800A610C(WorldObject *head) {
    if (head) {
        u8 firstSect = D_800C5398[0];
        do {
            WorldSection *section = &D_800C4D5C[head->sectionIdx];
            u8 key = section->key;
            if (key != WORLD_SLOT_NONE && firstSect != key && D_800C5398[2] != key) {
                return key;
            }
            head = head->next;
        } while (head != 0);
    }
    return WORLD_SLOT_NONE;
}


/**
 * @brief Upload two subimages from a packed-TIM bundle into VRAM.
 *
 * @p tim points to a back-to-back pair of TIM image blocks. The first image
 * (256×16, 16bpp) is uploaded to the VRAM rect at @c D_800C5388[tableIdx];
 * the second image (128×256, 16bpp) is uploaded to @c D_800C5378[tableIdx].
 *
 * Uses @c D_800D32F0 as a scratch @c RECT for each upload.
 */
static void func_800A6188(Tim *tim, u8 tableIdx) {
    u32 *img1 = (u32 *)tim->clut.data;
    /* The tim/img1 ++/-- pairs pin img1 into a callee-saved register so it
       survives the LoadImage/DrawSync calls and is reused for the second
       image; without them the register allocation doesn't match. */
    tim++;
    img1++;
    img1--;
    tim--;

    setRECT(&D_800D32F0, D_800C5388[tableIdx].x, D_800C5388[tableIdx].y, 256, 16);
    LoadImage(&D_800D32F0, img1);
    DrawSync(0);

    setRECT(&D_800D32F0, D_800C5378[tableIdx].x, D_800C5378[tableIdx].y, 128, 256);
    // FIXME: hard-coded offset to the second image block (0x803 u32 words =
    // 0x200C bytes past the first image). This should be derived from the TIM
    // itself (the first block's length) rather than baked in — as written it
    // silently assumes a fixed first-image size. Every structural form tried
    // (tim->image2, or walking clut.len) breaks the register-allocation match
    // with the current toolchain. Figure out how to reach the second block
    // structurally without losing the match.
    LoadImage(&D_800D32F0, img1 + 0x803);
    DrawSync(0);
}

/**
 * @brief Walk a WorldObject list and return 1 if any node's id hits D_800C9EF0's list.
 *
 * For each node in the chain starting at @p head, calls @c func_800A629C
 * (which checks whether the node's id is present in the @c D_800C9EF0
 * static list) and returns 1 as soon as one reports a match. Returns 0 if
 * the list is empty or no node matches.
 *
 * @param head Head of the WorldObject chain to test (may be NULL).
 * @return 1 if any node's id is in the D_800C9EF0 list, else 0.
 */
static s32 func_800A6254(WorldObject *head) {
    while (head != NULL) {
        if (func_800A629C(head) != 0) {
            return 1;
        }
        head = head->next;
    }
    return 0;
}

/**
 * @brief Check whether any node in the D_800C9EF0 list shares @p target's id.
 *
 * Walks the static list starting at @c D_800C9EF0 and returns 1 if any
 * node's @c id matches @p target->id, else 0.
 *
 * @param target Query node — only its @c id field is read.
 * @return 1 if a matching id was found in the list, 0 otherwise.
 */
s32 func_800A629C(WorldObject *target) {
    WorldObject *node = &D_800C9EF0;
    if (node != NULL) {
        s16 key = target->id;
        do {
            if (key == node->id) return 1;
            node = node->next;
        } while (node != NULL);
    }
    return 0;
}

/**
 * @brief Split a signed 16-bit value into coarse/fine (q,r × 4,32) components.
 *
 * Divides @p val by 128 (signed) into quotient @c q and remainder @c r, then
 * splits each further by 4:
 *   - @c *coarse = (q/4)*32 + r/4    — 7-bit high portions joined
 *   - @c *fine   = (q%4)*4  + r%4    — 4-bit low portions joined
 *
 * Likely a packed grid-cell / tile-offset decomposition where the coarse
 * output identifies a 32-step bucket and the fine output carries the
 * residual position within it.
 *
 * @param val    s16 input to decompose.
 * @param coarse Output — coarse bucket (high bits of q and r).
 * @param fine   Output — fine residual (low bits of q and r).
 */
static void func_800A62E0(s16 val, u16 *coarse, u16 *fine) {
    s32 r = val % 128;
    s32 q = val / 128;
    *coarse = (q / 4) * 32 + r / 4;
    *fine = (q % 4) * 4 + r % 4;
}

/**
 * @brief Free the WorldObject list at @c D_800D34E4 back to the free pool.
 *
 * Unconditionally sets @c D_800C4D60 = 0xFFFF as a marker/sentinel. If the
 * list at @c D_800D34E4 is non-empty, walks every node and clears its slot
 * in the @c D_800D34A0 table (keyed by @c sectionIdx), then splices the
 * whole list onto the front of the @c D_800D3318 free list and clears
 * @c D_800D34E4.
 *
 * @note Purpose uncertain — looks like a subsystem-reset that releases all
 *       active world objects into a reusable pool.
 */
static void func_800A6358(void) {
    WorldObject *head;
    D_800C4D60 = 0xFFFF;
    head = D_800D34E4;
    if (head) {
        WorldObject *node = head;
        do {
            D_800D34A0[node->sectionIdx] = 0;
            node = node->next;
        } while (node != 0);

        node = D_800D34E4;
        while (node->next != 0) {
            node = node->next;
        }

        node->next = D_800D3318;
        D_800D3318 = D_800D34E4;
        D_800D34E4 = 0;
    }
}


/**
 * @brief Gated table swap — copies one of two source halfword tables into
 *        @c D_800C53B8 and @c D_800C53EC depending on the current map id.
 *
 * No-op when @c D_800C4D2C is set (system busy). Otherwise, when the map
 * id @c D_800C4D38 is @c 0x32, copies @c D_800C53C4 (5 halfwords) ->
 * @c D_800C53B8 and @c D_800C53DC (4 halfwords) -> @c D_800C53EC.
 * For any other map id, copies from @c D_800C53D0 and @c D_800C53E4
 * respectively.
 */
static void func_800A63F0(void) {
    s32 i;
    if (D_800C4D2C != 0) return;
    if (D_800C4D38 == 0x32) {
        for (i = 0; i < 5; i++) D_800C53B8[i] = D_800C53C4[i];
        for (i = 0; i < 4; i++) D_800C53EC[i] = D_800C53DC[i];
        return;
    }
    for (i = 0; i < 5; i++) D_800C53B8[i] = D_800C53D0[i];
    for (i = 0; i < 4; i++) D_800C53EC[i] = D_800C53E4[i];
}
