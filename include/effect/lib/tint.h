#ifndef EFFECT_LIB_TINT_H
#define EFFECT_LIB_TINT_H

#include "effect/lib/particle.h"

/**
 * @file tint.h
 * @brief Play one step of a screen tint as four quads over the frame.
 */

/** @brief One step of the tint script: a colour and what to do with it. */
typedef struct {
    /* 0x00 */ u8 r;
    /* 0x01 */ u8 g;
    /* 0x02 */ u8 b;    /**< A hold step keeps the frame it waits for here. */
    /* 0x03 */ u8 op;   /**< 0xFF ends the script, 0xFE holds, anything else steps. */
} EffectTintStep; /* 0x04 */

/**
 * @name Script records
 *
 * @ref effectSpawnTask hands back as many bytes as the script asks for, with an
 * @ref EffectEntity at the front of them, so a script owns everything the
 * engine leaves alone. These two lay their own state over the pose fields.
 * @{
 */

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

/**
 * @brief Play one step of the screen tint and lay four quads over the frame.
 */
s32 effectTintPlay(EffectTintScript *script);

#endif /* EFFECT_LIB_TINT_H */
