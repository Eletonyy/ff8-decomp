#include "common.h"
#include "battle.h"

extern u8 D_801D3110[];
extern u8 D_801D31C0[];
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

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009A8CC);

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
 * @brief Reset battle state globals D_801D3328, D_801D3359, and D_801D3340 fields.
 *
 * Clears D_801D3328 (word), D_801D3359 (byte), and sets fields in D_801D3340:
 * halfwords at +6, +0xA, +0x10, +0x14 to 0, and +0xC, +0xE to 1.
 */
void func_8009B494(void) {
    u8 *base;
    D_801D3328 = 0;
    D_801D3359 = 0;
    base = D_801D3340;
    *(s16 *)(base + 0x6) = 0;
    *(s16 *)(base + 0xA) = 0;
    *(s16 *)(base + 0xC) = 1;
    *(s16 *)(base + 0xE) = 1;
    *(s16 *)(base + 0x10) = 0;
    *(s16 *)(base + 0x14) = 0;
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009B4CC);

/**
 * @brief Look up the active object and initialize its handler.
 *
 * Gets an index via func_8009A7A4. If non-negative, uses it to look up
 * the object type from D_801D31C0 (stride 36) and passes it to func_800A2114.
 */
void func_8009B644(void) {
    s32 idx = func_8009A7A4();
    if (idx >= 0) {
        u8 *base = D_801D31C0;
        u8 type = *(base + idx * 36);
        func_800A2114(type);
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
        void *p   = D_801D3340 + idx;

        switch (D_801D3358) {
        case 0: break;
        case 1: func_8009B690(p, idx); break;
        case 2: func_8009B7B4(p, idx); break;
        case 3: func_8009B8D8(p, idx); break;
        case 4:
        case 5: func_8009BA4C(p);      break;
        }

        func_8009B4CC(D_801D3358, (u32 *)(D_801D3340 + D_801D3358 * 4));

        if (!(D_801D3334 & 1)) {
            if (D_801D3330 & 0xC0) {
                D_801D3359 = 2;
                memcpy(&D_801D335C, D_801D3340 + D_801D3358 * 4, 4);
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
    {
        u8 *base;
        node[0xC] = 0;
        node[0xD] = a0;
        node[0xE] = a1;
        base = D_801D3340;
        *(s16 *)(base + 0xC) = 1;
        *(s16 *)(base + 0xE) = 1;
    }
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
    u8 *base = D_801D31C0;
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
    u8 *entry = D_801D31C0;
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
    u8 *base = D_801D31C0;
    u8 *entry;
    u8 *result;

    entry = base + a1 * 36;
    result = (u8 *)func_8009C890(a0, entry[0], *(s32 *)(entry + 8) & 1, a2, a3);
    result[3] = a1;
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009C9D4);

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009CB10);

/**
 * @brief Triple Triad Plus-rule resolver.
 *
 * Walks the 3x3 active play area; for each newly-placed card (@c flags & 0x4),
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
s32 applyPlusRule(TTCell board[][TT_BOARD_COLS]) {
    s32 winningSum;
    s32 captures;
    s32 row, col, i;
    s32 maxCount;
    s32 edgeSum;
    s32 nbrCol;
    u8 cellOwner;
    TTSumBucket *bucket;
    TTCell *cell;
    TTCell *neighbor;
    TTCard *cellCard;
    TTDir *offset;
    TTSumBucket sumHist[21];   /* indexed by edge-sum value (0..20) */

    captures = 0;

    for (row = 1; row < 4; row++) {
        for (col = 1; col < 4; col++) {
            cell = &board[row][col];
            if (cell->flags & 0x4) {
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
                    if (neighbor->flags & 0x2) {
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
                        if ((neighbor->flags & 0x2) &&
                            ((sumHist[winningSum].dirMask >> i) & 1)) {
                            neighbor->flags |= 0x100;
                            if (neighbor->owner != cellOwner) {
                                neighbor->owner = cellOwner;
                                neighbor->flags |= 1 << (i + 3);
                                captures++;
                                cell->flags |= 0x100;
                            }
                        }
                    }
                }
            }
        }
    }

    return captures;
}

INCLUDE_ASM("asm/ovl/battle_engine/nonmatchings/be_object2", func_8009CF5C);

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
