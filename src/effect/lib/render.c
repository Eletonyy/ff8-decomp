/**
 * @file render.c
 * @brief The effect renderer: pose a model and draw its meshes.
 */
#include "common.h"
#include "effect.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/inline_c.h"
#include "effect/lib/common.h"
#include "effect/lib/aim.h"
#include "effect/lib/render.h"

static void effectRenderSetPose(EffectRender *render, BattleEffectSlot *slot);
static void effectRenderFaceCamera(EffectRender *render);
static void effectRenderMesh(EffectMesh *mesh, u32 *ot, s32 mode,
                             EffectRender *render);

/** @brief Refresh the render matrices from @p pose and apply the offset. */
static void effectRenderSetPose(EffectRender *render, BattleEffectSlot *slot) {
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
void effectRenderPoseJoints(EffectSkeletonRef *ref, EffectPose *pose) {
    MATRIX *offset = g_effectJointOffsets;
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
void effectRebasePointers(s32 *table) {
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
static void effectRenderFaceCamera(EffectRender *render) {
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
    effectMatrixAim(&a->rot, &a->dir, &a->up);
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
void effectRenderReset(EffectRender *render, void *pose, EffectRenderPart *part,
                       void *texture, BattleEffectSlot *source, s32 frame) {
    render->unk00C = &D_800FA5F0;
    render->unk000 = pose;
    render->part = part;
    render->unk010 = &g_effectPrimCursor;
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
void effectRenderSetTexture(EffectRender *render, SVECTOR *pos, s16 tx, s16 ty,
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
static void effectRenderMesh(EffectMesh *mesh, u32 *ot, s32 mode,
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
void effectRenderModel(EffectRender *render) {
    BattleEffectSlot *slot = render->slot;

    effectRenderSetPose(render, slot);
    if (render->unk0DA != 0xFF) {
        func_800BC420(slot->unk060);
    } else if (func_800BCA3C(slot->unk060, slot->unk06C) != 0) {
        func_800BCF6C(slot->unk060, slot->unk06C, render->unk0D8);
    }
    CompMatrix(&D_800F02C8, &render->unk014, &render->unk034);
    if (!(slot->flags & BATTLE_SLOT_FLAG_UNK20)) {
        *render->unk00C = func_800BC060(slot, D_800FA5E8->unk4040, 0x10, *render->unk00C);
    }
    effectRenderFaceCamera(render);
    effectRenderMesh(slot->mesh, D_800FA5E8->ot, 4, render);
    if (slot->unk078 != NULL) {
        effectRenderMesh(slot->unk078->mesh, D_800FA5E8->ot, 4, render);
    }
    if (render->unk0DA != 0xFF) {
        func_800BC420(slot->unk060);
    }
}
