#ifndef EFFECT_EFFECT_025_H
#define EFFECT_EFFECT_025_H

#include "effect.h"

/**
 * @brief Entry point of the effect_025 overlay.
 *
 * battle.bin calls this at the overlay's load address once the image is in
 * place; it builds the effect's root entity and its task pools.
 *
 * @param animSet Animation table the effect draws its models from.
 * @return The task pool the effect's scripts run out of.
 */
void *func_801A0000(EffectAnimSet *animSet);

/** @brief One animated part of a posed model; its flags size its pose slot. */
typedef struct {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 flags;
} EffectPosePart;

/** @brief The part list a posed model carries, each part at a byte offset. */
typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk004;
    /* 0x08 */ s32 offsets[1];
} EffectPoseParts;

/** @brief A posed model: a header naming the part list behind it. */
typedef struct {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 partsOffset;
} EffectPoseModel;

/**
 * @name Overlay data
 *
 * Sprite animation tables in the overlay image, handed to an entity through
 * @ref EffectEntity::unk04C.
 * @{
 */

/** @brief Table of offsets battle.bin resolves when the effect starts. */
extern u8 D_801A8E50;

/** @brief TIM uploaded to VRAM when the effect starts; also the first prim bank. */
extern u8 D_801A9698;

/** @brief The second prim bank, behind the first. */
extern u8 D_801BB698;

/** @brief AKAO sound sequence, played on the script's first frame. */
extern u8 D_801A83D0[];

extern BattleSpriteAnim D_801A84E8;
extern BattleSpriteAnim D_801A8694;
extern BattleSpriteAnim D_801A8708;
extern BattleSpriteAnim D_801A875C;
extern BattleSpriteAnim D_801A8820;
extern BattleSpriteAnim D_801A894C;
extern BattleSpriteAnim D_801A8AF8;
extern BattleSpriteAnim D_801A8CA4;

/** @} */

/**
 * @name Overlay bss
 *
 * Working storage past the end of the overlay image (@c 0x801D3EA4). Each task
 * pool's 0x10-byte header sits directly behind the entries it hands out.
 * @{
 */

/** @brief Root entity pool: 2 entries of 0x64. */
extern s32 D_801D3EC4;
extern s32 D_801D3F94;

/** @brief Script entity pool entries; the header is @ref D_801D4104. */
extern s32 D_801D3FA4;

/** @brief Child entity pool entries; the header is @ref D_801D7314. */
extern s32 D_801D4114;

/** @brief Spark pool entries; the header is @ref D_801E74F4. */
extern s32 D_801D7324;

/** @brief Texture the posed model is drawn with. */
extern s32 D_801D3EA4;

/** @brief Cursor this frame's prims are written through. */
extern void *D_801D3EB8;

/** @brief Draw-mode command word every emitted prim carries. */
extern u32 D_801EEA0C;

/** @brief Script entity pool: 4 entries of 0x58. */
extern s32 D_801D4104;

/** @brief The two prim banks, one per parity of the frame counter. */
extern u8 *D_801D3EBC;
extern u8 *D_801D3EC0;

/** @brief Motes to spawn on each frame of a rising script. */
extern s16 D_801D3DF0[];
extern s16 D_801D3E2C[];
extern s16 D_801D3E68[];

/** @brief Models a script step poses through @ref EffectEntity::unk060. */
extern EffectPoseModel D_801D20B8;
extern EffectPoseModel D_801D238C;

/** @brief Child entity pool: 0x40 entries of 0xC8. */
extern s32 D_801D7314;

/** @brief Render state for the effect's own model. */
extern s32 D_801ED2F4;
extern EffectRender D_801ED304;

/** @brief Spark pool: 0x1F4 entries of 0x84. */
extern s32 D_801E74F4;
extern EffectRenderPart D_801E7504;
extern s32 D_801E7534;

/** @brief The sixteen hex digit glyphs, cached from the main string table. */
extern u8 D_801ED404[];

/** @brief Debug text cursor: X, Y, and the colour the glyphs are drawn in. */
extern s32 D_801ED418;
extern s32 D_801ED41C;
extern s32 D_801ED420;

/** @brief Per-joint world matrices, one per joint of the posed skeleton. */
extern MATRIX D_801ED424[];

/** @} */

#endif /* EFFECT_EFFECT_025_H */
