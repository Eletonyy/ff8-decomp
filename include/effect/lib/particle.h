#ifndef EFFECT_LIB_PARTICLE_H
#define EFFECT_LIB_PARTICLE_H

#include "effect.h"
#include "effect_params.h"

/**
 * @file particle.h
 * @brief The mote engine's records and state.
 */

/** @brief A packet's far colour: three bytes and the code byte beside them. */
typedef struct {
    /* 0x00 */ u8 r;
    /* 0x01 */ u8 g;
    /* 0x02 */ u8 b;
    /* 0x03 */ u8 code;
} EffectPrimColor; /* 0x04 */

/** @brief One step of the pose script the emitters read. */
typedef struct {
    /* 0x00 */ s32 offsetX;   /**< Offset added to the point, 16.16. */
    /* 0x04 */ s32 offsetY;
    /* 0x08 */ s32 offsetZ;
    /* 0x0C */ u8 pad00C[0x11 - 0xC];
    /* 0x11 */ u8 unk011;
    /* 0x12 */ u8 unk012;     /**< Frames an emitter allocated from this source lives. */
    /* 0x13 */ u8 unk013;  /**< Frame of the list's life the source fires on. */
    /* 0x14 */ u8 pad014[0x16 - 0x14];
    /* 0x16 */ u8 mode;       /**< 4 and 5 take the late step. */
    /* 0x17 */ u8 unk017;
    /* 0x18 */ u8 unk018;
    /* 0x19 */ u8 unk019;
    /* 0x1A */ u8 unk01A;
    /* 0x1B */ u8 unk01B;     /**< Set, the pose's Y is flattened every frame. */
    /* 0x1C */ u8 unk01C;
    /* 0x1D */ u8 unk01D;     /**< Which order the pose's three turns are applied in. */
    /* 0x1E */ u8 kind;       /**< Selects which of the four steps runs. */
    /* 0x1F */ u8 unk01F;
    /* 0x20 */ u8 unk020;
    /* 0x21 */ u8 unk021;     /**< Where a spawned node's own spin comes from. */
    /* 0x22 */ u8 unk022;     /**< Where the posed offsets are written back to. */
    /* 0x23 */ u8 pad023[0x25 - 0x23];
    /* 0x25 */ u8 anchor;     /**< Which of the trail's points the motes start at. */
    /* 0x26 */ u8 unk026;     /**< Set, the pose is turned before it is applied. */
    /* 0x27 */ u8 unk027;     /**< Which way the pose's offsets are built. */
    /* 0x28 */ u8 unk028;
    /* 0x29 */ u8 unk029;
    /* 0x2A */ u8 unk02A;
    /* 0x2B */ u8 unk02B;
    /* 0x2C */ u8 unk02C;
    /* 0x2D */ u8 unk02D;
    /* 0x2E */ u8 unk02E;
    /* 0x2F */ u8 unk02F;
    /* 0x30 */ u8 unk030;
    /* 0x31 */ u8 unk031;
    /* 0x32 */ u8 pad032;
    /* 0x33 */ u8 unk033;     /**< Set, the spawn speeds are taken as they come. */
    /* 0x34 */ u8 unk034;     /**< Which way a spawned node is aimed. */
    /* 0x35 */ u8 midpoint;   /**< Set, motes start midway along the node. */
    /* 0x36 */ u8 unk036;     /**< Splits a frame's work into chunks this big. */
    /* 0x37 */ u8 unk037;     /**< Set, the source fires once per strand point. */
    /* 0x038 */ s16 unk038;   /**< Added to the node's Z once it is placed. */
    /* 0x03A */ s16 unk03A;
    /* 0x03C */ SVECTOR unk03C;
    /* 0x044 */ SVECTOR unk044;
    /* 0x04C */ s32 unk04C;
    /* 0x050 */ s32 unk050;
    /* 0x054 */ s32 unk054;
    /* 0x058 */ s32 unk058;
    /* 0x05C */ s32 unk05C;
    /* 0x060 */ s32 unk060;
    /* 0x064 */ u16 unk064;   /**< Drag a spawned node keeps its motion with. */
    /* 0x066 */ u8 pad066[0x68 - 0x66];
    /* 0x068 */ VECTOR unk068;
    /* 0x078 */ VECTOR unk078;
    /* 0x088 */ VECTOR unk088;
    /* 0x098 */ VECTOR unk098;
    /* 0x0A8 */ VECTOR unk0A8;
    /* 0x0B8 */ VECTOR unk0B8;
    /*
     * 0x0C8 to 0x144 is one table per animated property, each indexed by the
     * frame the node is showing. They are stored as offsets from the pose
     * tables' own start; @ref func_801A8CE4 turns all thirty-two into
     * addresses the first time a script uses them.
     */
    /* 0x0C8 */ u8 *unk0C8;   /**< How many motes the source takes that frame. */
    /* 0x0CC */ u16 *unk0CC;
    /* 0x0D0 */ u16 *unk0D0;
    /* 0x0D4 */ u16 *unk0D4;
    /* 0x0D8 */ u16 *unk0D8;
    /* 0x0DC */ u16 *unk0DC;
    /* 0x0E0 */ u16 *unk0E0;
    /* 0x0E4 */ u16 *unk0E4;
    /* 0x0E8 */ u16 *unk0E8;
    /* 0x0EC */ u16 *unk0EC;
    /* 0x0F0 */ u8 *unk0F0;
    /* 0x0F4 */ u8 *unk0F4;
    /* 0x0F8 */ u8 *unk0F8;
    /* 0x0FC */ u16 *unk0FC;
    /* 0x100 */ u8 *unk100;
    /* 0x104 */ u8 *unk104;
    /* 0x108 */ u8 *unk108;
    /* 0x10C */ u8 *unk10C;
    /* 0x110 */ u16 *unk110;
    /* 0x114 */ u16 *unk114;
    /* 0x118 */ u16 *unk118;
    /* 0x11C */ u16 *unk11C;
    /* 0x120 */ u16 *unk120;
    /* 0x124 */ u16 *unk124;
    /* 0x128 */ s16 *unk128;
    /* 0x12C */ u8 *targets;  /**< One target point set per frame. */
    /* 0x130 */ u8 *framesA;  /**< First source point set per frame. */
    /* 0x134 */ u8 *framesB;  /**< Second source point set per frame. */
    /* 0x138 */ s16 *weights; /**< Weight of the second source, per frame. */
    /* 0x13C */ s16 *unk13C;
    /* 0x140 */ s16 *unk140;
    /* 0x144 */ s16 *unk144;
    /* 0x148 */ SVECTOR unk148;
    /* 0x150 */ SVECTOR unk150;
} EffectPoseStep; /* 0x158 */

/**
 * @brief The lighter of the two record kinds the draw list carries.
 *
 * Its bank packs these 0x6C apart, so it shares only the head of
 * @ref EffectParticle and is linked through the same two pointers.
 */
typedef struct EffectEmitter {
    /* 0x00 */ struct EffectParticle *prev;
    /* 0x04 */ struct EffectParticle *next;
    /* 0x08 */ u16 unk008;
    /* 0x0A */ u8 pad00A[0x2C - 0xA];
    /* 0x2C */ MATRIX mtx;   /**< Turns the step's offset into world space. */
    /* 0x4C */ VECTOR pos;   /**< Where the mote sits, 16.16. */
    /* 0x5C */ struct EffectParticle *owner;
    /* 0x60 */ s16 unk060;    /**< Frames this spark has been drawn for. */
    /* 0x62 */ s16 unk062;    /**< Zero until it has been set up once. */
    /* 0x64 */ s16 life;      /**< Counts down; the spark unlinks at zero. */
    /* 0x66 */ u8 pad066[0x68 - 0x66];
    /* 0x68 */ u8 kind;       /**< Selects the per-frame step below. */
    /* 0x69 */ u8 inUse;      /**< Cleared when the life runs out, freeing the slot. */
    /* 0x6A */ u8 unk06A;     /**< Which of the list's sources drives it. */
    /* 0x6B */ u8 unk06B;
} EffectEmitter; /* 0x6C */

/** @brief One drawn particle on the effect's list, linked both ways. */
typedef struct EffectParticle {
    /* 0x000 */ struct EffectParticle *prev;
    /* 0x004 */ struct EffectParticle *next;
    /* 0x008 */ s16 unk008;
    /* 0x00A */ u8 pad00A[0xC - 0xA];
    /* 0x00C */ MATRIX poses[2]; /**< One per copy; only two fit before the header. */
    /* 0x04C */ u8 pad04C[0x60 - 0x4C];
    /* 0x060 */ u16 unk060;    /**< Frames this node has been drawn for. */
    /* 0x062 */ s16 unk062;    /**< Zero until the node has been set up once. */
    /* 0x064 */ s16 life;      /**< Counts down; the node unlinks at zero. */
    /* 0x066 */ u8 pad066[0x68 - 0x66];
    /* 0x068 */ u8 kind;       /**< Selects the per-frame step below. */
    /* 0x069 */ u8 unk069;
    /* 0x06A */ u8 unk06A;     /**< Which of the list's sources drives the emitter. */
    /* 0x06B */ u8 pad06B[0x8C - 0x6B];
    /* 0x08C */ MATRIX orient; /**< Turn the particles hung off this emitter inherit. */
    /* 0x0AC */ MATRIX mtx;    /**< Pose the node's model is drawn with. */
    /* 0x0CC */ SVECTOR angle; /**< This frame's turn, wrapped to a whole circle. */
    /* 0x0D4 */ s16 scaleX;    /**< This frame's scale, before it is applied. */
    /* 0x0D6 */ s16 scaleY;
    /* 0x0D8 */ s16 scaleZ;
    /* 0x0DA */ u8 pad0DA[0xDC - 0xDA];
    /* 0x0DC */ VECTOR pos[4]; /**< Where each copy sits. */
    /* 0x11C */ VECTOR lastPos; /**< Where the first copy sat last frame. */
    /* 0x12C */ VECTOR offset[4]; /**< The turned offset each copy is drawn at. */
    /* 0x16C */ EffectPrimColor color;
    /* 0x170 */ void *stream;  /**< What the node draws -- a mesh stream or a
                                    sprite animation, by kind -- or NULL. */
    /* 0x174 */ s32 spin[4];   /**< How far each copy has spun about its pose. */
    /* 0x184 */ s32 spinRate[4];
    /* 0x194 */ SVECTOR angleBase; /**< The turn this frame's angle starts from. */
    /* 0x19C */ SVECTOR offsets[1]; /**< Where each copy past the first is placed. */
    /* 0x1A4 */ u8 pad1A4[0x1BC - 0x1A4];
    /* 0x1BC */ struct EffectEmitter *owner;
    /* 0x1C0 */ u8 pad1C0[0x1C8 - 0x1C0];
    /* 0x1C8 */ s16 unk1C8;   /**< Counts down; a step fires when it goes negative. */
    /* 0x1CA */ s16 unk1CA;   /**< Which frame of the pose the node is showing. */
    /* 0x1CC */ s16 unk1CC;   /**< Which phase of its life the node is in. */
    /* 0x1CE */ s16 unk1CE;   /**< Depth cue: zero leaves the node unshaded. */
    /* 0x1D0 */ u8 unk1D0;    /**< Frames the current step has held for. */
    /* 0x1D1 */ u8 unk1D1;    /**< How long it holds before advancing. */
    /* 0x1D2 */ u8 unk1D2;
    /* 0x1D3 */ u8 unk1D3;
    /* 0x1D4 */ u8 unk1D4;
    /* 0x1D5 */ u8 unk1D5;    /**< Repeats left before the hold gives up. */
    /* 0x1D6 */ u8 unk1D6;    /**< Which of the list's sources this node came from. */
    /* 0x1D7 */ u8 inUse;     /**< Cleared when the phase ends, freeing the slot. */
    /* 0x1D8 */ u8 copies;    /**< How many times the node draws its model. */
    /* 0x1D9 */ u8 unk1D9;
    /* 0x1DA */ s16 unk1DA;   /**< Drag: how much of each frame's motion is kept. */
    /* 0x1DC */ s32 unk1DC;   /**< Added to every copy's Y velocity each frame. */
    /* 0x1E0 */ VECTOR vel[4];
    /* 0x220 */ VECTOR accel[4];
    /* 0x260 */ VECTOR jerk[4];
} EffectParticle; /* 0x2A0 */

/** @brief A set of posed points: a two-word header, then @c count of them. */
typedef struct EffectPointSet {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 count;
    /* 0x08 */ SVECTOR points[1];
} EffectPointSet;

/**
 * @brief The tables a script is handed: sixteen sources and their point sets.
 *
 * @ref func_801A8CE4 fills the pointers in once and sets @c ready so a restart
 * does not do it again.
 */
typedef struct {
    /* 0x000 */ s32 ready;
    /* 0x004 */ EffectPoseStep *sources[16];
    /* 0x044 */ void *unk044[16];
    /* 0x084 */ EffectPointSet *targets[16];
    /* 0x0C4 */ EffectPointSet *frames[16];
} EffectPoseTables; /* 0x104 */

/** @brief The draw list the emitters walk, with its head and tail. */
typedef struct {
    /* 0x000 */ s32 spread;    /**< Distance between the two slots the effect spans. */
    /* 0x004 */ s32 parts[4];  /**< The model part each strand is built from. */
    /* 0x014 */ u16 count;
    /* 0x016 */ u16 live;      /**< Nodes still on the list. */
    /* 0x018 */ s16 age;       /**< Frames this list has been running. */
    /* 0x01A */ s16 span;      /**< How long it runs before it may finish. */
    /* 0x01C */ s16 strandLen; /**< How many points of each strand are used. */
    /* 0x01E */ s16 slot;      /**< Battle slot the effect is aimed at. */
    /* 0x020 */ s16 phase;     /**< Which stage of its life the list is in. */
    /* 0x022 */ u16 held;      /**< One bit per source: set, the source is held off. */
    /* 0x024 */ s16 unk024;
    /* 0x026 */ u8 pad026[0x2C - 0x26];
    /* 0x02C */ EffectParticle *head;
    /* 0x030 */ EffectParticle *tail;
    /* 0x034 */ u8 pad034[0x54 - 0x34];
    /* 0x054 */ VECTOR strands[5][4]; /**< The five strands the effect draws. */
    /* 0x194 */ VECTOR strandSeed;    /**< The point every strand starts from. */
    /* 0x1A4 */ VECTOR strandHead;    /**< The newest point, pushed on each frame. */
    /* 0x1B4 */ VECTOR trail[6];   /**< Where the head has been, newest first. */
    /* 0x214 */ s32 unk214;
    /* 0x218 */ s32 unk218;
    /* 0x21C */ s32 unk21C;
    /* 0x220 */ u8 pad220[0x224 - 0x220];
    /* 0x224 */ EffectPoseStep **sources;
    /* 0x228 */ void **unk228;
    /* 0x22C */ struct EffectPointSet **targets; /**< Point sets the poses write. */
    /* 0x230 */ struct EffectPointSet **frames;  /**< Point sets the poses read. */
} EffectDrawList; /* 0x234 */

/**
 * @name Script records
 *
 * @ref effectSpawnTask hands back as many bytes as the script asks for, with an
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

/** @} */

/**
 * @name Prim bank sizes
 *
 * A bank holds one record more than its ring's highest index, and the script
 * carves both out of the same arena behind the texture.
 * @{
 */
#define EFFECT_EMITTER_BANK_BYTES ((EFFECT_EMITTER_COUNT + 1) * sizeof(EffectEmitter))
#define EFFECT_PARTICLE_BANK_BYTES ((EFFECT_PARTICLE_COUNT + 1) * sizeof(EffectParticle))
/** @} */

/** @brief The one draw list this effect builds into. */
extern EffectDrawList *g_effectDrawList;
/** @brief Top of the scratchpad stack the draw packets are built on. */
extern u8 *g_effectStackTop;
/** @brief Where that stack started, kept so the frame can be unwound. */
extern MATRIX *g_effectStackBase;
/** @brief The two record banks the emitters write through. */
extern EffectEmitter *g_effectEmitterBank;
extern EffectParticle *g_effectParticleBank;
/** @brief Where the next take starts its search of each ring. */
extern s16 g_effectEmitterCursor;
extern s16 g_effectParticleCursor;
/** @brief Cleared with the banks; nothing reads them yet. */
extern s16 g_effectBankUnk90;
extern s16 g_effectBankUnk92;

#endif /* EFFECT_LIB_PARTICLE_H */
