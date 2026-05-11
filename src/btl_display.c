#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "battle.h"

extern BattleDisplayEntity g_battleEntities[];
extern void copyDisplayRect(RECT *dst);

/**
 * @brief Set a battle entity's type and compute draw mode from bit 0.
 * @param idx Entity index.
 * @param val Entity type; if bit 0 is set, drawMode = 0x3A000000, else 0x38000000.
 */
void setBattleEntityType(s32 idx, s32 val) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    s32 v;
    entity->entityType = val;
    v = 0x38;
    if (val & 1) {
        v = 0x3A;
    }
    entity->drawMode = v << 24;
}


/**
 * @brief Set a battle entity's update callback.
 * @param idx Entity index.
 * @param val Callback function pointer (or 0 to clear).
 */
void setBattleEntityField00(s32 idx, s32 val) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    entity->callback = (EntityCallback)val;
}


/**
 * @brief Set a battle entity's field04.
 * @param idx Entity index.
 * @param val Value to store.
 */
void setBattleEntityField04(s32 idx, s32 val) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    entity->field04 = val;
}


/**
 * @brief Set a battle entity's field36.
 * @param idx Entity index.
 * @param val Value to store.
 */
void setBattleEntityField36(s32 idx, s32 val) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    entity->field36 = val;
}


/**
 * @brief Get a battle entity's field36.
 * @param idx Entity index.
 * @return Value of field36.
 */
u32 getBattleEntityField36(s32 idx) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    return entity->field36;
}


/**
 * @brief Set a battle entity's field35.
 * @param idx Entity index.
 * @param val Value to store.
 */
void setBattleEntityField35(s32 idx, s32 val) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    entity->field35 = val;
}


/**
 * @brief Get a battle entity's field35.
 * @param idx Entity index.
 * @return Value of field35.
 */
u32 getBattleEntityField35(s32 idx) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    return entity->field35;
}


/**
 * @brief Set a battle entity's active flag; if 0, fully deactivate the entity.
 * @param idx Entity index.
 * @param value Active flag; if 0, also clears field36, field04, and field00.
 */
void setBattleEntityActive(s32 idx, s32 value) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];

    entity->activeFlag = value;
    if (value == 0) {
        setBattleEntityField36(idx, 0);
        setBattleEntityField04(idx, 0);
        setBattleEntityField00(idx, 0);
    }
}


/**
 * @brief Get a battle entity's active flag.
 * @param idx Entity index.
 * @return Active flag value (0 = inactive).
 */
s32 GetActiveFlag(s32 idx) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    return entity->activeFlag;
}


/**
 * @brief Set a battle entity's scale factor.
 * @param idx Entity index.
 * @param val Scale value (0x1000 = 1.0).
 */
void setBattleEntityScale(s32 idx, s32 val) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    entity->scale = val;
}


/**
 * @brief Get a battle entity's scale factor.
 * @param idx Entity index.
 * @return Scale value (0x1000 = 1.0).
 */
s32 getBattleEntityScale(s32 idx) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    return entity->scale;
}


/**
 * @brief Initialize a battle entity to default values.
 *
 * Sets up a default bounding rect (64,64,128,128), entity type 6,
 * clears fields, sets anim speed to 3, scale to 0x1000.
 *
 * @param idx Entity index.
 */
void initBattleEntity(s32 idx) {
    s16 rect[4];
    s32 i;
    rect[0] = 0x40;
    rect[1] = 0x40;
    rect[2] = 0x80;
    rect[3] = 0x80;
    setBattleEntityBoundRect(idx, rect);
    setBattleEntityRectClamp(idx, rect);
    setBattleEntityType(idx, 6);
    setBattleEntityField04(idx, 0);
    setBattleEntityField00(idx, 0);
    setBattleEntityActive(idx, 0);
    setBattleEntityAnimSpeed(idx, 3);
    for (i = 0; i < 2; i++) {
        setBattleEntitySubField(idx, i, 0);
    }
    setBattleEntitySubField(idx, 1, idx);
    setBattleEntityScale(idx, 0x1000);
    setBattleEntityField36(idx, 0);
}


/**
 * @brief Compute the rectangular intersection of two RECTs.
 *
 * Reads each RECT as two packed s32 words (xy at +0, wh at +4), sign-extracts
 * the four halves, then takes max(clip.x, src.x) / max(clip.y, src.y) for the
 * top-left and min(clip.right, src.right) / min(clip.bottom, src.bottom) for
 * the bottom-right. If either dimension collapses to <= 0 the intersection is
 * empty: width or height is forced to 0 and the function returns 0;
 * otherwise returns 1.
 *
 * The args are typed as a @c CustomRECT union view (s16 fields + RectWords
 * packed s32 view), reflecting the developer's "rect as packed (xy, wh) word
 * pair" mental model also used in @c clipBlitRects via
 * @c savedPos = *(s32 *)&rect.
 *
 * @param overlap Output RECT receiving the clipped rectangle.
 * @param clip    Clipping rectangle (input).
 * @param src     Source rectangle to clip (input).
 * @return 1 if @p clip and @p src overlap (positive area), 0 otherwise.
 *
 * Best attempt: ~88.81% match (up from 68% baseline) using a CustomRECT
 * union view + several "dead-variable reuse" tricks the permuter discovered.
 * The remaining diffs are gcc 2.7.2 register-allocation choices that we
 * cannot reach from clean source — would require either further permuter
 * iterations or a specific original-source idiom we haven't deduced.
 * The version below is the cleanest semantically-correct form:
 * @code
 * typedef struct { s32 xy; s32 wh; } RectWords;
 * typedef union {
 *     struct { s16 x, y, w, h; } r;
 *     RectWords words;
 * } CustomRECT;
 *
 * s32 func_8002B080(CustomRECT *overlap, CustomRECT *clip, CustomRECT *src) {
 *     s32 hasOverlap = 1;
 *     s32 clipXY = clip->words.xy;
 *     s32 srcXY = src->words.xy;
 *     s32 clipWH = clip->words.wh;
 *     s32 srcWH = src->words.wh;
 *     s32 clipY, clipX, srcY, srcX, clipH, clipW, srcH, srcW;
 *     s32 clipRight, srcRight, clipBottom, srcBottom;
 *     s32 left, top, right, bottom;
 *
 *     clipY = clipXY >> 16;
 *     clipX = (clipXY << 16) >> 16;
 *     srcY = srcXY >> 16;
 *     do {
 *         srcX = (srcXY << 16) >> 16;
 *         clipH = clipWH >> 16;  clipW = (clipWH << 16) >> 16;
 *         srcH  = srcWH >> 16;   srcW  = (srcWH << 16) >> 16;
 *         clipRight = clipW + clipX;
 *         srcRight  = srcW + srcX;
 *     } while (0);
 *     clipBottom = clipH + clipY;
 *     left = srcX;
 *     if (clipX >= srcX) left = clipX;
 *     srcBottom = srcH + srcY;
 *     top = srcY;
 *     if (clipY >= srcY) top = clipY;
 *     right = srcRight;
 *     if (srcRight >= clipRight) right = clipRight;
 *     bottom = srcBottom;
 *     if (srcBottom >= clipBottom) bottom = clipBottom;
 *     if (left >= right) { right = left; hasOverlap = 0; }
 *     if (top >= bottom) { bottom = top; hasOverlap = 0; }
 *     overlap->r.w = right - left;
 *     overlap->r.h = bottom - top;
 *     overlap->r.x = left;
 *     overlap->r.y = top;
 *     return hasOverlap;
 * }
 * @endcode
 */
INCLUDE_ASM("asm/nonmatchings/btl_display", func_8002B080);


/**
 * @brief Clip two source rectangles against the display area and write results.
 *
 * For each of the two source rects in @p arg, offsets by the display origin,
 * clips against the display rect via RECT intersection, clamps minimum size to
 * 2x1, and copies the 12-byte result to the output buffers.
 *
 * @param arg Blit parameters with source rects and destination buffers.
 * @return 1 if the first rectangle intersects the display area, 0 otherwise.
 */
s32 clipBlitRects(BlitParams *arg) {
    ClipWork *cw;
    ClipResult *disp;
    s32 result;

    GP_ALLOC(cw, sizeof(ClipWork));

    disp = &cw->disp;

    copyDisplayRect(&disp->rect);
    disp->savedPos = *(s32 *)&disp->rect;

    cw->work.rect = arg->srcRect1;
    cw->work.rect.x += disp->rect.x;
    cw->work.rect.y += disp->rect.y;
    cw->work.savedPos = *(s32 *)&cw->work.rect;

    if (cw->work.rect.w <= 0 || cw->work.rect.h <= 0) {
        result = 0;
    } else {
        result = func_8002B080(&cw->work.rect, &disp->rect, &cw->work.rect) != 0;
    }
    result++; result--; /* Regalloc */

    if (cw->work.rect.w < 2) cw->work.rect.w = 2;
    if (cw->work.rect.h < 2) cw->work.rect.h = 1;

    memcpy(arg->dstData1, cw, 12);

    copyDisplayRect(&disp->rect);
    disp->savedPos = *(s32 *)&disp->rect;

    cw->work.rect = arg->srcRect2;
    cw->work.rect.x += disp->rect.x;
    cw->work.rect.y += disp->rect.y;
    cw->work.savedPos = *(s32 *)&cw->work.rect;

    if (cw->work.rect.w > 0 && cw->work.rect.h > 0) {
        func_8002B080(&cw->work.rect, &disp->rect, &cw->work.rect);
    }

    if (cw->work.rect.w < 2) cw->work.rect.w = 2;
    if (cw->work.rect.h < 2) cw->work.rect.h = 1;

    memcpy(arg->dstData2, cw, 12);

    GP_FREE(sizeof(ClipWork));

    return result;
}
