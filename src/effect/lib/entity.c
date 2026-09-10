/**
 * @file entity.c
 * @brief Effect entities: task spawning, battle-slot anchors and model bounds.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/libc.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/entity.h"

/** @brief Opcode handler: release one frame of the linked script's wait. */
void effectReleaseWait(EffectEntity *entity) {
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
void *effectSpawnTask(void *pool, void *task, s32 stride,
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
void effectCacheSlotAnchors(EffectEntity *entity) {
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
void effectSlotAnchor0(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk030;
}

/** @brief Copy @c unk038 of the linked model out to @p out. */
void effectSlotAnchor1(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk038;
}

/** @brief Copy @c unk040 of the linked model out to @p out. */
void effectSlotAnchorMid(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk014->unk040;
}

/**
 * @brief As @ref effectCacheSlotAnchors, but for the slot the animation set
 *        names.
 */
void effectCacheAnimSlotAnchors(EffectEntity *entity) {
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
void effectAnimSlotAnchor0(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk030;
}

/** @brief Copy @c unk038 of the linked model out to @p out. */
void effectAnimSlotAnchor1(EffectEntity *entity, SVECTOR *out) {
    *out = entity->unk010->unk038;
}

/** @brief Copy @c unk040 of the linked model out to @p out. */
void effectAnimSlotAnchorMid(EffectEntity *entity, SVECTOR *out) {
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
void effectUpdateModelBounds(EffectEntity *entity) {
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
void effectGetModelBounds(EffectEntity *entity, SVECTOR *min, SVECTOR *max) {
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
s32 effectModelRadius(EffectEntity *entity) {
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
s16 effectModelHalfHeight(EffectEntity *entity) {
    EffectModel *model = entity->unk014;

    return (s16)(model->boundsMax.vy - model->boundsMin.vy) / 2;
}

/** @brief Half the linked model's larger horizontal extent. */
s16 effectModelHalfWidth(EffectEntity *entity) {
    EffectModel *model = entity->unk014;
    s16 spanX = model->boundsMax.vx - model->boundsMin.vx;
    s16 spanZ = model->boundsMax.vz - model->boundsMin.vz;
    /* Compared as `span` but returned as `spanX`: testing a separate copy is
       what puts the wider-X arm on the taken branch. */
    s16 span = spanX;

    return (span <= spanZ ? spanZ : spanX) / 2;
}

/** @brief Height of the linked model's bounding box. */
s16 effectModelHeight(EffectEntity *entity) {
    return entity->unk014->boundsMax.vy - entity->unk014->boundsMin.vy;
}

/** @brief Y of the top of the linked model, in battle-entity space. */
s16 effectModelTop(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];

    return slot->pos.vy + entity->unk014->boundsMax.vy;
}

/** @brief Y of the bottom of the linked model, in battle-entity space. */
s16 effectModelBottom(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];

    return slot->pos.vy + entity->unk014->boundsMin.vy;
}

/** @brief A random point up the linked model, in battle-entity space. */
s16 effectModelRandomY(EffectEntity *entity) {
    BattleEffectSlot *slot = &D_800EF2D0[entity->unk02D];
    EffectModel *model = entity->unk014;
    s16 height = model->boundsMax.vy - model->boundsMin.vy;
    s16 offset = (rand() & 0xFFF) * height / 4096;

    return slot->pos.vy + model->boundsMax.vy - offset / 2;
}

/** @brief Y of the centre of the linked model, in battle-entity space. */
s16 effectModelCentreY(EffectEntity *entity) {
    EffectModel *model = entity->unk014;

    return D_800EF2D0[entity->unk02D].pos.vy +
           (model->boundsMax.vy + model->boundsMin.vy) / 2;
}

/** @brief Centre of the linked model's bounding box. */
void effectModelCentre(EffectEntity *entity, SVECTOR *out) {
    EffectModel *model = entity->unk014;

    out->vx = (model->boundsMax.vx + model->boundsMin.vx) / 2;
    out->vy = (model->boundsMax.vy + model->boundsMin.vy) / 2;
    out->vz = (model->boundsMax.vz + model->boundsMin.vz) / 2;
}

/** @brief Zero @p size bytes' worth of words starting at @p dst. */
void effectZeroWords(void *dst, s32 size) {
    s32 *p = dst;
    s32 i;

    for (i = 0; i < size / 4; i++) {
        *p = 0;
        p++;
    }
}
