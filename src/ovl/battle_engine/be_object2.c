#include "common.h"
#include "battle.h"

extern u8 D_801D3110[];
extern u8 D_801D3360[];
extern u8 D_801D3380[];
extern u8 D_801D3798[];
extern u8 D_801D3C58[];
extern s32 D_801D3328;
extern s32 func_8009BDC0();

/* Substate handlers dispatched from func_8009BAF4. Same names exist in
 * the battle_code overlay with different signatures; keep these as
 * file-local externs so the two overlays don't collide. */
extern void func_8009B690(void *entry, s32 idx);
extern void func_8009B7B4(void *entry, s32 idx);
extern void func_8009B8D8(void *entry, s32 idx);

/**
 * @brief Set up a battle object slot and cancel lower-priority siblings in its group.
 *
 * Stores the three action params in slot @p idx, marks it active via
 * @c func_8009C0A0(idx, 1), then sweeps the 10 slots: any sibling sharing
 * @c groupId with strictly lower @c priority gets reset via
 * @c func_8009C0A0(i, 7).
 *
 * @param idx     Index of the slot being placed (0..9).
 * @param param0  First action parameter (stored at @c slot.param0).
 * @param param1  Second action parameter (stored at @c slot.param1).
 * @param param2  Third action parameter (stored at @c slot.param2).
 */
void setBattleObjectAction(s32 idx, s32 param0, s32 param1, s32 param2) {
    s32 i;

    D_801D31C0[idx].param0 = param0;
    D_801D31C0[idx].param1 = param1;
    D_801D31C0[idx].param2 = param2;
    func_8009C0A0(idx, 1);

    for (i = 0; i < 10; i++) {
        if (D_801D31C0[i].groupId == D_801D31C0[idx].groupId
            && D_801D31C0[i].priority < D_801D31C0[idx].priority) {
            func_8009C0A0(i, 7);
        }
    }
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009A970);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009AA68);

/**
 * @brief Call func_80098D28 with D_801D3110.
 */
void func_8009AD00(void) {
    func_80098D28(D_801D3110);
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009AD24);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009AE6C);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009B3EC);

/**
 * @brief Reset battle state globals and the substate parameter table.
 *
 * Clears @c D_801D3328, @c D_801D3359, and the per-substate slots in
 * @c D_801D3340 (zeroing most fields, but setting both halves of slot 3
 * to 1 — slot 3 is the only substate that uses both halfwords).
 */
void func_8009B494(void) {
    D_801D3328 = 0;
    D_801D3359 = 0;
    D_801D3340[1].field2 = 0;
    D_801D3340[2].field2 = 0;
    D_801D3340[3].field0 = 1;
    D_801D3340[3].field2 = 1;
    D_801D3340[4].field0 = 0;
    D_801D3340[5].field0 = 0;
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009B4CC);

/**
 * @brief Look up the active object and initialize its handler.
 *
 * Gets an index via func_8009A7A4. If non-negative, passes the slot's
 * @c entityType byte to func_800A2114.
 */
void func_8009B644(void) {
    s32 idx = func_8009A7A4();
    if (idx >= 0) {
        func_800A2114(D_801D31C0[idx].entityType);
    }
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009B690);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009B7B4);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009B8D8);

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
void func_8009BA4C(u16 *a0) {
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
    D_801D335C = *a0;
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
 * (@c func_8009B690 / @c func_8009B7B4 / @c func_8009B8D8 /
 * @c func_8009BA4C) based on @c D_801D3358 (0..5), then calls
 * @c func_8009B4CC. Finally checks two completion triggers against
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
void func_8009BAF4(void) {
    s32 state = (s32)*(u8 *)&D_801D3338;

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
        case 1: func_8009B690(p, idx); break;
        case 2: func_8009B7B4(p, idx); break;
        case 3: func_8009B8D8(p, idx); break;
        case 4:
        case 5: func_8009BA4C(p);      break;
        }

        func_8009B4CC(D_801D3358, (u32 *)&D_801D3340[D_801D3358]);

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

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009BD24);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009BDC0);

/**
 * @brief Initialize the D_801D3380 linked list with a battle callback.
 *
 * Sets up D_801D3380 as a linked list (pool at D_801D3360, node size 0x14,
 * capacity 1), then appends func_8009BDC0 as a callback. Sets byte fields
 * 0xC, 0xD, 0xE on the node from the parameters. Resets D_801D3340 fields
 * at +0xC and +0xE to 1.
 *
 * @param a0 Value stored at node byte 0xD.
 * @param a1 Value stored at node byte 0xE.
 * @return Pointer to D_801D3380 list header.
 */
u8 *func_8009C010(s32 a0, s32 a1) {
    u8 *list = D_801D3380;
    u8 *node;

    func_80098BC0(list, D_801D3360, 0x14, 1);
    node = (u8 *)func_80098C44(list, (s32)func_8009BDC0);
    node[0xC] = 0;
    node[0xD] = a0;
    node[0xE] = a1;
    D_801D3340[3].field0 = 1;
    D_801D3340[3].field2 = 1;
    return list;
}

/**
 * @brief Set the type for a battle entity in D_801D31C0 and optionally trigger an effect.
 *
 * Computes entry at D_801D31C0 + a0 * 36, sets entry[1] = a1 (type),
 * clears entry[2..3]. If type is in range [2, 5], calls func_800A2364(0x5A, 1).
 *
 * @param a0 Entity index.
 * @param a1 Entity type.
 */
void func_8009C0A0(s32 a0, s32 a1) {
    u8 *base = (u8 *)D_801D31C0;
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
 * @brief Check if any battle entity in D_801D31C0 has a non-zero type.
 *
 * Iterates up to 10 entries (stride 36) and checks byte at offset 1.
 *
 * @return 1 if any entity has non-zero type, 0 otherwise.
 */
s32 func_8009C0F4(void) {
    s32 i = 0;
    u8 *entry = (u8 *)D_801D31C0;
    do {
        if (entry[1] != 0) {
            return 1;
        }
        i++;
        entry += 0x24;
    } while (i < 10);
    return 0;
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009C12C);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009C440);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009C59C);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009C6D8);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009C890);

/**
 * Starts an animation sequence on a battle entity.
 *
 * Looks up entity data from the global entity table, then calls the
 * animation handler with the entity's type flag and callback info.
 *
 * @param a0 Animation command or mode.
 * @param a1 Entity index.
 * @param a2 Animation parameter.
 * @param a3 Additional parameter (passed as 5th stack arg to handler).
 */
void func_8009C978(s32 a0, s32 a1, s32 a2, s32 a3) {
    u8 *base = (u8 *)D_801D31C0;
    u8 *entry;
    u8 *result;

    entry = base + a1 * 36;
    result = (u8 *)func_8009C890(a0, entry[0], *(s32 *)(entry + 8) & 1, a2, a3);
    result[3] = a1;
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

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009D058);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009D154);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009D2B0);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009D72C);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009DBE8);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009DECC);

/**
 * @brief Initialize the D_801D3C58 list with node pool D_801D3798.
 *
 * Sets up a linked list with node size 0x4C and capacity 0x10.
 */
void func_8009E1F0(void) {
    func_80098BC0(D_801D3C58, D_801D3798, 0x4C, 0x10);
}

/**
 * @brief Call func_80098D28 with D_801D3C58.
 */
void func_8009E224(void) {
    func_80098D28(D_801D3C58);
}
