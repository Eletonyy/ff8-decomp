#ifndef EFFECT_EFFECT_025_H
#define EFFECT_EFFECT_025_H

#include "effect.h"
#include "effect/lib/pose.h"

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

/** @} */

/**
 * @brief Root script dispatcher: swap the prim bank, run one step, then run
 *        every child pool.
 */
s32 func_801A81D0(EffectEntity *entity);

#endif /* EFFECT_EFFECT_025_H */
