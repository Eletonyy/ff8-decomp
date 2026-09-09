/**
 * @file tint.c
 * @brief Play one step of a screen tint as four quads over the frame.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/libgpu.h"
#include "effect/lib/common.h"
#include "effect/lib/entity.h"
#include "effect/lib/tint.h"

/** @brief Pack a screen corner the way the tint quads carry them. */
#define EFFECT_TINT_XY(x, y) ((x) | ((y) << 16))

/** @brief Prim code of a tint quad: a semi-transparent flat quad. */
#define EFFECT_TINT_CODE 0x2A

/**
 * @name Script records
 *
 * @ref effectSpawnTask hands back as many bytes as the script asks for, with an
 * @ref EffectEntity at the front of them, so a script owns everything the
 * engine leaves alone. These two lay their own state over the pose fields.
 * @{
 */

/** @brief The full-screen quad the tint script draws, four to a frame. */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 rgb;  /**< The step's colour; its top byte is the prim code. */
    /* 0x08 */ u32 xy0;  /**< X in the low half, Y in the high. */
    /* 0x0C */ u32 xy1;
    /* 0x10 */ u32 xy2;
    /* 0x14 */ u32 xy3;
} EffectTintQuad; /* 0x18 */

/** @} */

/**
 * @brief Play one step of the screen tint and lay four quads over the frame.
 *
 * @return 2 once the script has stopped and its children have drained.
 */
s32 effectTintPlay(EffectTintScript *script) {
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
        EffectTintQuad *quad = g_effectPrimCursor;
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
        g_effectPrimCursor = mode + 1;
    }
    script->frame++;
    if (script->flags & EFFECT_FLAG_STOP) {
        if (script->wait == 0) {
            effectReleaseWait((EffectEntity *)script);
            return 2;
        }
    }
    return 0;
}
