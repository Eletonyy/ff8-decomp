/**
 * @file effect_025.c
 * @brief Cura
 */
#include "common.h"
#include "game.h"
#include "effect.h"
#include "psxsdk/libc.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/inline_c.h"
#include "effect/effect_025.h"
#include "btl_entity.h"

/**
 * @name Pose channel flags
 *
 * @ref EffectPosePart::flags carries one trio per channel: the channel is
 * keyed, it also carries a velocity, it also carries an acceleration. The
 * trios are the translation, the rotation, the scale and the colour, in the
 * order @ref func_801A6354 walks them; the top two bits are single channels
 * of their own, the depth ramp and the vertex-animation cursor.
 * @{
 */
#define EFFECT_POSE_KEY0 0x1
#define EFFECT_POSE_VEL0 0x2
#define EFFECT_POSE_ACC0 0x4
#define EFFECT_POSE_KEY1 0x8
#define EFFECT_POSE_VEL1 0x10
#define EFFECT_POSE_ACC1 0x20
#define EFFECT_POSE_SCALE_KEY 0x40
#define EFFECT_POSE_SCALE_VEL 0x80
#define EFFECT_POSE_SCALE_ACC 0x100
#define EFFECT_POSE_KEY3 0x200
#define EFFECT_POSE_VEL3 0x400
#define EFFECT_POSE_ACC3 0x800
#define EFFECT_POSE_DEPTH 0x1000
#define EFFECT_POSE_ANIM 0x2000
/** @} */

/** @brief Ceiling of the depth and vertex-animation ramps: @ref ONE in 16.16. */
#define EFFECT_POSE_RAMP_MAX 0x10000000

/**
 * @name Scrolled UV words
 *
 * A UV pair is one halfword, U in the low byte and V in the high one, so a
 * step of one V unit is 0x100; the third and fourth pairs of a quad share a
 * word. A scroll masks V out, steps it and puts the rest back, and a step that
 * carries out of the byte has wrapped.
 * @{
 */
#define EFFECT_UV_V 0xFF00           /**< V of a pair, in place. */
#define EFFECT_UV_KEEP 0xFFFF00FF    /**< What a scroll of one pair leaves alone. */
#define EFFECT_UV_KEEP23 0x00FF00FF  /**< Both U bytes of the shared third and fourth pair. */
#define EFFECT_UV_WRAP 0x8000        /**< One V byte's worth of wrap. */
#define EFFECT_UV_SCROLL_STEP (4 << 8) /**< The fixed scroll: four whole V units. */
/** @} */

/**
 * @brief Words of a mesh stream's block header ahead of the quad list's count.
 *
 * The scroll walkers step over the stream's own header, then this, to reach
 * the count of gouraud textured quads and the quads themselves.
 */
#define EFFECT_QUAD_LIST_HEADER 7

/**
 * @name Prim emitter flags -- @ref EffectPrimBuild::flags.
 * @{
 */
#define EFFECT_EMIT_SEMITRANS 0x1
#define EFFECT_EMIT_OPAQUE 0x4
#define EFFECT_EMIT_TWO_SIDED 0x10
#define EFFECT_EMIT_DEPTH_CUE 0x40
/**
 * @name Per-part draw flags -- @ref EffectDrawRequest::colours
 *
 * The low half of each word is the prim's own flag word; the top bits steer
 * how @ref func_801A6A00 places and shades the part.
 * @{
 */
#define EFFECT_PART_WORLD_MATRIX 0x10000000
#define EFFECT_PART_SCROLL_UV 0x20000000
#define EFFECT_PART_DEPTH_RAMP 0x40000000
/** @} */

/* The gouraud emitters read their own copy of each of the four bits above. */
#define EFFECT_EMIT_G_SEMITRANS 0x2
#define EFFECT_EMIT_G_OPAQUE 0x8
#define EFFECT_EMIT_G_TWO_SIDED 0x20
#define EFFECT_EMIT_G_DEPTH_CUE 0x80
#define EFFECT_EMIT_TPAGE_SET 0x100
#define EFFECT_EMIT_CLUT_SET 0x200
#define EFFECT_EMIT_TPAGE_ADD 0x400
#define EFFECT_EMIT_CLUT_ADD 0x800
#define EFFECT_EMIT_UNK1000 0x1000
#define EFFECT_EMIT_UNK2000 0x2000
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

/** @brief What the prim emitters walk: the mesh stream and the GTE results. */
typedef struct {
    /* 0x00 */ s32 *stream;
    /* 0x04 */ u32 *verts;
    /* 0x08 */ u8 r;              /**< Far colour the depth cue fades toward. */
    /* 0x09 */ u8 g;
    /* 0x0A */ u8 b;
    /* 0x0B */ u8 pad00B[0xC - 0xB];
    /* 0x0C */ s32 depth;         /**< Depth-cue weight handed to DPCS. */
    /* 0x10 */ u16 tpage;        /**< Texture page the textured prims take. */
    /* 0x12 */ u8 pad012[0x14 - 0x12];
    /* 0x14 */ u16 clut;         /**< CLUT id the textured prims take. */
    /* 0x16 */ u8 pad016[0x18 - 0x16];
    /* 0x18 */ s32 unk018;
    /* 0x1C */ u32 flags;
    /* 0x20 */ s32 *cursor;       /**< Walks the stream, one entry per prim kind. */
    /* 0x24 */ s32 nclip;
    /* 0x28 */ u8 pad028[0x2C - 0x28];
    /* 0x2C */ s32 otz;
    /* 0x30 */ u32 gteFlag;
} EffectPrimBuild; /* 0x34 */

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
    /* 0x14 */ u32 unk014;
} EffectTri; /* 0x18 */

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
    /* 0x18 */ u32 unk018;
} EffectQuad; /* 0x1C */

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
    /* 0x20 */ u32 unk020;
} EffectTexTri; /* 0x24 */

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
    /* 0x1C */ u32 unk01C;
} EffectGouraudTri; /* 0x20 */

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
    /* 0x24 */ u32 unk024;
} EffectGouraudQuad; /* 0x28 */

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
    /* 0x28 */ u32 unk028;
} EffectGouraudTexTri; /* 0x2C */

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
    /* 0x34 */ u32 unk034;
} EffectGouraudTexQuad; /* 0x38 */

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
    /* 0x28 */ u32 unk028;
} EffectTexQuad; /* 0x2C */

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

/** @brief A vertex animation: a header, then @c count posed points per frame. */
typedef struct {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 count;
    /* 0x08 */ s32 unk008;
    /* 0x0C */ SVECTOR frames[1];
} EffectVertexAnim;

/**
 * @brief One posed part, rebuilt from the pose script for every draw.
 *
 * @ref func_801A6354 zeroes this, walks the script into it and hands it to the
 * per-part callback; the fields past @c colour are the animation cursor.
 */
typedef struct {
    /* 0x00 */ s16 index;    /**< The part's own slot in the pose script. */
    /* 0x02 */ s16 mesh;     /**< Which of the model's meshes it draws. */
    /* 0x04 */ u32 unk004;   /**< The part entry's header word; @c mesh is its low byte. */
    /* 0x08 */ s16 pos[3];
    /* 0x0E */ u8 pad00E[0x10 - 0xE];
    /* 0x10 */ s16 rot[3];
    /* 0x16 */ u8 pad016[0x18 - 0x16];
    /* 0x18 */ s16 scale[3];
    /* 0x1E */ u8 pad01E[0x20 - 0x1E];
    /* 0x20 */ CVECTOR colour;
    /* 0x24 */ s16 depth;
    /* 0x26 */ s16 weight;   /**< Blend between @c frameA and @c frameB. */
    /* 0x28 */ s16 frameA;
    /* 0x2A */ s16 frameB;
} EffectPartPose; /* 0x2C */

/** @brief What one pose walk draws: the matrix, its colours and its texture. */
typedef struct {
    /* 0x00 */ MATRIX *m;
    /* 0x04 */ u32 *colours;
    /* 0x08 */ void *unk08;
    /* 0x0C */ s32 unk00C;
    /* 0x10 */ s16 unk010;
    /* 0x12 */ u8 pad012[0x14 - 0x12];
} EffectDrawRequest; /* 0x14 */

/** @brief What @ref func_801A6354 hands each part it has posed to. */
typedef void (*EffectPartFn)(s32 *pose, EffectPartPose *part,
                             EffectDrawRequest *req);

static EffectEntity *func_801A21C0(void *pool, void *task, s32 stride,
                                   EffectEntity *owner);
static s32 func_801A6354(s32 *pose, EffectPartFn fn, EffectDrawRequest *req);
static EffectTri *func_801A2D08(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A2FE8(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A3320(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A36B4(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A3AAC(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A3DD4(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A4174(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static EffectTri *func_801A4548(EffectPrimBuild *s, u32 *ot, s32 otShift,
                               EffectTri *prim);
static s32 func_801A81D0(EffectEntity *entity);

/**
 * @brief Start the effect's script and hand back its task pool.
 *
 * Builds the four task pools, allocates the root entity, seeds it from the
 * animation set, and publishes the two prim banks the renderer draws through.
 *
 * @param animSet Animation set the effect plays.
 * @return The pool the root entity lives in.
 */
void *func_801A0000(EffectAnimSet *animSet) {
    EffectEntity *entity;
    u8 slot;

    slot = animSet->anims->parts->unk000;
    func_800B2A00(&D_801D3F94, &D_801D3EC4, 0x64, 2);
    entity = func_801A21C0(&D_801D3F94, func_801A81D0, 0x64, NULL);
    entity->animSet = animSet;
    entity->unk02D = slot;
    entity->unk05A = animSet->anims->unk010;
    entity->unk058 = animSet->anims->unk011;
    if (!(animSet->flags & EFFECT_ANIMSET_FLAG_LOADED)) {
        func_800C3BE0(&D_801A8E50);
        func_800BB084(&D_801A9698);
    }
    D_801D3EBC = &D_801A9698;
    D_801D3EC0 = &D_801BB698;
    func_800B2A00(&D_801D4104, &D_801D3FA4, 0x58, 4);
    func_800B2A00(&D_801D7314, &D_801D4114, 0xC8, 0x40);
    func_800B2A00(&D_801E74F4, &D_801D7324, 0x84, 0x1F4);
    return &D_801D3F94;
}

/**
 * @brief Cache the sixteen hex digit glyphs and reset the debug text cursor.
 *
 * @note The lookup runs once per glyph -- the target holds 16 separate calls,
 *       so a loop or a cached pointer does not match.
 */
static void func_801A014C(void) {
    D_801ED404[0] = getMenuString(11)[1];
    D_801ED404[1] = getMenuString(11)[2];
    D_801ED404[2] = getMenuString(11)[3];
    D_801ED404[3] = getMenuString(11)[4];
    D_801ED404[4] = getMenuString(11)[5];
    D_801ED404[5] = getMenuString(11)[6];
    D_801ED404[6] = getMenuString(11)[7];
    D_801ED404[7] = getMenuString(11)[8];
    D_801ED404[8] = getMenuString(11)[9];
    D_801ED404[9] = getMenuString(11)[10];
    D_801ED404[10] = getMenuString(11)[11];
    D_801ED404[11] = getMenuString(11)[12];
    D_801ED404[12] = getMenuString(11)[13];
    D_801ED404[13] = getMenuString(11)[14];
    D_801ED404[14] = getMenuString(11)[15];
    D_801ED404[15] = getMenuString(11)[16];
    D_801ED404[16] = 0;
    D_801ED418 = 10;
    D_801ED41C = 20;
    D_801ED420 = 3;
}

/** @brief Draw @p value as eight hex glyphs at the current debug text cursor. */
static void func_801A029C(u32 value) {
    u8 *font = D_800FA5E8->font;
    u8 text[9];
    s32 i;

    for (i = 7; i >= 0; i--) {
        text[i] = D_801ED404[value & 0xF];
        value >>= 4;
    }
    text[8] = 0;
    D_801D3EB8 = func_8002C56C(font, D_801D3EB8, D_801ED418, D_801ED41C,
                               text, D_801ED420);
    D_801ED418 += 0x48;
}

/** @brief Opcode handler: arm the frame delay and advance the running total. */
static void func_801A0348(void) {
    D_801ED418 = 10;
    D_801ED41C += 10;
}

/** @brief Opcode handler: no-op. */
static void func_801A0368(EffectEntity *entity) {
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
static void func_801A0370(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                   s32 abr) {
    POLY_FT4 *poly = D_801D3EB8;

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
    D_801D3EB8 = poly;
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
static void func_801A046C(s16 x, s16 y, s16 tx, s16 ty, u32 clutX, s32 clutY,
                   s32 abr) {
    POLY_FT4 *poly = D_801D3EB8;

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
    D_801D3EB8 = poly;
}

/** @brief Load the identity matrix. */
static void func_801A0568(MATRIX *m) {
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
static void func_801A05A0(MATRIX *m, s32 angle) {
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
static void func_801A0730(MATRIX *m, s32 angle) {
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

/** @brief Build a matrix in @p m that aims along @p dir with @p up as the roll. */
static void func_801A08BC(MATRIX *m, SVECTOR *dir, SVECTOR *up) {
    MATRIX *basis = func_800B3698(sizeof(MATRIX));
    SVECTOR *row1 = (SVECTOR *)basis->m[1];
    SVECTOR *row2 = (SVECTOR *)basis->m[2];

    /* A matrix row is 6 bytes, so the whole-vector store spills one halfword
       into row 2 -- harmless, row 2 is written next. The width is load-bearing:
       it is the unaligned 8-byte copy the target does here, where the
       element-wise stores below are three halfwords. */
    *row1 = *up;
    row2->vx = dir->vx;
    row2->vy = dir->vy;
    row2->vz = dir->vz;
    gte_ldopv1SV(row1);
    gte_ldopv2SV(row2);
    gte_op12();
    gte_stsv(basis->m[0]);
    MatrixNormal_2(basis, basis);
    TransposeMatrix(basis, m);
    func_800B36B8(sizeof(MATRIX));
}

/** @brief Aim @p m along the vector from @p from to @p to, Y up. */
static void func_801A09C8(MATRIX *m, SVECTOR *from, SVECTOR *to) {
    SVECTOR up;
    SVECTOR delta;

    up.vx = 0;
    up.vy = ONE;
    up.vz = 0;
    delta.vx = to->vx - from->vx;
    delta.vy = to->vy - from->vy;
    delta.vz = to->vz - from->vz;
    func_801A08BC(m, &delta, &up);
}

/** @brief The heading @p m faces, a quarter turn ahead of its Z axis. */
static s32 func_801A0A34(MATRIX *m) {
    return (ratan2(m->m[2][2], m->m[0][2]) + 0x400) & 0xFFF;
}

/** @brief Push @p entity to a random point within @p radius of where it is. */
static void func_801A0A64(EffectEntity *entity, s16 radius) {
    MATRIX m;
    SVECTOR angles;
    SVECTOR in;
    SVECTOR out;

    if (radius == 0) {
        radius = 1;
    }
    angles.vy = rand() & 0xFFF;
    angles.vx = rand() & 0xFFF;
    func_801A0568(&m);
    func_801A0730(&m, angles.vy);
    func_801A05A0(&m, angles.vx);
    in.vx = 0;
    in.vy = 0;
    in.vz = rand() % radius;
    ApplyMatrixSV(&m, &in, &out);
    entity->pos.vx += out.vx;
    entity->pos.vy += out.vy;
    entity->pos.vz += out.vz;
}

/** @brief Refresh the render matrices from @p pose and apply the offset. */
static void func_801A0B6C(EffectRender *render, BattleEffectSlot *slot) {
    SVECTOR offset;

    render->unk014 = slot->mtx;
    render->unk054 = render->unk014;
    switch (render->unk0D2) {
    case 0:
        ApplyMatrixSV(&render->unk054, &render->unk0B4, &offset);
        render->unk054.t[0] += offset.vx;
        render->unk054.t[1] += offset.vy;
        render->unk054.t[2] += offset.vz;
        break;
    case 1:
        render->unk054.t[0] += render->unk0B4.vx;
        render->unk054.t[1] += render->unk0B4.vy;
        render->unk054.t[2] += render->unk0B4.vz;
        break;
    }
}

/** @brief Compose each joint's matrix through the pose and the per-joint offset. */
static void func_801A0CB0(EffectSkeletonRef *ref, EffectPose *pose) {
    MATRIX *offset = D_801ED424;
    EffectSkeleton *skeleton = ref->mesh->skeleton;
    EffectJoint *joint = skeleton->joints;
    s32 i;

    for (i = 0; i < skeleton->count; i++) {
        CompMatrix(&pose->world, &joint->mtx, offset);
        CompMatrix(&pose->view, &joint->mtx, &joint->mtx);
        joint++;
        offset++;
    }
}

/** @brief Rebase a table of absolute pointers into offsets from its own head. */
static void func_801A0D5C(s32 *table) {
    s32 *base;
    s32 count;
    s32 i;

    base = table;
    count = *table;
    for (i = 0; i < count; i++) {
        table++;
        *table -= (s32)base;
    }
}

/** @brief Aim the render's model at the camera reference point. */
static void func_801A0D9C(EffectRender *render) {
    EffectAimScratch *a = func_800B3698(sizeof(EffectAimScratch));
    s32 dx;
    s32 dy;
    s32 dz;

    a->pos.vx = render->unk054.t[0];
    a->pos.vy = render->unk054.t[1];
    a->pos.vz = render->unk054.t[2];
    dx = a->pos.vx - render->unk0DC;
    a->delta.vx = dx;
    dy = a->pos.vy - render->unk0DE;
    a->delta.vy = dy;
    dz = a->pos.vz - render->unk0E0;
    a->delta.vz = dz;
    a->distSq = dx * dx + dy * dy + dz * dz;
    a->dist = SquareRoot0(a->distSq);
    a->up.vx = 0;
    a->up.vy = ONE;
    a->up.vz = 0;
    VectorNormalS(&a->delta, &a->dir);
    func_801A08BC(&a->rot, &a->dir, &a->up);
    TransposeMatrix(&a->rot, &a->out);
    a->out.t[0] = 0;
    a->out.t[1] = 0;
    a->out.t[2] = render->scale;
    a->rot = render->unk014;
    a->rot.t[0] = render->unk014.t[0] - render->unk054.t[0];
    a->rot.t[1] = render->unk014.t[1] - render->unk054.t[1];
    a->rot.t[2] = render->unk014.t[2] - render->unk054.t[2];
    gte_MulMatrix0(&a->out, &a->rot, &render->unk074);
    gte_SetTransMatrix(&a->out);
    gte_ldlv0(a->rot.t);
    gte_mvmva(1, 0, 0, 0, 0);
    gte_stlvnl(render->unk074.t);
    func_800B36B8(sizeof(EffectAimScratch));
}

/** @brief Reset a render request to its default pose, white, and unit scale. */
static void func_801A1088(EffectRender *render, void *pose, EffectRenderPart *part,
                   void *texture, BattleEffectSlot *source, s32 frame) {
    render->unk00C = &D_800FA5F0;
    render->unk000 = pose;
    render->part = part;
    render->unk010 = &D_801D3EB8;
    render->unk0B4.vx = 0;
    render->unk0B4.vy = 0;
    render->unk0B4.vz = 0;
    render->r = 0x80;
    render->g = 0x80;
    render->b = 0x80;
    render->scale = 0x2000;
    render->unk0D2 = 1;
    render->unk0CE = 0;
    render->unk0F2 = 0;
    render->unk0A4.vx = 0;
    render->unk0A4.vy = 0;
    render->unk0A4.vz = 0;
    render->unk0AC.vx = 0;
    render->unk0AC.vy = 0;
    render->unk0AC.vz = 0;
    render->unk0BC.vx = 0;
    render->unk0BC.vy = 0;
    render->unk0BC.vz = 0;
    render->slot = source;
    render->unk0DA = frame;
    part->unk018 = 0x140;
    part->unk004 = texture;
    part->unk014 = 0;
    part->unk016 = 0;
    part->unk01A = 0;
    part->r = 0x80;
    part->g = 0x80;
    part->b = 0x80;
    part->unk020 = -1;
    part->unk024 = 0;
}

/** @brief Point a render request at a texture page, CLUT and source rectangle. */
static void func_801A1144(EffectRender *render, SVECTOR *pos, s16 tx, s16 ty,
                   s16 clutX, s16 clutY, s16 w, s16 h) {
    POLY_FT3 poly;

    render->unk0DC = pos->vx;
    render->unk0DE = pos->vy;
    render->unk0E0 = pos->vz;
    setPolyFT3(&poly);
    setSemiTrans(&poly, 1);
    setShadeTex(&poly, 1);
    poly.tpage = getTPage(1, 1, tx, ty);
    poly.clut = getClut(clutX, clutY);
    render->tpage = poly.tpage;
    render->clut = poly.clut;
    render->unk0E4 = w;
    render->unk0E6 = h;
    /* The u coordinate within the page, doubled for the 4bpp texture. */
    render->unk0E8 = ((s16)tx - (s16)(tx & ~0x3F)) * 2;
    render->unk0EA = ty & 0xFF;
}

/**
 * @brief Draw one mesh: pose its vertices by joint, then emit its triangles and
 *        quads into the ordering table.
 *
 * Each part of the mesh is a command stream. Its first section lists vertex
 * groups, each posed through one joint of the skeleton with the GTE's light
 * matrix (the part's vertices are normals: @c mvmva against the loaded joint).
 * The aligned tail holds a triangle list and a quad list. Every primitive is
 * transformed twice: once with the view matrix, which gives the screen
 * coordinates and depth for BOTH the shaded prim (whose UVs are the screen
 * coordinates -- an environment map) and the textured prim; then with the
 * light matrix, whose winding decides the back-face cull when the render asks
 * for it. A primitive is also rejected when any projected vertex leaves the
 * clip rectangle, in which case only the textured prim is emitted.
 *
 * The prim cursors come from and return to the render's two list heads; the
 * scratch record lives in the battle scratchpad for the duration.
 *
 * @param mesh   Skeleton and part table to draw.
 * @param ot     Ordering table the prims are linked into.
 * @param mode   Unused.
 * @param render Render state: matrices, colour, texture page and clip rect.
 */
static void func_801A120C(EffectMesh *mesh, u32 *ot, s32 mode,
                          EffectRender *render) {
    EffectMeshVertex *vbuf = render->part->unk004;
    EffectJoint *joints = mesh->skeleton->joints;
    u32 *parts = mesh->parts;
    s32 count = parts[0];
    /* battle.bin keeps this list head as a word (see D_800FA5F0). */
    POLY_FT3 *ft3 = (POLY_FT3 *)render->unk00C[0];
    POLY_GT3 *gt3 = render->unk010[0];
    BattleEffectSlot *slot = render->slot;
    EffectMeshScratch *s = func_800B3698(sizeof(EffectMeshScratch));
    s32 part;
    POLY_FT4 *ft4;
    POLY_GT4 *gt4;
    u32 colour;

    s->visible = slot->unk07C;
    s->unk0CE = render->unk0F4;
    s->tpage = render->tpage;
    s->clut = render->clut;
    s->clipX0 = -(render->unk0E4 / 2);
    s->clipX1 = render->unk0E4 / 2 - 1;
    s->clipY0 = -(render->unk0E6 / 2);
    s->clipY1 = render->unk0E6 / 2 - 1;
    s->clipX0 += EFFECT_SCREEN_CX;
    s->clipX1 += EFFECT_SCREEN_CX;
    s->clipY0 += EFFECT_SCREEN_CY;
    s->clipY1 += EFFECT_SCREEN_CY;
    s->offX = render->unk0E8 + render->unk0E4 / 2 - EFFECT_SCREEN_CX;
    s->offY = render->unk0EA + render->unk0E6 / 2 - EFFECT_SCREEN_CY;
    /* The shaded prims are semi-transparent (code bit 2), the textured are not. */
    s->colour = *(u32 *)&render->r & EFFECT_PRIM_RGB;
    s->unk0AC = s->colour | EFFECT_PRIM_CODE(0x3C | 0x02);
    s->colour = s->colour | EFFECT_PRIM_CODE(0x34 | 0x02);
    s->unk0B0 = slot->unk028 & EFFECT_PRIM_RGB;
    s->unk0B4 = s->unk0B0 | EFFECT_PRIM_CODE(0x2C);
    s->unk0B0 = s->unk0B0 | EFFECT_PRIM_CODE(0x24);
    s->view = render->unk034;
    s->light = render->unk074;
    s->unk0A4 = render->unk0CE;
    s->unk0A6 = render->unk0CC;
    parts++;
    gte_SetRotMatrix(&s->view);
    gte_SetTransMatrix(&s->view);
    for (part = 0; part < count; part++) {
        EffectMeshVertex *vb = vbuf;
        s16 *stream = (s16 *)((u8 *)mesh->parts + *parts++);
        s32 groups;
        s32 g;
        s32 n;
        s32 v;
        s32 tris;
        EffectMeshTri *tri;
        MATRIX *jm;
        EffectMeshQuad *quad;
        s32 quads;
        POLY_GT3 *gt;
        POLY_FT3 *ft;

        if (!((s->visible >> part) & 1)) {
            continue;
        }
        groups = *stream++;
        for (g = 0; g < groups; g++) {
            jm = &joints[*stream++].mtx;
            gte_SetLightMatrix(jm);
            gte_ldbkdir(jm->t[0], jm->t[1], jm->t[2]);
            n = *stream++;
            for (v = 0; v < n; v++) {
                s->normal.vx = *stream++;
                s->normal.vy = *stream++;
                s->normal.vz = *stream++;
                gte_ldv0(&s->normal);
                gte_mvmva(1, 1, 0, 1, 0);
                gte_stsv(&vb->pos);
                vb++;
            }
        }
        stream = (s16 *)(((u32)stream + 3) & ~3);
        tris = *stream++;
        quads = *stream++;
        stream += 4;
        tri = (EffectMeshTri *)stream;
        vb = vbuf;
        gt = gt3;
        ft = ft3;
        for (g = 0; g < tris; g++) {
            s->idx0 = tri->idx0 & EFFECT_MESH_INDEX_MASK;
            s->idx1 = tri->idx1 & EFFECT_MESH_INDEX_MASK;
            s->idx2 = tri->idx2 & EFFECT_MESH_INDEX_MASK;
            gte_ldv3(&vb[s->idx0].pos, &vb[s->idx1].pos, &vb[s->idx2].pos);
            gte_rtpt();
            gte_nclip();
            gte_stopz(&s->nclip);
            if (s->nclip > 0) {
                gte_avsz3();
                gte_stotz(&s->otz);
                s->otz = (u32)s->otz >> 2;
                gte_stsxy3_gt3(gt);
                gte_stsxy3_ft3(ft);
                gte_SetRotMatrix(&s->light);
                gte_SetTransMatrix(&s->light);
                gte_ldv3(&vb[s->idx0].pos, &vb[s->idx1].pos,
                         &vb[s->idx2].pos);
                gte_rtpt();
                s->reject = 0;
                gte_nclip();
                gte_stopz(&s->nclip);
                if (s->unk0CE == 1 && s->nclip <= 0) {
                    s->reject = 1;
                }
                if (!s->reject) {
                    gte_stsxy3c(s->sxy);
                    for (v = 0; v < 3; v++) {
                        if (s->sxy[v].vx < s->clipX0) {
                            s->reject = 1;
                            break;
                        }
                        if (s->clipX1 < s->sxy[v].vx) {
                            s->reject = 1;
                            break;
                        }
                        if (s->sxy[v].vy < s->clipY0) {
                            s->reject = 1;
                            break;
                        }
                        if (s->clipY1 < s->sxy[v].vy) {
                            s->reject = 1;
                            break;
                        }
                        s->sxy[v].vx += s->offX;
                        s->sxy[v].vy += s->offY;
                    }
                }
                if (!s->reject) {
                    gt->tag = EFFECT_PRIM_TAG(POLY_GT3);
                    gt->u0 = s->sxy[0].vx;
                    gt->v0 = s->sxy[0].vy;
                    gt->u1 = s->sxy[1].vx;
                    gt->v1 = s->sxy[1].vy;
                    gt->u2 = s->sxy[2].vx;
                    gt->v2 = s->sxy[2].vy;
                    gt->tpage = s->tpage;
                    gt->clut = s->clut;
                    /* Colour and code as one word; four byte stores do not match. */
                    colour = s->colour;
                    *(u32 *)&gt->r2 = colour;
                    *(u32 *)&gt->r1 = colour;
                    *(u32 *)&gt->r0 = colour;
                    addPrim(&ot[s->otz], gt);
                    gt++;
                }
                ft->tag = EFFECT_PRIM_TAG(POLY_FT3);
                *(u32 *)&ft->u0 = tri->uv0;
                *(u32 *)&ft->u1 = *(u32 *)&tri->uv1;
                *(u16 *)&ft->u2 = tri->uv2;
                *(u32 *)&ft->r0 = s->unk0B0;
                if (tri->tpage & EFFECT_MESH_TPAGE_ABE) {
                    ft->code |= 2;
                }
                addPrim(&ot[s->otz], ft);
                ft++;
                gte_SetRotMatrix(&s->view);
                gte_SetTransMatrix(&s->view);
            }
            tri++;
        }
        quad = (EffectMeshQuad *)tri;
        ft4 = (POLY_FT4 *)ft;
        gt4 = (POLY_GT4 *)gt;
        for (g = 0; g < quads; g++) {
            s->idx0 = quad->idx0 & EFFECT_MESH_INDEX_MASK;
            s->idx1 = quad->idx1 & EFFECT_MESH_INDEX_MASK;
            s->idx2 = quad->idx2 & EFFECT_MESH_INDEX_MASK;
            gte_ldv3(&vb[s->idx0].pos, &vb[s->idx1].pos, &vb[s->idx2].pos);
            gte_rtpt();
            gte_nclip();
            gte_stopz(&s->nclip);
            if (s->nclip > 0) {
                gte_stsxy3_gt3(gt4);
                gte_stsxy3_ft3(ft4);
                s->idx3 = quad->idx3 & EFFECT_MESH_INDEX_MASK;
                gte_ldv0(&vb[s->idx3].pos);
                gte_rtps();
                gte_stsxy(&gt4->x3);
                gte_stsxy(&ft4->x3);
                gte_avsz4();
                gte_stotz(&s->otz);
                s->otz = (u32)s->otz >> 2;
                gte_SetRotMatrix(&s->light);
                gte_SetTransMatrix(&s->light);
                gte_ldv3(&vb[s->idx0].pos, &vb[s->idx1].pos,
                         &vb[s->idx2].pos);
                gte_rtpt();
                s->reject = 0;
                gte_nclip();
                gte_stopz(&s->nclip);
                if (s->unk0CE == 1 && s->nclip <= 0) {
                    s->reject = 1;
                }
                if (!s->reject) {
                    gte_stsxy3c(s->sxy);
                    gte_ldv0(&vb[s->idx3].pos);
                    gte_rtps();
                    gte_stsxy(&s->sxy[3]);
                    for (v = 0; v < 4; v++) {
                        if (s->sxy[v].vx < s->clipX0) {
                            s->reject = 1;
                            break;
                        }
                        if (s->clipX1 < s->sxy[v].vx) {
                            s->reject = 1;
                            break;
                        }
                        if (s->sxy[v].vy < s->clipY0) {
                            s->reject = 1;
                            break;
                        }
                        if (s->clipY1 < s->sxy[v].vy) {
                            s->reject = 1;
                            break;
                        }
                        s->sxy[v].vx += s->offX;
                        s->sxy[v].vy += s->offY;
                    }
                }
                if (!s->reject) {
                    gt4->tag = EFFECT_PRIM_TAG(POLY_GT4);
                    gt4->u0 = s->sxy[0].vx;
                    gt4->v0 = s->sxy[0].vy;
                    gt4->u1 = s->sxy[1].vx;
                    gt4->v1 = s->sxy[1].vy;
                    gt4->u2 = s->sxy[2].vx;
                    gt4->v2 = s->sxy[2].vy;
                    gt4->u3 = s->sxy[3].vx;
                    gt4->v3 = s->sxy[3].vy;
                    gt4->tpage = s->tpage;
                    gt4->clut = s->clut;
                    colour = s->unk0AC;
                    *(u32 *)&gt4->r3 = colour;
                    *(u32 *)&gt4->r2 = colour;
                    *(u32 *)&gt4->r1 = colour;
                    *(u32 *)&gt4->r0 = colour;
                    addPrim(&ot[s->otz], gt4);
                    gt4++;
                }
                ft4->tag = EFFECT_PRIM_TAG(POLY_FT4);
                *(u32 *)&ft4->u0 = quad->uv0;
                *(u32 *)&ft4->u1 = *(u32 *)&quad->uv1;
                *(u16 *)&ft4->u2 = quad->uv2;
                *(u16 *)&ft4->u3 = quad->uv3;
                *(u32 *)&ft4->r0 = s->unk0B4;
                if (quad->tpage & EFFECT_MESH_TPAGE_ABE) {
                    ft4->code |= 2;
                }
                addPrim(&ot[s->otz], ft4);
                ft4++;
                gte_SetRotMatrix(&s->view);
                gte_SetTransMatrix(&s->view);
            }
            quad++;
        }
        ft3 = (POLY_FT3 *)ft4;
        gt3 = (POLY_GT3 *)gt4;
    }
    render->unk00C[0] = (s32)ft3;
    render->unk010[0] = gt3;
    func_800B36B8(sizeof(EffectMeshScratch));
}

/** @brief Pose the effect's model and link its skeletons into the battle OT. */
static void func_801A205C(EffectRender *render) {
    BattleEffectSlot *slot = render->slot;

    func_801A0B6C(render, slot);
    if (render->unk0DA != 0xFF) {
        func_800BC420(slot->unk060);
    } else if (func_800BCA3C(slot->unk060, slot->unk06C) != 0) {
        func_800BCF6C(slot->unk060, slot->unk06C, render->unk0D8);
    }
    CompMatrix(&D_800F02C8, &render->unk014, &render->unk034);
    if (!(slot->flags & BATTLE_SLOT_FLAG_UNK20)) {
        *render->unk00C = func_800BC060(slot, D_800FA5E8->unk4040, 0x10, *render->unk00C);
    }
    func_801A0D9C(render);
    func_801A120C(slot->mesh, D_800FA5E8->ot, 4, render);
    if (slot->unk078 != NULL) {
        func_801A120C(slot->unk078->mesh, D_800FA5E8->ot, 4, render);
    }
    if (render->unk0DA != 0xFF) {
        func_800BC420(slot->unk060);
    }
}

/** @brief Opcode handler: release one frame of the linked script's wait. */
static void func_801A2198(EffectEntity *entity) {
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
static EffectEntity *func_801A21C0(void *pool, void *task, s32 stride,
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
static void func_801A234C(EffectEntity *entity) {
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
static void func_801A2480(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk030;
}

/** @brief Copy @c unk038 of the linked model out to @p out. */
static void func_801A24B0(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk038;
}

/** @brief Copy @c unk040 of the linked model out to @p out. */
static void func_801A24E0(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk040;
}

/** @brief As @ref func_801A234C, but for the slot the animation set names. */
static void func_801A2510(EffectEntity *entity) {
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
static void func_801A264C(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk030;
}

/** @brief Copy @c unk038 of the linked model out to @p out. */
static void func_801A267C(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk038;
}

/** @brief Copy @c unk040 of the linked model out to @p out. */
static void func_801A26AC(EffectEntity *entity, SVECTOR *out) {
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
static void func_801A26DC(EffectEntity *entity) {
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
static void func_801A2928(EffectEntity *entity, SVECTOR *min, SVECTOR *max) {
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
static s32 func_801A2978(EffectEntity *entity) {
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
static s16 func_801A2A1C(EffectEntity *entity) {
    EffectModel *model = entity->unk014;

    return (s16)(model->boundsMax.vy - model->boundsMin.vy) / 2;
}

/** @brief Half the linked model's larger horizontal extent. */
static s16 func_801A2A4C(EffectEntity *entity) {
    EffectModel *model = entity->unk014;
    s16 spanX = model->boundsMax.vx - model->boundsMin.vx;
    s16 spanZ = model->boundsMax.vz - model->boundsMin.vz;
    /* Compared as `span` but returned as `spanX`: testing a separate copy is
       what puts the wider-X arm on the taken branch. */
    s16 span = spanX;

    return (span <= spanZ ? spanZ : spanX) / 2;
}

/** @brief Height of the linked model's bounding box. */
static s16 func_801A2AA4(EffectEntity *entity) {
    return entity->unk014->boundsMax.vy - entity->unk014->boundsMin.vy;
}

/** @brief Y of the top of the linked model, in battle-entity space. */
static s16 func_801A2AC8(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];

    return slot->unk01E + entity->unk014->boundsMax.vy;
}

/** @brief Y of the bottom of the linked model, in battle-entity space. */
static s16 func_801A2B10(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];

    return slot->unk01E + entity->unk014->boundsMin.vy;
}

/** @brief A random point up the linked model, in battle-entity space. */
static s16 func_801A2B58(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];
    EffectModel *model = entity->unk014;
    s16 height = model->boundsMax.vy - model->boundsMin.vy;
    s16 offset = (rand() & 0xFFF) * height / 4096;

    return slot->unk01E + model->boundsMax.vy - offset / 2;
}

/** @brief Y of the centre of the linked model, in battle-entity space. */
static s16 func_801A2C10(EffectEntity *entity) {
    EffectModel *model = entity->unk014;

    return D_800EF2D0[entity->unk02D].unk01E +
           (model->boundsMax.vy + model->boundsMin.vy) / 2;
}

/** @brief Centre of the linked model's bounding box. */
static void func_801A2C64(EffectEntity *entity, SVECTOR *out) {
    EffectModel *model = entity->unk014;

    out->vx = (model->boundsMax.vx + model->boundsMin.vx) / 2;
    out->vy = (model->boundsMax.vy + model->boundsMin.vy) / 2;
    out->vz = (model->boundsMax.vz + model->boundsMin.vz) / 2;
}

/** @brief Zero @p size bytes' worth of words starting at @p dst. */
static void func_801A2CD0(s32 *dst, s32 size) {
    s32 i;

    for (i = 0; i < size / 4; i++) {
        *dst = 0;
        dst++;
    }
}

/**
 * @brief Emit the flat triangles the stream cursor points at.
 *
 * The entry starts with a triangle count; the cursor is stepped past it before
 * anything is drawn, so the caller always resumes at the next entry. Each
 * triangle is projected, given the build's colour with the semi-transparency
 * bit forced on or off, and dropped when the GTE could not project it, when it
 * faces away and the build is not two-sided, or when all three vertices leave
 * the screen on the same axis. Survivors optionally take a depth cue and are
 * linked into @p ot at their own depth.
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
static EffectTri *func_801A2D08(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 5);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED))) {
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
                        prim->unk014 = D_801EEA0C;
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
static EffectTri *func_801A2FE8(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 6);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED))) {
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
                        prim->unk018 = D_801EEA0C;
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
static EffectTri *func_801A3320(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 8);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED))) {
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
                        prim->unk020 = D_801EEA0C;
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
static EffectTri *func_801A36B4(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 0xA);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_TWO_SIDED))) {
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
                        prim->unk028 = D_801EEA0C;
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
static EffectTri *func_801A3AAC(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 7);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED))) {
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
                        prim->unk01C = D_801EEA0C;
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
static EffectTri *func_801A3DD4(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 9);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED))) {
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
                        prim->unk024 = D_801EEA0C;
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
static EffectTri *func_801A4174(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 0xA);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED))) {
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
                        prim->unk028 = D_801EEA0C;
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
static EffectTri *func_801A4548(EffectPrimBuild *s, u32 *ot, s32 otShift,
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
            setlen(prim, 0xD);
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
                if (s->nclip != 0 &&
                    (s->nclip >= 0 || (s->flags & EFFECT_EMIT_G_TWO_SIDED))) {
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
                        prim->unk034 = D_801EEA0C;
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
 * @brief Run every prim kind of one mesh stream into the ordering table.
 *
 * The stream names one entry per kind; a zero entry means the mesh has none
 * of that kind, and the emitters hand the prim cursor on between them.
 *
 * @return The prim cursor past everything emitted.
 */
static EffectTri *func_801A49A8(EffectPrimBuild *s, u32 *ot, s32 otShift,
                                EffectTri *prim) {
    if (!(s->flags & EFFECT_EMIT_UNK2000)) {
        s->verts = (u32 *)(s->stream + 2);
    }
    s->cursor = (s32 *)((u8 *)s->stream + s->stream[0]);
    if (!(s->flags & EFFECT_EMIT_UNK1000)) {
        s->unk018 = 0;
    }
    D_801EEA0C = 0xE1000220;
    gte_SetFarColor(s->r, s->g, s->b);
    if (*s->cursor != 0) {
        prim = func_801A2D08(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A2FE8(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A3320(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A36B4(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A3AAC(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A3DD4(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A4174(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    if (*s->cursor != 0) {
        prim = func_801A4548(s, ot, otShift, prim);
    } else {
        s->cursor++;
    }
    return prim;
}

/**
 * @brief Scroll the textured quad list's V coordinates by one fixed step.
 *
 * The list @p header names holds the gouraud textured quads; each carries four
 * UV pairs, the last two packed into one word. Only the V byte of each moves,
 * and all four wrap together so a quad never tears.
 *
 * @param header Mesh stream to walk.
 */
static void func_801A4BFC(s32 *header) {
    EffectEmitGouraudTexQuad *e;
    u32 keep[3];
    u32 ch[4];
    u32 uv23;
    s32 count;
    s32 i;

    header = (s32 *)((u8 *)header + header[0] / 4 * 4);
    header += EFFECT_QUAD_LIST_HEADER;
    count = *header;
    header++;
    for (i = 0; i < count; i++) {
        e = (EffectEmitGouraudTexQuad *)header;
        ch[0] = e->uv0;
        keep[0] = ch[0] & EFFECT_UV_KEEP;
        ch[0] &= EFFECT_UV_V;
        ch[1] = e->uv1;
        keep[1] = ch[1] & EFFECT_UV_KEEP;
        ch[1] &= EFFECT_UV_V;
        uv23 = e->uv23;
        ch[3] = uv23;
        ch[2] = uv23;
        keep[2] = uv23 & EFFECT_UV_KEEP23;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] >> 16) & EFFECT_UV_V;
        ch[0] -= EFFECT_UV_SCROLL_STEP;
        ch[1] -= EFFECT_UV_SCROLL_STEP;
        ch[2] -= EFFECT_UV_SCROLL_STEP;
        ch[3] -= EFFECT_UV_SCROLL_STEP;
        if (ch[0] > EFFECT_UV_V || ch[1] > EFFECT_UV_V || ch[2] > EFFECT_UV_V || ch[3] > EFFECT_UV_V) {
            ch[0] += EFFECT_UV_WRAP;
            ch[1] += EFFECT_UV_WRAP;
            ch[2] += EFFECT_UV_WRAP;
            ch[3] += EFFECT_UV_WRAP;
        }
        ch[0] &= EFFECT_UV_V;
        ch[1] &= EFFECT_UV_V;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] & EFFECT_UV_V) << 16;
        e->uv0 = keep[0] | ch[0];
        e->uv1 = keep[1] | ch[1];
        e->uv23 = keep[2] | ch[2] | ch[3];
        header += sizeof(EffectEmitGouraudTexQuad) / sizeof(s32);
    }
}

/**
 * @brief Scroll the textured quad list's V coordinates by @p delta.
 *
 * @ref func_801A4BFC with the step supplied by the caller.
 *
 * @param header Mesh stream to walk.
 * @param delta  Step in whole V units.
 */
static void func_801A4D9C(s32 *header, s32 delta) {
    EffectEmitGouraudTexQuad *e;
    u32 keep[3];
    u32 ch[4];
    u32 uv23;
    s32 count;
    s32 i;

    header = (s32 *)((u8 *)header + header[0] / 4 * 4);
    header += EFFECT_QUAD_LIST_HEADER;
    count = *header;
    header++;
    delta <<= 8;
    for (i = 0; i < count; i++) {
        e = (EffectEmitGouraudTexQuad *)header;
        ch[0] = e->uv0;
        keep[0] = ch[0] & EFFECT_UV_KEEP;
        ch[0] &= EFFECT_UV_V;
        ch[1] = e->uv1;
        keep[1] = ch[1] & EFFECT_UV_KEEP;
        ch[1] &= EFFECT_UV_V;
        uv23 = e->uv23;
        ch[3] = uv23;
        ch[2] = uv23;
        keep[2] = uv23 & EFFECT_UV_KEEP23;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] >> 16) & EFFECT_UV_V;
        ch[0] += delta;
        ch[1] += delta;
        ch[2] += delta;
        ch[3] += delta;
        if (ch[0] > EFFECT_UV_V || ch[1] > EFFECT_UV_V || ch[2] > EFFECT_UV_V || ch[3] > EFFECT_UV_V) {
            ch[0] -= EFFECT_UV_WRAP;
            ch[1] -= EFFECT_UV_WRAP;
            ch[2] -= EFFECT_UV_WRAP;
            ch[3] -= EFFECT_UV_WRAP;
        }
        ch[0] &= EFFECT_UV_V;
        ch[1] &= EFFECT_UV_V;
        ch[2] &= EFFECT_UV_V;
        ch[3] = (ch[3] & EFFECT_UV_V) << 16;
        e->uv0 = keep[0] | ch[0];
        e->uv1 = keep[1] | ch[1];
        e->uv23 = keep[2] | ch[2] | ch[3];
        header += sizeof(EffectEmitGouraudTexQuad) / sizeof(s32);
    }
}

/** @brief Build this effect's primitive and link it into the battle display list. */
static void func_801A4F44(EffectEntity *entity) {
    s16 count;
    BattleSpritePrim *prim;

    if (entity->flags & EFFECT_FLAG_DONE) {
        return;
    }
    prim = func_800B3698(sizeof(BattleSpritePrim));
    func_800C96E4(&entity->pos, ONE, entity->unk054);
    prim->anim = entity->unk04C;
    count = entity->unk050;
    prim->flags = 0;
    prim->frame = count;
    D_801D3EB8 = func_800C9E10(prim, D_800FA5E8->ot, 2, D_801D3EB8);
    func_800B36B8(sizeof(BattleSpritePrim));
}

/** @brief Draw @p anim upright at the entity's position, tinted with @p colour. */
static void func_801A4FE0(EffectEntity *entity, BattleSpriteAnim *anim,
                          CVECTOR *colour) {
    MATRIX m;
    BattleSpritePrim *prim;

    if (entity->flags & EFFECT_FLAG_DONE) {
        return;
    }
    func_801A0568(&m);
    func_801A05A0(&m, 0x400);
    m.t[0] = entity->pos.vx;
    m.t[1] = 0;
    m.t[2] = entity->pos.vz;
    CompMatrix(&D_800F02C8, &m, &m);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    prim = func_800B3698(sizeof(BattleSpritePrim));
    prim->anim = anim;
    prim->frame = entity->unk050;
    prim->flags = BATTLE_SPRITE_FLAG_COLOUR;
    prim->colour = *colour;
    D_801D3EB8 = func_800C9E10(prim, D_800FA5E8->ot, 2, D_801D3EB8);
    func_800B36B8(sizeof(BattleSpritePrim));
}

/** @brief As @ref func_801A4FE0, rolled by @p angle and scaled by @p scale. */
static void func_801A50D4(EffectEntity *entity, BattleSpriteAnim *anim,
                          CVECTOR *colour, s16 angle, s16 scale) {
    MATRIX m;
    VECTOR scaleVec;
    BattleSpritePrim *prim;

    if (entity->flags & EFFECT_FLAG_DONE) {
        return;
    }
    scaleVec.vz = scale;
    scaleVec.vy = scale;
    scaleVec.vx = scale;
    func_801A0568(&m);
    func_801A0730(&m, angle);
    func_801A05A0(&m, 0x400);
    ScaleMatrix(&m, &scaleVec);
    m.t[0] = entity->pos.vx;
    m.t[1] = 0;
    m.t[2] = entity->pos.vz;
    CompMatrix(&D_800F02C8, &m, &m);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    prim = func_800B3698(sizeof(BattleSpritePrim));
    prim->anim = anim;
    prim->frame = entity->unk050;
    prim->flags = BATTLE_SPRITE_FLAG_COLOUR;
    prim->colour = *colour;
    D_801D3EB8 = func_800C9E10(prim, D_800FA5E8->ot, 2, D_801D3EB8);
    func_800B36B8(sizeof(BattleSpritePrim));
}

/** @brief Advance the script's counter, clamping at its limit. */
static s32 func_801A5204(EffectEntity *entity) {
    entity->unk050++;
    if (entity->unk052 < entity->unk050) {
        entity->unk050 = entity->unk052;
        entity->flags |= EFFECT_FLAG_DONE;
        return 1;
    }
    return 0;
}

/** @brief Advance the animation frame, wrapping at its limit. */
static void func_801A524C(EffectEntity *entity) {
    entity->unk050++;
    if (entity->unk052 < entity->unk050) {
        entity->unk050 = 0;
    }
}

/** @brief Opcode handler: start a rise with a randomised speed. */
static void func_801A527C(EffectEntity *entity) {
    entity->unk04C = &D_801A8820;
    entity->unk052 = 0xA;
    entity->unk054 = -0x400;
    entity->unk05A = (rand() & 7) + 4;
    entity->pc++;
}

/** @brief Opcode handler: rise by the current speed until the count runs out. */
static void func_801A52D8(EffectEntity *entity) {
    entity->pos.vy += entity->unk05A;
    if (func_801A5204(entity) != 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A5334(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A533C(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A527C, func_801A52D8, func_801A5334 };

    handlers[entity->pc](entity);
    if (entity->pos.vy > 0) {
        entity->flags |= EFFECT_FLAG_STOP | EFFECT_FLAG_DONE;
    }
    func_801A4F44(entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: pick one of two table pairs at random and start a fall. */
static void func_801A5404(EffectEntity *entity) {
    if (rand() & 1) {
        entity->unk04C = &D_801A84E8;
        entity->unk070 = &D_801A8AF8;
    } else {
        entity->unk04C = &D_801A894C;
        entity->unk070 = &D_801A8CA4;
    }
    entity->unk07A = rand() & 0xFFF;
    entity->unk052 = 0xF;
    entity->unk054 = -0x200;
    entity->pc++;
}

/** @brief Opcode handler: hold still until the count runs out. */
static void func_801A548C(EffectEntity *entity) {
    if (func_801A5204(entity) != 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A54D4(EffectEntity *entity) {
}

/** @brief Script dispatcher: the falling-mote script, drawn spun and tinted. */
static s32 func_801A54DC(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A5404, func_801A548C, func_801A54D4 };
    CVECTOR colour;

    handlers[entity->pc](entity);
    func_801A4F44(entity);
    entity->unk07A = (entity->unk07A + 0x40) & 0xFFF;
    colour.b = 0x30;
    colour.g = 0x30;
    colour.r = 0x30;
    func_801A50D4(entity, entity->unk070, &colour, entity->unk07A, 0x2000);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: start a fast rise that gravity pulls back. */
static void func_801A55C0(EffectEntity *entity) {
    entity->unk04C = &D_801A8820;
    entity->unk050 = 0;
    entity->unk052 = 0xA;
    entity->unk05A = (rand() & 7) + 0x20;
    ((SVECTOR *)entity->unk060)->vy = -3;
    entity->pc++;
}

/** @brief Opcode handler: apply gravity, then rise until the count runs out. */
static void func_801A5620(EffectEntity *entity) {
    entity->unk05A += ((SVECTOR *)entity->unk060)->vy;
    if (entity->unk05A < 2) {
        entity->unk05A = 2;
    }
    entity->pos.vy += entity->unk05A;
    if (func_801A5204(entity) != 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A56A8(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A56B0(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A55C0, func_801A5620, func_801A56A8 };

    handlers[entity->pc](entity);
    if (entity->pos.vy > 0) {
        entity->flags |= EFFECT_FLAG_STOP | EFFECT_FLAG_DONE;
    }
    func_801A4F44(entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: keep the start point and start a randomised arc. */
static void func_801A5778(EffectEntity *entity) {
    entity->unk078 = rand() & 0xFFF;
    *(SVECTOR *)&entity->unk068 = entity->pos;
    entity->unk04C = &D_801A8708;
    entity->unk052 = 1;
    entity->unk05A = (rand() & 0x3F) - 0x30;
    entity->unk07C = (rand() & 0x1F) + 0x50;
    entity->unk07E = -entity->unk07C / 16;
    entity->pc++;
}

/** @brief Opcode handler: switch to the burst table once the frame count passes 16. */
static void func_801A581C(EffectEntity *entity) {
    func_801A524C(entity);
    if (entity->unk024 >= 0x10) {
        entity->unk04C = &D_801A875C;
        entity->unk052 = 6;
        entity->unk050 = 0;
        entity->flags |= EFFECT_FLAG_UNK10;
        entity->pc++;
    }
}

/** @brief Opcode handler: hold still until the count runs out. */
static void func_801A5884(EffectEntity *entity) {
    if (func_801A5204(entity) != 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A58CC(EffectEntity *entity) {
}

/** @brief Script dispatcher: an arc under gravity that trails motes behind it. */
static s32 func_801A58D4(EffectEntity *entity) {
    EffectHandler handlers[4] = { func_801A5778, func_801A581C, func_801A5884,
                                  func_801A58CC };
    EffectEntity *spark;
    CVECTOR colour;

    handlers[entity->pc](entity);
    entity->unk07C += entity->unk07E;
    if (entity->unk07C < 0) {
        entity->unk07C = 0;
    }
    entity->unk068 += rsin(entity->unk040.vy) * entity->unk07C / 4096;
    entity->unk06C += rcos(entity->unk040.vy) * entity->unk07C / 4096;
    entity->pos.vx = entity->unk068;
    entity->pos.vz = entity->unk06C;
    entity->unk05A += 0xA;
    entity->pos.vy += entity->unk05A;
    if (!(entity->flags & EFFECT_FLAG_UNK10) && (entity->unk024 & 1)) {
        spark = func_801A21C0(&D_801E74F4, func_801A56B0, 0x84, entity);
        func_801A0A64(spark, 0x80);
        spark->pos.vy = entity->pos.vy;
        ((VECTOR *)&spark->unk030)->vx = ONE;
        ((VECTOR *)&spark->unk030)->vy = ONE;
        ((VECTOR *)&spark->unk030)->vz = ONE;
    }
    if (entity->pos.vy > 0) {
        entity->flags |= EFFECT_FLAG_STOP | EFFECT_FLAG_DONE;
    }
    func_801A4F44(entity);
    colour.b = 0x50;
    colour.g = 0x50;
    colour.r = 0x50;
    func_801A4FE0(entity, entity->unk04C, &colour);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: ring eight sparks around the model's first point. */
static void func_801A5AE8(EffectEntity *entity) {
    EffectEntity *spark;
    s32 angle;
    s32 i;

    func_801A2480(entity, &entity->pos);
    entity->unk04C = &D_801A8694;
    entity->unk052 = 3;
    entity->pos.vy -= 0x600;
    angle = rand() & 0xFFF;
    for (i = 0; i < 8; i++) {
        spark = func_801A21C0(&D_801E74F4, func_801A58D4, 0x84, entity);
        ((VECTOR *)&spark->unk030)->vx = ONE;
        ((VECTOR *)&spark->unk030)->vy = ONE;
        ((VECTOR *)&spark->unk030)->vz = ONE;
        spark->unk040.vy = angle + (rand() & 0x7F);
        angle = (angle + 0x200) & 0xFFF;
    }
    entity->pc++;
}

/** @brief Opcode handler: hold still until the count runs out. */
static void func_801A5BD4(EffectEntity *entity) {
    if (func_801A5204(entity) != 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A5C1C(EffectEntity *entity) {
}

/** @brief Script dispatcher: the rising-mote script, drawn tinted. */
static s32 func_801A5C24(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A5AE8, func_801A5BD4, func_801A5C1C };
    CVECTOR colour;

    handlers[entity->pc](entity);
    func_801A4F44(entity);
    colour.b = 0x20;
    colour.g = 0x20;
    colour.r = 0x20;
    func_801A4FE0(entity, entity->unk04C, &colour);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Fill @p count words at @p dst with @p value. */
static void func_801A5CEC(s32 *dst, s32 value, s32 count) {
    s32 i;

    for (i = count - 1; i != -1; i--) {
        *dst++ = value;
    }
}

/** @brief Build the rotation of @p angle about X in @p m. */
static void func_801A5D10(s16 angle, MATRIX *m) {
    s32 *p = (s32 *)m;
    s32 i;
    s16 sin;
    s16 cos;

    for (i = 5; i != 0; i--) {
        *p++ = 0;
    }
    sin = rsin(angle);
    cos = rcos(angle);
    m->m[0][0] = ONE;
    m->m[1][1] = cos;
    m->m[2][1] = -sin;
    m->m[1][2] = sin;
    m->m[2][2] = cos;
}

/** @brief Build the rotation of @p angle about Y in @p m. */
static void func_801A5D8C(s16 angle, MATRIX *m) {
    s32 *p = (s32 *)m;
    s32 i;
    s16 sin;
    s16 cos;

    for (i = 5; i != 0; i--) {
        *p++ = 0;
    }
    sin = rsin(angle);
    cos = rcos(angle);
    m->m[2][0] = sin;
    m->m[0][0] = cos;
    m->m[1][1] = ONE;
    m->m[0][2] = -sin;
    m->m[2][2] = cos;
}

/** @brief Build the rotation of @p angle about Z in @p m. */
static void func_801A5E08(s16 angle, MATRIX *m) {
    s32 *p = (s32 *)m;
    s32 i;
    s16 sin;
    s16 cos;

    for (i = 5; i != 0; i--) {
        *p++ = 0;
    }
    sin = rsin(angle);
    cos = rcos(angle);
    m->m[1][0] = -sin;
    m->m[0][0] = cos;
    m->m[1][1] = cos;
    m->m[0][1] = sin;
    m->m[2][2] = ONE;
}

/** @brief Build the Z, X then Y rotation of @p angles in @p out. */
static void func_801A5E84(SVECTOR *angles, MATRIX *out) {
    MATRIX rz;
    MATRIX rx;
    MATRIX ry;
    MATRIX zx;

    func_801A5E08(angles->vz, &rz);
    func_801A5D10(angles->vx, &rx);
    MulMatrix0(&rz, &rx, &zx);
    func_801A5D8C(angles->vy, &ry);
    MulMatrix0(&zx, &ry, out);
}

/**
 * @brief Clear @p size bytes of pose state at @p out and seed it from @p model.
 *
 * Every part of the model takes a slot sized by its flags; the three-word
 * scale slot starts at 1.0 rather than zero.
 */
static void func_801A5F14(EffectPoseModel *model, s32 *out, s32 size) {
    EffectPoseParts *parts;
    EffectPosePart *part;
    s32 *offset;
    s32 *slot;
    s32 count;
    s32 flags;
    s32 i;

    func_801A5CEC(out, 0, size >> 2);
    out[0] = (s32)model;
    slot = out + 2;
    parts = (EffectPoseParts *)((u8 *)model + model->partsOffset);
    count = parts->count;
    offset = parts->offsets;
    for (i = 0; i < count; i++) {
        part = (EffectPosePart *)((u8 *)parts + *offset++);
        flags = part->flags;
        if (flags & EFFECT_POSE_ACC0) {
            slot += 9;
        } else if (flags & EFFECT_POSE_VEL0) {
            slot += 6;
        } else if (flags & EFFECT_POSE_KEY0) {
            slot += 3;
        }
        if (flags & EFFECT_POSE_ACC1) {
            slot += 9;
        } else if (flags & EFFECT_POSE_VEL1) {
            slot += 6;
        } else if (flags & EFFECT_POSE_KEY1) {
            slot += 3;
        }
        if (flags & (EFFECT_POSE_SCALE_KEY | EFFECT_POSE_SCALE_VEL |
                     EFFECT_POSE_SCALE_ACC)) {
            slot[2] = ONE << 16;
            slot[1] = ONE << 16;
            slot[0] = ONE << 16;
        }
        if (flags & EFFECT_POSE_SCALE_ACC) {
            slot += 9;
        } else if (flags & EFFECT_POSE_SCALE_VEL) {
            slot += 6;
        } else if (flags & EFFECT_POSE_SCALE_KEY) {
            slot += 3;
        }
        if (flags & EFFECT_POSE_ACC3) {
            slot += 6;
        } else if (flags & EFFECT_POSE_VEL3) {
            slot += 4;
        } else if (flags & EFFECT_POSE_KEY3) {
            slot += 2;
        }
        if (flags & EFFECT_POSE_DEPTH) {
            slot += 3;
        }
        if (flags & EFFECT_POSE_ANIM) {
            slot += 4;
        }
    }
}

/**
 * @brief Look @p key up in a table of 12-byte entries and copy its three
 *        halfwords out.
 *
 * The entries are ordered by key and closed by -1; nothing is written when the
 * key is missing.
 */
static void func_801A6094(s32 key, s32 *table, s16 *out) {
    s32 k = *table;

    while (k < key) {
        if (-1 == k) {
            return;
        }
        table += 3;
        k = *table;
    }
    if (k == key) {
        table++;
        *out++ = ((s16 *)table)[0];
        *out++ = ((s16 *)table)[1];
        *out = ((s16 *)table)[2];
    }
}

/** @brief As @ref func_801A6094, for 16-byte entries holding three words. */
static void func_801A60FC(s32 key, s32 *table, s32 *out) {
    s32 k = *table;

    while (k < key) {
        if (-1 == k) {
            return;
        }
        table += 4;
        k = *table;
    }
    if (k == key) {
        table++;
        *out++ = *table++;
        *out = table[0];
        out[1] = table[1];
    }
}

/**
 * @brief Step one channel of a pose and hand its current value to @p out.
 *
 * @param flags  Bit 0 keyed, bit 1 velocity, bit 2 acceleration.
 * @param base   The keyed tables are at byte offsets from here.
 * @param cursorPtr Walks the channel's table offsets; advanced past the ones read.
 * @param slot   The channel's state; position, then velocity, then acceleration.
 * @param key    Frame the keyed tables are looked up at.
 * @param out    Receives the channel's three components.
 * @return The state past this channel.
 *
 * @note The gotos are the shape the original had: each arm falls into the next
 *       one's body, so the move and emit blocks exist once. Written as an
 *       if/else-if chain the compiler copies them instead (143 instructions
 *       against 124); written as three independent tests it reaches the emit
 *       block through a mask of 6 and 7 rather than 2 and 1.
 */
static s32 *func_801A6164(s32 flags, u8 *base, s32 **cursorPtr, s32 *slot, s32 key,
                          s16 *out) {
    s32 *cursor = *cursorPtr;
    s32 *vel;
    s32 *acc;
    s16 *p;
    s32 v;

    if (flags & EFFECT_POSE_ACC0) {
        vel = slot + 3;
        acc = slot + 6;
        vel[0] += acc[0];
        vel[1] += acc[1];
        vel[2] += acc[2];
        goto move;
    }
    if (flags & EFFECT_POSE_VEL0) {
        vel = slot + 3;
move:
        slot[0] += vel[0];
        slot[1] += vel[1];
        slot[2] += vel[2];
        p = (s16 *)slot;
        goto emit;
    }
    if (flags & EFFECT_POSE_KEY0) {
        p = (s16 *)slot;
emit:
        v = p[1];
        out[0] = v;
        v = p[3];
        out[1] = v;
        v = p[5];
        out[2] = v;
    }
    if (flags & EFFECT_POSE_KEY0) {
        func_801A60FC(key, (s32 *)(base + *cursor++), slot);
        v = ((s16 *)slot)[1];
        slot++;
        out[0] = v;
        v = ((s16 *)slot)[1];
        slot++;
        out[1] = v;
        v = ((s16 *)slot)[1];
        slot++;
        out[2] = v;
    } else if (flags & (EFFECT_POSE_VEL0 | EFFECT_POSE_ACC0)) {
        slot += 3;
    }
    if (flags & EFFECT_POSE_VEL0) {
        func_801A60FC(key, (s32 *)(base + *cursor++), slot);
        slot += 3;
    } else if (flags & EFFECT_POSE_ACC0) {
        slot += 3;
    }
    if (flags & EFFECT_POSE_ACC0) {
        func_801A60FC(key, (s32 *)(base + *cursor++), slot);
        slot += 3;
    }
    *cursorPtr = cursor;
    return slot;
}

/**
 * @brief Walk one pose script and hand every part it poses to @p fn.
 *
 * The script opens with a part count and a frame count, then one byte offset
 * per part. A part entry names the model part it drives and the channels it
 * carries, and is followed by one keyed table per channel the flags claim.
 * @p pose holds the model, the frame the script is on, and then the running
 * state of every channel of every part -- the layout @ref func_801A5FA4 sizes.
 * That state is one array of words, but a channel holds either words or pairs
 * of halfwords, which is what every cast of @c slot below is reading.
 *
 * A channel that carries an acceleration steps its velocity by it, and one
 * that carries a velocity steps its value; the keyed table is read after that,
 * and overwrites the state only on a frame it holds. Channels the flags leave
 * out keep whatever the zeroed pose gives them, scale excepted -- that starts
 * at 1.0.
 *
 * @param pose The model and the frame the script is on.
 * @param fn   Called once per part, with the pose the script left.
 * @param req  Draw state handed straight to @p fn.
 * @return The frames left after this one; 0 once the script has run out.
 *
 * @note The gotos are the shape the original had. The colour channel's three
 *       arms fall into each other -- acceleration into the velocity ramp, and
 *       that into the keyed arm's colour store -- so each body exists once;
 *       spelled as an if/else-if chain the compiler copies them instead, the
 *       same way it does in @ref func_801A6164. The two out of the table walks
 *       land on the code that keeps the stepped value: reaching it with a
 *       @c break costs an instruction, because the frame comparison the walk
 *       falls out to is then on the path.
 */
static s32 func_801A6354(s32 *pose, EffectPartFn fn, EffectDrawRequest *req) {
    EffectPartPose part;
    s32 *cursor;
    s32 *script;
    s32 *offsets;
    s32 *depthTab;
    u8 *entry;
    s32 *slot;
    u16 *vel;
    u16 *rgb;
    u16 frame;
    u16 *acc;
    s32 count;
    s32 frames;
    u32 flags;
    s32 depthKey;
    s32 i;
    s32 v;
    s32 *animTab;
    s32 animKey;
    s32 value;
    s32 limit;

    script = (s32 *)((u8 *)pose[0] + ((s32 *)pose[0])[1]);
    frames = script[1];
    if (pose[1] >= frames) {
        return 0;
    }
    offsets = script + 2;
    slot = pose + 2;
    count = script[0];
    for (i = 0; i < count; i++) {
        func_801A5CEC((s32 *)&part, 0, sizeof(part) / sizeof(s32));
        entry = (u8 *)script + *offsets++;
        part.index = i;
        part.scale[2] = ONE;
        part.scale[1] = ONE;
        part.scale[0] = ONE;
        cursor = (s32 *)entry;
        part.unk004 = *cursor++;
        part.mesh = (u8)part.unk004;
        flags = *cursor++;
        slot = func_801A6164(flags, entry, &cursor, slot, pose[1], part.pos);
        slot = func_801A6164(flags >> 3, entry, &cursor, slot, pose[1], part.rot);
        slot = func_801A6164(flags >> 6, entry, &cursor, slot, pose[1], part.scale);
        if (flags & EFFECT_POSE_ACC3) {
            vel = (u16 *)slot + 4;
            acc = (u16 *)slot + 8;
            vel[0] += acc[0];
            vel[1] += acc[1];
            vel[2] += acc[2];
            goto ramp;
        }
        if (flags & EFFECT_POSE_VEL3) {
            vel = (u16 *)slot + 4;
ramp:
            rgb = (u16 *)slot;
            v = rgb[0] + (s16)vel[0];
            if (v < 0) {
                v = 0;
            } else if (v > 0xFFFF) {
                v = 0xFFFF;
            }
            rgb[0] = v;
            v = rgb[1] + (s16)vel[1];
            if (v < 0) {
                v = 0;
            } else if (v > 0xFFFF) {
                v = 0xFFFF;
            }
            rgb[1] = v;
            v = rgb[2] + (s16)vel[2];
            if (v < 0) {
                v = 0;
            } else if (v > 0xFFFF) {
                v = 0xFFFF;
            }
            rgb[2] = v;
            goto emit;
        }
        if (flags & EFFECT_POSE_KEY3) {
            rgb = (u16 *)slot;
emit:
            part.colour.r = rgb[0] >> 8;
            part.colour.g = rgb[1] >> 8;
            part.colour.b = rgb[2] >> 8;
            part.colour.cd = 0;
        }
        if (flags & EFFECT_POSE_KEY3) {
            func_801A6094(pose[1], (s32 *)(entry + *cursor++), (s16 *)slot);
            part.colour.r = *(u16 *)slot >> 8;
            slot = (s32 *)((u16 *)slot + 1);
            part.colour.g = *(u16 *)slot >> 8;
            slot = (s32 *)((u16 *)slot + 1);
            part.colour.b = *(u16 *)slot >> 8;
            part.colour.cd = 0;
            slot++;
        } else if (flags & (EFFECT_POSE_VEL3 | EFFECT_POSE_ACC3)) {
            slot += 2;
        }
        if (flags & EFFECT_POSE_VEL3) {
            func_801A6094(pose[1], (s32 *)(entry + *cursor++), (s16 *)slot);
            slot += 2;
        } else if (flags & EFFECT_POSE_ACC3) {
            slot += 2;
        }
        if (flags & EFFECT_POSE_ACC3) {
            func_801A6094(pose[1], (s32 *)(entry + *cursor++), (s16 *)slot);
            slot += 2;
        }
        if (flags & EFFECT_POSE_DEPTH) {
            slot[1] += slot[2];
            slot[0] += slot[1];
            if (slot[0] < 0) {
                slot[0] = 0;
            } else {
                limit = EFFECT_POSE_RAMP_MAX;
                if (slot[0] > limit) {
                    slot[0] = limit;
                }
            }
            depthTab = (s32 *)(entry + *cursor++);
            depthKey = *depthTab;
            while (depthKey < pose[1]) {
                if (depthKey == -1) {
                    goto noDepthKey;
                }
                depthTab += 4;
                depthKey = *depthTab;
            }
            if (depthKey == pose[1]) {
                depthTab++;
                value = *depthTab++;
                *slot++ = value;
                part.depth = value >> 16;
                *slot++ = *depthTab;
                *slot++ = depthTab[1];
            } else {
noDepthKey:
                value = ((s16 *)slot)[1];
                part.depth = value;
                slot += 3;
            }
        }
        if (flags & EFFECT_POSE_ANIM) {
            slot[2] += slot[3];
            slot[1] += slot[2];
            if (slot[1] < 0) {
                slot[1] = 0;
            } else {
                limit = EFFECT_POSE_RAMP_MAX;
                if (slot[1] > limit) {
                    slot[1] = limit;
                }
            }
            animTab = (s32 *)(entry + *cursor++);
            animKey = *animTab;
            while (animKey < pose[1]) {
                if (animKey == -1) {
                    goto noAnimKey;
                }
                animTab += 5;
                animKey = *animTab;
            }
            if (animKey == pose[1]) {
                animTab++;
                frame = *(u16 *)animTab;
                *(u16 *)slot = frame;
                part.frameA = frame;
                slot = (s32 *)((u16 *)slot + 1);
                animTab = (s32 *)((u16 *)animTab + 1);
                frame = *(u16 *)animTab;
                *(u16 *)slot = frame;
                part.frameB = frame;
                slot = (s32 *)((u16 *)slot + 1);
                animTab = (s32 *)((u16 *)animTab + 1);
                value = *animTab++;
                *slot++ = value;
                part.weight = value >> 16;
                *slot++ = *animTab;
                *slot++ = animTab[1];
            } else {
noAnimKey:
                part.frameA = *(u16 *)slot;
                slot = (s32 *)((u16 *)slot + 1);
                part.frameB = *(u16 *)slot;
                slot = (s32 *)((u16 *)slot + 1);
                value = ((s16 *)slot)[1];
                part.weight = value;
                slot += 3;
            }
        }
        fn(pose, &part, req);
    }
    pose[1]++;
    return frames - pose[1];
}

/**
 * @brief Interpolate every point of @p anim between two of its frames.
 *
 * @param frameA First frame; 0 selects the rest pose.
 * @param frameB Second frame.
 * @param t      Weight of @p frameB, 0x1000 being all of it.
 * @param out    Receives @c anim->count points.
 */
static void func_801A6910(EffectVertexAnim *anim, s32 frameA, s32 frameB, s32 t,
                          SVECTOR *out) {
    SVECTOR *a;
    SVECTOR *b;
    s32 n;
    s32 s = ONE - t;

    if (frameA == 0) {
        a = anim->frames;
    } else {
        a = &anim->frames[frameA * anim->count];
    }
    if (frameB == 0) {
        b = anim->frames;
    } else {
        b = &anim->frames[frameB * anim->count];
    }
    n = anim->count;
    while (--n != -1) {
        gte_lddp(s);
        gte_ldsv(a);
        gte_gpf1();
        gte_lddp(t);
        gte_ldsv(b);
        gte_gpl1();
        gte_stsv(out);
        out++;
        a++;
        b++;
    }
}

/*
 * battle.bin's own header spells this with its view of the 0x58-byte packet
 * and keeps the list head in a word; the overlay passes its own view and a
 * pointer, so the prototype it compiles against stays file-local.
 */
extern void *func_800CBC68(EffectPrimBuild *prim, u32 *ot, s32 mode, void *head);

/**
 * @brief Build and submit one posed part's primitive packet.
 *
 * Skips the part outright when it has no scale, or when it is fully faded and
 * carries no colour. Otherwise it takes a packet off the scratchpad, points it
 * at the part's vertex animation and at the frame the pose asks for -- one of
 * the two keyed frames, or the interpolated buffer when the pose sits between
 * them -- builds the part's matrix, and hands the packet to the battle
 * particle system.
 *
 * @param pose The pose script and its current frame.
 * @param part The part as the pose walk left it.
 * @param req  Draw state: the parent matrix, the per-part colours and flags.
 */
static void func_801A6A00(s32 *pose, EffectPartPose *part,
                          EffectDrawRequest *req) {
    EffectPrimBuild *prim;
    EffectVertexAnim *anim;
    MATRIX m;
    VECTOR scale;
    u32 partFlags;
    s32 depth;
    void *head;

    /* The two scale halves and the four colour bytes are tested a word at a
       time; no single field type spells that. */
    if ((*(u32 *)&part->scale[0] | part->scale[2]) == 0) {
        return;
    }
    if (part->depth >= ONE && *(u32 *)&part->colour == 0) {
        return;
    }
    prim = func_800B3698(0x58);
    anim = (EffectVertexAnim *)((u8 *)pose[0] + ((s32 *)pose[0])[part->mesh + 2]);
    prim->stream = (s32 *)anim;
    if (part->frameA == part->frameB) {
        if (part->frameB == 0) {
            anim = (EffectVertexAnim *)anim->frames;
        } else {
            anim = (EffectVertexAnim *)&anim->frames[part->frameB * anim->count];
        }
        prim->verts = (u32 *)anim;
    } else if (part->weight == 0) {
        if (part->frameA == 0) {
            anim = (EffectVertexAnim *)anim->frames;
        } else {
            anim = (EffectVertexAnim *)&anim->frames[part->frameA * anim->count];
        }
        prim->verts = (u32 *)anim;
    } else if (part->weight == ONE) {
        if (part->frameB == 0) {
            anim = (EffectVertexAnim *)anim->frames;
        } else {
            anim = (EffectVertexAnim *)&anim->frames[part->frameB * anim->count];
        }
        prim->verts = (u32 *)anim;
    } else {
        func_801A6910((EffectVertexAnim *)prim->stream, part->frameA, part->frameB,
                      part->weight, req->unk08);
        prim->verts = (u32 *)req->unk08;
    }
    RotMatrixYXZ((SVECTOR *)part->rot, &m);
    m.t[0] = part->pos[0];
    m.t[1] = part->pos[1];
    m.t[2] = part->pos[2];
    partFlags = req->colours[part->index];
    if (partFlags & EFFECT_PART_WORLD_MATRIX) {
        ApplyMatrixLV(req->m, (VECTOR *)&m.t[0], (VECTOR *)&m.t[0]);
        m.t[0] += req->m->t[0];
        m.t[1] += req->m->t[1];
        m.t[2] += req->m->t[2];
    } else {
        CompMatrix(req->m, &m, &m);
    }
    if (*(u32 *)&part->scale[0] != ((ONE << 16) | ONE) || part->scale[2] != ONE) {
        scale.vx = part->scale[0];
        scale.vy = part->scale[1];
        scale.vz = part->scale[2];
        ScaleMatrix(&m, &scale);
    }
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    prim->flags = (u16)req->colours[part->index] | EFFECT_EMIT_UNK2000;
    depth = part->depth;
    prim->depth = depth;
    if (req->colours[part->index] & EFFECT_PART_DEPTH_RAMP) {
        prim->depth = depth + (ONE - depth) * req->unk010 / 4096;
    }
    if (prim->depth != 0) {
        prim->flags |= EFFECT_EMIT_DEPTH_CUE | EFFECT_EMIT_G_DEPTH_CUE;
        *(u32 *)&prim->r = *(u32 *)&part->colour;
    }
    if (req->colours[part->index] & EFFECT_PART_SCROLL_UV) {
        func_801A4D9C(prim->stream, ((s16 *)req->unk00C)[part->index]);
    }
    head = D_801D3EB8;
    D_801D3EB8 = func_800CBC68(prim, D_800FA5E8->ot, 2, head);
    func_800B36B8(0x58);
}

/** @brief Opcode handler: load the 0x34-point aim set. */
static void func_801A6D68(EffectEntity *entity) {
    func_801A5F14(&D_801D20B8, (s32 *)entity->unk060, 0x34);
    entity->pc++;
}

/** @brief Opcode handler: pose the model at the entity and draw it. */
static void func_801A6DB0(EffectEntity *entity) {
    EffectDrawRequest req;
    MATRIX m;
    u32 colours[1];
    s32 i;

    for (i = 0; i <= 0; i++) {
        colours[i] = 0x30;
    }
    func_801A24B0(entity, &entity->pos);
    RotMatrixYXZ((SVECTOR *)&entity->unk050, &m);
    req.m = &m;
    m.t[0] = entity->pos.vx;
    m.t[1] = entity->pos.vy;
    m.t[2] = entity->pos.vz;
    ScaleMatrix(&m, (VECTOR *)&entity->unk030);
    CompMatrix(&D_800F02C8, &m, &m);
    req.colours = colours;
    req.unk08 = &D_801D3EA4;
    if (func_801A6354((s32 *)entity->unk060, func_801A6A00, &req) == 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A6E9C(EffectEntity *entity) {
}

/** @brief Script dispatcher: the orbiting script, whose two angles wrap. */
static s32 func_801A6EA4(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A6D68, func_801A6DB0, func_801A6E9C };

    handlers[entity->pc](entity);
    entity->unk050 += entity->unk058;
    entity->unk054 += entity->unk05C;
    entity->unk024++;
    entity->unk050 &= 0xFFF;
    entity->unk054 &= 0xFFF;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: reset the scale and load the 0x64-point aim set. */
static void func_801A6F7C(EffectEntity *entity) {
    ((VECTOR *)&entity->unk030)->vx = ONE;
    ((VECTOR *)&entity->unk030)->vy = ONE;
    ((VECTOR *)&entity->unk030)->vz = ONE;
    func_801A5F14(&D_801D238C, (s32 *)entity->unk060, 0x64);
    entity->pc++;
}

/** @brief Opcode handler: draw the model upright in the mode @c unk0C4 names. */
static void func_801A6FD4(EffectEntity *entity) {
    MATRIX m;
    EffectDrawRequest req;
    u32 colours[1];
    s32 i;

    for (i = 0; i <= 0; i++) {
        colours[i] = 0x40000030;
    }
    req.unk010 = entity->unk0C4;
    func_801A2480(entity, &entity->pos);
    func_801A0568(&m);
    req.m = &m;
    m.t[0] = entity->pos.vx;
    m.t[1] = entity->pos.vy;
    m.t[2] = entity->pos.vz;
    ScaleMatrix(&m, (VECTOR *)&entity->unk030);
    CompMatrix(&D_800F02C8, &m, &m);
    req.colours = colours;
    req.unk08 = &D_801D3EA4;
    if (func_801A6354((s32 *)entity->unk060, func_801A6A00, &req) == 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A70C8(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A70D0(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A6F7C, func_801A6FD4, func_801A70C8 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: clear the screen tint. */
static void func_801A7170(EffectEntity *entity) {
    BattleTint *tint = D_800EF738;
    s32 i;

    entity->pos.vx = 0;
    for (i = 0; i < 4; i++) {
        tint->level = 0;
        tint->b = 0;
        tint->g = 0;
        tint->r = 0;
        tint++;
    }
    entity->pc++;
}

/** @brief Opcode handler: fade the screen tint up to full. */
static void func_801A71B8(EffectEntity *entity) {
    BattleTint *tint = D_800EF738;
    s32 i;

    entity->pos.vx += 0xC0;
    if (entity->pos.vx >= 0x600) {
        entity->pos.vx = 0x600;
        entity->pc++;
    }
    for (i = 3; i >= 0; i--) {
        tint->level = entity->pos.vx;
        tint++;
    }
}

/** @brief Opcode handler: drop back to the ground once the count passes 20. */
static void func_801A7214(EffectEntity *entity) {
    if (entity->unk024 >= 0x14) {
        entity->pos.vy = 0;
        entity->pc++;
    }
}

/** @brief Opcode handler: fade the screen tint back out, then stop. */
static void func_801A7240(EffectEntity *entity) {
    BattleTint *tint = D_800EF738;
    s32 i;

    entity->pos.vx -= 0xC0;
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
static void func_801A729C(EffectEntity *entity) {
}

/** @brief Script dispatcher: the five-step screen-tint script. */
static s32 func_801A72A4(EffectEntity *entity) {
    EffectHandler handlers[5] = { func_801A7170, func_801A71B8, func_801A7214,
                                  func_801A7240, func_801A729C };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: move the entity onto the model's second point. */
static void func_801A7354(EffectEntity *entity) {
    func_801A24B0(entity, &entity->pos);
    entity->pc++;
}

/** @brief Opcode handler: spawn the frame's share of orbiting children. */
static void func_801A7390(EffectEntity *entity) {
    EffectEntity *child;
    s32 i;

    for (i = 0; i < D_801D3DF0[entity->unk024]; i++) {
        child = func_801A21C0(&D_801D7314, func_801A6EA4, 0xC8, entity);
        ((VECTOR *)&child->unk030)->vx = ONE;
        ((VECTOR *)&child->unk030)->vy = ONE;
        ((VECTOR *)&child->unk030)->vz = ONE;
        child->unk050 = rand() & 0xFFF;
        child->unk054 = rand() & 0xFFF;
        child->unk058 = (rand() & 0x3F) + 0x20;
        child->unk05C = (rand() & 0x3F) + 0x20;
        if (rand() & 1) {
            child->unk058 = ONE - child->unk058;
        }
        if (rand() & 1) {
            child->unk05C = ONE - child->unk05C;
        }
    }
    if (entity->unk024 >= 0x12) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A74F8(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A7500(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A7354, func_801A7390, func_801A74F8 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: sit on the model's first point and aim at the ground. */
static void func_801A75A0(EffectEntity *entity) {
    s16 y;

    func_801A2480(entity, &entity->pos);
    y = func_801A2B10(entity);
    entity->pos.vy = y;
    entity->unk040.vy = (0x100 - y) / 18;
    entity->pc++;
}

/** @brief Opcode handler: rise, spawning the frame's share of falling motes. */
static void func_801A7614(EffectEntity *entity) {
    EffectEntity *spark;
    s32 i;

    entity->pos.vy += entity->unk040.vy;
    for (i = 0; i < D_801D3E2C[entity->unk024]; i++) {
        spark = func_801A21C0(&D_801E74F4, func_801A54DC, 0x84, entity);
        func_801A0A64(spark, 0x200);
        spark->pos.vy = entity->pos.vy;
        ((VECTOR *)&spark->unk030)->vx = ONE;
        ((VECTOR *)&spark->unk030)->vy = ONE;
        ((VECTOR *)&spark->unk030)->vz = ONE;
    }
    if (entity->unk024 >= 0x12) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A7728(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A7730(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A75A0, func_801A7614, func_801A7728 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief As @ref func_801A75A0, over twenty frames rather than eighteen. */
static void func_801A77D0(EffectEntity *entity) {
    s16 y;

    func_801A2480(entity, &entity->pos);
    y = func_801A2B10(entity);
    entity->pos.vy = y;
    entity->unk040.vy = (0x100 - y) / 20;
    entity->pc++;
}

/** @brief Opcode handler: rise, spawning the frame's share of rising motes. */
static void func_801A7844(EffectEntity *entity) {
    EffectEntity *spark;
    s32 i;

    entity->pos.vy += entity->unk040.vy;
    for (i = 0; i < D_801D3E68[entity->unk024]; i++) {
        spark = func_801A21C0(&D_801E74F4, func_801A533C, 0x84, entity);
        spark->pos.vy = entity->pos.vy - 0x180;
        func_801A0A64(spark, 0x200);
    }
    if (entity->unk024 >= 0x14) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A7940(EffectEntity *entity) {
}

/**
 * @brief Script dispatcher: run this frame's step, draw, and retire the entity.
 *
 * @return 2 once the script has stopped and its children have drained, 0 while
 *         it is still running.
 */
static s32 func_801A7948(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A77D0, func_801A7844, func_801A7940 };

    handlers[entity->pc](entity);
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: take the model over and set it up to be drawn dim. */
static void func_801A79E8(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];
    EffectRender *render = &D_801ED304;
    s16 y;

    func_801A2480(entity, &entity->pos);
    y = entity->pos.vy - 0x600;
    entity->pos.vy = y;
    entity->unk040.vy = (0x100 - y) / 18;
    func_801A1088(render, &D_801ED2F4, &D_801E7504, &D_801E7534,
                  &D_800EF2D0[entity->unk02D], entity->unk02D);
    render->r = 0x40;
    render->g = 0x40;
    render->b = 0x40;
    entity->flags |= EFFECT_FLAG_UNK08;
    slot->flags |= BATTLE_SLOT_FLAG_UNK04;
    entity->pc++;
}

/** @brief Opcode handler: fall at the armed speed, then release the model. */
static void func_801A7B1C(EffectEntity *entity) {
    BattleEffectSlot *slot;
    EffectModel *model;

    entity->pos.vy += entity->unk040.vy;
    if (entity->unk024 >= 0x12) {
        slot = &D_800EF2D0[entity->unk02D];
        model = entity->unk010;
        entity->flags &= ~EFFECT_FLAG_UNK08;
        slot->flags &= ~BATTLE_SLOT_FLAG_UNK04;
        model->unk063 = 0;
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A7BA0(EffectEntity *entity) {
}

/** @brief Script dispatcher: the model-taking script, redrawn each frame. */
static s32 func_801A7BA8(EffectEntity *entity) {
    EffectHandler handlers[3] = { func_801A79E8, func_801A7B1C, func_801A7BA0 };
    EffectRender *render;
    /* Occupies sp+0x30: the original reserved this slot and never read it, but
       dropping it moves every local below and the frame no longer matches. */
    SVECTOR unused;
    SVECTOR point;

    handlers[entity->pc](entity);
    if (entity->flags & EFFECT_FLAG_UNK08) {
        render = &D_801ED304;
        func_801A2480(entity, &point);
        entity->unk052 = func_801A0A34(&D_800F02C8);
        entity->pos.vx = point.vx + rsin(entity->unk052) / 4;
        entity->pos.vz = point.vz + rcos(entity->unk052) / 4;
        render->unk0B4.vy = entity->pos.vy;
        func_801A1144(render, &entity->pos, 0x280, 0x1C0, 0x140, 0xF4, 0x80, 0x40);
        func_801A205C(render);
    }
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: start the five child scripts the effect runs. */
static void func_801A7D18(EffectEntity *entity) {
    EffectEntity *child;

    func_801A21C0(&D_801E74F4, func_801A5C24, 0x84, entity);
    child = func_801A21C0(&D_801D7314, func_801A70D0, 0xC8, entity);
    child->unk0C4 = 0;
    func_801A21C0(&D_801D7314, func_801A7500, 0xC8, entity);
    func_801A21C0(&D_801D7314, func_801A7730, 0xC8, entity);
    func_801A21C0(&D_801D7314, func_801A72A4, 0xC8, entity);
    entity->pc++;
}

/** @brief Opcode handler: start the two child scripts this step drives. */
static void func_801A7DDC(EffectEntity *entity) {
    EffectEntity *child = func_801A21C0(&D_801D7314, func_801A70D0, 0xC8, entity);

    child->unk0C4 = 0x800;
    func_801A21C0(&D_801D7314, func_801A7BA8, 0xC8, entity);
    entity->pc++;
}

/** @brief Opcode handler: start a child script in mode @c 0xC00. */
static void func_801A7E58(EffectEntity *entity) {
    EffectEntity *child = func_801A21C0(&D_801D7314, func_801A70D0, 0xC8, entity);

    child->unk0C4 = 0xC00;
    entity->pc++;
}

/** @brief Opcode handler: start a child script in mode @c 0xE00. */
static void func_801A7EB0(EffectEntity *entity) {
    EffectEntity *child = func_801A21C0(&D_801D7314, func_801A70D0, 0xC8, entity);

    child->unk0C4 = 0xE00;
    entity->pc++;
}

/** @brief Opcode handler: start the third child script once the count passes 12. */
static void func_801A7F08(EffectEntity *entity) {
    if (entity->unk024 >= 0xC) {
        func_801A21C0(&D_801D7314, func_801A7948, 0xC8, entity);
        entity->pc++;
    }
}

/** @brief Opcode handler: hand the animation's part list to the battle renderer. */
static void func_801A7F68(EffectEntity *entity) {
    if (entity->unk024 >= 0x1F) {
        func_800BFE1C(entity->animSet->anims[entity->unk02A].parts);
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A7FE0(EffectEntity *entity) {
}

/** @brief Script dispatcher: the seven-step main script, which starts the sound. */
static s32 func_801A7FE8(EffectEntity *entity) {
    EffectHandler handlers[7] = { func_801A7D18, func_801A7DDC, func_801A7E58,
                                  func_801A7EB0, func_801A7F08, func_801A7F68,
                                  func_801A7FE0 };

    func_801A234C(entity);
    func_801A26DC(entity);
    handlers[entity->pc](entity);
    if (entity->unk024 == 0) {
        func_800C4764(D_801A83D0, 0, 0x80);
    }
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}

/** @brief Opcode handler: consume the opcode and do nothing else. */
static void func_801A80DC(EffectEntity *entity) {
    entity->pc++;
}

/** @brief Opcode handler: start a task running @ref func_801A7FE8. */
static void func_801A80F0(EffectEntity *entity) {
    entity->unk063 = 1;
    func_801A21C0(&D_801D4104, func_801A7FE8, 0x58, entity);
    entity->pc++;
}

/** @brief Opcode handler: loop back one step until the animation list runs out. */
static void func_801A8148(EffectEntity *entity) {
    if (entity->unk063 != 0) {
        return;
    }
    if (entity->unk02A < entity->unk058) {
        entity->unk02A++;
        entity->pc--;
    } else {
        entity->pc++;
    }
}

/** @brief Opcode handler: stop once @c unk05E reaches zero. */
static void func_801A8198(EffectEntity *entity) {
    if (entity->unk05E == 0) {
        entity->flags |= EFFECT_FLAG_STOP;
        entity->pc++;
    }
}

/** @brief Opcode handler: no-op. */
static void func_801A81C8(EffectEntity *entity) {
}

/**
 * @brief Root script dispatcher: swap the prim bank, run one step, then run
 *        every child pool.
 *
 * @return 2 once the script has stopped and its children have drained.
 */
static s32 func_801A81D0(EffectEntity *entity) {
    EffectHandler handlers[5] = { func_801A80DC, func_801A80F0, func_801A8148,
                                  func_801A8198, func_801A81C8 };

    if (entity->unk05C & 1) {
        D_801D3EB8 = D_801D3EBC;
    } else {
        D_801D3EB8 = D_801D3EC0;
    }
    func_801A2510(entity);
    handlers[entity->pc](entity);
    entity->unk05E = func_800B2B68(&D_801D4104);
    entity->unk05E += func_800B2B68(&D_801D7314);
    entity->unk05E += func_800B2B68(&D_801E74F4);
    entity->unk05C++;
    entity->unk024++;
    if (entity->flags & EFFECT_FLAG_STOP) {
        if (entity->wait != 0) {
            return 0;
        }
        func_801A2198(entity);
        return 2;
    }
    return 0;
}
