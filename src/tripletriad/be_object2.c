#include "common.h"
#include "battle.h"
#include "tripletriad.h"
#include "tripletriad/be_object2.h"

extern u8 D_801D3110[];
extern u8 D_801D3120[];
extern u8 D_801D3360[];
extern u8 D_801D3380[];
extern u8 D_801D3798[];
extern u8 D_801D3C58[];
extern s32 D_801D3328;
extern s32 D_801A2C74;
extern u16 D_801C2EC4;
extern u8  D_801C2DCA;

/** @brief Game RNG — returns a pseudo-random value. */
extern s32 func_80023D04(void);

/**
 * @brief Per-ply state of the AI move search (one node of @ref searchBestMoveStack).
 *
 * @c D_801D3460 holds 9 of these — one per search ply. Each node carries the
 * (card, row, col) iterators the resumable minimax is currently trying at that
 * ply, the best move found so far, and its scores. Entry @c [0] is the root:
 * once the search completes its @c bestCol / @c bestRow give the chosen board
 * cell and @c bestCard the chosen hand slot.
 * @note @c D_801D3462 / @c D_801D3466 are separate splat symbols that alias
 *       @c D_801D3460[0].card / @c D_801D3460[0].bestCard.
 */
typedef struct {
    /* 0x00 */ u8 col;          /* board column currently being tried (0..2) */
    /* 0x01 */ u8 row;          /* board row currently being tried (0..2) */
    /* 0x02 */ u8 card;         /* hand slot currently being tried (0..4); reachable as D_801D3462 */
    /* 0x03 */ u8 noBest;       /* 1 = no best move recorded yet in this pass */
    /* 0x04 */ u8 bestCol;      /* best/chosen board column */
    /* 0x05 */ u8 bestRow;      /* best/chosen board row */
    /* 0x06 */ u8 bestCard;     /* best/chosen hand slot; reachable as D_801D3466 */
    /* 0x07 */ u8 checkBound;   /* 1 = prune this pass against `bound` (a prior pass completed) */
    /* 0x08 */ u8 pad08[4];
    /* 0x0C */ s32 bestWeighted; /* best difficulty-weighted score (raw * weight >> 12) */
    /* 0x10 */ s32 bestScore;    /* best raw board score */
    /* 0x14 */ s32 bound;        /* pruning threshold carried over from the completed pass */
} AiMove;                      /* 0x18 */

/**
 * @brief AI turn/placement controller — a 0x14-byte callback list node.
 *
 * Allocated by @ref spawnAiTurn via @c func_80098C44 with @ref updateAiTurn
 * registered as its per-frame callback; the leading 0xC bytes are the list-node
 * header (links/callback managed by the @c func_80098Bxx pool routines).
 */
typedef struct {
    /* 0x00 */ u8 pad00[0xC];  /* list-node header (links / callback) */
    /* 0x0C */ u8 seat;        /* player / seat index */
    /* 0x0D */ u8 cardSlot;    /* hand-slot index of the card being worked */
    /* 0x0E */ u8 state;       /**< Outer state — see @ref AiTurnState. */
    /* 0x0F */ u8 sub;         /**< Phase within SEARCH (@ref AiSearchPhase); reused as a
                                    frame counter within ANIMATE. */
    /* 0x10 */ u8 unk10;       /* search-depth parameter passed to searchBestMove */
    /* 0x11 */ u8 result;      /* searchBestMove result status */
    /* 0x12 */ u8 pad12[2];
} func_8009DBE8_arg0;          /* 0x14 */

/** @brief Outer state machine driven by @ref updateAiTurn (@c func_8009DBE8_arg0.state). */
enum AiTurnState {
    AI_TURN_INIT    = 0,  /**< Reset the move-search workspace @c D_801D3460, then SEARCH. */
    AI_TURN_SEARCH  = 1,  /**< Run the move search (@ref AiSearchPhase sub-machine). */
    AI_TURN_ANIMATE = 2,  /**< Play the card-selection animation, then PLACE. */
    AI_TURN_PLACE   = 3   /**< Commit the chosen card to the board, then return 2. */
};

/** @brief Phase within @ref AI_TURN_SEARCH (@c func_8009DBE8_arg0.sub). */
enum AiSearchPhase {
    AI_SEARCH_PREP = 0,   /**< Arm the search timer; latch the candidate card slot. */
    AI_SEARCH_RUN  = 1,   /**< Run @ref searchBestMove and handle its result. */
    AI_SEARCH_WAIT = 2    /**< Tick the timer; re-search (result 3) or advance to ANIMATE. */
};

/** @brief Frames the picked card is shown during @ref AI_TURN_ANIMATE before placement. */
#define AI_ANIM_FRAMES 15

extern AiMove D_801D3460[9];   /* AI move-search workspace (root = [0], one entry per ply) */
extern u8     D_801D3462;      /* = D_801D3460[0].card (root card iterator / latched slot) */
extern u8     D_801D3466;      /* = D_801D3460[0].bestCard (chosen placement card) */
extern s32    D_801D3538;      /* minimax placement budget per search slice (see searchBestMoveStack) */
extern s32    D_801D353C;      /* search-phase frame timer */
extern s32    D_801D35C0;      /* current player / seat index */

/** @brief One row of the AI evaluation-weight table (@ref D_80182DAC). */
typedef struct { s32 w0, w1, w2, w3, w4; } WeightSet;  /* 0x14 */

extern u8        D_80182D64[8][9]; /* [difficulty][cards-left] → AI search-depth param */
extern WeightSet D_80182DAC[8];    /* per-difficulty AI evaluation-weight rows */
extern u8        D_80082C97;       /* = D_80082C90.field_07 (distinct splat symbol) */
extern u8        D_801D3540[];     /* AI turn-node pool */
extern u8        D_801D3560[];     /* AI turn-node list head */

/**
 * @brief 20-byte linked-list node owned by the @c D_801D3380 list and
 *        driven by the @ref updateCardSelectCursor callback.
 *
 * @c state is a small dispatch index (0..3) for the card-selection
 * cursor; @c fieldD and @c fieldE seed the row-cursor substate
 * subscriptions; @c snapshot is a 4-byte mirror of the current
 * @c D_801D3340 entry copied in when state 1 commits the choice.
 */
typedef struct {
    u8           pad00[0xC];   /* linked-list header (flags / next / callback) */
    u8           state;        /* 0x0C: dispatch state (0..3) */
    u8           fieldD;       /* 0x0D: cursor row index seed */
    u8           fieldE;       /* 0x0E: state byte forwarded to activateMenuSubstate */
    u8           pad0F;        /* 0x0F */
    SubstateSlot snapshot;     /* 0x10: copy of D_801D335C after the user's pick */
} SubstateMachineNode;

extern s32  updateCardSelectCursor(SubstateMachineNode *p);
extern void activateMenuSubstate(s32 idx, s32 mask, u8 stateByte, s32 suppressFlags);
extern void drawMenuPrim(s32 mode, SubstateSlot *slot);
extern void commitCardToBoard(TripleTriadBoard *board, s32 entityIdx, s32 col, s32 row);
extern void func_800A26C8(void);

/* Per-player Triple Triad match state (region starts at 0x801A2C40). */
extern u8 D_801A2C48[2][5];  /* Two players' 5-card hands (card ids). */

extern void  func_80098BC0(u8 *list, u8 *pool, s32 nodeSize, s32 count);
extern void *func_80098C44(u8 *list, s32 callback);
extern void *func_8002FF34(s32 *otBase, void *pkt, s32 ch, s32 yPos, s32 w, s32 col);
extern s32   func_8009A7A4(s32 a, s32 b, s32 c);
extern void  func_8009A878(s32 a, s32 b);
extern void  func_800A233C(s32 a);
extern void  func_800A2114(u8 entityType);

/* SVECTOR tables used by the card render path (func_8009AE6C). */
extern SVECTOR D_80182C30[4];   /* main card-face quad corners */
extern SVECTOR D_80182C50[4];   /* outer border quad corners */
extern SVECTOR D_80182C70[4];   /* per-corner offsets used per rank digit */
extern SVECTOR D_80182C90[4];   /* per-rank digit center positions */
extern SVECTOR D_80182CD0[4];   /* element marker quad corners */
extern SVECTOR D_80182CF0[4];   /* shadow quad corners (card drop-shadow) */

/* Substate handlers dispatched from updateTriadMenu. Same names exist in
 * the battle overlay with different signatures; keep these as
 * file-local externs so the two overlays don't collide. */
extern void handleCursorSubstate1(SubstateSlot *slot, s32 idx);
extern void handleCursorSubstate2(SubstateSlot *slot, s32 idx);
extern void handleCursorSubstate3(SubstateSlot *slot, s32 idx);

/**
 * @brief Set up a battle object slot and cancel lower-priority siblings in its group.
 *
 * Stores the three action params in slot @p idx, marks it active via
 * @c setCardEntityType(idx, 1), then sweeps the 10 slots: any sibling sharing
 * @c groupId with strictly lower @c priority gets reset via
 * @c setCardEntityType(i, 7).
 *
 * @param idx     Index of the slot being placed (0..9).
 * @param param0  First action parameter (stored at @c slot.param0).
 * @param param1  Second action parameter (stored at @c slot.param1).
 * @param param2  Third action parameter (stored at @c slot.param2).
 */
void setBattleObjectAction(s32 idx, s32 param0, s32 param1, s32 param2) {
    s32 i;

    g_tripleTriadCardHands[idx].param0 = param0;
    g_tripleTriadCardHands[idx].param1 = param1;
    g_tripleTriadCardHands[idx].param2 = param2;
    setCardEntityType(idx, 1);

    for (i = 0; i < 10; i++) {
        if (g_tripleTriadCardHands[i].groupId == g_tripleTriadCardHands[idx].groupId
            && g_tripleTriadCardHands[i].priority < g_tripleTriadCardHands[idx].priority) {
            setCardEntityType(i, 7);
        }
    }
}

/**
 * @brief Emit a 24x16 TSPRT overlay sprite anchored at a node's world XY.
 *
 * Builds a single textured sprite primitive (combined DR_TPAGE + SPRT in
 * one packet, 24 bytes) at @c (node.spriteX + 0xB4, node.spriteY + 0x68)
 * and links it into the OT @p ot via @c AddPrim. The @p variant selector
 * picks between two adjacent textures sharing the same tpage:
 *  - @c variant @c > @c 0: U=0,  CLUT=0x3A80
 *  - @c variant @c <= @c 0: U=24, CLUT=0x3AC0
 *
 * The function increments and returns the output buffer pointer so the
 * caller can chain further primitives. The single-iteration do-while
 * mirrors the original (likely a degenerate loop body retained for
 * scheduling reasons).
 *
 * @param node     Animation node providing screen-space anchor (only the
 *                 @c spriteX and @c spriteY union members are read).
 * @param variant  Texture selector — positive selects the first texture/CLUT.
 * @param ot       OT bucket pointer used by @c AddPrim.
 * @param out      Output primitive buffer (pre-allocated by caller).
 * @return         Pointer to the next free TSPRT slot in @p out.
 */
TSPRT *drawCardOverlaySprite(BattleAnimNode *node, s32 variant, void *ot, TSPRT *out) {
    s32 i = 0;

    do {
        out->tag      = 0x05000000;
        out->drawMode = 0xE100060C;
        *(u32 *)&out->r0 = 0x64808080;
        out->x0 = (u16)node->mat.t[0] + 0xB4;
        {
            s32 y0 = (u16)node->mat.t[1];
            out->w  = 24;
            out->h  = 16;
            out->y0 = y0 + 0x68;
        }
        if (variant > 0) {
            out->u0   = 0;
            out->clut = 0x3A80;
        } else {
            out->u0   = 24;
            out->clut = 0x3AC0;
        }
        out->v0 = 0x48;

        AddPrim(ot, out);
        out++;
        i++;
    } while (i <= 0);

    return out;
}

/**
 * @brief Per-frame update for a @c TripleTriadCardObject — rotation, transform, render.
 *
 * Called by the be_object2 dispatch (registered via @c func_80098C44 in
 * @c setupTripleTriadHands). For one @c TripleTriadCardObject the function:
 *  - Advances @c angle toward 0x1000 (clockwise) or 0 (counter-clockwise)
 *    based on the @c CTRL_FLAG_02 bit, and clamps the result.
 *  - Allocates a 40-byte @c BattleAnimNode work buffer.
 *  - Runs the transform chain (@c func_8009C12C, @c func_8009A6EC,
 *    @c func_80041274) to populate the node's base position, then folds
 *    in @c offX/Y/Z and @c offSort to produce the world position.
 *  - Applies a small angle-driven X displacement
 *    (@c (sin(angle/4) * 12) >> 12), sign-flipped by @c groupId.
 *  - For Triple-Triad cards (@c state==0, @c groupId==2), looks up the
 *    cell's @c elementMod in @c D_801D3398 — if non-zero, emits the
 *    elemental modifier overlay sprite via @c drawCardOverlaySprite.
 *  - Calls the per-frame render helpers (@c func_8009AE6C and
 *    @c transformCardEffect) to draw the rest of the object.
 *  - Frees the @c BattleAnimNode.
 *
 * @param ctl Handler context whose @c entry slot points at the
 *            @c TripleTriadCardObject being driven.
 * @return 0 (kept on the stack for compat with @c s32 callback signature).
 */
s32 updateCardObject(BattleObjectCtl *ctl) {
    TripleTriadCardObject *entity;
    BattleAnimNode *node;

    node = func_80098B80(0x28);
    entity = ctl->entry;

    if (entity->flags & CTRL_FLAG_02) {
        if (entity->angle < 0x1000) {
            s16 newAngle = entity->angle + 0x800;
            entity->angle = newAngle;
            if (newAngle > 0x1000) {
                entity->angle = 0x1000;
            }
        }
        entity->flags &= ~CTRL_FLAG_02;
    } else {
        if (entity->angle != 0) {
            s16 newAngle = entity->angle - 0x800;
            entity->angle = newAngle;
            if (newAngle < 0) {
                entity->angle = 0;
            }
        }
    }

    func_8009C12C(entity);
    func_8009A6EC(&entity->groupId, &node->base.vx);
    func_80041274(&entity->posData[0], node);

    node->mat.t[0]  = node->base.vx + entity->offX;
    node->mat.t[1]  = node->base.vy + entity->offY;
    node->mat.t[2]    = node->base.vz + entity->offZ;
    node->base.pad  += entity->offSort;

    if (entity->angle != 0) {
        if (entity->groupId == 0) {
            node->mat.t[0] += (func_8003ED64(entity->angle / 4) * 12) >> 12;
        } else {
            node->mat.t[0] -= (func_8003ED64(entity->angle / 4) * 12) >> 12;
        }
    }

    if (entity->state == 0 && entity->groupId == 2) {
        s32 col = entity->fieldD + 1;
        s8 elementMod = D_801D3398.cells[entity->priority + 1][col].elementMod;
        if (elementMod != 0) {
            D_801C2EB4 = drawCardOverlaySprite(node, elementMod,
                                        &D_801C2EB0[node->base.pad], D_801C2EB4);
        }
    }

    SetRotMatrix(&node->mat);
    SetTransMatrix(&node->mat);

    D_801C2EB4 = func_8009AE6C(entity->cardId, entity->initFlags,
                                &D_801C2EB0[node->base.pad], D_801C2EB4);
    transformCardEffect(entity, node, &D_801C2EB0[node->base.pad]);

    func_80098BA0(0x28);
    return 0;
}

/**
 * @brief Call func_80098D28 with D_801D3110.
 */
void processCardObjects(void) {
    func_80098D28(D_801D3110);
}

/**
 * @brief Initialize the 10 hand-card @c TripleTriadCardObject slots for a Triple Triad match.
 *
 * Builds the linked list at @c D_801D3110 (10 nodes from the @c D_801D3120 pool,
 * 16 bytes each), then for each of the two players sets up 5 hand-card entries
 * in @c g_tripleTriadCardHands. Each entry is wired up so that its per-frame
 * @c updateCardObject callback can find it via @c BattleObjectCtl.entry.
 *
 * Per-entry fields:
 *  - @c cardId   ← @c D_801A2C48[player][slot] (card id into @c g_tripleTriadCardStats).
 *  - @c flags        ← 1 (active).
 *  - @c initFlags    ← @c 0x12 | player (flag bits read by the card render path).
 *  - @c groupId      ← player.
 *  - @c priority     ← slot.
 *  - @c posData[1]   ← @c 0x800 when rule bit 0 is off and @c D_801A2C70[player] == 3
 *                       (hand-position mode for that player type); 0 otherwise.
 *  - all remaining fields cleared to 0.
 */
void setupTripleTriadHands(void) {
    s32 player;
    s32 slot;
    TripleTriadCardObject *entity;
    BattleObjectCtl *node;
    u8 *hand;

    func_80098BC0(D_801D3110, D_801D3120, 0x10, 0xA);

    entity = g_tripleTriadCardHands;
    for (player = 0; player < 2; player++) {
        u8 *playerType;
        hand = D_801A2C48[player];
        for (slot = 0; slot < 5; slot++) {
            playerType = &D_801A2C70[player];
            node = (BattleObjectCtl *)func_80098C44(D_801D3110, (s32)updateCardObject);
            node->entry = entity;
            entity->cardId = hand[slot];
            entity->state      = 0;
            entity->initFlags  = 0x12 | player;
            entity->groupId    = player;
            entity->fieldD     = 0;
            entity->priority   = slot;
            entity->posData[0] = 0;
            if ((g_tripleTriadRules & 1) == 0 && *playerType == 3) {
                entity->posData[1] = 0x800;
            } else {
                entity->posData[1] = 0;
            }
            entity->field18 = 0;
            entity->offX    = 0;
            entity->offY    = 0;
            entity->offZ    = 0;
            entity->offSort = 0;
            entity->angle   = 0;
            entity->flags   = 1;
            entity++;
        }
    }
}

/**
 * @brief Render one Triple Triad card (up to 4 ranks + element marker +
 *        body shading + gradient border, or a single back-face quad).
 *
 * Allocates a 60-byte @c CardRenderWork buffer and projects the card
 * outline through the GTE via @c RotTransPers4. If the projected normal
 * is backfacing (@c NormalClip returns negative), emits a single
 * textured POLY_FT4 with the card-back UVs. Otherwise:
 *  - @c flags @c & @c 0x02 — emit 4 POLY_FT4s for the rank digits.
 *  - @c flags @c & @c 0x10 (and @c card->element @c != @c 0) — emit
 *    one POLY_FT4 for the element marker.
 *  - Always emit the card-body shading POLY_FT4 with per-card UVs and
 *    a POLY_G4 gradient border (palette picked by @c flags @c & @c 0x20
 *    / @c 0x01).
 *
 * @verbatim
 * void *func_8009AE6C(u8 cardId, s32 flags, void *ot, void *out) {
 *     CardRenderWork *work;
 *     POLY_FT4 *ftPrim;
 *     POLY_G4  *gPrim;
 *     s32 baseColor;
 *     s32 transBit;
 *
 *     work = func_80098B80(0x3C);
 *
 *     if (flags & 0x20) {
 *         baseColor = 0x2C404040;
 *     } else {
 *         baseColor = 0x2C808080;
 *     }
 *     if (flags & 0x4) {
 *         transBit = 0x02000000;
 *         baseColor |= transBit;
 *     } else {
 *         transBit = 0;
 *     }
 *
 *     RotTransPers4(&D_80182C30[0], &D_80182C30[1], &D_80182C30[2], &D_80182C30[3],
 *                   &work->outXY[0], &work->outXY[1], &work->outXY[2], &work->outXY[3],
 *                   &work->P, &work->flag);
 *
 *     if (NormalClip(work->outXY[0], work->outXY[1], work->outXY[2]) >= 0) {
 *         TripleTriadCard *card = &g_tripleTriadCardStats[cardId];
 *
 *         if (flags & 0x2) {
 *             s32 i, j;
 *             for (i = 0; i < 4; i++) {
 *                 for (j = 0; j < 4; j++) {
 *                     work->digitVerts[j].vx = D_80182C90[i].vx + D_80182C70[j].vx;
 *                     work->digitVerts[j].vy = D_80182C90[i].vy + D_80182C70[j].vy;
 *                     work->digitVerts[j].vz = D_80182C90[i].vz + D_80182C70[j].vz;
 *                 }
 *                 ftPrim = (POLY_FT4 *)out;
 *                 RotTransPers4(&work->digitVerts[0], &work->digitVerts[1],
 *                               &work->digitVerts[2], &work->digitVerts[3],
 *                               (s32 *)&ftPrim->x0, (s32 *)&ftPrim->x1,
 *                               (s32 *)&ftPrim->x2, (s32 *)&ftPrim->x3,
 *                               &work->P, &work->flag);
 *                 ftPrim->tag = 0x09000000;
 *                 *(s32 *)&ftPrim->r0 = transBit | 0x2C808080;
 *                 ftPrim->tpage = 0xC;
 *                 ftPrim->clut  = 0x3800;
 *                 {
 *                     u8 rank = card->sides[i];
 *                     ftPrim->u0 = rank * 16;        ftPrim->v0 = 0;
 *                     ftPrim->u1 = rank * 16 + 11;   ftPrim->v1 = 0;
 *                     ftPrim->u2 = rank * 16;        ftPrim->v2 = 0xB;
 *                     ftPrim->u3 = rank * 16 + 11;   ftPrim->v3 = 0xB;
 *                 }
 *                 func_8004D584(ot, ftPrim);
 *                 out = ftPrim + 1;
 *             }
 *         }
 *
 *         if ((flags & 0x10) && card->element != 0) {
 *             s32 bit = 0;
 *             while (((card->element >> bit) & 1) == 0 && bit < 8) {
 *                 bit++;
 *             }
 *             ftPrim = (POLY_FT4 *)out;
 *             RotTransPers4(&D_80182CD0[0], &D_80182CD0[1], &D_80182CD0[2], &D_80182CD0[3],
 *                           (s32 *)&ftPrim->x0, (s32 *)&ftPrim->x1,
 *                           (s32 *)&ftPrim->x2, (s32 *)&ftPrim->x3,
 *                           &work->P, &work->flag);
 *             ftPrim->tag = 0x09000000;
 *             *(s32 *)&ftPrim->r0 = 0x2C808080;
 *             ftPrim->tpage = 0xC;
 *             ftPrim->clut  = (bit + 0xE1) << 6;
 *             {
 *                 s32 uLeft = (bit % 4) * 64;
 *                 s32 vTop  = (bit / 4) * 16 + 16;
 *                 ftPrim->u0 = uLeft;       ftPrim->v0 = vTop;
 *                 ftPrim->u1 = uLeft + 15;  ftPrim->v1 = vTop;
 *                 ftPrim->u2 = uLeft;       ftPrim->v2 = vTop + 15;
 *                 ftPrim->u3 = uLeft + 15;  ftPrim->v3 = vTop + 15;
 *             }
 *             func_8004D584(ot, ftPrim);
 *             out = ftPrim + 1;
 *         }
 *
 *         ftPrim = (POLY_FT4 *)out;
 *         ftPrim->tag = 0x09000000;
 *         *(s32 *)&ftPrim->r0 = baseColor | 0x2C000000;
 *         *(s32 *)&ftPrim->x0 = work->outXY[0];
 *         *(s32 *)&ftPrim->x1 = work->outXY[1];
 *         *(s32 *)&ftPrim->x2 = work->outXY[2];
 *         *(s32 *)&ftPrim->x3 = work->outXY[3];
 *         {
 *             s32 uLeft = (cardId & 0x1) << 6;
 *             s32 vTop  = (cardId << 5) & 0xC0;
 *             ftPrim->u0 = uLeft;          ftPrim->v0 = vTop;
 *             ftPrim->u1 = uLeft + 0x3F;   ftPrim->v1 = vTop;
 *             ftPrim->u2 = uLeft;          ftPrim->v2 = vTop + 0x3F;
 *             ftPrim->u3 = uLeft + 0x3F;   ftPrim->v3 = vTop + 0x3F;
 *             ftPrim->tpage = (((cardId >> 1) + 0xC8) << 6) | ((((cardId & 0x1) << 7) + 0x300) >> 4);
 *             ftPrim->clut  = ((cardId >> 3) + 0x10) | 0x80;
 *         }
 *         func_8004D584(ot, ftPrim);
 *         out = ftPrim + 1;
 *
 *         {
 *             u32 color0, color1, color2;
 *             gPrim = (POLY_G4 *)out;
 *             RotTransPers4(&D_80182C50[0], &D_80182C50[1], &D_80182C50[2], &D_80182C50[3],
 *                           (s32 *)&gPrim->x0, (s32 *)&gPrim->x1,
 *                           (s32 *)&gPrim->x2, (s32 *)&gPrim->x3,
 *                           &work->P, &work->flag);
 *             gPrim->tag = 0x08000000;
 *             if (flags & 0x20) {
 *                 color0 = 0x38807060; color1 = 0x807060; color2 = 0x402018;
 *             } else if (flags & 0x1) {
 *                 color0 = 0x38FFE0C0; color1 = 0xFFE0C0; color2 = 0x804030;
 *             } else {
 *                 color0 = 0x38E0C0FF; color1 = 0xE0C0FF; color2 = 0x403080;
 *             }
 *             *(u32 *)&gPrim->r0 = color0;
 *             *(u32 *)&gPrim->r1 = color1;
 *             *(u32 *)&gPrim->r2 = color2;
 *             *(u32 *)&gPrim->r3 = color2;
 *             func_8004D584(ot, gPrim);
 *             out = gPrim + 1;
 *         }
 *     } else {
 *         ftPrim = (POLY_FT4 *)out;
 *         ftPrim->tag = 0x09000000;
 *         *(s32 *)&ftPrim->r0 = transBit | 0x2C808080;
 *         *(s32 *)&ftPrim->x0 = work->outXY[0];
 *         *(s32 *)&ftPrim->x1 = work->outXY[1];
 *         *(s32 *)&ftPrim->x2 = work->outXY[2];
 *         *(s32 *)&ftPrim->x3 = work->outXY[3];
 *         ftPrim->v0 = 0xC0;
 *         ftPrim->v1 = 0xC0;
 *         ftPrim->v2 = 0xFF;
 *         ftPrim->v3 = 0xFF;
 *         ftPrim->tpage = 0x9D;
 *         ftPrim->u0 = 0x3F;
 *         ftPrim->u1 = 0;
 *         ftPrim->u2 = 0x3F;
 *         ftPrim->u3 = 0;
 *         ftPrim->clut = 0x3FF0;
 *         func_8004D584(ot, ftPrim);
 *         out = ftPrim + 1;
 *     }
 *
 *     func_80098BA0(0x3C);
 *     return out;
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object2", func_8009AE6C);

/**
 * @brief Emit a single semi-transparent black @c POLY_F4 quad — a "card shadow".
 *
 * Allocates a 60-byte @c CardRenderWork scratch, projects the four shadow-quad
 * corners (@c D_80182CF0) through the GTE via @c RotTransPers4 into the
 * primitive's @c x0..x3 fields, then links the primitive into the OT.
 *
 * The tag is set to @c 0x05000000 (5-word POLY_F4 payload) and the colour/code
 * word to @c 0x2A000000 — RGB=0, code @c 0x2A (semi-transparent flat quad).
 *
 * @param ot   OT bucket pointer.
 * @param prim Pre-allocated @c POLY_F4 slot in the primitive buffer.
 * @return     @p prim incremented past this primitive (i.e. the next free
 *             @c POLY_F4 slot).
 */
POLY_F4 *drawCardShadow(u32 *ot, POLY_F4 *prim) {
    CardRenderWork *work;

    work = func_80098B80(0x3C);
    prim->tag = 0x05000000;
    *(u32 *)&prim->r0 = 0x2A000000;

    RotTransPers4(&D_80182CF0[0], &D_80182CF0[1], &D_80182CF0[2], &D_80182CF0[3],
                  (s32 *)&prim->x0, (s32 *)&prim->x1,
                  (s32 *)&prim->x2, (s32 *)&prim->x3,
                  &work->P, &work->flag);

    AddPrim(ot, prim);
    func_80098BA0(0x3C);
    return prim + 1;
}

/**
 * @brief Reset battle state globals and the substate parameter table.
 *
 * Clears @c D_801D3328, @c D_801D3359, and the per-substate slots in
 * @c D_801D3340 (zeroing most fields, but setting both halves of slot 3
 * to 1 — slot 3 is the only substate that uses both halfwords).
 */
void resetTriadMenuState(void) {
    D_801D3328 = 0;
    D_801D3359 = 0;
    D_801D3340[1].field2 = 0;
    D_801D3340[2].field2 = 0;
    D_801D3340[3].field0 = 1;
    D_801D3340[3].field2 = 1;
    D_801D3340[4].field0 = 0;
    D_801D3340[5].field0 = 0;
}

/**
 * @brief Emit one HUD primitive at a substate-driven anchor.
 *
 * Dispatches on @p mode (1..5); modes outside that range are no-ops. Each
 * case calls @c func_8002FF34 to emit a single GPU packet into
 * @c &D_801C2EB0[4] (OT bucket #4), advancing @c D_801C2EB4 (the global
 * primitive-pool tail). The substate slot's two halfwords (@c field0,
 * @c field2) supply per-mode anchor offsets:
 *
 *  - mode 1: char @c 1   at y=0x58, w=@c (field2*32)+0x30
 *  - mode 2: char @c 0   at y=0x110, w=@c (field2*32)+0x30
 *  - mode 3: char @c 0   at y=@c (field0*64)+0x68, w=@c (field2*64)|0x30
 *  - mode 4: char @c 0   at y=@c (field0*64)+0x28, w=0x4C
 *  - mode 5: char @c 0   at y=@c (field0*64)+0x28, w=0x94
 *
 * Color is always white (@c 0x808080).
 *
 * @param mode  Substate index (1..5; other values are ignored).
 * @param slot  Substate parameter slot supplying @c field0 / @c field2 anchors.
 */
void drawMenuPrim(s32 mode, SubstateSlot *slot) {
    switch (mode) {
    case 1:
        D_801C2EB4 = func_8002FF34(&D_801C2EB0[4], D_801C2EB4,
                                    1, 0x58,
                                    (slot->field2 << 5) + 0x30, 0x808080);
        break;
    case 2:
        D_801C2EB4 = func_8002FF34(&D_801C2EB0[4], D_801C2EB4,
                                    0, 0x110,
                                    (slot->field2 << 5) + 0x30, 0x808080);
        break;
    case 3:
        D_801C2EB4 = func_8002FF34(&D_801C2EB0[4], D_801C2EB4,
                                    0, (slot->field0 << 6) + 0x68,
                                    (slot->field2 << 6) | 0x30, 0x808080);
        break;
    case 4:
        D_801C2EB4 = func_8002FF34(&D_801C2EB0[4], D_801C2EB4,
                                    0, (slot->field0 << 6) + 0x28,
                                    0x4C, 0x808080);
        break;
    case 5:
        D_801C2EB4 = func_8002FF34(&D_801C2EB0[4], D_801C2EB4,
                                    0, (slot->field0 << 6) + 0x28,
                                    0x94, 0x808080);
        break;
    }
}

/**
 * @brief Look up the active object and initialize its handler.
 *
 * Forwards @p a / @p b / @p c through to @c func_8009A7A4 (the function
 * never touches a-regs itself, so the args land in the helper unchanged).
 * If the helper returns a valid index, passes the slot's @c cardId
 * byte to @c func_800A2114.
 *
 * @param a Search key (passed through to @c func_8009A7A4 as arg 0).
 * @param b Filter mask (passed through as arg 1).
 * @param c Row/column key (passed through as arg 2).
 */
void initMenuObjectHandler(s32 a, s32 b, s32 c) {
    s32 idx = func_8009A7A4(a, b, c);
    if (idx >= 0) {
        func_800A2114(g_tripleTriadCardHands[idx].cardId);
    }
}

/**
 * @brief Substate-1 handler: cursor left/right movement on the substate row.
 *
 * Reads input bits from @c D_801D332E and the controller mask
 * @c D_801D3328 to update @c slot->field2 (the row cursor) and/or queue a
 * sub-dispatch:
 *
 *  - bit 0x1000 (left) : tries @c func_8009A7A4 with @c field2-1; if the
 *    candidate is valid (returns @c >= 0), plays the move SFX via
 *    @c func_800A233C(1), decrements @c field2, and falls through to the
 *    common dispatch.
 *  - bit 0x4000 (right): same pattern, with @c field2+1.
 *  - bit 0x2000 (select) AND @c D_801D3328 has bit 0x8 set → store
 *    @c 3 into @c D_801D3358 and return (skip the dispatch).
 *  - bit 0x2000 (select) AND @c D_801D3328 has bit 0x4 set → store
 *    @c 2 into @c D_801D3358 and return.
 *
 * The common dispatch at the bottom calls @c func_8009A878(0, field2) and
 * @c initMenuObjectHandler(0, 0, field2) — the latter forwards its three args
 * through to @c func_8009A7A4 inside the helper.
 *
 * @param slot Substate parameter slot — @c field2 is the row cursor.
 * @param idx  Substate index (unused here; preserved for the dispatcher
 *             callback signature).
 */
void handleCursorSubstate1(SubstateSlot *slot, s32 idx) {
    if ((D_801D332E & 0x1000) && func_8009A7A4(0, 0, slot->field2 - 1) >= 0) {
        func_800A233C(1);
        slot->field2 = slot->field2 - 1;
    } else if ((D_801D332E & 0x4000) && func_8009A7A4(0, 0, slot->field2 + 1) >= 0) {
        func_800A233C(1);
        slot->field2 = slot->field2 + 1;
    } else if (D_801D332E & 0x2000) {
        if (D_801D3328 & 0x8) {
            D_801D3358 = 3;
            return;
        }
        if (D_801D3328 & 0x4) {
            D_801D3358 = 2;
            return;
        }
    }

    func_8009A878(0, slot->field2);
    initMenuObjectHandler(0, 0, slot->field2);
}

/**
 * @brief Substate-2 handler: cursor left/right movement on the substate row.
 *
 * Mirror of @ref handleCursorSubstate1 but with @c a0=1 (substate-2 search key)
 * and a different cancel mapping. Reads input bits from @c D_801D332E and
 * the controller mask @c D_801D3328 to update @c slot->field2 (column
 * cursor) and/or queue a sub-dispatch:
 *
 *  - bit 0x1000 (left) : tries @c func_8009A7A4(1, 0, field2-1); if valid,
 *    play move SFX and decrement @c field2.
 *  - bit 0x4000 (right): same pattern, with @c field2+1.
 *  - bit 0x8000 (select) AND @c D_801D3328 has bit 0x8 set → @c D_801D3358
 *    = 3 and return.
 *  - bit 0x8000 (select) AND @c D_801D3328 has bit 0x2 set → @c D_801D3358
 *    = 1 and return.
 *
 * Common dispatch: @c func_8009A878(1, field2) and
 * @c initMenuObjectHandler(1, 0, field2).
 *
 * @param slot Substate parameter slot — @c field2 is the column cursor.
 * @param idx  Substate index (unused; preserved for the dispatcher
 *             callback signature).
 */
void handleCursorSubstate2(SubstateSlot *slot, s32 idx) {
    if ((D_801D332E & 0x1000) && func_8009A7A4(1, 0, slot->field2 - 1) >= 0) {
        func_800A233C(1);
        slot->field2 = slot->field2 - 1;
    } else if ((D_801D332E & 0x4000) && func_8009A7A4(1, 0, slot->field2 + 1) >= 0) {
        func_800A233C(1);
        slot->field2 = slot->field2 + 1;
    } else if (D_801D332E & 0x8000) {
        if (D_801D3328 & 0x8) {
            D_801D3358 = 3;
            return;
        }
        if (D_801D3328 & 0x2) {
            D_801D3358 = 1;
            return;
        }
    }

    func_8009A878(1, slot->field2);
    initMenuObjectHandler(1, 0, slot->field2);
}

/**
 * @brief Substate-3 handler: 2D cursor (row × column) with edge-wrap dispatch.
 *
 * Reads input bits from @c D_801D332E and the controller mask
 * @c D_801D3328 to drive a row/column cursor (@c slot->field0 row,
 * @c slot->field2 column):
 *
 *  - bit 0x8000 (up)    : @c field0--; if it doesn't underflow to @c -1,
 *    play SFX. An underflow falls into the "row wrapped below 0" handler
 *    at the bottom (clamps to 0 and conditionally sets @c D_801D3358=1).
 *  - bit 0x2000 (down)  : @c field0++; if still less than 3, play SFX.
 *    A value of 3+ falls into the "row past last" handler at the bottom
 *    (clamps to 2 and conditionally sets @c D_801D3358=2).
 *  - bit 0x1000 (left), only if up/down didn't fire: if @c field2 > 0,
 *    play SFX and @c field2--.
 *  - bit 0x4000 (right), only if up/down/left didn't fire: if
 *    @c field2 < 2, play SFX and @c field2++.
 *
 * Row-wrap tail:
 *  - if @c field0 < 0  : set @c field0 = 0; if @c D_801D3328 has bit 0x2
 *    set, @c D_801D3358 = 1 (queue submenu).
 *  - if @c field0 >= 3 : set @c field0 = 2; if @c D_801D3328 has bit 0x4
 *    set, @c D_801D3358 = 2.
 *
 * Common dispatch at the end: @c initMenuObjectHandler(2, field0, field2).
 *
 * @param slot Substate parameter slot — @c field0 row, @c field2 column.
 * @param idx  Substate index (unused; preserved for the dispatcher
 *             callback signature).
 */
void handleCursorSubstate3(SubstateSlot *slot, s32 idx) {
    if (D_801D332E & 0x8000) {
        slot->field0 = slot->field0 - 1;
        if (slot->field0 >= 0) {
            func_800A233C(1);
        }
    } else if (D_801D332E & 0x2000) {
        slot->field0 = slot->field0 + 1;
        if (slot->field0 < 3) {
            func_800A233C(1);
        }
    } else if ((D_801D332E & 0x1000) && slot->field2 > 0) {
        func_800A233C(1);
        slot->field2 = slot->field2 - 1;
    } else if ((D_801D332E & 0x4000) && slot->field2 < 2) {
        func_800A233C(1);
        slot->field2 = slot->field2 + 1;
    }

    if (slot->field0 < 0) {
        slot->field0 = 0;
        if (D_801D3328 & 0x2) {
            D_801D3358 = 1;
        }
    } else if (slot->field0 >= 3) {
        slot->field0 = 2;
        if (D_801D3328 & 0x4) {
            D_801D3358 = 2;
        }
    }

    initMenuObjectHandler(2, slot->field0, slot->field2);
}

/**
 * @brief Adjust a battle speed/volume parameter based on controller input.
 *
 * If D_801D332E has bit 0x8000 set and the current value at a0 is positive,
 * decrements the value and triggers a sound effect. If D_801D332E has bit
 * 0x2000 set and the value is less than 4, increments it and triggers a
 * sound effect. Stores the final value to D_801D335C.
 *
 * @param a0 Pointer to a halfword value to adjust.
 */
void adjustConfigParam(u16 *a0) {
    u16 val;

    if (D_801D332E & 0x8000) {
        if (*(s16 *)a0 > 0) {
            func_800A233C(1);
            val = *a0 - 1;
            *a0 = val;
            goto store;
        }
    }
    if (D_801D332E & 0x2000) {
        if (*(s16 *)a0 < 4) {
            func_800A233C(1);
            val = *a0 + 1;
            *a0 = val;
            goto store;
        }
    }
store:
    D_801D335C.field0 = *a0;
}

/**
 * @brief Per-frame battle-engine tick: latch state masks then dispatch substate.
 *
 * Reads @c D_801D3338 as a signed byte. State 0/1 latches the per-state
 * mask entries from @c D_801C2EC8 / @c D_801C2EB8 / @c D_801C2EC0 into
 * @c D_801D332C / @c D_801D332E / @c D_801D3330; state 2 latches the
 * OR of the first two entries; state < 0 or > 2 skips the latch.
 *
 * If @c D_801D3359 == 1, dispatches to one of four substate handlers
 * (@c handleCursorSubstate1 / @c handleCursorSubstate2 / @c handleCursorSubstate3 /
 * @c adjustConfigParam) based on @c D_801D3358 (0..5), then calls
 * @c drawMenuPrim. Finally checks two completion triggers against
 * @c D_801D3334 / @c D_801D3330: bit-0xC0 sets @c D_801D3359=2 and
 * snapshots @c D_801D3340[D_801D3358] to @c D_801D335C; bit-0x10 sets
 * @c D_801D3359=3.
 *
 * @note The switch dispatcher's jump table lives at the fixed overlay
 *       offset @c 0x11C inside the @c be_dispatch region. The splat
 *       yaml carves a @c be_object2 .rodata subsegment there with
 *       @c linker_section_order: .text so the linker places
 *       @c be_object2.o(.rodata) (which contains the gcc-generated
 *       jtbl) at exactly that offset.
 */
void updateTriadMenu(void) {
    s32 state = *(u8 *)&D_801D3338;

    switch (state) {
    case 0:
    case 1:
        D_801D332C = D_801C2EC8[state];
        D_801D332E = D_801C2EB8[state];
        D_801D3330 = D_801C2EC0[state];
        break;
    case 2:
        D_801D332C = D_801C2EC8[0] | D_801C2EC8[1];
        D_801D332E = D_801C2EB8[0] | D_801C2EB8[1];
        D_801D3330 = D_801C2EC0[0] | D_801C2EC0[1];
        break;
    }

    if (D_801D3359 == 1) {
        s32   idx = D_801D3358 * 4;
        void *p   = &D_801D3340[D_801D3358];

        switch (D_801D3358) {
        case 0: break;
        case 1: handleCursorSubstate1(p, idx); break;
        case 2: handleCursorSubstate2(p, idx); break;
        case 3: handleCursorSubstate3(p, idx); break;
        case 4:
        case 5: adjustConfigParam(p);      break;
        }

        drawMenuPrim(D_801D3358, &D_801D3340[D_801D3358]);

        if (!(D_801D3334 & 1)) {
            if (D_801D3330 & 0xC0) {
                D_801D3359 = 2;
                memcpy(&D_801D335C, &D_801D3340[D_801D3358], 4);
                return;
            }
        }
        if (!(D_801D3334 & 2) && (D_801D3330 & 0x10)) {
            D_801D3359 = 3;
        }
    }
}

/**
 * @brief Activate a substate slot and seed its column cursor.
 *
 * Latches the substate index and per-call control fields into the global
 * tick state, OR-merging @p mask into @c D_801D3328 with the new
 * substate's mask bit set. Then, for substates 1 and 2, the active column
 * cursor (@c D_801D3340[idx].field2) is probed via @c func_8009A7A4 — if
 * the current position is not valid, it's nudged by +1 to find a valid
 * candidate.
 *
 *  - @c D_801D3358 = @p idx          (active substate index)
 *  - @c D_801D3328 = @p mask | (1<<@p idx)  (cumulative subscribed-substate mask)
 *  - @c D_801D3359 = 1               (arm completion)
 *  - @c D_801D3338 = @p stateByte    (state byte)
 *  - @c D_801D3334 = @p suppressFlags (completion-suppress flags)
 *
 * Substates 0 and 3..5 skip the cursor probe and return immediately.
 *
 * @param idx           Substate index (0..5).
 * @param mask          Caller-supplied subscriber mask (this substate's bit
 *                      is OR'd in before storing).
 * @param stateByte     State byte to latch into @c D_801D3338.
 * @param suppressFlags Completion-suppress flags to latch into @c D_801D3334.
 */
void activateMenuSubstate(s32 idx, s32 mask, u8 stateByte, s32 suppressFlags) {
    D_801D3358 = idx;
    D_801D3328 = mask | (1 << idx);
    D_801D3359 = 1;
    D_801D3338 = stateByte;
    D_801D3334 = suppressFlags;

    if (idx >= 3) return;
    if (idx <= 0) return;

    if (func_8009A7A4(idx - 1, 0, D_801D3340[idx].field2) < 0) {
        D_801D3340[idx].field2 = D_801D3340[idx].field2 + 1;
    }
}

/**
 * @brief Per-frame state machine for the player's Triple Triad card-selection cursor.
 *
 * Linked-list callback registered by @ref spawnCardSelectCursor on the
 * @c D_801D3380 list. Drives a 4-state machine stored in @p p->state
 * (offset @c 0x0C of the 20-byte node):
 *
 *  - state 0 : prime the column cursor for substate (@c fieldD+1) via
 *              @ref activateMenuSubstate, then advance to state 1.
 *  - state 1 : if @c D_801D3359 already equals the current state (= 1),
 *              the selection has already been committed — bail with @c 0.
 *              Otherwise probe the snapshot's column at @c D_801D335C.field2
 *              via @ref func_8009A7A4: on success, copy
 *              @c D_801D335C into the node's @c snapshot field, advance to
 *              state 2 and play SFX 1; on failure, fall back to state 0.
 *  - state 2 : arm row-cursor substate 3 via @ref activateMenuSubstate, advance
 *              to state 3.
 *  - state 3 : per-frame display tick:
 *              - if @c D_801C2DCA is set, draw the snapshot via
 *                @ref drawMenuPrim.
 *              - update the active cursor entity via @ref func_8009A878.
 *              - inspect @c D_801D3359 (= a UI trigger code):
 *                  * @c 2 : commit the chosen card. If the destination
 *                          board slot is already occupied (returned >= 0),
 *                          play SFX @c 0x10 and stay; otherwise allocate
 *                          a new battle entity, prime it with
 *                          @ref setBattleObjectAction and @ref commitCardToBoard,
 *                          play SFX 1, and return 2 (placement done).
 *                  * @c 1 : exit (return 0).
 *                  * @c 3 (matches current state) : cancel — play SFX 9,
 *                          reset to state 0.
 *
 * Bails out at the very top if the global "battle paused" flag
 * (@c D_801A2C74 bit @c 0x4) is clear AND @c D_801C2EC4 has bit @c 0x20
 * set — calls @ref func_800A26C8 (the "open battle menu" handler) and
 * returns 0 immediately.
 *
 * @param p Linked-list node owned by @c D_801D3380.
 * @return  @c 0 normally / when bailing on commit; @c 2 once a card has
 *          been placed on the board.
 */
s32 updateCardSelectCursor(SubstateMachineNode *p) {
    s32 s1;

    if (!(D_801A2C74 & 0x4) && (D_801C2EC4 & 0x20)) {
        func_800A26C8();
        return 0;
    }

    while (1) {
        s1 = p->state;
        switch (s1) {
        case 0:
            activateMenuSubstate(p->fieldD + 1, 0, p->fieldE, 2);
            p->state = 1;
            break;
        case 1:
            if (D_801D3359 == s1) return 0;
            if (func_8009A7A4(p->fieldD, 0, D_801D335C.field2) >= 0) {
                p->snapshot = D_801D335C;
                p->state = 2;
                func_800A233C(1);
            } else {
                p->state = 0;
            }
            break;
        case 2:
            activateMenuSubstate(3, 0, p->fieldE, 0);
            p->state = 3;
            break;
        case 3: {
            s32 trig;
            if (D_801C2DCA != 0) {
                drawMenuPrim(p->fieldD + 1, &p->snapshot);
            }
            func_8009A878(p->fieldD, p->snapshot.field2);
            trig = D_801D3359;
            switch (trig) {
            case 2:
                if (func_8009A7A4(2, D_801D335C.field0, D_801D335C.field2) < 0) {
                    s32 entIdx = func_8009A7A4(p->fieldD, 0, p->snapshot.field2);
                    setBattleObjectAction(entIdx, 2, D_801D335C.field0, D_801D335C.field2);
                    commitCardToBoard(&D_801D3398, entIdx, D_801D335C.field0, D_801D335C.field2);
                    func_800A233C(1);
                    return 2;
                }
                func_800A233C(0x10);
                p->state = trig;
                break;
            case 1:
                return 0;
            case 3:
                if (trig == s1) {
                    func_800A233C(9);
                    p->state = 0;
                }
                break;
            }
            break;
        }
        }
    }
}

/**
 * @brief Initialize the D_801D3380 linked list with a battle callback.
 *
 * Sets up D_801D3380 as a linked list (pool at D_801D3360, node size 0x14,
 * capacity 1), then appends updateCardSelectCursor as a callback. Sets byte fields
 * 0xC, 0xD, 0xE on the node from the parameters. Resets D_801D3340 fields
 * at +0xC and +0xE to 1.
 *
 * @param a0 Value stored at node byte 0xD.
 * @param a1 Value stored at node byte 0xE.
 * @return Pointer to D_801D3380 list header.
 */
u8 *spawnCardSelectCursor(s32 a0, s32 a1) {
    u8 *list = D_801D3380;
    u8 *node;

    func_80098BC0(list, D_801D3360, 0x14, 1);
    node = (u8 *)func_80098C44(list, (s32)updateCardSelectCursor);
    node[0xC] = 0;
    node[0xD] = a0;
    node[0xE] = a1;
    D_801D3340[3].field0 = 1;
    D_801D3340[3].field2 = 1;
    return list;
}

/**
 * @brief Set the type for a battle entity in g_tripleTriadCardHands and optionally trigger an effect.
 *
 * Computes entry at g_tripleTriadCardHands + a0 * 36, sets entry[1] = a1 (type),
 * clears entry[2..3]. If type is in range [2, 5], calls func_800A2364(0x5A, 1).
 *
 * @param a0 Entity index.
 * @param a1 Entity type.
 */
void setCardEntityType(s32 a0, s32 a1) {
    u8 *base = (u8 *)g_tripleTriadCardHands;
    u8 *entry = base + a0 * 36;
    entry[1] = a1;
    *(s16 *)(entry + 2) = 0;
    if (a1 < 6) {
        if (a1 >= 2) {
            func_800A2364(0x5A, 1);
        }
    }
}

/**
 * @brief Check if any battle entity in g_tripleTriadCardHands has a non-zero type.
 *
 * Iterates up to 10 entries (stride 36) and checks byte at offset 1.
 *
 * @return 1 if any entity has non-zero type, 0 otherwise.
 */
s32 anyCardEffectActive(void) {
    s32 i = 0;
    u8 *entry = (u8 *)g_tripleTriadCardHands;
    do {
        if (entry[1] != 0) {
            return 1;
        }
        i++;
        entry += 0x24;
    } while (i < 10);
    return 0;
}

/**
 * @brief Per-frame transform for a card-effect @c TripleTriadCardObject (state machine).
 *
 * Drives the per-frame animation update for a card's render state. The
 * function is dispatched once per frame from @c updateCardObject and chooses
 * its behavior from @c state (0..7):
 *  - @b 0 / @b 6 — idle: no update this frame.
 *  - @b 1 — open/flip sequence keyed by @c field02:
 *      - frames 0..19: ease-in rotation via @c rsin (raises @c offY);
 *        plays SFX 0x59 on the first frame.
 *      - frames 20..24: hold (no visual change).
 *      - frames 25..49: drop sequence: on frame 25 the @c param0..pad13
 *        block (the source colour/index quartet at 0x10) is copied over
 *        the active @c groupId..pad0F block at 0x0C, @c offSort sinks to
 *        @c -9, then the next 24 frames sweep @c offY and @c offZ via
 *        @c rsin/rcos.
 *      - frame 50+: terminate (state=0, @c offSort cleared).
 *  - @b 2..5 — slide-in trajectory: indexes a direction vector from
 *    @c D_80182D10 (one of 4 cardinals) and moves @c posData/field18 along
 *    that vector for 25 frames, with a small @c offZ bob and a single
 *    @c initFlags^=1 toggle on frame 12. Frames 25..29 then settle to
 *    @c offSort = 0; frame 30 terminates.
 *  - @b 7 — short shake/twitch animation: 10-frame @c offY sweep via
 *    @c rsin; on the last frame the cleanup clears @c offY / @c state /
 *    @c field02 and increments @c priority (the byte at @c 0x0E).
 *
 * @param entity Battle object slot being driven this frame.
 */
void func_8009C12C(TripleTriadCardObject *entity) {
    s32 state;
    s32 field02;
    s32 t;

    state = entity->state;
    field02 = entity->field02;

    switch (state) {
    case 0:
    case 6:
        break;

    case 1:
        if (field02 < 20) {
            if (field02 == 0) {
                func_800A233C(0x59);
            }
            t = ((field02 + 1) * 4096) / 20;
            entity->offY = -(rsin(t / 4) * 224) >> 12;
            entity->angle = 4096;
        } else {
            field02 -= 20;
            if (field02 < 5) {
                /* hold */
            } else {
                field02 -= 5;
                if (field02 < 25) {
                    if (field02 == 0) {
                        entity->posData[1] = 0;
                        *(Tetra4 *)&entity->groupId = *(Tetra4 *)&entity->param0;
                        entity->offSort = -9;
                    }
                    t = ((field02 + 1) * 4096) / 25;
                    entity->offY = ((rsin(t / 4) * 224) >> 12) - 224;
                    entity->offZ = (((4096 - rcos(t / 4)) * 128) >> 12) - 128;
                    entity->angle = 0;
                } else {
                    entity->state = 0;
                    entity->offSort = 0;
                }
            }
        }
        entity->field02++;
        break;

    case 2:
    case 3:
    case 4:
    case 5:
        state -= 2;
        if (field02 < 25) {
            if (field02 == 12) {
                entity->initFlags ^= 1;
            }
            t = ((field02 + 1) * 4096) / 25;
            entity->offZ = -((rsin(t / 2) * 128) >> 12);
            entity->offSort = -9;
            entity->posData[0] = (D_80182D10[state].vx * t) >> 12;
            entity->posData[1] = (D_80182D10[state].vy * t) >> 12;
            entity->field18   = (D_80182D10[state].vz * t) >> 12;
        } else {
            field02 -= 25;
            if (field02 < 5) {
                entity->offSort = 0;
            } else {
                entity->state = 0;
            }
        }
        entity->field02++;
        break;

    case 7:
        entity->field02++;
        t = (entity->field02 << 12) / 10;
        t = rsin(t / 4);
        entity->offY = (u32)t >> 7;
        if (entity->field02 >= 10) {
            entity->offY = 0;
            entity->state = 0;
            entity->field02 = 0;
            entity->priority++;
        }
        break;
    }
}

/**
 * @brief Emit a 62x62 semi-transparent gouraud quad over a card-effect node.
 *
 * Builds a @c POLY_G4 (semi-transparent code 0x3A) plus a @c DR_TPAGE in the
 * caller's primitive buffer and links both into @p ot. Each of the four
 * corners gets its own colour via @c DpqColor — the depth-queue input is
 * @c rsin(angle + D_80182D30[i]) clamped to @c >=0, so the corners
 * brighten/dim out of phase with each other as @p angle rotates.
 *
 * The quad spans pixels (spriteX+0xA1, spriteY+0x51) to (+0xDF, +0x8F)
 * relative to the @c BattleAnimNode anchor.
 *
 * @param node     Display-list node providing the screen-space anchor
 *                 (@c spriteX / @c spriteY at offsets 0x14 / 0x18).
 * @param angle    Animation phase added to each corner's @c D_80182D30 offset.
 * @param ot       Ordering-table bucket to link the two primitives into.
 * @param primBuf  Caller's primitive buffer; the @c POLY_G4 is written at
 *                 offset 0 (36 bytes) and the @c DR_TPAGE at offset 0x24
 *                 (8 bytes).
 * @return Pointer past the emitted primitives (@p primBuf + 0x2C).
 */
extern s32     D_80182D30[];     /**< Per-corner phase offsets for the gouraud flicker. */
extern CVECTOR D_80182D40;        /**< Base colour passed to @c DpqColor. */
extern CVECTOR *D_801D3390;       /**< Scratch walker: current vertex-colour slot in the @c POLY_G4. */

u8 *drawCardEffectQuad(BattleAnimNode *node, s32 angle, void *ot, u8 *primBuf) {
    POLY_G4 *poly = (POLY_G4 *)primBuf;

    D_801D3390 = ((CVECTOR *)poly) + 1;
    SetFarColor(0xFF, 0xFF, 0xFF);

    {
        s32 sinVal;
        s32 i;
        for (i = 0; i < 4; i++) {
            sinVal = rsin(angle + D_80182D30[i]);
            if (sinVal < 0) sinVal = 0;
            DpqColor(&D_80182D40, sinVal, D_801D3390);
            D_801D3390 += 2;
        }
    }

    poly->tag  = 0x08000000;
    poly->code = 0x3A;

    poly->x0 = poly->x2 = (u16)node->mat.t[0] + 0xA1;
    poly->x1 = poly->x3 = (u16)node->mat.t[0] + 0xDF;
    poly->y0 = poly->y1 = (u16)node->mat.t[1] + 0x51;
    poly->y2 = poly->y3 = (u16)node->mat.t[1] + 0x8F;

    AddPrim(ot, poly);

    {
        DR_TPAGE *tpage = (DR_TPAGE *)(poly + 1);
        SetDrawTPage(tpage, 1, 1, 0x20);
        AddPrim(ot, tpage);
        return (u8 *)(tpage + 1);
    }
}

/** @brief Card scale constant {0x1000, 0x1000, 0, 0} (1.0 in 12-bit fixed point),
 *         applied to a card's matrix during the slide-in animation. */
extern const VECTOR func_80098154;

/**
 * @brief Per-frame transform stage 2 for a card-effect @c TripleTriadCardObject.
 *
 * Dispatches on @c state, mirroring the small state machine in
 * @c func_8009C12C but handling the matrix transform + render side:
 *  - @b 0, @b 7+: nothing this frame.
 *  - @b 1..5 (slide-in trajectory): scale @c node's matrix by the constant
 *    @c cardScale, set @c worldZ to @c 0x200, push rotation/translation
 *    into the GTE, then walk the next display primitive via
 *    @c drawCardShadow into @p otBucket.
 *  - @b 6 (flip): drive an angle from @c field02 (0..19, swept over 20
 *    frames as @c (n+1)*4096/20 — a 12-bit fixed-point sweep through
 *    [0, 4096]) and emit the gouraud quad via @c drawCardEffectQuad into a fixed
 *    OT layer (@c &D_801C2EB0[6], vs the per-card sort-key layer used by the
 *    normal render). @c field02 advances; when it reaches @c 20 the state is
 *    cleared.
 *
 * @note The dispatch is written with @c goto so gcc emits a flat three-way
 *       branch (@c >=6 to the flip block, @c !=0 to the slide block, else
 *       fall through to return) with both bodies out-of-line. @c state is also
 *       copied into a second local (@c stateCopy) so the flip-state check binds
 *       a separate register, matching the original's codegen.
 *
 * @param entity   The card object being animated.
 * @param node     Its render node (transform matrix + base position).
 * @param otBucket OT layer the slide-in primitive links into.
 */
void transformCardEffect(TripleTriadCardObject *entity, BattleAnimNode *node, void *otBucket) {
    VECTOR scaleVec = func_80098154;
    s32 state = entity->state;
    u8 stateCopy;
    s32 field02 = entity->field02;

    stateCopy = state;
    if (state >= 6) {
        goto flip;
    }
    if (state != 0) {
        goto slide;
    }
    return;

flip:
    if (stateCopy == 6) {
        D_801C2EB4 = drawCardEffectQuad(node, ((field02 + 1) * 4096) / 20, &D_801C2EB0[6], D_801C2EB4);
        entity->field02++;
        if (entity->field02 >= 20) {
            entity->state = 0;
            entity->field02 = 0;
        }
    }
    return;

slide:
    ScaleMatrix(&node->mat, &scaleVec);
    node->mat.t[2] = 0x200;
    SetRotMatrix(&node->mat);
    SetTransMatrix(&node->mat);
    D_801C2EB4 = drawCardShadow(otBucket, D_801C2EB4);
}

/**
 * @brief Reset the Triple Triad board for a new game.
 *
 * Clears every slot's @c flags / @c element / @c elementMod across the 5x5
 * grid, marks the sentinel border (row/col 0 and 4) with @c TT_CELL_WALL so
 * edge neighbor lookups see a wall, and — when the Elemental rule is active
 * (@c TT_RULE_ELEMENTAL) — gives each interior cell (rows/cols 1..3) a 30%
 * chance of a random board element (@c 1 << rand%8).
 */
void resetTriadBoard(void) {
    s32 row;
    s32 col;

    for (row = 0; row < 5; row++) {
        for (col = 0; col < 5; col++) {
            D_801D3398.cells[row][col].flags = 0;
            D_801D3398.cells[row][col].element = 0;
            D_801D3398.cells[row][col].elementMod = 0;
        }
    }

    for (col = 0; col < 5; col++) {
        D_801D3398.cells[0][col].flags |= TT_CELL_WALL;
        D_801D3398.cells[4][col].flags |= TT_CELL_WALL;
    }

    for (row = 1; row < 4; row++) {
        D_801D3398.cells[row][0].flags |= TT_CELL_WALL;
        D_801D3398.cells[row][4].flags |= TT_CELL_WALL;
    }

    if (g_tripleTriadRules & TT_RULE_ELEMENTAL) {
        for (row = 1; row < 4; row++) {
            for (col = 1; col < 4; col++) {
                if (func_80023D04() % 100 < 30) {
                    D_801D3398.cells[row][col].element = 1 << (func_80023D04() % 8);
                }
            }
        }
    }
}

/**
 * @brief Place a Triple Triad card on the board and apply the Elemental rule.
 *
 * Clears the per-turn flags (@c JUST_PLACED / @c SAME_MATCHED / @c PLUS_COMBO)
 * across the playable 3x3 cells, then writes the card into
 * @c board->cells[row+1][col+1]: stores @c cardId and @c owner and marks the
 * cell @c OCCUPIED | @c JUST_PLACED. If the cell carries an element and the
 * Elemental rule (@c TT_RULE_ELEMENTAL) is active, sets @c elementMod to +1
 * when the card's element matches the cell's element, or -1 when it differs
 * (the modifier is later added to each of the placed card's edges).
 *
 * @param board  The Triple Triad board.
 * @param cardId Card stat index (into @c g_tripleTriadCardStats).
 * @param owner  Placing player (0 or 1).
 * @param col    Play-area column (cell column is @c col+1).
 * @param row    Play-area row (cell row is @c row+1).
 * @return The placed board cell.
 */
TripleTriadBoardSlot *placeCard(TripleTriadBoard *board, s32 cardId, s32 owner, s32 col, s32 row) {
    s32 r, c;
    TripleTriadBoardSlot *cell;
    s32 e;

    for (r = 1; r < 4; r++) {
        for (c = 1; c < 4; c++) {
            board->cells[r][c].flags &= ~(TT_CELL_JUST_PLACED | TT_CELL_SAME_MATCHED | TT_CELL_PLUS_COMBO);
        }
    }

    cell = &board->cells[row + 1][col + 1];
    cell->owner = owner;
    e = cell->element;
    cell->cardId = cardId;
    cell->flags |= TT_CELL_OCCUPIED | TT_CELL_JUST_PLACED;
    if (e != 0) {
        if (g_tripleTriadCardStats[cardId & 0xFF].element & e) {
            if (g_tripleTriadRules & TT_RULE_ELEMENTAL) {
                cell->elementMod = 1;
            }
        } else {
            if (g_tripleTriadRules & TT_RULE_ELEMENTAL) {
                cell->elementMod = -1;
            }
        }
    }
    return cell;
}

/**
 * @brief Place a battle entity's card onto the Triple Triad board.
 *
 * Looks up the @c TripleTriadCardObject at @p entityIdx, places its
 * @c cardId (owner taken from @c initFlags & @c TT_OWNER_MASK) at board
 * cell (@p col, @p row) via @ref placeCard, then tags the placed cell
 * with @p entityIdx so the cell's flip animation is driven by that card.
 *
 * @param board     The 5x5 Triple Triad board.
 * @param entityIdx Slot index into @c g_tripleTriadCardHands.
 * @param col       Board column.
 * @param row       Board row.
 */
void commitCardToBoard(TripleTriadBoard *board, s32 entityIdx, s32 col, s32 row) {
    TripleTriadBoardSlot *cell;

    cell = placeCard(board, g_tripleTriadCardHands[entityIdx].cardId,
                     g_tripleTriadCardHands[entityIdx].initFlags & TT_OWNER_MASK, col, row);
    cell->entityIdx = entityIdx;
}

/**
 * @brief Triple Triad basic-capture rule (with FF8 Elemental modifier).
 *
 * Runs unconditionally after Same/Plus. For each newly-placed card
 * (@c TT_CELL_JUST_PLACED), checks the 4 neighbors and captures any
 * weaker-edge neighbor of a different owner. Each edge value is adjusted
 * by the cell's @c elementMod (signed +1/-1 from the Elemental rule), so
 * the actual comparison is @c (myEdge + myElementMod) vs
 * @c (nbrEdge + nbrElementMod).
 *
 * Already-flipped neighbors from Same/Plus are skipped automatically via
 * the same-owner check.
 *
 * @param board The 5x5 Triple Triad board.
 * @return Number of cards captured this call.
 */
s32 applyBasicCapture(TripleTriadBoard *board) {
    s32 captureCount;
    s32 row, col, dir;
    s32 nbrCol, nbrRow;
    u8 cellOwner, myEdge, nbrEdge;
    s8 cellMod, nbrMod;
    TripleTriadBoardSlot *cell;
    TripleTriadBoardSlot *neighbor;
    TripleTriadCard *cellCard;
    TripleTriadCard *neighborCard;
    TripleTriadDirection *dirOffset;

    captureCount = 0;

    for (row = 1; row <= 3; row++) {
        for (col = 1; col <= 3; col++) {
            cell = &board->cells[row][col];
            if (!(cell->flags & TT_CELL_JUST_PLACED))
                continue;

            cellOwner = cell->owner;
            cellCard  = &g_tripleTriadCardStats[cell->cardId];

            for (dir = 0; dir < TT_DIR_COUNT; dir++) {
                dirOffset = &g_tripleTriadDirectionOffsets[dir];
                nbrCol = col + dirOffset->dx;
                nbrRow = row + dirOffset->dy;
                neighbor = &board->cells[nbrRow][nbrCol];

                if (!(neighbor->flags & TT_CELL_OCCUPIED))
                    continue;
                neighborCard = &g_tripleTriadCardStats[neighbor->cardId];
                if (neighbor->owner == cellOwner)
                    continue;

                myEdge  = cellCard->sides[dir];
                cellMod = cell->elementMod;
                nbrEdge = neighborCard->sides[dir ^ 1];
                nbrMod  = neighbor->elementMod;

                if (!((myEdge + cellMod) > (nbrEdge + nbrMod)))
                    continue;

                neighbor->owner = cellOwner;
                neighbor->flags |= 1 << (dir + 3);   /* captured-from-dir bit */
                captureCount++;
            }
        }
    }

    return captureCount;
}

/**
 * @brief Triple Triad Same-rule resolver (including the Same-Wall extension).
 *
 * Walks the 3x3 active play area; for each newly-placed card
 * (@c TT_CELL_JUST_PLACED), counts how many neighbors have a matching edge
 * value (or, if Same-Wall is enabled, a rank-A edge facing a wall sentinel).
 * If at least 2 matches occur, captures all weaker-or-equal-edge neighbors
 * (different owner) — flipping the owner and recording the direction in the
 * @c flags field.
 *
 * @param board The 5x5 Triple Triad board (typically @c &g_tripleTriadBoard).
 * @return Number of cards captured this call.
 */
s32 applySameRule(TripleTriadBoard *board) {
    s32 captureCount;
    s32 row, col, dir;
    s32 matchCount;
    s32 nbrCol, nbrRow;
    u8 cellOwner, myEdge, neighborOpposingEdge;
    TripleTriadBoardSlot *cell;
    TripleTriadBoardSlot *neighbor;
    TripleTriadCard *myCardStats;
    TripleTriadCard *neighborStats;
    TripleTriadDirection *dirOffset;

    captureCount = 0;

    for (row = 1; row <= 3; row++) {
        for (col = 1; col <= 3; col++) {
            cell = &board->cells[row][col];

            if (!(cell->flags & TT_CELL_JUST_PLACED))
                continue;

            cellOwner   = cell->owner;
            myCardStats = &g_tripleTriadCardStats[cell->cardId];
            matchCount  = 0;

            for (dir = 0; dir < TT_DIR_COUNT; dir++) {
                dirOffset = &g_tripleTriadDirectionOffsets[dir];
                nbrCol = col + dirOffset->dx;
                nbrRow = row + dirOffset->dy;
                neighbor = &board->cells[nbrRow][nbrCol];

                if (g_tripleTriadRules & TT_RULE_SAME_WALL) {
                    if (myCardStats->sides[dir] == TT_RANK_A) {
                        if (neighbor->flags & TT_CELL_WALL) {
                            matchCount++;
                            continue;
                        }
                    }
                }

                if (!(neighbor->flags & TT_CELL_OCCUPIED))
                    continue;

                neighborStats        = &g_tripleTriadCardStats[neighbor->cardId];
                myEdge               = myCardStats->sides[dir];
                neighborOpposingEdge = neighborStats->sides[dir ^ 1];

                if (myEdge == neighborOpposingEdge) {
                    neighbor->flags |= TT_CELL_SAME_MATCHED;
                    matchCount++;
                }
            }

            if (matchCount < 2)
                continue;

            /* Same rule fired: capture all weaker-or-equal neighbors */
            for (dir = 0; dir < TT_DIR_COUNT; dir++) {
                dirOffset = &g_tripleTriadDirectionOffsets[dir];
                nbrCol = col + dirOffset->dx;
                nbrRow = row + dirOffset->dy;
                neighbor = &board->cells[nbrRow][nbrCol];

                if (!(neighbor->flags & TT_CELL_OCCUPIED))
                    continue;
                neighborStats = &g_tripleTriadCardStats[neighbor->cardId];
                if (neighbor->owner == cellOwner)
                    continue;

                myEdge               = myCardStats->sides[dir];
                neighborOpposingEdge = neighborStats->sides[dir ^ 1];

                if (myEdge < neighborOpposingEdge)
                    continue;

                neighbor->owner = cellOwner;
                neighbor->flags |= 1 << (dir + 3);   /* captured-from-dir bit */
                cell->flags     |= TT_CELL_SAME_MATCHED;
                captureCount++;
            }
        }
    }

    return captureCount;
}

/**
 * @brief Triple Triad Plus-rule resolver.
 *
 * Walks the 3x3 active play area; for each newly-placed card (@c TT_CELL_JUST_PLACED),
 * builds a histogram of @c (myEdge + neighborOppositeEdge) sums across the
 * 4 cardinal neighbors. If any sum is hit by >=2 neighbors, the Plus rule
 * fires and those neighbors are captured (owner flipped, capture-direction
 * bit set in @c flags) provided they aren't already on the placer's side.
 *
 * The 5-wide board layout with a 1-cell sentinel border means neighbor
 * lookups at the edges fall through cleanly without bounds checks.
 *
 * @param board The 5xN Triple Triad board (typically @c D_801D3398).
 * @return Number of cards captured this call.
 */
s32 applyPlusRule(TripleTriadBoardSlot board[][TT_BOARD_COLS]) {
    s32 winningSum;
    s32 captures;
    s32 row, col, i;
    s32 maxCount;
    s32 edgeSum;
    s32 nbrCol;
    u8 cellOwner;
    TripleTriadPlusBucket *bucket;
    TripleTriadBoardSlot *cell;
    TripleTriadBoardSlot *neighbor;
    TripleTriadCard *cellCard;
    TripleTriadDirection *offset;
    TripleTriadPlusBucket sumHist[21];   /* indexed by edge-sum value (0..20) */

    captures = 0;

    for (row = 1; row < 4; row++) {
        for (col = 1; col < 4; col++) {
            cell = &board[row][col];
            if (cell->flags & TT_CELL_JUST_PLACED) {
                cellCard = &g_tripleTriadCardStats[cell->cardId];
                cellOwner = cell->owner;

                for (i = 0; i < 21; i++) {
                    sumHist[i].count = 0;
                    sumHist[i].dirMask = 0;
                }

                maxCount = 0;
                for (i = 0; i < 4; i++) {
                    offset = &g_tripleTriadDirectionOffsets[i];
                    nbrCol = col + offset->dx;
                    neighbor = &board[row + offset->dy][nbrCol];
                    if (neighbor->flags & TT_CELL_OCCUPIED) {
                        edgeSum = cellCard->sides[i] +
                                  g_tripleTriadCardStats[neighbor->cardId].sides[i ^ 1];
                        bucket = &sumHist[edgeSum];
                        bucket->count++;
                        bucket->dirMask |= 1 << i;
                        if (bucket->count > maxCount) {
                            maxCount = bucket->count;
                            winningSum = edgeSum;
                        }
                    }
                }

                if (maxCount >= 2) {
                    for (i = 0; i < 4; i++) {
                        offset = &g_tripleTriadDirectionOffsets[i];
                        nbrCol = col + offset->dx;
                        neighbor = &board[row + offset->dy][nbrCol];
                        if ((neighbor->flags & TT_CELL_OCCUPIED) &&
                            ((sumHist[winningSum].dirMask >> i) & 1)) {
                            neighbor->flags |= TT_CELL_PLUS_COMBO;
                            if (neighbor->owner != cellOwner) {
                                neighbor->owner = cellOwner;
                                neighbor->flags |= 1 << (i + 3);   /* captured-from-dir bit */
                                captures++;
                                cell->flags |= TT_CELL_PLUS_COMBO;
                            }
                        }
                    }
                }
            }
        }
    }

    return captures;
}

/**
 * @brief Triple Triad rule orchestrator — runs the rule cascade for a placed card.
 *
 * Resolves all captures triggered by a newly-placed card. If @p mode bit 0 is
 * set, runs the optional rules (Same and Plus) before the unconditional basic
 * capture; otherwise skips straight to basic. Same/Plus are gated on
 * @c g_tripleTriadRules bits @c TT_RULE_SAME / @c TT_RULE_PLUS being active.
 * After all captures, clears @c TT_CELL_JUST_PLACED from the 3x3 active grid
 * so the rules don't fire again next turn.
 *
 * The bit-coded return value lets callers drive the combo loop: after the
 * initial @c mode=1 call, any flipped neighbors are themselves "just placed"
 * and the caller invokes this function again with @c mode=prev_result (bit 0
 * cleared) to do a basic-rule-only combo sweep.
 *
 * @param board The 5x5 Triple Triad board.
 * @param mode  Bit 0 = run Same/Plus rules (initial call); cleared on combo
 *              re-entry. Other bits carried through from prior call's return.
 * @return Bitmap of what fired:
 *           @c 0x2 — basic capture flipped a card (combo step only),
 *           @c 0x4 — Same rule fired,
 *           @c 0x8 — Plus rule fired.
 */
s32 applyCardRules(TripleTriadBoard *board, s32 mode) {
    s32 result;
    s32 row, col;

    result = 0;

    if (mode & 1) {
        if (g_tripleTriadRules & TT_RULE_SAME) {
            result = (applySameRule(board) != 0) << 2;
        }
        if (g_tripleTriadRules & TT_RULE_PLUS) {
            if (applyPlusRule((TripleTriadBoardSlot (*)[TT_BOARD_COLS])board) != 0) {
                result |= 0x8;
            }
        }
    }

    if (applyBasicCapture(board) != 0) {
        if (!(mode & 1)) {
            result |= 0x2;
        }
    }

    for (row = 1; row <= 3; row++) {
        for (col = 1; col <= 3; col++) {
            board->cells[row][col].flags &= ~TT_CELL_JUST_PLACED;
        }
    }

    return result;
}

/**
 * @brief Resolve captured cells: trigger flip animations and clear capture bits.
 *
 * Walks the playable 3x3 cells. For each cell that was captured this turn
 * (any @c TT_CELL_CAP_FROM_* bit set), finds the capture direction via the
 * @c D_80182D54 table (matching the cell's captured-from bit against
 * @c CaptureDir.capBit) and drives that cell entity's flip animation with
 * @c setCardEntityType. It then clears the captured-from bits and marks the cell
 * @c TT_CELL_JUST_PLACED so the next rule pass re-evaluates it.
 *
 * @param board The Triple Triad board.
 */
void resolveCaptures(TripleTriadBoard *board) {
    s32 row, col, dir;

    for (row = 1; row <= 3; row++) {
        for (col = 1; col <= 3; col++) {
            s32 flags = board->cells[row][col].flags;
            if (flags & TT_CELL_CAP_FROM_MASK) {
                for (dir = 0; dir < 4; dir++) {
                    if (flags & D_80182D54[dir].capBit) {
                        setCardEntityType(board->cells[row][col].entityIdx, D_80182D54[dir].animType);
                        break;
                    }
                }
                flags &= ~TT_CELL_CAP_FROM_MASK;
                flags |= TT_CELL_JUST_PLACED;
                board->cells[row][col].flags = flags;
            }
        }
    }
}

/**
 * @brief Score a Triple Triad board for @p player (AI board evaluation).
 *
 * The static evaluator the minimax (@ref searchBestMoveStack) calls at its leaves.
 * Sums three contributions into a signed score (higher = better for @p player):
 *  - **Board material**: for every occupied cell in the 3x3 play area, take
 *    @c D_801D35C8 + the card's value (@c D_801D35E0[cardId]) and add it if
 *    @p player owns the cell, subtract it otherwise.
 *  - **Hand potential**: for each still-playable card in @p player's hand
 *    (@c id != 0xFF), add @c (cardValue * D_801D35D8) >> 12.
 *  - **Random tiebreaker**: when @c D_801D35D0 is nonzero, add
 *    @c rand() % (D_801D35D0 + 1) so equal positions don't always tie.
 *
 * @param board  The 5x5 board (the play area is rows/cols 1..3).
 * @param player The seat (0 or 1) to score for.
 * @return The position's heuristic score for @p player.
 */
s32 evaluateBoard(TripleTriadBoard *board, s32 player) {
    s32 score;
    s32 row, col;
    s32 slot;

    score = 0;
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            TripleTriadBoardSlot *cell = &board->cells[row + 1][col + 1];
            if (cell->flags & TT_CELL_OCCUPIED) {
                s32 val = D_801D35C8 + D_801D35E0[cell->cardId];
                if (cell->owner == player) {
                    score += val;
                } else {
                    score -= val;
                }
            }
        }
    }

    for (slot = 0; slot < 5; slot++) {
        if (D_801D3570[player].cards[slot].id != 0xFF) {
            score += (D_801D35E0[D_801D3570[player].cards[slot].id] * D_801D35D8) >> 12;
        }
    }

    if (D_801D35D0 != 0) {
        return score + func_80023D04() % (D_801D35D0 + 1);
    }

    return score;
}

/**
 * @brief Recursive Triple Triad AI move search (resumable depth-limited minimax).
 *
 * Tries every move at this ply: for each (card, row, col) combination selected
 * by the iterators in @p node, skips played cards (id 0xFF) and occupied cells,
 * then copies @p board, places the card (@ref placeCard), cascades the capture
 * rules (@ref applyCardRules), and recurses one ply deeper with the opponent to
 * move (child state in @c node[1], the played card masked out of the hand). A
 * child that ran out of depth (result 1) is scored with @ref evaluateBoard from
 * the AI seat's perspective; a child that completed its own pass (result 0)
 * reports back through its @c bestScore / @c bestWeighted fields. The raw score
 * is tracked in @c node->bestScore and its difficulty-weighted counterpart
 * (scaled by @c D_801D35D4 in 4.12 fixed point whenever a capture cascade
 * occurred on the AI seat's own placement) in @c node->bestWeighted; the AI
 * seat maximizes the weighted score while the opponent minimizes the raw one,
 * and the winning move is recorded in @c bestCol / @c bestRow / @c bestCard.
 * After a node completes a full pass it latches its result into @c bound and
 * sets @c checkBound, letting later passes prune (by forcing the iterators to
 * the wrap position) as soon as a partial result already beats it.
 *
 * The search is time-sliced: every placement decrements the shared budget
 * @c D_801D3538, and the recursion unwinds with result 2 once it hits zero.
 * Because each ply's iterators persist in @c D_801D3460, the next slice
 * resumes exactly where this one yielded.
 *
 * @param board  Position to search (copied per placement; never modified).
 * @param player Seat to move at this ply (0/1).
 * @param node   This ply's search state in the @c D_801D3460 workspace; the
 *               child ply uses @c node[1].
 * @param depth  Remaining search depth in plies.
 *
 * @return 0 when this node completed a full pass (best move/scores recorded),
 *         1 if @p depth was already exhausted (caller evaluates the position),
 *         2 if the @c D_801D3538 budget ran out (yield; resume next slice),
 *         3 if the root advanced to its next hand card.
 */
s32 searchBestMoveStack(TripleTriadBoard *board, s32 player, AiMove *node, s32 depth) {
    TripleTriadBoard boardCopy;
    s32 cardId;
    s32 weight;
    s32 weighted;
    s32 score;
    s32 ruleMode;
    s32 result;
    s32 product;

    if (depth <= 0) {
        return 1;
    }
    while (D_801D3538 > 0) {
        cardId = D_801D3570[player].cards[node->card].id;
        if (cardId != 0xFF) {
            if (!(board->cells[node->row + 1][node->col + 1].flags & TT_CELL_OCCUPIED)) {
                D_801D3538--;
                boardCopy = *board;

                placeCard(&boardCopy, cardId, player, node->col, node->row);
                ruleMode = 1;
                weight = 0x1000;
                while (applyCardRules(&boardCopy, ruleMode) != 0) {
                    ruleMode &= -2;
                    if (player == D_801D35C0) {
                        weight = D_801D35D4;
                    }
                }

                D_801D3570[player].cards[node->card].id = 0xFF;
                result = searchBestMoveStack(&boardCopy, player ^ 1, &node[1], depth - 1);
                D_801D3570[player].cards[node->card].id = cardId;

                switch (result) {
                case 0:
                    score = node[1].bestScore;
                    product = node[1].bestWeighted * weight;
                    weighted = product >> 12;
                    break;
                case 1: {
                    s32 eval = evaluateBoard(&boardCopy, D_801D35C0);
                    product = eval * weight;
                    score = eval;
                    weighted = product >> 12;
                    break;
                }
                case 2:
                    return 2;
                }

                if (player == D_801D35C0) {
                    if (node->noBest || node->bestWeighted < weighted) {
                        node->bestWeighted = weighted;
                        node->bestScore = score;
                        node->noBest = 0;
                        node->bestCol = node->col;
                        node->bestRow = node->row;
                        node->bestCard = node->card;
                    }
                } else {
                    if (node->noBest || score < node->bestScore) {
                        node->bestWeighted = weighted;
                        node->bestScore = score;
                        node->noBest = 0;
                        node->bestCol = node->col;
                        node->bestRow = node->row;
                        node->bestCard = node->card;
                    }
                }

                if (node->checkBound) {
                    if (player == D_801D35C0) {
                        if (node->bound < score) {
                            node->col = 2;
                            node->row = 2;
                            node->card = 4;
                        }
                    } else {
                        if (weighted < node->bound) {
                            node->col = 2;
                            node->row = 2;
                            node->card = 4;
                        }
                    }
                }
            }
        }

        node->col++;
        if (node->col >= 3) {
            node->col = 0;
            node->row++;
            if (node->row >= 3) {
                node->row = 0;
                node->card++;
                if (node->card >= 5) {
                    node->card = 0;
                    node->noBest = 1;
                    node->checkBound = 1;
                    if (player == D_801D35C0) {
                        node->bound = node->bestScore;
                    } else {
                        node->bound = node->bestWeighted;
                    }
                    return 0;
                }
                if (node == D_801D3460) {
                    return 3;
                }
            }
        }
    }
    return 2;
}

/**
 * @brief Live entry point of the AI move search (pool-allocated twin of
 *        @ref searchBestMoveStack).
 *
 * Identical search to @ref searchBestMoveStack — same resumable time-sliced minimax
 * over the @c D_801D3460 ply workspace, same scoring, pruning and return codes —
 * but each recursion level draws its scratch board from the per-frame work pool
 * (@c func_80098B80) instead of the CPU stack, returning it (@c func_80098BA0)
 * on every exit after the allocation. This is the variant @ref updateAiTurn
 * actually runs each AI turn; @ref searchBestMoveStack (stack-allocated copy, no
 * in-overlay caller) appears to be its unused sibling.
 *
 * @param board  Position to search (copied per placement; never modified).
 * @param player Seat to move at this ply (0/1).
 * @param node   This ply's search state in the @c D_801D3460 workspace; the
 *               child ply uses @c node[1].
 * @param depth  Remaining search depth in plies.
 *
 * @return 0 when this node completed a full pass (best move/scores recorded),
 *         1 if @p depth was already exhausted (caller evaluates the position),
 *         2 if the @c D_801D3538 budget ran out (yield; resume next slice),
 *         3 if the root advanced to its next hand card.
 */
s32 searchBestMove(TripleTriadBoard *board, s32 player, AiMove *node, s32 depth) {
    TripleTriadBoard *boardCopy;
    s32 cardId;
    s32 weight;
    s32 weighted;
    s32 score;
    s32 ruleMode;
    s32 result;
    s32 product;

    if (depth <= 0) {
        return 1;
    }
    boardCopy = (TripleTriadBoard *)func_80098B80(sizeof(TripleTriadBoard));
    while (D_801D3538 > 0) {
        cardId = D_801D3570[player].cards[node->card].id;
        if (cardId != 0xFF) {
            if (!(board->cells[node->row + 1][node->col + 1].flags & TT_CELL_OCCUPIED)) {
                D_801D3538--;
                *boardCopy = *board;

                placeCard(boardCopy, cardId, player, node->col, node->row);
                ruleMode = 1;
                weight = 0x1000;
                while (applyCardRules(boardCopy, ruleMode) != 0) {
                    ruleMode &= -2;
                    if (player == D_801D35C0) {
                        weight = D_801D35D4;
                    }
                }

                D_801D3570[player].cards[node->card].id = 0xFF;
                result = searchBestMove(boardCopy, player ^ 1, &node[1], depth - 1);
                D_801D3570[player].cards[node->card].id = cardId;

                switch (result) {
                case 0:
                    score = node[1].bestScore;
                    product = node[1].bestWeighted * weight;
                    weighted = product >> 12;
                    break;
                case 1: {
                    s32 eval = evaluateBoard(boardCopy, D_801D35C0);
                    product = eval * weight;
                    score = eval;
                    weighted = product >> 12;
                    break;
                }
                case 2:
                    func_80098BA0(sizeof(TripleTriadBoard));
                    return 2;
                }

                if (player == D_801D35C0) {
                    if (node->noBest || node->bestWeighted < weighted) {
                        node->bestWeighted = weighted;
                        node->bestScore = score;
                        node->noBest = 0;
                        node->bestCol = node->col;
                        node->bestRow = node->row;
                        node->bestCard = node->card;
                    }
                } else {
                    if (node->noBest || score < node->bestScore) {
                        node->bestWeighted = weighted;
                        node->bestScore = score;
                        node->noBest = 0;
                        node->bestCol = node->col;
                        node->bestRow = node->row;
                        node->bestCard = node->card;
                    }
                }

                if (node->checkBound) {
                    if (player == D_801D35C0) {
                        if (node->bound < score) {
                            node->col = 2;
                            node->row = 2;
                            node->card = 4;
                        }
                    } else {
                        if (weighted < node->bound) {
                            node->col = 2;
                            node->row = 2;
                            node->card = 4;
                        }
                    }
                }
            }
        }

        node->col++;
        if (node->col >= 3) {
            node->col = 0;
            node->row++;
            if (node->row >= 3) {
                node->row = 0;
                node->card++;
                if (node->card >= 5) {
                    node->card = 0;
                    node->noBest = 1;
                    node->checkBound = 1;
                    if (player == D_801D35C0) {
                        node->bound = node->bestScore;
                    } else {
                        node->bound = node->bestWeighted;
                    }
                    func_80098BA0(sizeof(TripleTriadBoard));
                    return 0;
                }
                if (node == D_801D3460) {
                    func_80098BA0(sizeof(TripleTriadBoard));
                    return 3;
                }
            }
        }
    }
    func_80098BA0(sizeof(TripleTriadBoard));
    return 2;
}

/**
 * @brief Advance the AI player's Triple Triad turn: search for a move, play the
 *        selection animation, then commit the chosen card to the board.
 *
 * Runs a small state machine over @p a (re-entered once per frame) and is a
 * no-op while card input is globally disabled (@c D_801A2C74 bit @c 0x4).
 *
 *  - @b State @b 0 — reset the 9-entry move workspace @c D_801D3460, enter state 1.
 *  - @b State @b 1 — three sub-steps:
 *      - @e sub @e 0: arm the search timer (@c D_801D353C = 10), latch the chosen
 *        card slot (@c D_801D3462) into @p a, advance.
 *      - @e sub @e 1: highlight the card (@ref func_8009A878), refill the minimax
 *        placement budget (@c D_801D3538 = 100) and run the move search
 *        (@ref searchBestMove). If it reports busy (result 2 — budget exhausted)
 *        tick the search timer and yield; otherwise, if the chosen card is
 *        already played (id @c 0xFF) clear the timer, advance.
 *      - @e sub @e 2: keep highlighting; while the search timer is positive tick it
 *        and yield. Once it expires, restart the search (sub 0) if the result was 3,
 *        else advance to state 2.
 *  - @b State @b 2 — animate the placement card (@c D_801D3466) for 15 frames, then
 *    enter state 3.
 *  - @b State @b 3 — set the card object's action (@ref setBattleObjectAction) and
 *    commit it to the board (@ref commitCardToBoard).
 *
 * @param a AI turn/placement controller for the current player.
 * @return 2 once the card has been placed (state 3); 0 while still working or when
 *         input is disabled.
 */
s32 updateAiTurn(func_8009DBE8_arg0 *a) {
    TripleTriadBoard board;

    if (D_801A2C74 & 4) {
        return 0;
    }

    while (1) {
        switch (a->state) {
        case AI_TURN_INIT: {
            s32 i;
            for (i = 0; i < 9; i++) {
                D_801D3460[i].col = 0;
                D_801D3460[i].row = 0;
                D_801D3460[i].card = 0;
                D_801D3460[i].noBest = 1;
                D_801D3460[i].checkBound = 0;
            }
            a->state = AI_TURN_SEARCH;
            a->sub = AI_SEARCH_PREP;
            break;
        }
        case AI_TURN_SEARCH: {
            s32 sub = a->sub;
            switch (sub) {
            case AI_SEARCH_PREP:
                D_801D353C = 10;
                a->cardSlot = D_801D3462;
                a->sub++;
                break;
            case AI_SEARCH_RUN:
                func_8009A878(D_801D35C0, a->cardSlot);
                D_801D3538 = 100;
                a->result = searchBestMove(&D_801D3398, D_801D35C0, D_801D3460, a->unk10);
                if ((a->result & 0xFF) == 2) {
                    D_801D353C--;
                    return 0;
                }
                if (D_801D3570[D_801D35C0].cards[a->cardSlot].id == 0xFF) {
                    D_801D353C = 0;
                }
                a->sub++;
                break;
            case AI_SEARCH_WAIT:
                func_8009A878(D_801D35C0, a->cardSlot);
                if (D_801D353C > 0) {
                    D_801D353C--;
                    return 0;
                }
                if (a->result == 3) {
                    a->sub = AI_SEARCH_PREP;  /* re-run the search from the top */
                } else {
                    /* sub == AI_TURN_ANIMATE here; assign the cached local (not the
                       literal) — that variable reuse is what makes the prologue match. */
                    a->state = sub;
                    a->sub = 0;               /* start the ANIMATE frame counter */
                }
                break;
            }
            break;
        }
        case AI_TURN_ANIMATE:
            func_8009A878(D_801D35C0, D_801D3466);
            a->sub++;
            if ((a->sub & 0xFF) < AI_ANIM_FRAMES) {
                return 0;
            }
            a->state = AI_TURN_PLACE;
            a->sub = 0;
            break;
        case AI_TURN_PLACE:
            setBattleObjectAction(
                D_801D3570[D_801D35C0].cards[D_801D3460[0].bestCard].entityIdx, 2,
                D_801D3460[0].bestCol, D_801D3460[0].bestRow);
            commitCardToBoard(&D_801D3398,
                D_801D3570[D_801D35C0].cards[D_801D3460[0].bestCard].entityIdx,
                D_801D3460[0].bestCol, D_801D3460[0].bestRow);
            return 2;
        }
    }
}

/**
 * @brief Set up the AI player's Triple Triad turn and spawn its turn handler.
 *
 * Runs once when it becomes seat @p seat's turn:
 *  1. Builds both players' working search hands @c D_801D3570 from the live card
 *     objects @c g_tripleTriadCardHands (via @ref func_8009A7A4): each slot's
 *     @c entityIdx is the object index and @c id its card id (@c 0xFF if the slot
 *     holds no card); @c slotCount tracks how many cards remain in play.
 *  2. If the Open rule is off (@c g_tripleTriadRules bit 0) and the matching
 *     config bit is clear, randomizes the opponent's hidden hand — each guessed
 *     id is @c (rand()%(cardLevel+1))*11 + rand()%11, seeded from the real hand
 *     card levels in @c D_801A2C48.
 *  3. Spawns the per-frame turn handler: a list node with @ref updateAiTurn as
 *     its callback, seeds its search-depth @c unk10 from @c D_80182D64, loads the
 *     difficulty weight row from @c D_80182DAC into @c D_801D35C8..D_801D35D8, and
 *     fills the per-card value table @c D_801D35E0[0..109].
 *
 * @param seat Player/seat index taking the turn.
 * @return The AI turn-node list head (@c D_801D3560).
 */
u8 *spawnAiTurn(s32 seat) {
    func_8009DBE8_arg0 *node;
    s32 mult;
    s32 slotCount;
    s32 player;
    s32 weight;
    s32 card;
    s32 i;

    slotCount = 9;
    for (player = 0; player < 2; player++) {
        for (card = 0; card < 5; card++) {
            s32 entity;
            entity = func_8009A7A4(player, 0, card);
            D_801D3570[player].cards[card].entityIdx = entity;
            if (entity < 0) {
                slotCount--;
                D_801D3570[player].cards[card].id = 0xFF;
            } else {
                D_801D3570[player].cards[card].id = g_tripleTriadCardHands[entity].cardId;
            }
        }
    }

    if (!(g_tripleTriadRules & 1) && !(D_80082C97 & 0x10)) {
        s32 opp = seat ^ 1;
        for (i = 0; i < 5; i++) {
            s32 r1, r2, divisor;
            D_801D3570[opp].cards[i].entityIdx = -1;
            r1 = func_80023D04();
            r2 = func_80023D04();
            divisor = D_801A2C48[seat][i] / 11 + 1;
            D_801D3570[opp].cards[i].id = (r1 % divisor) * 11 + r2 % 11;
        }
    }

    func_80098BC0(D_801D3560, D_801D3540, 0x14, 1);
    node = (func_8009DBE8_arg0 *)func_80098C44(D_801D3560, (s32)updateAiTurn);
    D_801D35C0 = seat;
    node->seat = seat;
    {
        s32 depth = D_80182D64[D_80082C90.field_07 & 7][slotCount - 1];
        node->state = 0;
        node->sub = 0;
        node->unk10 = depth;
    }

    mult = D_80182DAC[D_80082C90.field_06 & 7].w1;
    D_801D35C8 = D_80182DAC[D_80082C90.field_06 & 7].w0;
    D_801D35CC = mult;
    D_801D35D0 = D_80182DAC[D_80082C90.field_06 & 7].w2;
    D_801D35D4 = D_80182DAC[D_80082C90.field_06 & 7].w3;
    D_801D35D8 = D_80182DAC[D_80082C90.field_06 & 7].w4;
    weight = mult;

    for (i = 0; i < 110; i++) {
        D_801D35E0[i] = (g_tripleTriadCardStats[i].pad05[0] * weight / 200) >> 12;
    }

    return D_801D3560;
}

/**
 * @brief Initialize the D_801D3C58 list with node pool D_801D3798.
 *
 * Sets up a linked list with node size 0x4C and capacity 0x10.
 */
void initTriadTaskPool(void) {
    func_80098BC0(D_801D3C58, D_801D3798, 0x4C, 0x10);
}

/**
 * @brief Call func_80098D28 with D_801D3C58.
 */
void processTriadTasks(void) {
    func_80098D28(D_801D3C58);
}
