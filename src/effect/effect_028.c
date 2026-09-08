/**
 * @file effect_028.c
 * @brief Curaga
 */
#include "common.h"
#include "game.h"
#include "effect.h"
#include "psxsdk/libc.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/inline_c.h"
#include "effect/effect_028.h"
#include "btl_entity.h"
#include "battle/bc_object11.h"
#include "battle/bc_object15.h"
#include "battle/bc_object13.h"

/** @brief Where in the scratchpad the effect keeps its view matrix. */
#define EFFECT_SCRATCHPAD ((MATRIX *)0x1F8002E0)

/** @brief Stride between the prim and frame banks carved out of the TIM. */
#define EFFECT_BANK_SIZE 0x6000

/** @brief @ref EffectEntity::unk05C -- the frame counter's low bit picks a bank. */
#define EFFECT_FRAME_ODD 0x1

/** @brief Pack a screen corner the way the tint quads carry them. */
#define EFFECT_TINT_XY(x, y) ((x) | ((y) << 16))

/** @brief Prim code of a tint quad: a semi-transparent flat quad. */
#define EFFECT_TINT_CODE 0x2A

/** @brief Bytes cleared in each prim bank when the script restarts. */
#define EFFECT_BANK0_BYTES 0x25F8
/** @brief How far the mote ring runs before it wraps. */
#define EFFECT_MOTE_COUNT 0x59
/** @brief How far the node ring runs before it wraps. */
#define EFFECT_NODE_COUNT 0x8B
#define EFFECT_BANK1_BYTES 0x16F80


/**
 * @name Prim emitter flags -- @ref EffectPrimBuild::flags.
 * @{
 */
#define EFFECT_EMIT_SEMITRANS 0x1
#define EFFECT_EMIT_OPAQUE 0x4
#define EFFECT_EMIT_TWO_SIDED 0x10
#define EFFECT_EMIT_DEPTH_CUE 0x40

/* The gouraud emitters read their own copy of each of the four bits above. */
#define EFFECT_EMIT_G_SEMITRANS 0x2
#define EFFECT_EMIT_G_OPAQUE 0x8
#define EFFECT_EMIT_G_TWO_SIDED 0x20
#define EFFECT_EMIT_G_DEPTH_CUE 0x80
#define EFFECT_EMIT_TPAGE_SET 0x100
#define EFFECT_EMIT_CLUT_SET 0x200
#define EFFECT_EMIT_TPAGE_ADD 0x400
#define EFFECT_EMIT_CLUT_ADD 0x800
/** @brief Set, the vertex buffer the stream names is already resolved. */
#define EFFECT_EMIT_VERTS_SET 0x2000
/** @brief Set, the emitters keep the depth the packet came in with. */
#define EFFECT_EMIT_KEEP_DEPTH 0x1000
/** @} */

/**
 * @name Prim emitter clip result
 *
 * One bit per projected vertex per axis, accumulated in the emitters' @c reject.
 * A prim is dropped only when every one of its vertices left the screen on the
 * same axis, so a prim straddling an edge still draws.
 * @{
 */
#define EFFECT_CLIP_X0 0x1
#define EFFECT_CLIP_X1 0x2
#define EFFECT_CLIP_X2 0x4
#define EFFECT_CLIP_X3 0x8
#define EFFECT_CLIP_Y0 0x10
#define EFFECT_CLIP_Y1 0x20
#define EFFECT_CLIP_Y2 0x40
#define EFFECT_CLIP_Y3 0x80
#define EFFECT_CLIP_TRI_X (EFFECT_CLIP_X0 | EFFECT_CLIP_X1 | EFFECT_CLIP_X2)
#define EFFECT_CLIP_TRI_Y (EFFECT_CLIP_Y0 | EFFECT_CLIP_Y1 | EFFECT_CLIP_Y2)
#define EFFECT_CLIP_QUAD_X (EFFECT_CLIP_TRI_X | EFFECT_CLIP_X3)
#define EFFECT_CLIP_QUAD_Y (EFFECT_CLIP_TRI_Y | EFFECT_CLIP_Y3)
/** @} */

/**
 * @name Prim emitter clip test
 *
 * A projected vertex is off screen once its unsigned screen coordinate reaches
 * these; a prim is dropped only when all of its vertices fail the same axis.
 * @{
 */
#define EFFECT_EMIT_CLIP_X 0x141
#define EFFECT_EMIT_CLIP_Y 0xD9
/** @} */

/** @brief GTE @c FLAG bits 18 and 17: SZ3/OTZ saturated, divide overflow. */
#define EFFECT_GTE_OUT_OF_RANGE ((1 << 18) | (1 << 17))




/** @brief The flat triangle an emitter writes; six words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ u16 x1;
    /* 0x0E */ u16 y1;
    /* 0x10 */ u16 x2;
    /* 0x12 */ u16 y2;
} EffectTri; /* 0x14 */

/** @brief One flat triangle in the mesh stream: a colour and three vertex slots. */
typedef struct {
    /* 0x00 */ u32 colour;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u8 pad00A[0xC - 0xA];
} EffectEmitTri; /* 0xC */

/** @brief The flat quad an emitter writes; seven words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ u16 x1;
    /* 0x0E */ u16 y1;
    /* 0x10 */ u16 x2;
    /* 0x12 */ u16 y2;
    /* 0x14 */ u16 x3;
    /* 0x16 */ u16 y3;
} EffectQuad; /* 0x18 */

/** @brief One flat quad in the mesh stream: a colour and four vertex slots. */
typedef struct {
    /* 0x00 */ u32 colour;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u16 idx3;
} EffectEmitQuad; /* 0xC */

/**
 * @brief A prim's UV pair and the id that shares its word.
 *
 * The stream carries both halves together, so the emitters add the build's
 * offset to the whole word and only then patch the id half.
 */
typedef union {
    u32 word;
    struct {
        /* 0x00 */ u16 uv;   /**< U in the low byte, V in the high. */
        /* 0x02 */ u16 id;   /**< CLUT id or texture page, by slot. */
    } h;
} EffectPrimUV; /* 0x4 */

/** @brief The textured triangle an emitter writes; nine words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ EffectPrimUV uv0; /**< Its id half is the CLUT. */
    /* 0x10 */ u16 x1;
    /* 0x12 */ u16 y1;
    /* 0x14 */ EffectPrimUV uv1; /**< Its id half is the texture page. */
    /* 0x18 */ u16 x2;
    /* 0x1A */ u16 y2;
    /* 0x1C */ EffectPrimUV uv2;
} EffectTexTri; /* 0x20 */

/** @brief One textured triangle in the mesh stream. */
typedef struct {
    /* 0x00 */ u32 colour;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u32 idx2uv2; /**< Third vertex index, with its UV pair above it. */
    /* 0x0C */ u32 uv0;
    /* 0x10 */ u32 uv1;
} EffectEmitTexTri; /* 0x14 */

/** @brief The gouraud triangle an emitter writes; eight words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb0;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ u32 rgb1;
    /* 0x10 */ u16 x1;
    /* 0x12 */ u16 y1;
    /* 0x14 */ u32 rgb2;
    /* 0x18 */ u16 x2;
    /* 0x1A */ u16 y2;
} EffectGouraudTri; /* 0x1C */

/** @brief One gouraud triangle in the mesh stream: three corner colours. */
typedef struct {
    /* 0x00 */ u32 colour0;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u8 pad00A[0xC - 0xA];
    /* 0x0C */ u32 colour1;
    /* 0x10 */ u32 colour2;
} EffectEmitGouraudTri; /* 0x14 */

/** @brief The gouraud quad an emitter writes; ten words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb0;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ u32 rgb1;
    /* 0x10 */ u16 x1;
    /* 0x12 */ u16 y1;
    /* 0x14 */ u32 rgb2;
    /* 0x18 */ u16 x2;
    /* 0x1A */ u16 y2;
    /* 0x1C */ u32 rgb3;
    /* 0x20 */ u16 x3;
    /* 0x22 */ u16 y3;
} EffectGouraudQuad; /* 0x24 */

/** @brief One gouraud quad in the mesh stream: four corner colours. */
typedef struct {
    /* 0x00 */ u32 colour0;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u16 idx3;
    /* 0x0C */ u32 colour1;
    /* 0x10 */ u32 colour2;
    /* 0x14 */ u32 colour3;
} EffectEmitGouraudQuad; /* 0x18 */

/** @brief The gouraud textured triangle an emitter writes; eleven words. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb0;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ EffectPrimUV uv0; /**< Its id half is the CLUT. */
    /* 0x10 */ u32 rgb1;
    /* 0x14 */ u16 x1;
    /* 0x16 */ u16 y1;
    /* 0x18 */ EffectPrimUV uv1; /**< Its id half is the texture page. */
    /* 0x1C */ u32 rgb2;
    /* 0x20 */ u16 x2;
    /* 0x22 */ u16 y2;
    /* 0x24 */ EffectPrimUV uv2;
} EffectGouraudTexTri; /* 0x28 */

/** @brief One gouraud textured triangle in the mesh stream. */
typedef struct {
    /* 0x00 */ u32 colour0;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u32 idx2uv2; /**< Third vertex index, with its UV pair above it. */
    /* 0x0C */ u32 uv0;
    /* 0x10 */ u32 uv1;
    /* 0x14 */ u32 colour1;
    /* 0x18 */ u32 colour2;
} EffectEmitGouraudTexTri; /* 0x1C */

/** @brief The gouraud textured quad an emitter writes; fourteen words. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb0;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ EffectPrimUV uv0; /**< Its id half is the CLUT. */
    /* 0x10 */ u32 rgb1;
    /* 0x14 */ u16 x1;
    /* 0x16 */ u16 y1;
    /* 0x18 */ EffectPrimUV uv1; /**< Its id half is the texture page. */
    /* 0x1C */ u32 rgb2;
    /* 0x20 */ u16 x2;
    /* 0x22 */ u16 y2;
    /* 0x24 */ EffectPrimUV uv2;
    /* 0x28 */ u32 rgb3;
    /* 0x2C */ u16 x3;
    /* 0x2E */ u16 y3;
    /* 0x30 */ EffectPrimUV uv3;
} EffectGouraudTexQuad; /* 0x34 */

/**
 * @brief One gouraud textured quad in the mesh stream.
 *
 * The last two UV pairs share a word, so the build's offset is doubled into
 * both halves and the sum split again on the way out.
 */
typedef struct {
    /* 0x00 */ u32 colour0;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u16 idx3;
    /* 0x0C */ u32 uv0;
    /* 0x10 */ u32 uv1;
    /* 0x14 */ u32 uv23;
    /* 0x18 */ u32 colour1;
    /* 0x1C */ u32 colour2;
    /* 0x20 */ u32 colour3;
} EffectEmitGouraudTexQuad; /* 0x24 */

/** @brief The textured quad an emitter writes; eleven words including its tag. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;
    /* 0x08 */ u16 x0;
    /* 0x0A */ u16 y0;
    /* 0x0C */ EffectPrimUV uv0; /**< Its id half is the CLUT. */
    /* 0x10 */ u16 x1;
    /* 0x12 */ u16 y1;
    /* 0x14 */ EffectPrimUV uv1; /**< Its id half is the texture page. */
    /* 0x18 */ u16 x2;
    /* 0x1A */ u16 y2;
    /* 0x1C */ EffectPrimUV uv2;
    /* 0x20 */ u16 x3;
    /* 0x22 */ u16 y3;
    /* 0x24 */ EffectPrimUV uv3;
} EffectTexQuad; /* 0x28 */

/** @brief One textured quad in the mesh stream; its last two UV pairs share a word. */
typedef struct {
    /* 0x00 */ u32 colour;
    /* 0x04 */ u16 idx0;
    /* 0x06 */ u16 idx1;
    /* 0x08 */ u16 idx2;
    /* 0x0A */ u16 idx3;
    /* 0x0C */ u32 uv0;
    /* 0x10 */ u32 uv1;
    /* 0x14 */ u32 uv23;
} EffectEmitTexQuad; /* 0x18 */


/**
 * @brief What @ref func_801A3CDC walks: the mesh stream and how to shade it.
 *
 * Built on the scratchpad stack @ref D_801D3CD8 walks. The overlay reserves
 * battle's 0x58 bytes for it even though it fills far less.
 */
typedef struct {
    /* 0x00 */ s32 *stream;           /**< The mesh the packet draws. */
    /* 0x04 */ u32 *verts;            /**< Where the projected points are kept. */
    /* 0x08 */ EffectPrimColor color; /**< Far colour the depth cue fades toward. */
    /* 0x0C */ s32 depth;             /**< Depth-cue weight handed to DPCS. */
    /* 0x10 */ u16 tpage;             /**< Texture page the textured prims take. */
    /* 0x12 */ u8 pad012[0x14 - 0x12];
    /* 0x14 */ u16 clut;              /**< CLUT id the textured prims take. */
    /* 0x16 */ u8 pad016[0x18 - 0x16];
    /* 0x18 */ s32 unk018;
    /* 0x1C */ u32 flags;
    /* 0x20 */ s32 *cursor;           /**< Walks the stream, one entry per prim kind. */
    /* 0x24 */ s32 nclip;
    /* 0x28 */ u8 pad028[0x2C - 0x28];
    /* 0x2C */ s32 otz;
    /* 0x30 */ u32 gteFlag;
    /* 0x34 */ u8 pad034[0x58 - 0x34];
} EffectPrimBuild; /* 0x58 */


/** @brief The scratch a spawned node's aim and speed are worked out in. */
typedef struct {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ SVECTOR dir;    /**< Straight down, then turned by @c m. */
    /* 0x28 */ SVECTOR angle;  /**< The turn @c m was built from. */
    /* 0x30 */ SVECTOR spin;   /**< The turn the node's own pose takes. */
    /* 0x38 */ VECTOR vel;
    /* 0x48 */ s16 speedX;
    /* 0x4A */ s16 speedY;
    /* 0x4C */ s16 speedZ;
    /* 0x4E */ s16 frame;      /**< The mote's age, which indexes the tables. */
} EffectSpawnScratch; /* 0x50 */


/**
 * @name Script records
 *
 * @ref func_801A0B5C hands back as many bytes as the script asks for, with an
 * @ref EffectEntity at the front of them, so a script owns everything the
 * engine leaves alone. These two lay their own state over the pose fields.
 * @{
 */

/** @brief The entity of the script that builds and draws the list. */
typedef struct {
    /* 0x000 */ u8 pad000[0xC];   /**< The @ref EffectEntity the engine steps. */
    /* 0x00C */ EffectAnimSet *animSet;
    /* 0x010 */ EffectModel *model;
    /* 0x014 */ u8 pad014[0x2A - 0x14];
    /* 0x02A */ u8 anim;          /**< Which of the set's animations is playing. */
    /* 0x02B */ u8 pad02B;
    /* 0x02C */ u8 slot;          /**< Battle slot the effect is aimed at. */
    /* 0x02D */ u8 pad02D[0x30 - 0x2D];
    /* 0x030 */ EffectPoseTables *tables; /**< The tables the script was started on. */
    /* 0x034 */ EffectDrawList list;
    /* 0x268 */ u8 pad268[0x288 - 0x268];
    /* 0x288 */ s16 unk288;       /**< Where the strands are seeded from. */
    /* 0x28A */ s16 unk28A;
    /* 0x28C */ s16 unk28C;
    /* 0x28E */ u8 pad28E[0x290 - 0x28E];
    /* 0x290 */ s16 unk290;       /**< Where the trail's newest point goes. */
    /* 0x292 */ s16 unk292;
    /* 0x294 */ s16 unk294;
    /* 0x296 */ u8 pad296[0x298 - 0x296];
    /* 0x298 */ s16 stopFrame;    /**< Frame the script starts drawing on. */
    /* 0x29A */ u16 unk29A;
    /* 0x29C */ s16 unk29C;
    /* 0x29E */ s16 unk29E;
    /* 0x2A0 */ u8 pad2A0[0x2A4 - 0x2A0];
} EffectDrawScript; /* 0x2A4 */


/** @brief The full-screen quad the tint script draws, four to a frame. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;  /**< The step's colour; its top byte is the prim code. */
    /* 0x08 */ u32 xy0;  /**< X in the low half, Y in the high. */
    /* 0x0C */ u32 xy1;
    /* 0x10 */ u32 xy2;
    /* 0x14 */ u32 xy3;
} EffectTintQuad; /* 0x18 */


/** @brief The entity of the script that tints the screen. */
typedef struct {
    /* 0x00 */ u8 pad000[0x24];   /**< The @ref EffectEntity the engine steps. */
    /* 0x24 */ s16 frame;         /**< Frames the script has run. */
    /* 0x26 */ u16 flags;         /**< See @c EFFECT_FLAG_*. */
    /* 0x28 */ u8 wait;           /**< Live children. */
    /* 0x29 */ u8 pad029[0x30 - 0x29];
    /* 0x30 */ EffectTintStep *steps;
    /* 0x34 */ EffectTintStep tint; /**< The colour this frame's quads take. */
    /* 0x38 */ s16 step;            /**< How far into @c steps the play has got. */
    /* 0x3A */ u8 pad03A[0x3C - 0x3A];
} EffectTintScript; /* 0x3C */

/** @} */

/** @brief GTE @c FLAG bits 18 and 17: SZ3/OTZ saturated, divide overflow. */
#define BATTLE_SPRITE_GTE_OUT_OF_RANGE ((1 << 18) | (1 << 17))

/** @brief The draw-mode packet an additive sprite set is linked behind. */
#define BATTLE_SPRITE_ADDITIVE_MODE (0xE1000000 | (1 << 9) | getTPage(0, 1, 0, 0))

/** @brief Highest texture coordinate a sprite's right or bottom edge takes. */
#define BATTLE_SPRITE_UV_MAX 0xFF

static void func_801A80A4(EffectDrawNode *node);
static void func_801A9050(EffectDrawScript *script);
static void func_801A8184(EffectMote *mote);
static void func_801A87F8(void);
static void func_801A71E0(EffectMote *mote);
static void func_801A6F58(EffectMote *mote);
static void func_801A75F8(EffectMote *mote);
static void func_801A73E8(EffectMote *mote);
static EffectTri *func_801A3CDC(EffectPrimBuild *prim, u32 *ot, s32 otShift, EffectTri *head);
static EffectTri *func_801A20EC(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A23B8(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A26DC(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A2A64(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A2E48(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A3150(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A34D8(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static EffectTri *func_801A3894(EffectPrimBuild *s, u32 *ot, s32 otShift, EffectTri *poly);
static void func_801A1F58(EffectDrawNode *node, EffectPoseStep *step);
static void func_801A8904(EffectDrawScript *script);
static void func_801A8CE4(EffectPoseTables *tables);
static void func_801A8A1C(EffectDrawScript *script);
static void func_801A85A8(EffectDrawScript *script);
static EffectDrawNode *func_801A541C(EffectMote *owner, u8 source);
static void func_801A846C(void);
static void func_801A6074(void);
static void func_801A5FB4(EffectDrawNode *node, EffectPoseStep *step);
static POLY_FT4 *func_801A5D4C(BattleSpritePrim *prim, u32 *ot, s32 otShift, POLY_FT4 *head);
static POLY_FT4 *func_801A5720(BattleSpritePrim *prim, u32 *ot, s32 otShift, POLY_FT4 *head);
static s32 func_801A16A4(EffectTintScript *script);
static EffectMote *func_801A6194(void *owner, s16 source, s16 index);
static void func_801A8068(EffectDrawNode *node, s16 value);
static void func_801A631C(EffectMote *node, EffectPoseStep *step, s32 n);
static void func_801A41E0(EffectDrawNode *node, EffectPoseStep *step);
static void func_801A4ADC(EffectDrawNode *node, EffectPoseStep *step);
static void func_801A0978(MATRIX *m, s32 angle);
static void func_801A3F20(EffectDrawNode *node, EffectPoseStep *step);
void func_801A73E8(EffectMote *node);
static void func_801A75F8(EffectMote *node);


static void func_801A0208(void);
static void func_801A0358(u32 value);
static void func_801A0404(void);
static void func_801A0424(void);
static void func_801A042C(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY, s32 abr);
static void func_801A0528(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY, s32 abr);
static void func_801A0624(MATRIX *m);
static void func_801A065C(MATRIX *m, s32 angle);
static void func_801A07EC(MATRIX *m, s32 angle);
static s32 func_801A0B04(MATRIX *m);
static void *func_801A0B5C(void *pool, void *task, s32 stride, EffectEntity *owner);
static s32 func_801A92F8(EffectDrawScript *script);
static void func_801A0E1C(EffectEntity *entity, SVECTOR *out);
static void func_801A0E4C(EffectEntity *entity, SVECTOR *out);
static void func_801A0E7C(EffectEntity *entity, SVECTOR *out);
static void func_801A0FE8(EffectEntity *entity, SVECTOR *out);
static void func_801A1018(EffectEntity *entity, SVECTOR *out);
static void func_801A1048(EffectEntity *entity, SVECTOR *out);
static void func_801A12C4(EffectEntity *entity, SVECTOR *min, SVECTOR *max);
static s16 func_801A13B8(EffectEntity *entity);
static s16 func_801A13E8(EffectEntity *entity);
static s16 func_801A1440(EffectEntity *entity);
static s16 func_801A1464(EffectEntity *entity);
static s16 func_801A14AC(EffectEntity *entity);
static s16 func_801A14F4(EffectEntity *entity);
static s16 func_801A15AC(EffectEntity *entity);
static void func_801A1600(EffectEntity *entity, SVECTOR *out);
static void func_801A166C(void *dst, s32 size);
static void func_801A9568(EffectEntity *entity);
static void func_801A9684(EffectEntity *entity);
static void func_801A97D0(EffectEntity *entity);
static void func_801A998C(EffectEntity *entity);
static void func_801A9CEC(EffectEntity *entity);
static void func_801A83B8(EffectDrawNode *node, EffectPoseStep *step);
static s32 func_801A9CF4(EffectEntity *entity);

/**
 * @brief Start the effect's script and hand back its task pool.
 *
 * @param animSet Animation set the effect plays.
 * @return The pool the root entity lives in.
 */
void *func_801A0000(EffectAnimSet *animSet) {
    u8 *bank0;
    EffectEntity *entity;
    u8 *bank1;
    u8 *frames;

    D_801D3CCC = 0;
    D_801D3CD0 = 0;
    func_800B2A00(&D_801D488C, &D_801D47BC, 0x64, 2);
    entity = func_801A0B5C(&D_801D488C, func_801A9CF4, 0x64, NULL);
    entity->animSet = animSet;
    entity->unk02C = animSet->anims[entity->unk02A].unk000;
    entity->unk02D =
        entity->animSet->anims[entity->unk02A].parts[entity->unk02B].unk000;
    entity->pc = 0;
    entity->unk05A = animSet->anims->unk010;
    entity->unk058 = animSet->anims->unk011;
    entity->unk02F = entity->unk05A - 1;
    if (entity->unk02F < entity->unk058) {
        entity->unk02F = entity->unk058;
    }
    if (!(animSet->flags & EFFECT_ANIMSET_FLAG_LOADED)) {
        func_800C3BE0(&D_801AA10C);
        func_800BB084(&D_801AACAC);
    }
    D_801D3CDC = &D_801AACAC;
    bank0 = &D_801AACAC;
    D_801D47A4 = bank0;
    bank1 = bank0 + EFFECT_BANK_SIZE;
    D_801D47AC = bank0 + EFFECT_BANK_SIZE;
    D_801D3CDC = bank0 + EFFECT_BANK_SIZE;
    frames = bank1;
    D_801D47A8 = frames;
    frames += EFFECT_BANK_SIZE;
    D_801D3CDC = frames;
    D_801D47B0 = frames;
    func_800B2A00(&D_801D49FC, &D_801D489C, 0x58, 4);
    func_800B2A00(&D_801D4ACC, &D_801D4A0C, 0x3C, 3);
    func_800B2A00(&D_801D478C, &D_801D3F9C, 0x2A4, 3);
    func_800B2A00(&D_801D3F6C, &D_801D3CEC, 0x40, 0xA);
    return &D_801D488C;
}

/**
 * @brief Cache the sixteen hex digit glyphs and reset the debug text cursor.
 *
 * @note The lookup runs once per glyph -- the target holds 16 separate calls,
 *       so a loop or a cached pointer does not match.
 */
static void func_801A0208(void) {
    D_801D4ADC[0] = getMenuString(11)[1];
    D_801D4ADC[1] = getMenuString(11)[2];
    D_801D4ADC[2] = getMenuString(11)[3];
    D_801D4ADC[3] = getMenuString(11)[4];
    D_801D4ADC[4] = getMenuString(11)[5];
    D_801D4ADC[5] = getMenuString(11)[6];
    D_801D4ADC[6] = getMenuString(11)[7];
    D_801D4ADC[7] = getMenuString(11)[8];
    D_801D4ADC[8] = getMenuString(11)[9];
    D_801D4ADC[9] = getMenuString(11)[10];
    D_801D4ADC[10] = getMenuString(11)[11];
    D_801D4ADC[11] = getMenuString(11)[12];
    D_801D4ADC[12] = getMenuString(11)[13];
    D_801D4ADC[13] = getMenuString(11)[14];
    D_801D4ADC[14] = getMenuString(11)[15];
    D_801D4ADC[15] = getMenuString(11)[16];
    D_801D4ADC[16] = 0;
    D_801D4AF0 = 10;
    D_801D4AF4 = 20;
    D_801D4AF8 = 3;
}

/** @brief Draw @p value as eight hex glyphs at the current debug text cursor. */
static void func_801A0358(u32 value) {
    u32 *ot = D_800FA5E8->frontOT;
    u8 text[9];
    s32 i;

    for (i = 7; i >= 0; i--) {
        text[i] = D_801D4ADC[value & 0xF];
        value >>= 4;
    }
    text[8] = 0;
    D_801D479C = func_8002C56C(ot, D_801D479C, D_801D4AF0, D_801D4AF4,
                               text, D_801D4AF8);
    D_801D4AF0 += 0x48;
}

/** @brief Opcode handler: arm the frame delay and advance the running total. */
static void func_801A0404(void) {
    D_801D4AF0 = 10;
    D_801D4AF4 += 10;
}

/** @brief Opcode handler: no-op. */
static void func_801A0424(void) {
}

/**
 * @brief Emit a 128x128 textured quad at the head of the prim buffer.
 *
 * @param x     Left edge in screen space.
 * @param y     Top edge in screen space.
 * @param tx    Texture page X.
 * @param ty    Texture page Y.
 * @param clutX Palette X.
 * @param clutY Palette Y.
 * @param abr   Semi-transparency rate for the page.
 */
static void func_801A042C(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                   s32 abr) {
    POLY_FT4 *poly = D_801D479C;

    setPolyFT4(poly);
    setSemiTrans(poly, 1);
    setShadeTex(poly, 1);
    poly->tpage = getTPage(1, abr, tx, ty);
    poly->clut = getClut(clutX, clutY);
    poly->u0 = poly->u2 = 0;
    poly->u1 = poly->u3 = 0xFF;
    poly->v0 = poly->v1 = 0;
    poly->v2 = poly->v3 = 0xFF;
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = x + 0x7F;
    poly->y0 = poly->y1 = y;
    poly->y2 = poly->y3 = y + 0x7F;
    AddPrim(D_800FA5E8, poly);
    poly++;
    D_801D479C = poly;
}

/**
 * @brief Emit a 256x256 textured quad at the head of the prim buffer.
 *
 * @param x     Left edge in screen space.
 * @param y     Top edge in screen space.
 * @param tx    Texture page X.
 * @param ty    Texture page Y.
 * @param clutX Palette X.
 * @param clutY Palette Y.
 * @param abr   Semi-transparency rate for the page.
 */
static void func_801A0528(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                   s32 abr) {
    POLY_FT4 *poly = D_801D479C;

    setPolyFT4(poly);
    setSemiTrans(poly, 1);
    setShadeTex(poly, 1);
    poly->tpage = getTPage(1, abr, tx, ty);
    poly->clut = getClut(clutX, clutY);
    poly->u0 = poly->u2 = 0;
    poly->u1 = poly->u3 = 0xFF;
    poly->v0 = poly->v1 = 0;
    poly->v2 = poly->v3 = 0xFF;
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = x + 0xFF;
    poly->y0 = poly->y1 = y;
    poly->y2 = poly->y3 = y + 0xFF;
    AddPrim(D_800FA5E8, poly);
    poly++;
    D_801D479C = poly;
}

/** @brief Load the identity matrix. */
static void func_801A0624(MATRIX *m) {
    m->m[0][0] = ONE;
    m->m[1][0] = 0;
    m->m[2][0] = 0;
    m->m[0][1] = 0;
    m->m[1][1] = ONE;
    m->m[2][1] = 0;
    m->m[0][2] = 0;
    m->m[1][2] = 0;
    m->m[2][2] = ONE;
    m->t[0] = 0;
    m->t[1] = 0;
    m->t[2] = 0;
}

/** @brief Post-multiply @p m by a rotation of @p angle about X. */
static void func_801A065C(MATRIX *m, s32 angle) {
    EffectRotScratch *rot = func_800B3698(sizeof(EffectRotScratch));

    rot->sin = rsin(angle);
    rot->cos = rsin((angle + 0x400) & 0xFFF);
    rot->m.m[0][0] = ONE;
    rot->m.m[0][1] = 0;
    rot->m.m[0][2] = 0;
    rot->m.m[1][0] = 0;
    rot->m.m[1][1] = rot->cos;
    rot->m.m[1][2] = -rot->sin;
    rot->m.m[2][0] = 0;
    rot->m.m[2][1] = rot->sin;
    rot->m.m[2][2] = rot->cos;
    gte_MulMatrix0(m, &rot->m, m);
    func_800B36B8(sizeof(EffectRotScratch));
}

/**
 * @brief Compose a rotation about the Y axis into @p m.
 *
 * @param m     Matrix rotated in place.
 * @param angle Rotation, in the 0x1000-per-turn units @ref rsin takes.
 */
static void func_801A07EC(MATRIX *m, s32 angle) {
    EffectRotScratch *rot = func_800B3698(sizeof(EffectRotScratch));

    rot->sin = rsin(angle);
    rot->cos = rsin((angle + 0x400) & 0xFFF);
    rot->m.m[0][0] = rot->cos;
    rot->m.m[0][1] = 0;
    rot->m.m[0][2] = rot->sin;
    rot->m.m[1][0] = 0;
    rot->m.m[1][1] = ONE;
    rot->m.m[1][2] = 0;
    rot->m.m[2][0] = -rot->sin;
    rot->m.m[2][1] = 0;
    rot->m.m[2][2] = rot->cos;
    gte_MulMatrix0(m, &rot->m, m);
    func_800B36B8(sizeof(EffectRotScratch));
}

/** @brief Turn @p m about Z by @p angle. */
static void func_801A0978(MATRIX *m, s32 angle) {
    EffectRotScratch *rot = func_800B3698(sizeof(EffectRotScratch));

    rot->sin = rsin(angle);
    rot->cos = rsin((angle + 0x400) & 0xFFF);
    rot->m.m[0][0] = rot->cos;
    rot->m.m[0][1] = -rot->sin;
    rot->m.m[0][2] = 0;
    rot->m.m[1][0] = rot->sin;
    rot->m.m[1][1] = rot->cos;
    rot->m.m[1][2] = 0;
    rot->m.m[2][0] = 0;
    rot->m.m[2][1] = 0;
    rot->m.m[2][2] = ONE;
    gte_MulMatrix0(m, &rot->m, m);
    func_800B36B8(sizeof(EffectRotScratch));
}

/** @brief The heading @p m faces, a quarter turn ahead of its Z axis. */
static s32 func_801A0B04(MATRIX *m) {
    return (ratan2(m->m[2][2], m->m[0][2]) + 0x400) & 0xFFF;
}

/** @brief Opcode handler: release one frame of the linked script's wait. */
static void func_801A0B34(EffectEntity *entity) {
    if (entity->unk018 != NULL) {
        entity->unk018->wait--;
    }
}

/**
 * @brief Allocate an effect entity from @p pool and hang it off @p owner.
 *
 * The pool keeps its own 12-byte task header ahead of the entity fields, so
 * only the remainder of @p stride is cleared. A child inherits the owner's
 * model, animation set and position; an entity with no owner becomes its own
 * model.
 *
 * @param pool   Task pool to allocate from.
 * @param task   Per-frame step to install on the new entity.
 * @param stride Total size of the allocation, header included.
 * @param owner  Parent entity, or NULL for a root.
 * @return The new entity, or NULL if the pool is full.
 */
static void *func_801A0B5C(void *pool, void *task, s32 stride,
                                   EffectEntity *owner) {
    EffectEntity *node = func_800B2A84(pool, task);
    s32 *clear;
    s32 words;
    s32 i;

    if (node == NULL) {
        return NULL;
    }
    clear = (s32 *)&node->animSet;
    stride = stride - 0xC;
    words = stride / 4;
    for (i = 0; i < words; i++) {
        *clear = 0;
        clear++;
    }
    if (owner == NULL) {
        node->unk018 = NULL;
        node->unk014 = NULL;
        /* A root drives its own geometry, so it is its own model. */
        node->unk010 = (EffectModel *)node;
    } else {
        if (owner->unk014 == NULL) {
            node->unk014 = (EffectModel *)node;
        } else {
            node->unk014 = owner->unk014;
        }
        node->unk018 = owner;
        node->unk010 = owner->unk010;
        node->animSet = owner->animSet;
        node->pos = owner->pos;
        node->unk02A = owner->unk02A;
        node->unk02B = owner->unk02B;
        node->unk02E = owner->unk02E;
        node->unk02F = owner->unk02F;
        owner->wait++;
        node->unk02C = node->animSet->anims[node->unk02A].unk000;
        node->unk02D =
            node->animSet->anims[node->unk02A].parts[node->unk02B].unk000;
    }
    return node;
}

/** @brief Cache the battle slot's two anchor points and their midpoint. */
static void func_801A0CE8(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];
    SVECTOR top;
    SVECTOR bottom;

    if (!(slot->flags & BATTLE_SLOT_FLAG_UNK02)) {
        return;
    }
    func_800B3960(slot, 0xF0, 0, &top);
    func_800B3960(slot, 0xF1, 0, &bottom);
    entity->unk040.vx = top.vx;
    entity->unk040.vy = top.vy;
    entity->unk040.vz = top.vz;
    entity->unk038.vx = (top.vx + bottom.vx) / 2;
    entity->unk038.vy = (top.vy + bottom.vy) / 2;
    entity->unk038.vz = (top.vz + bottom.vz) / 2;
    entity->unk030.vx = bottom.vx;
    entity->unk030.vy = slot->unk024;
    entity->unk030.vz = bottom.vz;
}

/** @brief Copy @c unk030 of the linked model out to @p out. */
static void func_801A0E1C(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk030;
}

/** @brief Copy @c unk038 of the linked model out to @p out. */
static void func_801A0E4C(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk038;
}

/** @brief Copy @c unk040 of the linked model out to @p out. */
static void func_801A0E7C(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk040;
}

/** @brief As @ref func_801A0CE8, but for the slot the animation set names. */
static void func_801A0EAC(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->animSet->slot];
    SVECTOR top;
    SVECTOR bottom;

    if (!(slot->flags & BATTLE_SLOT_FLAG_UNK02)) {
        return;
    }
    func_800B3960(slot, 0xF0, 0, &top);
    func_800B3960(slot, 0xF1, 0, &bottom);
    entity->unk040.vx = top.vx;
    entity->unk040.vy = top.vy;
    entity->unk040.vz = top.vz;
    entity->unk038.vx = (top.vx + bottom.vx) / 2;
    entity->unk038.vy = (top.vy + bottom.vy) / 2;
    entity->unk038.vz = (top.vz + bottom.vz) / 2;
    entity->unk030.vx = bottom.vx;
    entity->unk030.vy = slot->unk024;
    entity->unk030.vz = bottom.vz;
}

/** @brief Copy @c unk030 of the linked model out to @p out. */
static void func_801A0FE8(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk030;
}

/** @brief Copy @c unk038 of the linked model out to @p out. */
static void func_801A1018(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk038;
}

/** @brief Copy @c unk040 of the linked model out to @p out. */
static void func_801A1048(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk040;
}

/**
 * @brief Recompute the model's bounding box from its posed skeleton.
 *
 * Transforms every joint after the root through the GTE and keeps the running
 * minimum and maximum, then publishes the box back onto the entity.
 *
 * @param entity Entity whose model is measured.
 */
static void func_801A1078(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];
    EffectSkeleton *skeleton = slot->mesh->skeleton;
    EffectJoint *joint = skeleton->joints;
    EffectBoundsScratch *b = func_800B3698(sizeof(EffectBoundsScratch));
    SVECTOR *bounds;
    s32 i;
    s16 x = joint->mtx.t[0];
    s16 y;
    s16 z;

    b->max.vx = x;
    b->min.vx = x;
    y = joint->mtx.t[1];
    b->max.vy = y;
    b->min.vy = y;
    z = joint->mtx.t[2];
    b->max.vz = z;
    b->min.vz = z;
    b->v.vx = 0;
    b->v.vy = 0;
    joint = &skeleton->joints[1];
    for (i = 1; i < skeleton->count; i++) {
        SetRotMatrix(&joint->mtx);
        SetTransMatrix(&joint->mtx);
        b->v.vz = -joint->unk002;
        gte_ldv0(&b->v);
        gte_mvmva(1, 0, 0, 0, 0);
        gte_stlvnl(&b->pos);
        gte_stflg(&b->flag);
        if (b->pos.vx < b->min.vx) {
            b->min.vx = b->pos.vx;
        } else if (b->max.vx < b->pos.vx) {
            b->max.vx = b->pos.vx;
        }
        if (b->pos.vy < b->min.vy) {
            b->min.vy = b->pos.vy;
        } else if (b->max.vy < b->pos.vy) {
            b->max.vy = b->pos.vy;
        }
        if (b->pos.vz < b->min.vz) {
            b->min.vz = b->pos.vz;
        } else if (b->max.vz < b->pos.vz) {
            b->max.vz = b->pos.vz;
        }
        joint++;
    }
    /* The box overlays unk048/unk04C, which the script opcodes use as a table
       pointer -- the two never overlap in time. */
    bounds = (SVECTOR *)entity->unk048;
    bounds[0] = b->min;
    bounds[1] = b->max;
    func_800B36B8(sizeof(EffectBoundsScratch));
}

/** @brief Copy the linked model's bounding box out to @p min and @p max. */
static void func_801A12C4(EffectEntity *entity, SVECTOR *min, SVECTOR *max) {
    EffectModel *model = entity->unk014;

    *min = model->boundsMin;
    *max = model->boundsMax;
}

/**
 * @brief Half the longest side of the model's bounding box.
 *
 * @param entity Entity whose model is measured.
 * @return Half the largest of the box's three extents.
 */
static s32 func_801A1314(EffectEntity *entity) {
    EffectModel *model = entity->unk014;
    s16 dx = model->boundsMax.vx - model->boundsMin.vx;
    s16 dy = model->boundsMax.vy - model->boundsMin.vy;
    s16 dz = model->boundsMax.vz - model->boundsMin.vz;
    if (dx > dy) {
        if (dx > dz) {
            return dx / 2;
        }
        return dz / 2;
    }
    if (dy > dz) {
        return dy / 2;
    }
    return dz / 2;
}

/** @brief Half the height of the linked model's bounding box. */
static s16 func_801A13B8(EffectEntity *entity) {
    EffectModel *model = entity->unk014;

    return (s16)(model->boundsMax.vy - model->boundsMin.vy) / 2;
}

/** @brief Half the linked model's larger horizontal extent. */
static s16 func_801A13E8(EffectEntity *entity) {
    EffectModel *model = entity->unk014;
    s16 spanX = model->boundsMax.vx - model->boundsMin.vx;
    s16 spanZ = model->boundsMax.vz - model->boundsMin.vz;
    /* Compared as `span` but returned as `spanX`: testing a separate copy is
       what puts the wider-X arm on the taken branch. */
    s16 span = spanX;

    return (span <= spanZ ? spanZ : spanX) / 2;
}

/** @brief Height of the linked model's bounding box. */
static s16 func_801A1440(EffectEntity *entity) {
    return entity->unk014->boundsMax.vy - entity->unk014->boundsMin.vy;
}

/** @brief Y of the top of the linked model, in battle-entity space. */
static s16 func_801A1464(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];

    return slot->pos.vy + entity->unk014->boundsMax.vy;
}

/** @brief Y of the bottom of the linked model, in battle-entity space. */
static s16 func_801A14AC(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];

    return slot->pos.vy + entity->unk014->boundsMin.vy;
}

/** @brief A random point up the linked model, in battle-entity space. */
static s16 func_801A14F4(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];
    EffectModel *model = entity->unk014;
    s16 height = model->boundsMax.vy - model->boundsMin.vy;
    s16 offset = (rand() & 0xFFF) * height / 4096;

    return slot->pos.vy + model->boundsMax.vy - offset / 2;
}

/** @brief Y of the centre of the linked model, in battle-entity space. */
static s16 func_801A15AC(EffectEntity *entity) {
    EffectModel *model = entity->unk014;

    return D_800EF2D0[entity->unk02D].pos.vy +
           (model->boundsMax.vy + model->boundsMin.vy) / 2;
}

/** @brief Centre of the linked model's bounding box. */
static void func_801A1600(EffectEntity *entity, SVECTOR *out) {
    EffectModel *model = entity->unk014;

    out->vx = (model->boundsMax.vx + model->boundsMin.vx) / 2;
    out->vy = (model->boundsMax.vy + model->boundsMin.vy) / 2;
    out->vz = (model->boundsMax.vz + model->boundsMin.vz) / 2;
}

/** @brief Zero @p size bytes' worth of words starting at @p dst. */
static void func_801A166C(void *dst, s32 size) {
    s32 *p = dst;
    s32 i;

    for (i = 0; i < size / 4; i++) {
        *p = 0;
        p++;
    }
}

/**
 * @brief Play one step of the screen tint and lay four quads over the frame.
 *
 * @return 2 once the script has stopped and its children have drained.
 */
static s32 func_801A16A4(EffectTintScript *script) {
    EffectTintStep step = script->steps[script->step];
    s32 draw = 1;

    if (step.op == 0xFF) {
        script->flags |= EFFECT_FLAG_STOP;
        draw = 0;
    } else if (step.op == 0xFE) {
        if (script->frame >= step.b) {
            script->step++;
            script->tint = script->steps[script->step];
        }
    } else {
        script->tint = script->steps[script->step];
        script->step++;
    }
    if (draw == 1) {
        /* The step's four bytes are the quad's colour word, code byte and all. */
        u32 *ot = &D_800FA5E8->frontOT[2];
        EffectTintQuad *quad = D_801D479C;
        DR_MODE *mode;

        quad->rgb = *(u32 *)&script->tint;
        setlen(quad, 5);
        setcode(quad, EFFECT_TINT_CODE);
        quad->xy0 = EFFECT_TINT_XY(0, 0);
        quad->xy1 = EFFECT_TINT_XY(0xA0, 0);
        quad->xy2 = EFFECT_TINT_XY(0, 0x78);
        quad->xy3 = EFFECT_TINT_XY(0xA0, 0x78);
        AddPrim(ot, quad);
        quad++;
        quad->rgb = *(u32 *)&script->tint;
        setlen(quad, 5);
        setcode(quad, EFFECT_TINT_CODE);
        quad->xy0 = EFFECT_TINT_XY(0xA0, 0);
        quad->xy1 = EFFECT_TINT_XY(0x140, 0);
        quad->xy2 = EFFECT_TINT_XY(0xA0, 0x78);
        quad->xy3 = EFFECT_TINT_XY(0x140, 0x78);
        AddPrim(ot, quad);
        quad++;
        quad->rgb = *(u32 *)&script->tint;
        setlen(quad, 5);
        setcode(quad, EFFECT_TINT_CODE);
        quad->xy0 = EFFECT_TINT_XY(0, 0x78);
        quad->xy1 = EFFECT_TINT_XY(0xA0, 0x78);
        quad->xy2 = EFFECT_TINT_XY(0, 0xF0);
        quad->xy3 = EFFECT_TINT_XY(0xA0, 0xF0);
        AddPrim(ot, quad);
        quad++;
        quad->rgb = *(u32 *)&script->tint;
        setlen(quad, 5);
        setcode(quad, EFFECT_TINT_CODE);
        quad->xy0 = EFFECT_TINT_XY(0xA0, 0x78);
        quad->xy1 = EFFECT_TINT_XY(0x140, 0x78);
        quad->xy2 = EFFECT_TINT_XY(0xA0, 0xF0);
        quad->xy3 = EFFECT_TINT_XY(0x140, 0xF0);
        AddPrim(ot, quad);
        quad++;
        mode = (DR_MODE *)quad;
        SetDrawMode(mode, 0, 0, GetTPage(0, 1, 0x280, 0), NULL);
        AddPrim(ot, mode);
        D_801D479C = mode + 1;
    }
    script->frame++;
    if (script->flags & EFFECT_FLAG_STOP) {
        if (script->wait == 0) {
            func_801A0B34((EffectEntity *)script);
            return 2;
        }
    }
    return 0;
}

/** @brief A random value between @p lo and @p hi. */
static s32 func_801A1980(s32 lo, s32 hi) {
    s32 span;
    s32 r;

    if (lo == hi) {
        return lo;
    }
    span = hi - lo;
    r = rand() % span;
    if (span >= 0) {
        r = r + lo;
    } else {
        r = lo - r;
    }
    return r;
}

/** @brief A random value between @p lo and @p hi, from a product of two draws. */
static s32 func_801A1A04(s32 lo, s32 hi) {
    s32 span;
    s32 r;

    if (lo == hi) {
        return lo;
    }
    span = hi - lo;
    r = rand() * rand() % span;
    if (span >= 0) {
        r = r + lo;
    } else {
        r = lo - r;
    }
    return r;
}

/** @brief Combine @p a and @p b component-wise through @ref func_801A1A04. */
static void func_801A1AA8(VECTOR *out, VECTOR *a, VECTOR *b) {
    out->vx = func_801A1A04(a->vx, b->vx);
    out->vy = func_801A1A04(a->vy, b->vy);
    out->vz = func_801A1A04(a->vz, b->vz);
}

/** @brief Build @p m from @p angles, applying only the turns that are non-zero. */
static void func_801A1B20(VECTOR *angles, MATRIX *m) {
    SVECTOR a;

    a.vx = angles->vx / 0x10000;
    a.vy = angles->vy / 0x10000;
    a.vz = angles->vz / 0x10000;
    func_801A0624(m);
    if (a.vz != 0) {
        func_801A0978(m, a.vz);
    }
    if (a.vx != 0) {
        func_801A065C(m, a.vx);
    }
    if (a.vy != 0) {
        func_801A07EC(m, a.vy);
    }
}

/** @brief Scatter @p base by up to half of @p spread, wrapped to one turn. */
static s32 func_801A1BEC(s32 base, s32 spread) {
    s32 half = spread / 2;
    s32 v = func_801A1A04(-half, half) + base;

    if (v >= 0) {
        while (v > 0xFFFFFFF) {
            v -= 0x10000000;
        }
    } else {
        while (v <= 0) {
            v += 0x10000000;
        }
    }
    return v;
}

/** @brief Combine @p a and @p b component-wise through @ref func_801A1BEC. */
static void func_801A1C80(VECTOR *out, VECTOR *a, VECTOR *b) {
    out->vx = func_801A1BEC(a->vx, b->vx);
    out->vy = func_801A1BEC(a->vy, b->vy);
    out->vz = func_801A1BEC(a->vz, b->vz);
}

/** @brief Scatter @p base by up to half of @p spread, wrapped to one turn. */
static s32 func_801A1CF8(s16 base, s16 spread) {
    s16 half = spread / 2;
    s32 v = func_801A1980(-half, half) + base;

    if (v >= 0) {
        while (v >= 0x1000) {
            v -= 0x1000;
        }
    } else {
        while (v <= 0) {
            v += 0x1000;
        }
    }
    return v;
}

/** @brief Combine @p a and @p b component-wise through @ref func_801A1CF8. */
static void func_801A1D88(SVECTOR *out, SVECTOR *a, SVECTOR *b) {
    out->vx = func_801A1CF8(a->vx, b->vx);
    out->vy = func_801A1CF8(a->vy, b->vy);
    out->vz = func_801A1CF8(a->vz, b->vz);
}

/** @brief Copy the three words at @p src to @p dst. */
static void func_801A1E00(s32 *src, s32 *dst) {
    /* The original reserved a slot here and never read it; dropping it moves
       every local below and the frame no longer matches. */
    VECTOR unused;

    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/**
 * @brief Fade @p value towards zero by @p t, a 16.16 fraction of it.
 *
 * @param t     Clamped to the closed range 0 to 1.0.
 * @param value Read and written in place.
 */
static void func_801A1E30(s32 t, s32 *value) {
    s32 v;

    if (t > 0x10000) {
        t = 0x10000;
    }
    if (t < 0) {
        t = 0;
    }
    v = *value;
    *value = v - (v / 256 * t / 256);
}

/** @brief Fade all three components of @p v towards zero by @p t. */
static void func_801A1E8C(s32 t, VECTOR *v) {
    /* The original reserved a slot here and never read it. */
    VECTOR unused;
    s32 x;
    s32 y;
    s32 z;

    if (t > 0x10000) {
        t = 0x10000;
    }
    if (t < 0) {
        t = 0;
    }
    x = v->vx;
    v->vx = x - (x / 256 * t / 256);
    y = v->vy;
    v->vy = y - (y / 256 * t / 256);
    z = v->vz;
    v->vz = z - (z / 256 * t / 256);
}

/** @brief Blend this frame's two source point sets into the node's target set. */
static void func_801A1F58(EffectDrawNode *node, EffectPoseStep *step) {
    u8 *dstId = &step->targets[node->unk1CA];
    u8 *srcAId = &step->framesA[node->unk1CA];
    u8 *srcBId = &step->framesB[node->unk1CA];
    EffectPointSet *target = D_801D3F84->targets[*dstId];
    EffectPointSet *a = D_801D3F84->frames[*srcAId];
    EffectPointSet *b = D_801D3F84->frames[*srcBId];

    if (a != NULL && b != NULL) {
        s32 *weight = func_800B3698(8);
        SVECTOR *out = target->points;
        SVECTOR *pa = a->points;
        SVECTOR *pb = b->points;
        s32 i = 0;
        s32 t = step->weights[node->unk1CA];
        s32 n = target->count;

        weight[1] = t;
        weight[0] = ONE - t;
        for (; i < n; i++) {
            gte_lddp(weight[0]);
            gte_ldsv(pa);
            gte_gpf1();
            pa++;
            gte_lddp(weight[1]);
            gte_ldsv(pb);
            gte_gpl1();
            pb++;
            gte_stsv(out);
            out++;
        }
        func_800B36B8(8);
    }
}

/**
 * @brief Emit the flat triangles the stream cursor points at.
 *
 * The entry starts with a triangle count; the cursor is stepped past it before
 * anything is drawn, so the caller always resumes at the next entry.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 *
 * @note Two spellings here are load-bearing for the whole emitter family. The
 *       empty @c do/while(0) wraps carry no code: they raise the loop depth
 *       flow.c weights register references by, which is what puts each
 *       variable in the register the original picked, and which statement
 *       needs one differs per emitter. The prim and stream cursors are cast on
 *       the way in and out because the driver chains one cursor type through
 *       eight emitters that each write a different prim.
 */
static EffectTri *func_801A20EC(EffectPrimBuild *s, u32 *ot, s32 otShift,
                                EffectTri *poly) {
    EffectEmitTri *tri;
    EffectTri *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    tri = (EffectEmitTri *)next;
    if (count > 0) {
        do {
            do {
                gte_ldv3(&verts[tri->idx0], &verts[tri->idx1],
                         &verts[tri->idx2]);
            } while (0);
            gte_rtpt();
            setlen(prim, 4);
            colour = tri->colour;
            prim->rgb = colour;
            if (s->flags & EFFECT_EMIT_SEMITRANS) {
                prim->rgb = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_OPAQUE) {
                prim->rgb &= ~EFFECT_PRIM_CODE(2);
            }
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_avsz3();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    if ((reject & EFFECT_CLIP_TRI_X) != EFFECT_CLIP_TRI_X &&
                        (reject & EFFECT_CLIP_TRI_Y) != EFFECT_CLIP_TRI_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_DEPTH_CUE) {
                            gte_ldrgb(&prim->rgb);
                            gte_lddp(s->depth);
                            gte_dpcs();
                            gte_strgb(&prim->rgb);
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            tri++;
        } while (i < count);
    }
    s->cursor = (s32 *)tri;
    return prim;
}

/**
 * @brief Emit the flat quads the stream cursor points at.
 *
 * Same walk as @ref func_801A2D08 with a fourth vertex: the first three are
 * projected together, the fourth on its own, and the average Z takes all four.
 * A quad is dropped when every vertex leaves the screen on the same axis.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A23B8(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitQuad *quad;
    EffectQuad *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectQuad *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    quad = (EffectEmitQuad *)next;
    if (count > 0) {
        do {
            gte_ldv3(&verts[quad->idx0], &verts[quad->idx1],
                     &verts[quad->idx2]);
            gte_rtpt();
            setlen(prim, 5);
            colour = quad->colour;
            prim->rgb = colour;
            if (s->flags & EFFECT_EMIT_SEMITRANS) {
                prim->rgb = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_OPAQUE) {
                prim->rgb &= ~EFFECT_PRIM_CODE(2);
            }
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_ldv0(&verts[quad->idx3]);
                    gte_rtps();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    gte_stsxy(&prim->x3);
                    gte_avsz4();
                    if (prim->x3 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X3;
                    }
                    if (prim->y3 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y3;
                    }
                    if ((reject & EFFECT_CLIP_QUAD_X) != EFFECT_CLIP_QUAD_X &&
                        (reject & EFFECT_CLIP_QUAD_Y) != EFFECT_CLIP_QUAD_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_DEPTH_CUE) {
                            gte_ldrgb(&prim->rgb);
                            gte_lddp(s->depth);
                            gte_dpcs();
                            gte_strgb(&prim->rgb);
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            quad++;
        } while (i < count);
    }
    s->cursor = (s32 *)quad;
    return (EffectTri *)prim;
}

/**
 * @brief Emit the textured triangles the stream cursor points at.
 *
 * Same walk as @ref func_801A2D08 with texture coordinates: each UV word comes
 * from the stream biased by the build's offset, and the texture page and CLUT
 * that share the first two UV words are either left alone, offset by the
 * build's own, or replaced by it.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A26DC(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitTexTri *tri;
    EffectTexTri *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectTexTri *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    tri = (EffectEmitTexTri *)next;
    if (count > 0) {
        do {
            do {
                gte_ldv3(&verts[tri->idx0], &verts[tri->idx1],
                         &verts[(u16)tri->idx2uv2]);
            } while (0);
            gte_rtpt();
            setlen(prim, 7);
            colour = tri->colour;
            prim->rgb = colour;
            if (s->flags & EFFECT_EMIT_SEMITRANS) {
                prim->rgb = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_OPAQUE) {
                prim->rgb &= ~EFFECT_PRIM_CODE(2);
            }
            prim->uv0.word = tri->uv0 + s->unk018;
            prim->uv1.word = tri->uv1 + s->unk018;
            prim->uv2.word = (tri->idx2uv2 >> 16) + s->unk018;
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                if (s->flags & EFFECT_EMIT_TPAGE_ADD) {
                    prim->uv1.h.id = prim->uv1.h.id + s->tpage;
                } else if (s->flags & EFFECT_EMIT_TPAGE_SET) {
                    prim->uv1.h.id = s->tpage;
                }
                if (s->flags & EFFECT_EMIT_CLUT_ADD) {
                    prim->uv0.h.id = prim->uv0.h.id + s->clut;
                } else if (s->flags & EFFECT_EMIT_CLUT_SET) {
                    prim->uv0.h.id = s->clut;
                }
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_avsz3();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    if ((reject & EFFECT_CLIP_TRI_X) != EFFECT_CLIP_TRI_X &&
                        (reject & EFFECT_CLIP_TRI_Y) != EFFECT_CLIP_TRI_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_DEPTH_CUE) {
                            gte_ldrgb(&prim->rgb);
                            gte_lddp(s->depth);
                            gte_dpcs();
                            gte_strgb(&prim->rgb);
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            tri++;
        } while (i < count);
    }
    s->cursor = (s32 *)tri;
    return (EffectTri *)prim;
}

/**
 * @brief Emit the textured quads the stream cursor points at.
 *
 * @ref func_801A2FE8 with texture coordinates, laid out like
 * @ref func_801A4548's but with one flat colour for the whole quad.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A2A64(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitTexQuad *quad;
    EffectTexQuad *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    u32 uv;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectTexQuad *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    quad = (EffectEmitTexQuad *)next;
    if (count > 0) {
        do {
            gte_ldv3(&verts[quad->idx0], &verts[quad->idx1],
                     &verts[quad->idx2]);
            gte_rtpt();
            setlen(prim, 9);
            colour = quad->colour;
            prim->rgb = colour;
            if (s->flags & EFFECT_EMIT_SEMITRANS) {
                prim->rgb = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_OPAQUE) {
                prim->rgb &= ~EFFECT_PRIM_CODE(2);
            }
            prim->uv0.word = quad->uv0 + s->unk018;
            prim->uv1.word = quad->uv1 + s->unk018;
            uv = quad->uv23 + (s->unk018 + (s->unk018 << 16));
            prim->uv2.word = uv;
            prim->uv3.word = uv >> 16;
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                if (s->flags & EFFECT_EMIT_TPAGE_ADD) {
                    prim->uv1.h.id = prim->uv1.h.id + s->tpage;
                } else if (s->flags & EFFECT_EMIT_TPAGE_SET) {
                    prim->uv1.h.id = s->tpage;
                }
                if (s->flags & EFFECT_EMIT_CLUT_ADD) {
                    prim->uv0.h.id = prim->uv0.h.id + s->clut;
                } else if (s->flags & EFFECT_EMIT_CLUT_SET) {
                    prim->uv0.h.id = s->clut;
                }
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_ldv0(&verts[quad->idx3]);
                    gte_rtps();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    gte_stsxy(&prim->x3);
                    gte_avsz4();
                    if (prim->x3 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X3;
                    }
                    if (prim->y3 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y3;
                    }
                    if ((reject & EFFECT_CLIP_QUAD_X) != EFFECT_CLIP_QUAD_X &&
                        (reject & EFFECT_CLIP_QUAD_Y) != EFFECT_CLIP_QUAD_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_DEPTH_CUE) {
                            gte_ldrgb(&prim->rgb);
                            gte_lddp(s->depth);
                            gte_dpcs();
                            gte_strgb(&prim->rgb);
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            quad++;
        } while (i < count);
    }
    s->cursor = (s32 *)quad;
    return (EffectTri *)prim;
}

/**
 * @brief Emit the gouraud triangles the stream cursor points at.
 *
 * Same walk as @ref func_801A2D08 with a colour per corner. When the build
 * asks for a depth cue all three corners go through the GTE together, which
 * also refreshes the first corner; otherwise the two extra corners are copied
 * straight out of the stream.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A2E48(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitGouraudTri *tri;
    EffectGouraudTri *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectGouraudTri *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    tri = (EffectEmitGouraudTri *)next;
    if (count > 0) {
        do {
            do {
                gte_ldv3(&verts[tri->idx0], &verts[tri->idx1],
                         &verts[tri->idx2]);
            } while (0);
            gte_rtpt();
            setlen(prim, 6);
            colour = tri->colour0;
            prim->rgb0 = colour;
            if (s->flags & EFFECT_EMIT_G_SEMITRANS) {
                prim->rgb0 = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_G_OPAQUE) {
                prim->rgb0 &= ~EFFECT_PRIM_CODE(2);
            }
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_avsz3();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    if ((reject & EFFECT_CLIP_TRI_X) != EFFECT_CLIP_TRI_X &&
                        (reject & EFFECT_CLIP_TRI_Y) != EFFECT_CLIP_TRI_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_G_DEPTH_CUE) {
                            gte_ldrgb3(&tri->colour1, &tri->colour2,
                                       &prim->rgb0);
                            gte_lddp(s->depth);
                            gte_dpct();
                            gte_strgb3(&prim->rgb1, &prim->rgb2, &prim->rgb0);
                        } else {
                            prim->rgb1 = tri->colour1;
                            prim->rgb2 = tri->colour2;
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            tri++;
        } while (i < count);
    }
    s->cursor = (s32 *)tri;
    return (EffectTri *)prim;
}

/**
 * @brief Emit the gouraud quads the stream cursor points at.
 *
 * @ref func_801A3AAC with a fourth corner: the first three vertices are
 * projected together and the fourth on its own, and the depth cue runs over
 * the three stream colours together before the prim's own colour separately.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A3150(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitGouraudQuad *quad;
    EffectGouraudQuad *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectGouraudQuad *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    quad = (EffectEmitGouraudQuad *)next;
    if (count > 0) {
        do {
            do {
                gte_ldv3(&verts[quad->idx0], &verts[quad->idx1],
                         &verts[quad->idx2]);
            } while (0);
            gte_rtpt();
            setlen(prim, 8);
            colour = quad->colour0;
            prim->rgb0 = colour;
            if (s->flags & EFFECT_EMIT_G_SEMITRANS) {
                prim->rgb0 = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_G_OPAQUE) {
                prim->rgb0 &= ~EFFECT_PRIM_CODE(2);
            }
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_ldv0(&verts[quad->idx3]);
                    gte_rtps();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    gte_stsxy(&prim->x3);
                    gte_avsz4();
                    if (prim->x3 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X3;
                    }
                    if (prim->y3 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y3;
                    }
                    if ((reject & EFFECT_CLIP_QUAD_X) != EFFECT_CLIP_QUAD_X &&
                        (reject & EFFECT_CLIP_QUAD_Y) != EFFECT_CLIP_QUAD_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_G_DEPTH_CUE) {
                            gte_ldrgb3(&quad->colour1, &quad->colour2,
                                       &quad->colour3);
                            gte_lddp(s->depth);
                            gte_dpct();
                            gte_strgb3(&prim->rgb1, &prim->rgb2, &prim->rgb3);
                            gte_ldrgb(&prim->rgb0);
                            gte_dpcs();
                            gte_strgb(&prim->rgb0);
                        } else {
                            prim->rgb1 = quad->colour1;
                            prim->rgb2 = quad->colour2;
                            prim->rgb3 = quad->colour3;
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            quad++;
        } while (i < count);
    }
    s->cursor = (s32 *)quad;
    return (EffectTri *)prim;
}

/**
 * @brief Emit the gouraud textured triangles the stream cursor points at.
 *
 * @ref func_801A3AAC with texture coordinates: the UV words come from the
 * stream biased by the build's offset, and the texture page and CLUT sharing
 * the first two are left alone, offset, or replaced as the flags ask.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A34D8(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitGouraudTexTri *tri;
    EffectGouraudTexTri *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectGouraudTexTri *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    tri = (EffectEmitGouraudTexTri *)next;
    if (count > 0) {
        do {
            do {
                gte_ldv3(&verts[tri->idx0], &verts[tri->idx1],
                         &verts[(u16)tri->idx2uv2]);
            } while (0);
            gte_rtpt();
            setlen(prim, 9);
            colour = tri->colour0;
            prim->rgb0 = colour;
            if (s->flags & EFFECT_EMIT_G_SEMITRANS) {
                prim->rgb0 = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_G_OPAQUE) {
                prim->rgb0 &= ~EFFECT_PRIM_CODE(2);
            }
            prim->uv0.word = tri->uv0 + s->unk018;
            prim->uv1.word = tri->uv1 + s->unk018;
            prim->uv2.word = (tri->idx2uv2 >> 16) + s->unk018;
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                if (s->flags & EFFECT_EMIT_TPAGE_ADD) {
                    prim->uv1.h.id = prim->uv1.h.id + s->tpage;
                } else if (s->flags & EFFECT_EMIT_TPAGE_SET) {
                    prim->uv1.h.id = s->tpage;
                }
                if (s->flags & EFFECT_EMIT_CLUT_ADD) {
                    prim->uv0.h.id = prim->uv0.h.id + s->clut;
                } else if (s->flags & EFFECT_EMIT_CLUT_SET) {
                    prim->uv0.h.id = s->clut;
                }
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_avsz3();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    if ((reject & EFFECT_CLIP_TRI_X) != EFFECT_CLIP_TRI_X &&
                        (reject & EFFECT_CLIP_TRI_Y) != EFFECT_CLIP_TRI_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_G_DEPTH_CUE) {
                            gte_ldrgb3(&tri->colour1, &tri->colour2,
                                       &prim->rgb0);
                            gte_lddp(s->depth);
                            gte_dpct();
                            gte_strgb3(&prim->rgb1, &prim->rgb2, &prim->rgb0);
                        } else {
                            prim->rgb1 = tri->colour1;
                            prim->rgb2 = tri->colour2;
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            tri++;
        } while (i < count);
    }
    s->cursor = (s32 *)tri;
    return (EffectTri *)prim;
}

/**
 * @brief Emit the gouraud textured quads the stream cursor points at.
 *
 * The widest of the family: four corners, each with its own colour and UV
 * pair. @ref func_801A4174 with a fourth vertex.
 *
 * @param s        Emitter state: the stream cursor, the vertex buffer and the flags.
 * @param ot       Ordering table the prims are linked into.
 * @param otShift  Right shift applied to the GTE's average Z to pick the OT slot.
 * @param poly     Prim cursor to write from.
 * @return The prim cursor advanced past everything written.
 */
static EffectTri *func_801A3894(EffectPrimBuild *s, u32 *ot, s32 otShift,
                         EffectTri *poly) {
    EffectEmitGouraudTexQuad *quad;
    EffectGouraudTexQuad *prim;
    s32 *cursor;
    s32 *next;
    u32 *verts;
    u32 colour;
    u32 uv;
    s32 count;
    s32 reject;
    s32 i;

    i = 0;
    prim = (EffectGouraudTexQuad *)poly;
    cursor = s->cursor;
    verts = s->verts;
    count = cursor[0];
    next = cursor + 1;
    s->cursor = next;
    quad = (EffectEmitGouraudTexQuad *)next;
    if (count > 0) {
        do {
            gte_ldv3(&verts[quad->idx0], &verts[quad->idx1],
                     &verts[quad->idx2]);
            gte_rtpt();
            setlen(prim, 0xC);
            colour = quad->colour0;
            prim->rgb0 = colour;
            if (s->flags & EFFECT_EMIT_G_SEMITRANS) {
                prim->rgb0 = colour | EFFECT_PRIM_CODE(2);
            }
            if (s->flags & EFFECT_EMIT_G_OPAQUE) {
                prim->rgb0 &= ~EFFECT_PRIM_CODE(2);
            }
            prim->uv0.word = quad->uv0 + s->unk018;
            prim->uv1.word = quad->uv1 + s->unk018;
            uv = quad->uv23 + (s->unk018 + (s->unk018 << 16));
            prim->uv2.word = uv;
            prim->uv3.word = uv >> 16;
            gte_stflg(&s->gteFlag);
            if (!(s->gteFlag & EFFECT_GTE_OUT_OF_RANGE)) {
                gte_nclip();
                if (s->flags & EFFECT_EMIT_TPAGE_ADD) {
                    prim->uv1.h.id = prim->uv1.h.id + s->tpage;
                } else if (s->flags & EFFECT_EMIT_TPAGE_SET) {
                    prim->uv1.h.id = s->tpage;
                }
                if (s->flags & EFFECT_EMIT_CLUT_ADD) {
                    prim->uv0.h.id = prim->uv0.h.id + s->clut;
                } else if (s->flags & EFFECT_EMIT_CLUT_SET) {
                    prim->uv0.h.id = s->clut;
                }
                reject = 0;
                gte_stopz(&s->nclip);
                if (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED)) {
                    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
                    gte_ldv0(&verts[quad->idx3]);
                    gte_rtps();
                    if (prim->x0 >= EFFECT_EMIT_CLIP_X) {
                        reject = EFFECT_CLIP_X0;
                    }
                    if (prim->x1 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X1;
                    }
                    if (prim->x2 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X2;
                    }
                    if (prim->y0 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y0;
                    }
                    if (prim->y1 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y1;
                    }
                    if (prim->y2 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y2;
                    }
                    gte_stsxy(&prim->x3);
                    gte_avsz4();
                    if (prim->x3 >= EFFECT_EMIT_CLIP_X) {
                        reject |= EFFECT_CLIP_X3;
                    }
                    if (prim->y3 >= EFFECT_EMIT_CLIP_Y) {
                        reject |= EFFECT_CLIP_Y3;
                    }
                    if ((reject & EFFECT_CLIP_QUAD_X) != EFFECT_CLIP_QUAD_X &&
                        (reject & EFFECT_CLIP_QUAD_Y) != EFFECT_CLIP_QUAD_Y) {
                        gte_stotz(&s->otz);
                        if (s->flags & EFFECT_EMIT_G_DEPTH_CUE) {
                            gte_ldrgb3(&quad->colour1, &quad->colour2,
                                       &quad->colour3);
                            gte_lddp(s->depth);
                            gte_dpct();
                            gte_strgb3(&prim->rgb1, &prim->rgb2, &prim->rgb3);
                            gte_ldrgb(&prim->rgb0);
                            gte_dpcs();
                            gte_strgb(&prim->rgb0);
                        } else {
                            prim->rgb1 = quad->colour1;
                            prim->rgb2 = quad->colour2;
                            prim->rgb3 = quad->colour3;
                        }
                        addPrim(&ot[s->otz >> otShift], prim);
                        prim++;
                    }
                }
            }
            do {
                i++;
            } while (0);
            quad++;
        } while (i < count);
    }
    s->cursor = (s32 *)quad;
    return (EffectTri *)prim;
}

/**
 * @brief Walk @p prim's mesh stream, letting each emitter draw its own prims.
 *
 * @return The prim list head, advanced past everything the emitters wrote.
 */
static EffectTri *func_801A3CDC(EffectPrimBuild *prim, u32 *ot, s32 otShift, EffectTri *head) {
    if (!(prim->flags & EFFECT_EMIT_VERTS_SET)) {
        prim->verts = (u32 *)(prim->stream + 2);
    }
    prim->cursor = (s32 *)((u8 *)prim->stream + prim->stream[0]);
    if (!(prim->flags & EFFECT_EMIT_KEEP_DEPTH)) {
        prim->unk018 = 0;
    }
    gte_ldfcb(prim->color.r, prim->color.g, prim->color.b);
    if (*prim->cursor != 0) {
        head = func_801A20EC(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A23B8(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A26DC(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A2A64(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A2E48(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A3150(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A34D8(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    if (*prim->cursor != 0) {
        head = func_801A3894(prim, ot, otShift, head);
    } else {
        prim->cursor++;
    }
    return head;
}

/** @brief Build one mesh packet for @p node and link every copy into the OT. */
static void func_801A3F20(EffectDrawNode *node, EffectPoseStep *step) {
    EffectPrimBuild *prim;

    D_801D3CD8 -= sizeof(EffectPrimBuild);
    prim = (EffectPrimBuild *)D_801D3CD8;
    prim->stream = node->stream;
    prim->flags = 0;
    if (step->unk03A == 0) {
        prim->flags = EFFECT_EMIT_TWO_SIDED | EFFECT_EMIT_G_TWO_SIDED;
    }
    if (node->unk1CE != 0) {
        prim->color = node->color;
        prim->flags |= EFFECT_EMIT_DEPTH_CUE | EFFECT_EMIT_G_DEPTH_CUE;
        prim->depth = node->unk1CE;
    }
    if (node->copies == 1) {

        gte_SetRotMatrix(&node->mtx);
        gte_SetTransMatrix(&node->mtx);
        D_801D479C = func_801A3CDC(prim, D_800FA5E8->ot, 2, D_801D479C);
    } else {
        s32 i;

        for (i = 0; i < node->copies; i++) {
            node->mtx.t[0] = node->offsets[i].vx;
            node->mtx.t[1] = node->offsets[i].vy;
            node->mtx.t[2] = node->offsets[i].vz;
            gte_SetRotMatrix(&node->mtx);
            gte_SetTransMatrix(&node->mtx);
            D_801D479C = func_801A3CDC(prim, D_800FA5E8->ot, 2, D_801D479C);
        }
    }
    D_801D3CD8 += sizeof(EffectPrimBuild);
}

/** @brief Run the late step on every node whose source is in the right mode. */
static void func_801A4128(void) {
    EffectDrawNode *node = D_801D3F84->head;

    while (node != NULL) {
        if (node->unk008 == 1) {
            s32 idx = node->unk1D6;
            EffectPoseStep *step = D_801D3F84->sources[idx];

            if ((u32)(step->mode - 4) < 2 && node->stream != NULL) {
                func_801A3F20(node, step);
            }
        }
        node = node->next;
    }
}

/** @brief Advance every copy of @p node: spin it, drift it and turn its pose. */
static void func_801A41E0(EffectDrawNode *node, EffectPoseStep *step) {
    VECTOR posed[4];
    VECTOR spinOffset;
    VECTOR spun;
    MATRIX m;
    s32 i;

    node->angle = node->angleBase;
    node->angle.vx += step->unk0D8[node->unk1CA];
    node->angle.vy += step->unk0DC[node->unk1CA];
    node->angle.vz += step->unk0E0[node->unk1CA];
    node->angle.vx &= 0xFFF;
    node->angle.vy &= 0xFFF;
    node->angle.vz &= 0xFFF;
    node->lastPos = node->pos[0];
    switch (step->unk027) {
    case 0:
        for (i = 0; i < node->copies; i++) {
            if (node->spinRate != NULL || node->spin != NULL) {
                node->spin[i] += node->spinRate[i];
                if (node->unk1DA != 0) {
                    func_801A1E30(node->unk1DA, &node->spin[i]);
                }
                spinOffset.vx = 0;
                spinOffset.vy = -node->spin[i];
                spinOffset.vz = 0;
                ApplyMatrixLV(&node->poses[i], &spinOffset, &spun);
                node->offset[i].vx += spun.vx;
                node->offset[i].vy += spun.vy;
                node->offset[i].vz += spun.vz;
            }
            node->accel[i].vx += node->jerk[i].vx;
            node->accel[i].vy += node->jerk[i].vy;
            node->accel[i].vz += node->jerk[i].vz;
            node->vel[i].vx += node->accel[i].vx;
            node->vel[i].vy += node->accel[i].vy;
            node->vel[i].vz += node->accel[i].vz;
            node->vel[i].vy += node->unk1DC;
            if (node->unk1DA != 0) {
                func_801A1E8C(node->unk1DA, &node->vel[i]);
            }
            node->offset[i].vx += node->vel[i].vx;
            node->offset[i].vy += node->vel[i].vy;
            node->offset[i].vz += node->vel[i].vz;
        }
        break;
    case 1:
        for (i = 0; i < node->copies; i++) {
            node->offset[i].vx = step->unk13C[node->unk1CA] << 16;
            node->offset[i].vy = step->unk140[node->unk1CA] << 16;
            node->offset[i].vz = step->unk144[node->unk1CA] << 16;
            if (node->owner != NULL) {
                m = node->owner->mtx;
            }
            func_801A0624(&m);
            ApplyMatrixLV(&m, &node->offset[i], &node->offset[i]);
        }
        break;
    }
    if (step->unk026 == 1) {
        EffectMote *owner = node->owner;

        switch (step->unk01D) {
        case 0:
            func_801A0624(&m);
            if (owner != NULL) {
                m = owner->mtx;
            }
            if (node->angle.vz != 0) {
                func_801A0978(&m, node->angle.vz);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&m, node->angle.vx);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&m, node->angle.vy);
            }
            break;
        case 1:
            func_801A0624(&m);
            if (owner != NULL) {
                m = owner->mtx;
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&m, node->angle.vy);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&m, node->angle.vx);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&m, node->angle.vz);
            }
            break;
        case 2:
            TransposeMatrix(D_801D3CD4, &m);
            if (node->angle.vz != 0) {
                func_801A0978(&m, node->angle.vz);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&m, node->angle.vx);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&m, node->angle.vy);
            }
            break;
        case 3:
            TransposeMatrix(D_801D3CD4, &m);
            if (node->angle.vy != 0) {
                func_801A07EC(&m, node->angle.vy);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&m, node->angle.vx);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&m, node->angle.vz);
            }
            break;
        }
        for (i = 0; i < node->copies; i++) {
            ApplyMatrixLV(&m, &node->offset[i], &posed[i]);
        }
    } else {
        for (i = 0; i < node->copies; i++) {
            posed[i] = node->offset[i];
        }
    }
    switch (step->unk022) {
    case 0:
        for (i = 0; i < node->copies; i++) {
            node->pos[i] = posed[i];
        }
        break;
    case 1: {
        EffectMote *owner = node->owner;

        if (owner != NULL) {
            for (i = 0; i < node->copies; i++) {
                node->pos[i] = owner->pos;
                node->pos[i].vx += posed[i].vx;
                node->pos[i].vy += posed[i].vy;
                node->pos[i].vz += posed[i].vz;
            }
        }
        break;
    }
    }
    if (step->unk01B == 1) {
        for (i = 0; i < node->copies; i++) {
            node->pos[i].vy = 0;
        }
    }
}

/** @brief Age @p node's hold on its current step and advance when it expires. */
static void func_801A49E8(EffectDrawNode *node, EffectPoseStep *step) {
    switch (step->mode) {
    case 0:
        node->unk1D0++;
        if (node->unk1D1 < node->unk1D0) {
            node->unk1D0 = 0;
            node->unk1D2 = 1;
        }
        break;
    case 2:
        node->unk1D0++;
        if (node->unk1D4 < node->unk1D0) {
            if (node->unk1D5 != 0) {
                node->unk1D5--;
                node->unk1D0 = node->unk1D3;
            }
        }
        if (node->unk1D0 > node->unk1D1) {
            node->unk1D0 = 0;
            node->unk1D2 = 1;
        }
        break;
    case 1:
        node->unk1D0++;
        if (node->unk1D1 < node->unk1D0) {
            node->unk1D0 = 0;
        }
        break;
    }
}

/** @brief Build @p node's pose: orient it, colour it, scale it and place it. */
static void func_801A4ADC(EffectDrawNode *node, EffectPoseStep *step) {
    VECTOR scale;
    /* The original reserved a second slot here and never read it. */
    VECTOR unused;
    s32 i;

    if (step->mode == 4 || step->mode == 5) {
        u8 *target = &step->targets[node->unk1CA];

        node->stream = D_801D3F84->targets[*target];
        if (step->mode == 5) {
            /* Called with no arguments: node and step are already in $a0/$a1. */
            ((void (*)())func_801A1F58)();
        }
    }
    switch (step->unk01B) {
    case 0:
        TransposeMatrix(D_801D3CD4, &node->orient);
        if (node->angle.vz != 0) {
            func_801A0978(&node->orient, node->angle.vz);
        }
        break;
    case 3: {
        EffectMote *owner = node->owner;

        switch (step->unk01D) {
        case 0:
            if (owner != NULL) {
                node->orient = owner->mtx;
            } else {
                func_801A0624(&node->orient);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            break;
        case 1:
            if (owner != NULL) {
                node->orient = owner->mtx;
            } else {
                func_801A0624(&node->orient);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            break;
        case 2:
                TransposeMatrix(D_801D3CD4, &node->orient);
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            break;
        case 3:
                TransposeMatrix(D_801D3CD4, &node->orient);
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            break;
        }
        break;
    }
    case 1:
        func_801A0624(&node->orient);
        func_801A065C(&node->orient, 0x400);
        break;
    case 2: {
        s16 angle = func_801A0B04(D_801D3CD4);

        func_801A0624(&node->orient);
        if (angle != 0) {
            func_801A07EC(&node->orient, angle);
        }
        break;
    }
    case 4: {
        EffectMote *owner = node->owner;

        switch (step->unk01D) {
        case 0:
            if (owner != NULL) {
                node->orient = owner->mtx;
            } else {
                func_801A0624(&node->orient);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            break;
        case 1:
            if (owner != NULL) {
                node->orient = owner->mtx;
            } else {
                func_801A0624(&node->orient);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            break;
        case 2:
                TransposeMatrix(D_801D3CD4, &node->orient);
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            break;
        case 3:
                TransposeMatrix(D_801D3CD4, &node->orient);
            if (node->angle.vy != 0) {
                func_801A07EC(&node->orient, node->angle.vy);
            }
            if (node->angle.vx != 0) {
                func_801A065C(&node->orient, node->angle.vx);
            }
            if (node->angle.vz != 0) {
                func_801A0978(&node->orient, node->angle.vz);
            }
            break;
        }
        break;
    }
    }
    if (step->mode == 4 || step->mode == 5) {
        node->color.r = step->unk0F0[node->unk1CA];
        node->color.g = step->unk0F4[node->unk1CA];
        node->color.b = step->unk0F8[node->unk1CA];
        node->unk1CE = step->unk0FC[node->unk1CA];
        node->scaleX = step->unk0E4[node->unk1CA];
        node->scaleY = step->unk0E8[node->unk1CA];
        node->scaleZ = step->unk0EC[node->unk1CA];
        scale.vx = node->scaleX;
        scale.vy = node->scaleY;
        scale.vz = node->scaleZ;
        ScaleMatrix(&node->orient, &scale);
    }
    if (node->copies == 1) {
        node->orient.t[0] = node->pos[0].vx / 0x10000;
        node->orient.t[1] = node->pos[0].vy / 0x10000;
        node->orient.t[2] = node->pos[0].vz / 0x10000;
        gte_MulMatrix0(D_801D3CD4, &node->orient, &node->mtx);
        gte_SetTransMatrix(D_801D3CD4);
        gte_ldlv0(node->orient.t);
        gte_rt();
        gte_stlvnl(node->mtx.t);
        node->mtx.t[2] += step->unk038;
    } else {
        for (i = 0; i < node->copies; i++) {
            node->orient.t[0] = node->pos[i].vx / 0x10000;
            node->orient.t[1] = node->pos[i].vy / 0x10000;
            node->orient.t[2] = node->pos[i].vz / 0x10000;
            gte_MulMatrix0(D_801D3CD4, &node->orient, &node->mtx);
            gte_SetTransMatrix(D_801D3CD4);
            gte_ldlv0(node->orient.t);
            gte_rt();
            gte_stlvnl(node->mtx.t);
            node->offsets[i].vx = node->mtx.t[0];
            node->offsets[i].vy = node->mtx.t[1];
            node->offsets[i].vz = node->mtx.t[2];
            node->offsets[i].vz += step->unk038;
        }
    }
}

/**
 * @brief Take the next free node off bank 1 and put it on the draw list.
 *
 * The search starts where the last one left off and gives up after one lap.
 *
 * @return The node, or NULL when every slot is taken.
 */
static EffectDrawNode *func_801A541C(EffectMote *owner, u8 source) {
    EffectDrawNode *found = NULL;
    s32 i = D_801D3F8A;
    s32 n;

    for (n = 0; n < 0x8C; n++) {
        if (D_801D3F80[i].unk1D7 == 0) {
            EffectDrawNode *node = &D_801D3F80[i];

            found = node;
            func_801A166C(node, sizeof(EffectDrawNode));
            node->unk1D7 = 1;
            node->unk1D6 = source;
            node->owner = owner;
            node->unk1D9 = owner->unk06B;
            D_801D3F84->live++;
            func_801A8068(node, 1);
            break;
        }
        i++;
        if (i >= EFFECT_NODE_COUNT) {
            i = 0;
        }
    }
    i++;
    if (i >= EFFECT_NODE_COUNT) {
        i = 0;
    }
    D_801D3F8A = i;
    return found;
}

/** @brief Run @p step when it is the kind this handler serves. */
static void func_801A5538(EffectDrawNode *node, EffectPoseStep *step) {
    if (step->unk030 == 1) {
        func_801A6194(node, step->unk031, node->unk1D9);
    }
}

/**
 * @brief Decide whether @p step fires this frame, and run it if it does.
 * @return 1 when the step ran, 0 otherwise.
 */
static s32 func_801A5570(EffectDrawNode *node, EffectPoseStep *step) {
    switch (step->kind) {
    case 0:
        if (node->unk1D2 != 1) {
            return 0;
        }
        break;
    case 1:
        node->unk1C8--;
        if (node->unk1C8 >= 0) {
            return 0;
        }
        func_801A5538(node, step);
        return 1;
    case 2:
        node->unk1C8--;
        if (node->unk1C8 < 0) {
            break;
        }
        if (node->unk1D2 != 1) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    func_801A5538(node, step);
    return 1;
}

/** @brief Step one node of the pose list, or retire it once its phase is over. */
static void func_801A561C(EffectDrawNode *node) {
    EffectPoseStep *step = D_801D3F84->sources[node->unk1D6];

    switch (node->unk1CC) {
    case 0:
        func_801A49E8(node, step);
        if (func_801A5570(node, step) == 0) {
            func_801A41E0(node, step);
            func_801A4ADC(node, step);
            node->unk1CA++;
        } else {
            node->stream = NULL;
            node->unk1CC++;
        }
        break;
    case 1:
        func_801A80A4(node);
        node->unk1D7 = 0;
        D_801D3F84->live--;
        break;
    }
}

/** @brief One less than @p node's count, as a signed step. */
static s16 func_801A5708(EffectMote *node) {
    return node->unk008 - 1;
}

/**
 * @brief The overlay's own copy of @ref func_800C97E4: emit the frame's sprites.
 *
 * @return The prim buffer cursor past the packets emitted.
 */
static POLY_FT4 *func_801A5720(BattleSpritePrim *prim, u32 *ot, s32 otShift, POLY_FT4 *head) {
    POLY_FT4 *p = head;
    POLY_FT4 *next;
    BattleSprite *sprite = prim->sprites;
    s32 count = prim->spriteCount;
    s32 i;
    s32 flags;
    s32 value; /* angle, then each scale: one local, the target keeps all three in s0 */
    s32 w;
    s32 h;
    s32 uv;
    u32 shade;
    DR_MODE *tp;

    for (i = 0; i < count; i++, sprite++) {
        prim->m.m[2][1] = 0;
        prim->m.m[2][0] = 0;
        prim->m.m[1][2] = 0;
        prim->m.m[1][0] = 0;
        prim->m.m[0][2] = 0;
        prim->m.m[0][1] = 0;
        prim->m.m[2][2] = ONE;
        flags = sprite->flags;
        if (flags & BATTLE_SPRITE_TRANSFORMED) {
            value = sprite->angle;
            if (value == 0) {
                prim->cos = ONE;
                prim->sin = 0;
                prim->lastAngle = 0;
            } else if (value != prim->lastAngle) {
                prim->cos = rcos(value);
                prim->sin = rsin(value);
                prim->lastAngle = value;
            }
            value = sprite->scaleX;
            prim->m.m[0][0] = prim->cos * value >> 12;
            prim->m.m[1][0] = prim->sin * value >> 12;
            value = sprite->scaleY;
            prim->m.m[0][1] = -prim->sin * value >> 12;
            prim->m.m[1][1] = prim->cos * value >> 12;
        } else {
            prim->m.m[1][1] = ONE;
            prim->m.m[0][0] = ONE;
        }
        w = sprite->w;
        h = sprite->h;
        prim->corners[2].vx = -(w << 3);
        prim->corners[0].vx = -(w << 3);
        prim->corners[3].vx = w << 3;
        prim->corners[1].vx = w << 3;
        prim->corners[1].vy = -(h << 3);
        prim->corners[0].vy = -(h << 3);
        prim->corners[3].vy = h << 3;
        prim->corners[2].vy = h << 3;
        prim->m.t[0] = (sprite->x << 4) + (w << 3);
        prim->m.t[1] = (sprite->y << 4) + (h << 3);
        prim->m.t[2] = 0;
        /* m = mtx * m, a column at a time, with the packet filled in the GTE's shadow. */
        gte_SetRotMatrix(&prim->mtx);
        gte_ldclmv(&prim->m.m[0][0]);
        gte_rtir();
        setlen(p, 9);
        /* One word each: w, shade, h, code and u, v, clut are laid out as the packet's. */
        *(u32 *)&p->r0 = *(u32 *)&sprite->w;
        *(u32 *)&p->u0 = *(u32 *)&sprite->u;
        gte_stclmv(&prim->m.m[0][0]);
        gte_ldclmv(&prim->m.m[0][1]);
        gte_rtir();
        p->r0 = p->b0 = p->g0;
        shade = p->r0 * prim->colour.r;
        p->r0 = shade >> 7;
        gte_stclmv(&prim->m.m[0][1]);
        gte_ldclmv(&prim->m.m[0][2]);
        gte_rtir();
        p->u2 = sprite->u;
        shade = p->g0 * prim->colour.g;
        p->g0 = shade >> 7;
        gte_stclmv(&prim->m.m[0][2]);
        gte_SetTransMatrix(&prim->mtx);
        gte_ldlv0(prim->m.t);
        gte_rt();
        p->v1 = sprite->v;
        shade = p->b0 * prim->colour.b;
        p->b0 = shade >> 7;
        gte_stlvnl(prim->m.t);
        gte_SetRotMatrix(&prim->m);
        gte_SetTransMatrix(&prim->m);
        gte_ldv3(&prim->corners[0], &prim->corners[1], &prim->corners[2]);
        gte_rtpt();
        uv = sprite->u + w - prim->uvInset;
        if (uv > BATTLE_SPRITE_UV_MAX) {
            uv = BATTLE_SPRITE_UV_MAX;
        }
        p->u3 = uv;
        p->u1 = uv;
        /* The row select is the flag word's top nibble, unsigned. */
        p->clut += getClut(0, prim->clutRow[(u16)flags >> BATTLE_SPRITE_CLUT_ROW_SHIFT]);
        gte_stflg(&prim->gteFlag);
        if (!(prim->gteFlag & BATTLE_SPRITE_GTE_OUT_OF_RANGE)) {
            p->tpage = flags & BATTLE_SPRITE_TPAGE_MASK;
            gte_stsxy3(&p->x0, &p->x1, &p->x2);
            gte_ldv0(&prim->corners[3]);
            gte_rtps();
            uv = sprite->v + h - prim->uvInset;
            if (uv > BATTLE_SPRITE_UV_MAX) {
                uv = BATTLE_SPRITE_UV_MAX;
            }
            p->v3 = uv;
            p->v2 = uv;
            gte_stsxy(&p->x3);
            if (prim->otz >= 0) {
                addPrim(&ot[otShift], p);
                p++;
            } else {
                gte_avsz4();
                gte_stotz(&prim->otz);
                otShift = prim->otz >> otShift;
                if (prim->flags & BATTLE_SPRITE_FLAG_ADDITIVE) {
                    next = p + 1;
                    setlen(next, 1);
                    tp = (DR_MODE *)next;
                    next = (POLY_FT4 *)(tp + 1);
                    tp->code[0] = BATTLE_SPRITE_ADDITIVE_MODE;
                    addPrim(&ot[otShift], tp);
                    addPrim(&ot[otShift], p);
                    p = next;
                } else {
                    addPrim(&ot[otShift], p);
                    p++;
                }
            }
        }
    }
    return p;
}

/**
 * @brief The overlay's own copy of @ref func_800C9E10: set a sprite packet up.
 *
 * @return The prim buffer cursor past the sprites emitted.
 */
static POLY_FT4 *func_801A5D4C(BattleSpritePrim *prim, u32 *ot, s32 otShift, POLY_FT4 *head) {
    BattleSpriteAnim *anim = prim->anim;
    u16 *offsets = anim->offsets;
    u8 *frame;

    prim->frameCount = anim->frameCount;
    frame = (u8 *)anim + offsets[prim->frame];
    prim->sprites = (BattleSprite *)frame;
    prim->spriteCount = *(s32 *)frame;
    prim->sprites = (BattleSprite *)((s32 *)prim->sprites + 1);
    prim->nextOffset = offsets[prim->frame + 1];
    if (prim->spriteCount < 0) {
        prim->spriteCount &= ~BATTLE_SPRITE_COUNT_FLAG;
        prim->flags |= BATTLE_SPRITE_FLAG_ADDITIVE;
    }
    prim->corners[3].vz = 0;
    prim->corners[2].vz = 0;
    prim->corners[1].vz = 0;
    prim->corners[0].vz = 0;
    gte_ReadRotMatrix(&prim->mtx);
    if (prim->flags & BATTLE_SPRITE_FLAG_ROTATE) {
        RotMatrixZ(prim->angle - D_800F02A0, &prim->mtx);
    } else if (!(prim->flags & BATTLE_SPRITE_FLAG_NO_ROLL)) {
        RotMatrixZ(-D_800F02A0, &prim->mtx);
    }
    if (prim->flags & BATTLE_SPRITE_FLAG_SCALE) {
        prim->scale.vz = ONE;
        ScaleMatrixL(&prim->mtx, &prim->scale);
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_COLOUR)) {
        *(u32 *)&prim->colour = BATTLE_SPRITE_COLOUR_DEFAULT;
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT0)) {
        prim->clutRow[0] = anim->clutRow[0];
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT1)) {
        prim->clutRow[1] = anim->clutRow[1];
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT2)) {
        prim->clutRow[2] = anim->clutRow[2];
    }
    if (!(prim->flags & BATTLE_SPRITE_FLAG_KEEP_CLUT3)) {
        prim->clutRow[3] = anim->clutRow[3];
    }
    if (prim->flags & BATTLE_SPRITE_FLAG_UV_INSET) {
        prim->uvInset = 1;
    } else {
        prim->uvInset = 0;
    }
    prim->cos = ONE;
    prim->lastAngle = 0;
    prim->sin = 0;
    prim->otz = -1;
    return func_801A5720(prim, ot, otShift, head);
}

/**
 * @brief Build one draw packet for @p node's model and link it into the OT.
 *
 * @note @p step is not read; every caller has it to hand and passes it.
 */
static void func_801A5FB4(EffectDrawNode *node, EffectPoseStep *step) {
    BattleSpritePrim *prim;

    D_801D3CD8 -= sizeof(BattleSpritePrim);
    prim = (BattleSpritePrim *)D_801D3CD8;
    gte_SetRotMatrix(&node->mtx);
    gte_SetTransMatrix(&node->mtx);
    prim->anim = node->stream;
    prim->frame = node->unk1D0;
    prim->flags = 0;
    D_801D479C = func_801A5D4C(prim, D_800FA5E8->ot, 2, D_801D479C);
    D_801D3CD8 += sizeof(BattleSpritePrim);
}

/** @brief Draw every node on the list that has a model and is still posing. */
static void func_801A6074(void) {
    EffectDrawNode *node = D_801D3F84->head;

    while (node != NULL) {
        if (node->unk008 == 1) {
            EffectPoseStep *step = D_801D3F84->sources[node->unk1D6];
            EffectDrawNode *n = node;

            if (step->mode < 3 && node->stream != NULL) {
                if (node->copies == 1) {
                    func_801A5FB4(node, step);
                } else {
                    s32 i;

                    for (i = 0; i < n->copies; i++) {
                        n->mtx.t[0] = n->offsets[i].vx;
                        n->mtx.t[1] = n->offsets[i].vy;
                        n->mtx.t[2] = n->offsets[i].vz;
                        func_801A5FB4(n, step);
                    }
                }
            }
        }
        node = node->next;
    }
}

/**
 * @brief Take the next free mote off bank 0 and put it on the draw list.
 *
 * The search starts where the last one left off and gives up after one lap.
 *
 * @return The mote, or NULL when every slot is taken.
 */
static EffectMote *func_801A6194(void *owner, s16 source, s16 index) {
    EffectMote *found = NULL;
    s32 i = D_801D3F88;
    s32 n;

    for (n = 0; n < 0x5A; n++) {
        if (D_801D3F7C[i].taken == 0) {
            EffectMote *mote = &D_801D3F7C[i];

            found = mote;
            func_801A166C(mote, sizeof(EffectMote));
            mote->taken = 1;
            mote->unk06A = source;
            mote->unk06B = index;
            mote->owner = owner;
            D_801D3F84->count++;
            func_801A8068((EffectDrawNode *)mote, 0);
            break;
        }
        i++;
        if (i >= EFFECT_MOTE_COUNT) {
            i = 0;
        }
    }
    i++;
    if (i >= EFFECT_MOTE_COUNT) {
        i = 0;
    }
    D_801D3F88 = i;
    return found;
}


/** @brief Step @p node's life and retire it when it runs out. */
static void func_801A62B4(EffectMote *node) {
    node->life--;
    if (node->life <= 0) {
        func_801A80A4((EffectDrawNode *)node);
        node->taken = 0;
        D_801D3F84->count--;
    }
}

/** @brief Spawn a node off @p mote and give each of its @p copies its own aim. */
static void func_801A631C(EffectMote *mote, EffectPoseStep *step, s32 copies) {
    EffectSpawnScratch *s;
    EffectDrawNode *node;
    s32 i;

    D_801D3CD8 -= sizeof(EffectSpawnScratch);
    s = (EffectSpawnScratch *)D_801D3CD8;
    node = func_801A541C(mote, mote->unk06A);
    node->copies = copies;
    s->frame = mote->unk060;
    if (step->mode < 3) {
        node->stream = D_801D3F84->unk228[mote->unk06A];
        node->unk1D1 = func_801A5708(node->stream);
        if (step->mode == 2) {
            node->unk1D3 = step->unk017;
            node->unk1D4 = step->unk018;
            node->unk1D5 = func_801A1980(step->unk019, step->unk01A);
        }
    }
    node->unk1DA = step->unk064;
    for (i = 0; i < copies; i++) {
        node->pos[i] = mote->pos;
        if (step->unk022 == 0) {
            node->offset[i] = mote->pos;
        }
        if (step->unk034 == 1) {
            s->angle.vx = rand() & 0xFFF;
            s->angle.vy = rand() & 0xFFF;
            s->angle.vz = rand() & 0xFFF;
            func_801A0624(&s->m);
            if (s->angle.vz != 0) {
                func_801A0978(&s->m, s->angle.vz);
            }
            if (s->angle.vx != 0) {
                func_801A065C(&s->m, s->angle.vx);
            }
            if (s->angle.vy != 0) {
                func_801A07EC(&s->m, s->angle.vy);
            }
            if (step->unk033 == 0) {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
                if (s->speedX != 0) {
                    s->speedX = rand() % s->speedX;
                }
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                if (s->speedZ != 0) {
                    s->speedZ = rand() % s->speedZ;
                }
            } else {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
            }
            s->dir.vx = 0;
            s->dir.vy = -0x1000;
            s->dir.vz = 0;
            gte_SetRotMatrix(&s->m);
            gte_ldv0(&s->dir);
            gte_mvmva(1, 0, 0, 3, 0);
            gte_stsv(&s->dir);
            s->vel.vx = s->dir.vx * s->speedX << 4;
            s->vel.vy = s->dir.vy * s->speedY << 4;
            s->vel.vz = s->dir.vz * s->speedZ << 4;
            node->offset[i].vx += s->vel.vx;
            node->offset[i].vy += s->vel.vy;
            node->offset[i].vz += s->vel.vz;
        } else if (step->unk034 == 2) {
            s->angle.vx = rand() & 0xFFF;
            s->angle.vy = 0;
            s->angle.vz = 0x400;
            func_801A0624(&s->m);
            if (s->angle.vz != 0) {
                func_801A0978(&s->m, s->angle.vz);
            }
            if (s->angle.vx != 0) {
                func_801A065C(&s->m, s->angle.vx);
            }
            if (s->angle.vy != 0) {
                func_801A07EC(&s->m, s->angle.vy);
            }
            if (step->unk033 == 0) {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
                if (s->speedX != 0) {
                    s->speedX = rand() % s->speedX;
                }
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                if (s->speedZ != 0) {
                    s->speedZ = rand() % s->speedZ;
                }
            } else if (step->unk033 != 1) {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                s->speedZ = step->unk0D4[s->frame];
                if (s->speedX != 0) {
                    s->speedX = rand() % s->speedX;
                }
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                if (s->speedZ != 0) {
                    s->speedZ = rand() % s->speedZ;
                }
            } else {
                s->speedX = step->unk0CC[s->frame];
                s->speedY = step->unk0D0[s->frame];
                if (s->speedY != 0) {
                    s->speedY = rand() % s->speedY;
                }
                s->speedZ = step->unk0D4[s->frame];
            }
            if (rand() & 1) {
                s32 speed = s->speedY;

                s->speedY = -speed;
            }
            s->dir.vx = 0;
            s->dir.vy = -0x1000;
            s->dir.vz = 0;
            gte_SetRotMatrix(&s->m);
            gte_ldv0(&s->dir);
            gte_mvmva(1, 0, 0, 3, 0);
            gte_stsv(&s->dir);
            s->vel.vx = s->dir.vx * s->speedX << 4;
            s->vel.vy = s->speedY << 16;
            s->vel.vz = s->dir.vz * s->speedZ << 4;
            node->offset[i].vx += s->vel.vx;
            node->offset[i].vy += s->vel.vy;
            node->offset[i].vz += s->vel.vz;
        }
        switch (step->unk021) {
        case 0:
            func_801A1D88(&s->spin, &step->unk03C, &step->unk044);
            break;
        case 1:
            s->spin = s->angle;
            break;
        }
        func_801A0624(&node->poses[i]);
        if (s->spin.vz != 0) {
            func_801A0978(&node->poses[i], s->spin.vz);
        }
        if (s->spin.vx != 0) {
            func_801A065C(&node->poses[i], s->spin.vx);
        }
        if (s->spin.vy != 0) {
            func_801A07EC(&node->poses[i], s->spin.vy);
        }
        gte_MulMatrix0(&mote->mtx, &node->poses[i], &node->poses[i]);
        if (step->unk027 == 0) {
            node->spin[i] = func_801A1A04(step->unk04C, step->unk050);
            node->spinRate[i] = func_801A1A04(step->unk054, step->unk058);
        }
    }
    func_801A1D88(&node->angleBase, &step->unk148, &step->unk150);
    if (step->kind == 1 || step->kind == 2) {
        node->unk1C8 = func_801A1980(step->unk01F, step->unk020);
    }
    if (step->unk027 == 0) {
        node->unk1DC = func_801A1A04(step->unk05C, step->unk060);
        for (i = 0; i < copies; i++) {
            func_801A1AA8(&node->vel[i], &step->unk068, &step->unk078);
            func_801A1AA8(&node->accel[i], &step->unk088, &step->unk098);
            func_801A1AA8(&node->jerk[i], &step->unk0A8, &step->unk0B8);
        }
    }
    func_801A83B8(node, step);
    D_801D3CD8 += sizeof(EffectSpawnScratch);
}

/** @brief Emit this frame's share of @p node's work, in chunks its source sizes. */
static void func_801A6DB8(EffectMote *node) {
    EffectPoseStep *step = D_801D3F84->sources[node->unk06A];
    s32 age = node->unk060;
    s32 want;
    s32 full;
    s32 rest;
    s32 i;

    if (age < 0x28) {
        want = step->unk0C8[age];
        if (want != 0) {
            full = want / step->unk036;
            rest = want % step->unk036;
            for (i = 0; i < full; i++) {
                func_801A631C(node, step, step->unk036);
            }
            if (rest > 0) {
                func_801A631C(node, step, rest);
            }
        }
    }
}

/** @brief Step one drawn node: set it up once, run its kind, then age it. */
static void func_801A6EB8(EffectMote *node) {
    if (node->unk062 == 0) {
        /* Called with no argument: the mote is already in $a0 and stays there. */
        ((void (*)())func_801A8184)();
        node->unk062++;
    }
    switch (node->kind) {
    case 3:
        func_801A73E8(node);
        break;
    case 4:
        func_801A75F8(node);
        break;
    }
    func_801A6DB8(node);
    node->unk060++;
    func_801A62B4(node);
}

/** @brief Put @p mote on the strand point its source names, plus that source's offset. */
static void func_801A6F58(EffectMote *mote) {
    EffectPoseStep *step = D_801D3F84->sources[mote->unk06A];
    SVECTOR offset;
    SVECTOR turned;
    MATRIX m;

    switch (step->anchor) {
    case 0:
        mote->pos = D_801D3F84->strands[1][mote->unk06B];
        break;
    case 1:
        mote->pos = D_801D3F84->strands[2][mote->unk06B];
        break;
    case 2:
        mote->pos = D_801D3F84->strands[3][mote->unk06B];
        break;
    case 3:
        mote->pos = D_801D3F84->strands[4][mote->unk06B];
        break;
    }
    offset.vx = step->offsetX / 0x10000;
    offset.vy = step->offsetY / 0x10000;
    offset.vz = step->offsetZ / 0x10000;
    m = mote->mtx;
    gte_SetRotMatrix(&m);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    mote->pos.vx += turned.vx << 16;
    mote->pos.vy += turned.vy << 16;
    mote->pos.vz += turned.vz << 16;
}

/** @brief Put @p mote on the trail point its source names, plus that source's offset. */
static void func_801A71E0(EffectMote *mote) {
    EffectPoseStep *step = D_801D3F84->sources[mote->unk06A];
    SVECTOR offset;
    SVECTOR turned;

    switch (step->anchor) {
    case 0:
        mote->pos = D_801D3F84->trail[2];
        break;
    case 1:
        mote->pos = D_801D3F84->trail[3];
        break;
    case 2:
        mote->pos = D_801D3F84->trail[4];
        break;
    case 3:
        mote->pos = D_801D3F84->trail[5];
        break;
    }
    offset.vx = step->offsetX / 0x10000;
    offset.vy = step->offsetY / 0x10000;
    offset.vz = step->offsetZ / 0x10000;
    gte_SetRotMatrix(&mote->mtx);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    mote->pos.vx += turned.vx << 16;
    mote->pos.vy += turned.vy << 16;
    mote->pos.vz += turned.vz << 16;
}

/** @brief Put @p mote on the node that owns it, plus that source's turned offset. */
static void func_801A73E8(EffectMote *mote) {
    EffectPoseStep *step = D_801D3F84->sources[mote->unk06A];
    EffectDrawNode *owner = mote->owner;

    if (owner != NULL) {
        SVECTOR offset;
        SVECTOR turned;

        if (step->midpoint == 1) {
            mote->pos.vx = (owner->pos[0].vx + owner->lastPos.vx) / 2;
            mote->pos.vy = (owner->pos[0].vy + owner->lastPos.vy) / 2;
            mote->pos.vz = (owner->pos[0].vz + owner->lastPos.vz) / 2;
        } else {
            mote->pos = owner->pos[0];
        }
        if (step->unk01C == 3) {
            mote->mtx = owner->orient;
            offset.vx = step->offsetX / 0x10000;
            offset.vy = step->offsetY / 0x10000;
            offset.vz = step->offsetZ / 0x10000;
            gte_SetRotMatrix(&mote->mtx);
            gte_ldv0(&offset);
            gte_mvmva(1, 0, 0, 3, 0);
            gte_stsv(&turned);
            mote->pos.vx += turned.vx << 16;
            mote->pos.vy += turned.vy << 16;
            mote->pos.vz += turned.vz << 16;
        }
    }
}

/** @brief Slide @p mote between the two points its source names. */
static void func_801A75F8(EffectMote *mote) {
    EffectPoseStep *step = D_801D3F84->sources[mote->unk06A];
    u8 kind;
    s32 index;
    SVECTOR from;
    SVECTOR to;
    SVECTOR offset;
    SVECTOR turned;
    s32 t;

    kind = step->unk100[mote->unk060];
    index = step->unk108[mote->unk060];
    switch (kind) {
    case 0:
        switch (index) {
        case 0:
            from.vx = D_801D3F84->strands[1][mote->unk06B].vx / 0x10000;
            from.vy = D_801D3F84->strands[1][mote->unk06B].vy / 0x10000;
            from.vz = D_801D3F84->strands[1][mote->unk06B].vz / 0x10000;
            break;
        case 1:
            from.vx = D_801D3F84->strands[2][mote->unk06B].vx / 0x10000;
            from.vy = D_801D3F84->strands[2][mote->unk06B].vy / 0x10000;
            from.vz = D_801D3F84->strands[2][mote->unk06B].vz / 0x10000;
            break;
        case 2:
            from.vx = D_801D3F84->strands[3][mote->unk06B].vx / 0x10000;
            from.vy = D_801D3F84->strands[3][mote->unk06B].vy / 0x10000;
            from.vz = D_801D3F84->strands[3][mote->unk06B].vz / 0x10000;
            break;
        case 3:
            from.vx = D_801D3F84->strands[4][mote->unk06B].vx / 0x10000;
            from.vy = D_801D3F84->strands[4][mote->unk06B].vy / 0x10000;
            from.vz = D_801D3F84->strands[4][mote->unk06B].vz / 0x10000;
            break;
        }
        break;
    case 1:
        switch (index) {
        case 0:
            from.vx = D_801D3F84->trail[2].vx / 0x10000;
            from.vy = D_801D3F84->trail[2].vy / 0x10000;
            from.vz = D_801D3F84->trail[2].vz / 0x10000;
            break;
        case 1:
            from.vx = D_801D3F84->trail[3].vx / 0x10000;
            from.vy = D_801D3F84->trail[3].vy / 0x10000;
            from.vz = D_801D3F84->trail[3].vz / 0x10000;
            break;
        case 2:
            from.vx = D_801D3F84->trail[4].vx / 0x10000;
            from.vy = D_801D3F84->trail[4].vy / 0x10000;
            from.vz = D_801D3F84->trail[4].vz / 0x10000;
            break;
        case 3:
            from.vx = D_801D3F84->trail[5].vx / 0x10000;
            from.vy = D_801D3F84->trail[5].vy / 0x10000;
            from.vz = D_801D3F84->trail[5].vz / 0x10000;
            break;
        }
        break;
    }
    offset.vx = step->unk110[mote->unk060];
    offset.vy = step->unk114[mote->unk060];
    offset.vz = step->unk118[mote->unk060];
    gte_SetRotMatrix(&mote->mtx);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    from.vx += turned.vx;
    from.vy += turned.vy;
    from.vz += turned.vz;
    kind = step->unk104[mote->unk060];
    index = step->unk10C[mote->unk060];
    switch (kind) {
    case 0:
        switch (index) {
        case 0:
            to.vx = D_801D3F84->strands[1][mote->unk06B].vx / 0x10000;
            to.vy = D_801D3F84->strands[1][mote->unk06B].vy / 0x10000;
            to.vz = D_801D3F84->strands[1][mote->unk06B].vz / 0x10000;
            break;
        case 1:
            to.vx = D_801D3F84->strands[2][mote->unk06B].vx / 0x10000;
            to.vy = D_801D3F84->strands[2][mote->unk06B].vy / 0x10000;
            to.vz = D_801D3F84->strands[2][mote->unk06B].vz / 0x10000;
            break;
        case 2:
            to.vx = D_801D3F84->strands[3][mote->unk06B].vx / 0x10000;
            to.vy = D_801D3F84->strands[3][mote->unk06B].vy / 0x10000;
            to.vz = D_801D3F84->strands[3][mote->unk06B].vz / 0x10000;
            break;
        case 3:
            to.vx = D_801D3F84->strands[4][mote->unk06B].vx / 0x10000;
            to.vy = D_801D3F84->strands[4][mote->unk06B].vy / 0x10000;
            to.vz = D_801D3F84->strands[4][mote->unk06B].vz / 0x10000;
            break;
        }
        break;
    case 1:
        switch (index) {
        case 0:
            to.vx = D_801D3F84->trail[2].vx / 0x10000;
            to.vy = D_801D3F84->trail[2].vy / 0x10000;
            to.vz = D_801D3F84->trail[2].vz / 0x10000;
            break;
        case 1:
            to.vx = D_801D3F84->trail[3].vx / 0x10000;
            to.vy = D_801D3F84->trail[3].vy / 0x10000;
            to.vz = D_801D3F84->trail[3].vz / 0x10000;
            break;
        case 2:
            to.vx = D_801D3F84->trail[4].vx / 0x10000;
            to.vy = D_801D3F84->trail[4].vy / 0x10000;
            to.vz = D_801D3F84->trail[4].vz / 0x10000;
            break;
        case 3:
            to.vx = D_801D3F84->trail[5].vx / 0x10000;
            to.vy = D_801D3F84->trail[5].vy / 0x10000;
            to.vz = D_801D3F84->trail[5].vz / 0x10000;
            break;
        }
        break;
    }
    offset.vx = step->unk11C[mote->unk060];
    offset.vy = step->unk120[mote->unk060];
    offset.vz = step->unk124[mote->unk060];
    gte_SetRotMatrix(&mote->mtx);
    gte_ldv0(&offset);
    gte_mvmva(1, 0, 0, 3, 0);
    gte_stsv(&turned);
    to.vx += turned.vx;
    to.vy += turned.vy;
    to.vz += turned.vz;
    t = step->unk128[mote->unk060];
    mote->pos.vx = (from.vx << 16) + ((to.vx - from.vx) * t << 4);
    mote->pos.vy = (from.vy << 16) + ((to.vy - from.vy) * t << 4);
    mote->pos.vz = (from.vz << 16) + ((to.vz - from.vz) * t << 4);
}

/**
 * @brief Give @p node the count @p value and put it on the tail of the draw list.
 *
 * @param node Either record kind: both start with the same three link fields,
 *             and @c unk008 is the tag the walker later dispatches on.
 */
static void func_801A8068(EffectDrawNode *node, s16 value) {
    EffectDrawList *list = D_801D3F84;

    node->unk008 = value;
    if (list->head == NULL) {
        list->head = node;
    } else {
        EffectDrawNode *tail = list->tail;

        tail->next = node;
        node->prev = tail;
    }
    list->tail = node;
}

/**
 * @brief Take @p node off the draw list, mending both links.
 *
 * @param node Either record kind; see @ref func_801A8068.
 */
static void func_801A80A4(EffectDrawNode *node) {
    EffectDrawNode *prev = node->prev;
    EffectDrawNode *next = node->next;

    if (prev == NULL) {
        D_801D3F84->head = next;
    } else {
        prev->next = next;
    }
    if (next == NULL) {
        D_801D3F84->tail = prev;
    } else {
        next->prev = prev;
    }
}

/** @brief Empty the draw list. */
static void func_801A80E4(void) {
    D_801D3F84->head = NULL;
    D_801D3F84->tail = NULL;
}

/** @brief Run every node on the draw list through the step its kind wants. */
static void func_801A80FC(void) {
    EffectDrawNode *node = D_801D3F84->head;

    while (node != NULL) {
        /* Read signed here and unsigned where it is stepped; it is also the
           tag saying which record kind the walker is standing on. */
        switch (node->unk008) {
        case 0:
            func_801A6EB8((EffectMote *)node);
            break;
        case 1:
            func_801A561C(node);
            break;
        }
        node = node->next;
    }
}

/** @brief Set @p mote up for this frame: turn it, place it, and reset its life. */
static void func_801A8184(EffectMote *mote) {
    EffectPoseStep *step = D_801D3F84->sources[mote->unk06A];
    EffectDrawNode *owner = mote->owner;
    s32 angle;

    mote->kind = step->unk011;
    switch (step->unk01C) {
    case 0:
        func_801A0624(&mote->mtx);
        break;
    case 1:
        func_801A0624(&mote->mtx);
        angle = func_80041E84(
            D_801D3F84->trail[1].vx - D_801D3F84->strands[0][mote->unk06B].vx,
            D_801D3F84->trail[1].vz - D_801D3F84->strands[0][mote->unk06B].vz);
        if (angle != 0) {
            func_801A07EC(&mote->mtx, angle);
        }
        break;
    case 2:
        func_801A0624(&mote->mtx);
        angle = func_80041E84(
            D_801D3F84->strands[0][mote->unk06B].vx - D_801D3F84->trail[1].vx,
            D_801D3F84->strands[0][mote->unk06B].vz - D_801D3F84->trail[1].vz);
        if (angle != 0) {
            func_801A07EC(&mote->mtx, angle);
        }
        break;
    case 3:
        mote->mtx = owner->orient;
        break;
    case 4:
        func_801A0624(&mote->mtx);
        angle = D_800EF2D0[D_801D3F84->slot].facing;
        if (angle != 0) {
            func_801A07EC(&mote->mtx, angle);
        }
        break;
    }
    switch (mote->kind) {
    case 0:
        func_801A6F58(mote);
        break;
    case 1:
        func_801A71E0(mote);
        break;
    case 2:
        func_801A73E8(mote);
        break;
    case 3:
        func_801A73E8(mote);
        break;
    case 4:
        func_801A75F8(mote);
        break;
    }
    mote->life = step->unk012;
}

/** @brief Fire each of @p step's four slots that is armed. */
static void func_801A83B8(EffectDrawNode *node, EffectPoseStep *step) {
    if (step->unk028 == 1) {
        func_801A6194(node, step->unk02C, node->unk1D9);
    }
    if (step->unk029 == 1) {
        func_801A6194(node, step->unk02D, node->unk1D9);
    }
    if (step->unk02A == 1) {
        func_801A6194(node, step->unk02E, node->unk1D9);
    }
    if (step->unk02B == 1) {
        func_801A6194(node, step->unk02F, node->unk1D9);
    }
}

/** @brief Take a mote for every source whose cue falls on this frame. */
static void func_801A846C(void) {
    s32 source;

    for (source = 0; source < 0x10; source++) {
        EffectPoseStep *step = D_801D3F84->sources[source];

        if (step != NULL && !((D_801D3F84->held >> source) & 1)) {
            if (step->unk011 < 2 || step->unk011 == 4) {
                if (step->unk013 == D_801D3F84->age) {
                    if (step->unk037 == 1) {
                        s32 i;

                        for (i = 0; i < D_801D3F84->strandLen; i++) {
                            func_801A6194(NULL, source, i);
                        }
                    } else {
                        func_801A6194(NULL, source, 0);
                    }
                }
            }
        }
    }
}

/** @brief Push the node's current position onto the head of the trail. */
static void func_801A85A8(EffectDrawScript *script) {
    EffectDrawList *list = D_801D3F84;

    list->trail[0].vx = script->unk290 << 16;
    list->trail[0].vy = script->unk292 << 16;
    list->trail[0].vz = script->unk294 << 16;
    list->trail[5] = list->trail[0];
    list->trail[4] = list->trail[5];
    list->trail[3] = list->trail[4];
    list->trail[2] = list->trail[3];
    list->trail[1] = list->trail[2];
}

/** @brief Aim the trail at the battle slot: its two points and its height. */
static void func_801A8684(void) {
    BattleEffectSlot *slot = &D_800EF2D0[D_801D3F84->slot];
    SVECTOR pt;

    if (D_801D3F84->unk024 != 2) {
        func_800B3960(slot, 0xF1, 0, &pt);
    }
    pt.vy = slot->unk024;
    D_801D3F84->trail[1].vx = pt.vx << 16;
    D_801D3F84->trail[1].vy = pt.vy << 16;
    D_801D3F84->trail[1].vz = pt.vz << 16;
    D_801D3F84->trail[2].vx = D_801D3F84->trail[1].vx;
    D_801D3F84->trail[2].vy = 0;
    D_801D3F84->trail[2].vz = D_801D3F84->trail[1].vz;
    if (D_801D3F84->unk024 != 2) {
        func_800B3960(slot, 0xF1, 0, &pt);
    }
    D_801D3F84->trail[3].vx = pt.vx << 16;
    D_801D3F84->trail[3].vy = pt.vy << 16;
    D_801D3F84->trail[3].vz = pt.vz << 16;
    if (D_801D3F84->unk024 != 2) {
        func_800B3960(slot, 0xF0, 0, &pt);
    }
    D_801D3F84->trail[4].vx = pt.vx << 16;
    D_801D3F84->trail[4].vy = pt.vy << 16;
    D_801D3F84->trail[4].vz = pt.vz << 16;
    D_801D3F84->trail[5].vx = D_801D3F84->trail[1].vx;
    D_801D3F84->trail[5].vy = slot->unk03C << 16;
    D_801D3F84->trail[5].vz = D_801D3F84->trail[1].vz;
}

/** @brief Seed every point of all five strands from the list's seed point. */
static void func_801A87F8(void) {
    s32 i;

    for (i = 0; i < D_801D3F84->strandLen; i++) {
        D_801D3F84->strands[0][i] = D_801D3F84->strandSeed;
        D_801D3F84->strands[1][i] = D_801D3F84->strandSeed;
        D_801D3F84->strands[2][i] = D_801D3F84->strandSeed;
        D_801D3F84->strands[3][i] = D_801D3F84->strandSeed;
        D_801D3F84->strands[4][i] = D_801D3F84->strandSeed;
    }
}

/** @brief Push @p node's position onto the head of every strand. */
static void func_801A8904(EffectDrawScript *script) {
    s32 i;

    for (i = 0; i < D_801D3F84->strandLen; i++) {
        EffectDrawList *list = D_801D3F84;

        list->strandHead.vx = script->unk288 << 16;
        list->strandHead.vy = script->unk28A << 16;
        list->strandHead.vz = script->unk28C << 16;
        list->strands[4][i] = list->strandHead;
        list->strands[3][i] = list->strands[4][i];
        list->strands[2][i] = list->strands[3][i];
        list->strands[1][i] = list->strands[2][i];
        list->strands[0][i] = list->strands[1][i];
    }
}

/**
 * @brief Aim every strand at its own model part, then seed them from the middle.
 *
 * @note @p script is not read; @ref func_801A9050 passes the entity it has.
 */
static void func_801A8A1C(EffectDrawScript *script) {
    SVECTOR pt;
    s32 i;
    s16 n;
    s32 minX;
    s32 maxX;
    s32 minZ;
    s32 maxZ;

    for (i = 0; i < D_801D3F84->strandLen; i++) {
        BattleEffectSlot *slot = &D_800EF2D0[D_801D3F84->parts[i]];

        if (D_801D3F84->unk024 != 2) {
            func_800B3960(slot, 0xF1, 0, &pt);
        }
        pt.vy = slot->unk024;
        D_801D3F84->strands[0][i].vx = pt.vx << 16;
        D_801D3F84->strands[0][i].vy = pt.vy << 16;
        D_801D3F84->strands[0][i].vz = pt.vz << 16;
        D_801D3F84->strands[1][i].vx = D_801D3F84->strands[0][i].vx;
        D_801D3F84->strands[1][i].vy = 0;
        D_801D3F84->strands[1][i].vz = D_801D3F84->strands[0][i].vz;
        if (D_801D3F84->unk024 != 2) {
            func_800B3960(slot, 0xF1, 0, &pt);
        }
        D_801D3F84->strands[2][i].vx = pt.vx << 16;
        D_801D3F84->strands[2][i].vy = pt.vy << 16;
        D_801D3F84->strands[2][i].vz = pt.vz << 16;
        if (D_801D3F84->unk024 != 2) {
            func_800B3960(slot, 0xF0, 0, &pt);
        }
        D_801D3F84->strands[3][i].vx = pt.vx << 16;
        D_801D3F84->strands[3][i].vy = pt.vy << 16;
        D_801D3F84->strands[3][i].vz = pt.vz << 16;
        D_801D3F84->strands[4][i].vx = D_801D3F84->strands[0][i].vx;
        D_801D3F84->strands[4][i].vy = slot->unk03C << 16;
        D_801D3F84->strands[4][i].vz = D_801D3F84->strands[0][i].vz;
    }
    maxZ = 0;
    minZ = 0;
    maxX = 0;
    minX = 0;
    n = 0;
    i = 0;

    for (; i < D_801D3F84->strandLen; i++) {
        s32 v;

        if (n == 0) {
            maxX = D_801D3F84->strands[0][i].vx;
            maxZ = D_801D3F84->strands[0][i].vz;
            minX = maxX;
            minZ = maxZ;
        } else {
            v = D_801D3F84->strands[0][i].vx;
            if (v < minX) {
                minX = v;
            } else if (maxX < v) {
                maxX = v;
            }
            v = D_801D3F84->strands[0][i].vz;
            if (v < minZ) {
                minZ = v;
            } else if (maxZ < v) {
                maxZ = v;
            }
        }
        n++;
    }
    D_801D3F84->strandSeed.vx = (minX + maxX) / 2;
    D_801D3F84->strandSeed.vy = 0;
    D_801D3F84->strandSeed.vz = (minZ + maxZ) / 2;
}

/**
 * @brief Turn every offset in @p tables into an address.
 *
 * The tables are built with everything relative to their own start, so the
 * first script to use them walks each of the four arrays -- and then every
 * table inside each source -- adding that start. Each array is walked as
 * words because the entries are offsets until this pass has run.
 */
static void func_801A8CE4(EffectPoseTables *tables) {
    s32 base = (s32)tables;
    s32 *entry;
    s32 i;

    entry = (s32 *)D_801D3F84->sources;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    entry = (s32 *)D_801D3F84->unk228;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    entry = (s32 *)D_801D3F84->targets;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    entry = (s32 *)D_801D3F84->frames;
    for (i = 0; i < 16; i++) {
        if (entry[i] != 0) {
            entry[i] += base;
        }
    }
    for (i = 0; i < 16; i++) {
        EffectPoseStep *step = D_801D3F84->sources[i];

        if (step != NULL) {
        step->unk0C8 += base;
        step->unk0CC = (u16 *)((u8 *)step->unk0CC + base);
        step->unk0D0 = (u16 *)((u8 *)step->unk0D0 + base);
        step->unk0D4 = (u16 *)((u8 *)step->unk0D4 + base);
        step->unk0D8 = (u16 *)((u8 *)step->unk0D8 + base);
        step->unk0DC = (u16 *)((u8 *)step->unk0DC + base);
        step->unk0E0 = (u16 *)((u8 *)step->unk0E0 + base);
        step->unk0E4 = (u16 *)((u8 *)step->unk0E4 + base);
        step->unk0E8 = (u16 *)((u8 *)step->unk0E8 + base);
        step->unk0EC = (u16 *)((u8 *)step->unk0EC + base);
        step->unk0F0 += base;
        step->unk0F4 += base;
        step->unk0F8 += base;
        step->unk0FC = (u16 *)((u8 *)step->unk0FC + base);
        step->unk100 += base;
        step->unk104 += base;
        step->unk108 += base;
        step->unk10C += base;
        step->unk110 = (u16 *)((u8 *)step->unk110 + base);
        step->unk114 = (u16 *)((u8 *)step->unk114 + base);
        step->unk118 = (u16 *)((u8 *)step->unk118 + base);
        step->unk11C = (u16 *)((u8 *)step->unk11C + base);
        step->unk120 = (u16 *)((u8 *)step->unk120 + base);
        step->unk124 = (u16 *)((u8 *)step->unk124 + base);
        step->unk128 = (s16 *)((u8 *)step->unk128 + base);
        step->targets += base;
        step->framesA += base;
        step->framesB += base;
        step->weights = (s16 *)((u8 *)step->weights + base);
        step->unk13C = (s16 *)((u8 *)step->unk13C + base);
        step->unk140 = (s16 *)((u8 *)step->unk140 + base);
        step->unk144 = (s16 *)((u8 *)step->unk144 + base);
        }
    }
}

/** @brief Start @p task as a child of @p owner and seed its script fields. */
static void func_801A8F88(EffectEntity *owner, void *task, void *arg, s16 stopFrame,
                          s16 a, s16 b) {
    EffectDrawScript *child =
        func_801A0B5C(&D_801D478C, task, sizeof(EffectDrawScript), owner);

    child->tables = arg;
    child->stopFrame = stopFrame;
    child->unk29C = b;
    child->unk29E = a;
}

/** @brief Clear both prim banks and the script's frame counters. */
static void func_801A8FF8(void) {
    func_801A166C(D_801D3F7C, EFFECT_BANK0_BYTES);
    func_801A166C(D_801D3F80, EFFECT_BANK1_BYTES);
    D_801D3F88 = 0;
    D_801D3F8A = 0;
    D_801D3F90 = 0;
    D_801D3F92 = 0;
}

/** @brief Point the draw list at this script's tables and aim it at its slot. */
static void func_801A9050(EffectDrawScript *script) {
    EffectModel *model = script->model;
    EffectPoseTables *tables = script->tables;
    s32 slot = script->slot;
    EffectDrawList *list = &script->list;
    SVECTOR from;
    SVECTOR to;
    s32 i;

    list->held = script->unk29A;
    list->unk024 = script->unk29C;
    list->sources = tables->sources;
    list->unk228 = tables->unk044;
    list->targets = tables->targets;
    list->frames = tables->frames;
    D_801D3F84 = list;
    if (tables->ready == 0) {
        func_801A8CE4(tables);
        tables->ready = 1;
    }
    D_801D3F84->slot = slot;
    D_801D3F84->strandLen = model->unk05A;
    for (i = 0; i < D_801D3F84->strandLen; i++) {
        D_801D3F84->parts[i] =
            script->animSet->anims[script->anim].parts[i].unk000;
    }
    D_801D3F84->span = script->unk29E;
    func_801A8684();
    if (D_801D3F84->unk024 == 4) {
        func_801A85A8(script);
    }
    func_801A8A1C(script);
    if (D_801D3F84->unk024 == 1) {
        /* Handed the script the others take, though this one reads nothing. */
        ((void (*)())func_801A87F8)(script);
    }
    if (D_801D3F84->unk024 == 3) {
        func_801A8904(script);
    }
    from = D_800EF2D0[D_801D3F84->slot].pos;
    to = D_800EF2D0[D_801D3F84->parts[0]].pos;
    from.vx -= to.vx;
    from.vy -= to.vy;
    from.vz -= to.vz;
    D_801D3F84->spread =
        SquareRoot0(from.vx * from.vx + from.vy * from.vy + from.vz * from.vz);
}

/**
 * @brief Run one frame of the draw list @p script carries.
 *
 * @return 1 once the list has finished, 0 while it is still running.
 */
static s32 func_801A92F8(EffectDrawScript *script) {
    s32 done = 0;

    D_801D3F84 = &script->list;
    switch (D_801D3F84->phase) {
    case 0:
        D_801D3F84->phase++;
        break;
    case 1:
        func_801A846C();
        func_801A80FC();
        func_801A6074();
        func_801A4128();
        D_801D3F84->age++;
        if (D_801D3F84->age >= D_801D3F84->span) {
            /* Both counts are tested as one word; neither alone would do. */
            if (*(s32 *)&D_801D3F84->count == 0) {
                D_801D3F84->phase++;
            }
        }
        break;
    case 2:
        done = 1;
        break;
    }
    D_801D3F8C += D_801D3F84->count;
    D_801D3F8E += D_801D3F84->live;
    return done;
}


/** @brief Set the draw list's span from three times the slot's height, clamped. */
static void func_801A9420(s16 slot) {
    BattleEffectSlot *entry = &D_800EF2D0[slot];
    /* Both heights are subtracted as raw halfwords before the span is taken. */
    s16 span = (s16)((u16)entry->unk03C - entry->unk036) * 3;

    if (span > 0x4000) {
        span = 0x4000;
    } else if (span < 0x400) {
        span = 0x400;
    }
    D_801D3F84->unk21C = span;
    D_801D3F84->unk218 = span;
    D_801D3F84->unk214 = span;
}

/** @brief Opcode handler: clear the screen tint. */
static void func_801A94B4(EffectEntity *entity) {
    BattleTint *tint = D_800EF738;
    s32 i;

    entity->pos.vx = 0;
    for (i = 0; i < 4; i++) {
        tint->level = entity->pos.vx;
        tint->b = 0;
        tint->g = 0;
        tint->r = 0;
        tint++;
    }
    entity->pc++;
}

/** @brief Opcode handler: fade the screen tint up to full, then stop. */
static void func_801A9500(EffectEntity *entity) {
    BattleTint *tint = D_800EF738;
    s32 i;

    entity->pos.vx += 0x100;
    if (entity->pos.vx >= 0x400) {
        entity->pos.vx = 0x400;
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
    for (i = 3; i >= 0; i--) {
        tint->level = entity->pos.vx;
        tint++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A9568(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A9570(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A94B4, func_801A9500, func_801A9568 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A0B34(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: hold the tint at a quarter. */
static void func_801A9610(EffectEntity *entity) {
    entity->pos.vx = 0x400;
    entity->pc++;
}

/** @brief Opcode handler: fade the screen tint back out, then stop. */
static void func_801A9628(EffectEntity *entity) {
    BattleTint *tint = D_800EF738;
    s32 i;

    entity->pos.vx -= 0x100;
    if (entity->pos.vx <= 0) {
        entity->pos.vx = 0;
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
    for (i = 3; i >= 0; i--) {
        tint->level = entity->pos.vx;
        tint++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A9684(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A968C(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A9610, func_801A9628, func_801A9684 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A0B34(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: draw and retire once the frame count is reached. */
static void func_801A972C(EffectEntity *entity) {
    EffectDrawScript *script = (EffectDrawScript *)entity;

    if (entity->unk024 >= script->stopFrame) {
        /* Called with no argument: the entity is already in $a0 and stays there. */
        ((void (*)())func_801A9050)();
        func_801A92F8(script);
        entity->pc++;
    }
}

/** @brief Opcode handler: hold still until the count runs out. */
static void func_801A9788(EffectEntity *entity) {
    if (func_801A92F8((EffectDrawScript *)entity) != 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A97D0(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A97D8(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A972C, func_801A9788, func_801A97D0 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A0B34(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: start the pose script on the taken model. */
static void func_801A9878(EffectEntity *entity) {
    func_801A8F88(entity, func_801A97D8, &D_801D4AFC, 0, 0x2D, 0);
    entity->pc++;
}

/** @brief Opcode handler: release the model once the count reaches 30. */
static void func_801A98D0(EffectEntity *entity) {
    if (entity->unk024 == 30) {
        entity->unk010->unk063 = 0;
        entity->pc++;
    }
}

/** @brief Opcode handler: hand the current part to battle, then stop. */
static void func_801A9904(EffectEntity *entity) {
    if (entity->unk024 >= 30) {
        func_800BFE1C(&entity->animSet->anims[entity->unk02A].parts[entity->unk02B]);
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A998C(EffectEntity *entity) {
}

/** @brief Script dispatcher: the take-model script, with its two timed cues. */
static s32 func_801A9994(EffectEntity *entity) {
    EffectHandler handlers[4] = { func_801A9878, func_801A98D0, func_801A9904, func_801A998C };

    func_801A0CE8(entity);
    func_801A1078(entity);
    handlers[entity->pc](entity);
    if (entity->unk024 == 0) {
        func_800C4764(D_801A9FAC, 0, 0x80);
    }
    if (entity->unk024 == 20) {
        EffectTintScript *child =
            func_801A0B5C(&D_801D4ACC, func_801A16A4, sizeof(EffectTintScript), entity);

        child->steps = D_801A9FA0;
    }
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A0B34(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: consume the opcode and do nothing else. */
static void func_801A9AA4(EffectEntity *entity) {
    entity->pc++;
}

/** @brief Opcode handler: consume the opcode and do nothing else. */
static void func_801A9AB8(EffectEntity *entity) {
    entity->pc++;
}

/** @brief Opcode handler: carve both prim banks and start the draw script. */
static void func_801A9ACC(EffectEntity *entity) {
    if (entity->wait == 0) {
        /* Both banks are carved out of the same byte arena behind the TIM. */
        D_801D3F7C = (EffectMote *)D_801D3CDC;
        D_801D3CDC += EFFECT_BANK0_BYTES;
        D_801D3F80 = (EffectDrawNode *)D_801D3CDC;
        D_801D3CDC += EFFECT_BANK1_BYTES;
        func_801A8FF8();
        func_801A0B5C(&D_801D3F6C, func_801A9570, 0x40, entity);
        entity->pc++;
    }
}

/** @brief Opcode handler: start a task running @ref func_801A9994. */
static void func_801A9B5C(EffectEntity *entity) {
    entity->unk063 = 1;
    func_801A0B5C(&D_801D49FC, func_801A9994, 0x58, entity);
    entity->pc++;
}

/** @brief Opcode handler: repeat the step until the loop counter runs out. */
static void func_801A9BB4(EffectEntity *entity) {
    if (entity->unk063 == 0) {
        if (entity->unk02A < entity->unk058) {
            entity->unk02A++;
            entity->unk02E++;
            entity->pc--;
        } else {
            /* The byte trio at unk060 is this script's countdown, one halfword wide. */
            *(s16 *)entity->unk060 = 10;
            entity->pc++;
        }
    }
}

/** @brief Opcode handler: spawn the trailing spark once the wait runs out. */
static void func_801A9C18(EffectEntity *entity) {
    /* The byte trio at unk060 is this script's countdown, one halfword wide. */
    if (--*(s16 *)entity->unk060 <= 0) {
        func_801A0B5C(&D_801D3F6C, func_801A968C, 0x40, entity);
        entity->pc++;
    }
}

/** @brief Opcode handler: stall here until @c unk05E reaches zero. */
static void func_801A9C80(EffectEntity *entity) {
    if (entity->unk05E == 0) {
        entity->pc++;
    }
}

/** @brief Opcode handler: consume the opcode and do nothing else. */
static void func_801A9CA8(EffectEntity *entity) {
    entity->pc++;
}

/** @brief Opcode handler: consume the opcode and do nothing else. */
static void func_801A9CBC(EffectEntity *entity) {
    entity->pc++;
}

/** @brief Opcode handler: raise @ref EFFECT_FLAG_STOP and consume the opcode. */
static void func_801A9CD0(EffectEntity *entity) {
    entity->flags |= EFFECT_FLAG_STOP;
    entity->pc++;
}

/** @brief Opcode handler: no-op. */
static void func_801A9CEC(EffectEntity *entity) {
}

/**
 * @brief Run one frame of the effect: its opcode, then every task pool it owns.
 *
 * @return 2 once the script has stopped and its children have drained.
 */
static s32 func_801A9CF4(EffectEntity *entity) {
    EffectHandler handlers[11] = {
        func_801A9AA4, func_801A9AB8, func_801A9ACC, func_801A9B5C, func_801A9BB4,
        func_801A9C18, func_801A9C80, func_801A9CA8, func_801A9CBC, func_801A9CD0,
        func_801A9CEC
    };
    MATRIX *view = EFFECT_SCRATCHPAD;

    *view = D_800F02C8;
    D_801D3CD8 = (u8 *)view;
    D_801D3CD4 = view;
    if (entity->unk05C & EFFECT_FRAME_ODD) {
        D_801D479C = D_801D47A4;
        D_801D47A0 = D_801D47AC;
    } else {
        D_801D479C = D_801D47A8;
        D_801D47A0 = D_801D47B0;
    }
    func_801A0EAC(entity);
    handlers[entity->pc](entity);
    D_801D3F8C = 0;
    entity->unk05E = 0;
    D_801D3F8E = 0;
    entity->unk05E += func_800B2B68(&D_801D49FC);
    entity->unk05E += func_800B2B68(&D_801D4ACC);
    entity->unk05E += func_800B2B68(&D_801D478C);
    entity->unk05E += func_800B2B68(&D_801D3F6C);
    entity->unk05C++;
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait == 0) {
            func_801A0B34(entity);
            return 2;
        }
    }
    return 0;
}
