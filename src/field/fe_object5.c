#include "common.h"
#include "field.h"
#include "gamestate.h"
#include "field/fe_object5.h"

extern u8 D_800DE878[];
extern u8 D_800C5FB0[];
extern u8 D_80077E5F;
extern u8 D_800DE8D5;
extern u8 D_8005F388[];
extern u8 D_80063388[];
extern s32 sndGetEngineState(void);
extern s32 sndUploadSamples(s32 arg, s32 flag);
extern s32 sndCmd10(u8 *p);
extern s32 sndCmdC0(s32 a, s32 b);
extern u32 D_800772B8;
extern s32 func_801E8B98(void);
extern u32 toggleSoundBank(void);
extern s32 sndCmd12(u32 a, s32 b);
extern s32 sndCmd19(u8 *bank, s32 arg);
extern s32 sndCmd14(s32 bankHandle, s32 a, s32 b);
extern s32 sndCmd1A(s32 bankHandle, s32 ramp, s32 priority);
extern s32 sndCmdC2(s32 handle, s32 ramp, s32 depth, s32 vol);
extern Eline *D_8008538C;
extern void func_8009A8E0(Eline *e);
extern void resetCardSlots(s32 mode);
extern void func_80036D44(s32 arg);
extern void func_80036B90(s32 idx);
extern void func_800A97E4();
extern void func_800A59D0(void);
extern s32 func_80038868(s32 lba, u32 size, u32 dest, void (*cb)(void));
extern s32 func_800393C8(void);
extern void func_801E8000(s32 priority);
extern s32 func_801E8104(s32 a, s32 b, s32 c, s32 d);
extern u32 D_800C2D14[];
extern s32 D_800DE4EC;
extern void sndCmdF1(void);
extern void sndCmd11();
extern s32 sndCmdC1(s32 handle, s32 ramp, s32 vol);
extern void sndSetMasterVolume(s32 v);
extern void sndSeqSetTempo(s32 t);

/**
 * @brief op159 SEALEDOFF — pop a flag mask and unseal the matching
 *        Ultimecia's-Castle features.
 *
 * Clears the popped bits from @c g_seedState->fieldF3 (the "sealed
 * status" byte), mirrors the result into @c D_80082C10 / @c D_80077E5F,
 * and recalculates party stats so menus reflect the newly-available
 * features. Counterpart to @c opHandler_LASTIN (which sets the same
 * byte to all-sealed = @c 0xFF on entry).
 *
 * @return 2 (continue processing).
 */
s32 opHandler_SEALEDOFF(Eline *e) {
    s32 popped = POP(e);
    g_seedState->fieldF3 &= ~popped;
    D_80082C10 = g_seedState->fieldF3;
    D_80077E5F = g_seedState->fieldF3;
    recalcPartyStats();
    return 2;
}

/**
 * @brief Pop a character/party token, resolve it via @c findCharacterSlot,
 *        and if the slot is valid hand it to @c func_80036B90.
 */
s32 func_800B08CC(Eline *e) {
    u8 idx;
    s32 result;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    result = findCharacterSlot(e->stack[(s8)idx]);
    if (result != 0xFF) {
        func_80036B90(result);
    }
    return 2;
}

/**
 * @brief op131 HOLD — pop three values (top byte discarded, then a
 *        hold/release flag and an actor id) and toggle the
 *        @c CharacterData.exists bits @c 0x6 for that character.
 *
 * @c actorId values below @c 8 index @c g_gameState.chars directly;
 * higher values are looked up via @c findCharacterSlot (which can
 * return @c 0xFF for "no slot", in which case the opcode bails out
 * early without touching @c chars).
 *
 * @return 2 (continue processing).
 */
s32 func_800B0924(Eline *e) {
    u8 idx;
    s32 flag;
    s32 actorId;

    idx = e->stackPtr;
    e->stackPtr = idx - 2;
    flag = e->stack[(s8)(idx - 1)];
    e->stackPtr = idx - 3;
    actorId = e->stack[(s8)(idx - 2)];

    if (actorId >= 8) {
        actorId = findCharacterSlot(actorId);
        if (actorId == 0xFF) {
            return 2;
        }
    }
    if (flag != 0) {
        g_gameState.chars[actorId].exists |= 6;
    } else {
        g_gameState.chars[actorId].exists &= ~6;
    }
    return 2;
}

/**
 * @brief Disable an entity. Clears the "active" flag bit @c 0x8, hides
 *        the render slot (unless field-debug bit @c 0x2 is set), captures
 *        the @c 0x10 flag into @c field_0x24C, and clears the entity's
 *        secondary @c unk249 / @c field_0x24B flags so collision /
 *        scripting skip this entity.
 */
s32 func_800B0A08(Eline *e) {
    *(volatile s32 *)&e->flags = *(volatile s32 *)&e->flags & ~0x8;
    if (!(D_800DE8CC & 0x2)) {
        EntityRenderSlot **base = D_800D9630;
        u8 slot = e->field_0x256;
        *(u8 *)((s32)base[slot] + 0x60) = 0;
    }
    if (*(volatile s32 *)&e->flags & 0x10) {
        e->field_0x24C = 1;
    } else {
        e->field_0x24C = 0;
    }
    e->unk249 = 0;
    e->field_0x24B = 0;
    return 2;
}

/**
 * @brief Re-enable an entity. Counterpart of @c func_800B0A08 — sets the
 *        "active" flag bit @c 0x8, restores render-slot visibility,
 *        re-installs the @c 0x10 flag from @c field_0x24C, then sets
 *        @c field_0x24C / @c unk249 / @c field_0x24B back to 1.
 */
s32 func_800B0A7C(Eline *e) {
    *(volatile s32 *)&e->flags = *(volatile s32 *)&e->flags | 0x8;
    if (!(D_800DE8CC & 0x2)) {
        EntityRenderSlot **base = D_800D9630;
        u8 slot = e->field_0x256;
        *(u8 *)((s32)base[slot] + 0x60) = 1;
    }
    if (e->field_0x24C != 0) {
        e->flags |= 0x10;
    } else {
        e->flags &= ~0x10;
    }
    e->field_0x24C = 1;
    e->unk249 = 1;
    e->field_0x24B = 1;
    return 2;
}

/** @brief Clear @c field_0x24B (entity "B flag"). */
s32 func_800B0B04(Eline *e) {
    e->field_0x24B = 0;
    return 2;
}

/** @brief Set @c field_0x24B (entity "B flag") to 1. */
s32 func_800B0B10(Eline *e) {
    e->field_0x24B = 1;
    return 2;
}

/** @brief Clear @c unk249 (entity "self-collision enable" flag). */
s32 func_800B0B20(Eline *e) {
    e->unk249 = 0;
    return 2;
}

/** @brief Set @c unk249 (entity "self-collision enable" flag) to 1. */
s32 func_800B0B2C(Eline *e) {
    e->unk249 = 1;
    return 2;
}

/**
 * @brief op0AB — counterpart of @c func_800B0BE4. Finds the active-
 *        party slot whose @c battleParty member matches
 *        @c e->field_0x255, then disables that entity: clears flag
 *        bit @c 0x4, calls @c func_800B912C with the entity's
 *        animation byte, sets the @c 0x2040 flag pair, and marks the
 *        slot in @c D_800704A8.entityIndex as inactive (@c 0xFF).
 *
 * @return 2 (continue processing).
 */
s32 func_800B0B3C(Eline *e) {
    s32 i;
    for (i = 0; i < 3; i++) {
        if (g_gameState.battleParty[i] == e->field_0x255) {
            e->flags &= ~4;
            func_800B912C(e, e->field_0x24F);
            e->flags |= 0x2040;
            D_800704A8.entityIndex[i] = 0xFF;
            break;
        }
    }
    return 2;
}

/**
 * @brief op0AC — locate the active-party slot whose @c battleParty
 *        member matches @c e->field_0x255, then snapshot
 *        @c e->field_0x256 into @c D_800704A8.entityIndex for that
 *        slot and clear the entity's "needs broadcast" flag (0x40).
 *
 * @return 2 (continue processing).
 */
s32 func_800B0BE4(Eline *e) {
    s32 i;
    for (i = 0; i < 3; i++) {
        if (g_gameState.battleParty[i] == e->field_0x255) {
            e->flags &= ~0x40;
            D_800704A8.entityIndex[i] = e->field_0x256;
            break;
        }
    }
    return 2;
}

/** @brief Set @c field_0x24C (entity "C flag") to 1. */
s32 func_800B0C48(Eline *e) {
    e->field_0x24C = 1;
    return 2;
}

/** @brief Clear @c field_0x24C (entity "C flag"). */
s32 func_800B0C58(Eline *e) {
    e->field_0x24C = 0;
    return 2;
}

/**
 * @brief Pop an entity-id token and stash @c func_8009F74C's result for
 *        that entity in @c resultSlots[0] (so a following @c GETN can
 *        retrieve it).
 */
s32 func_800B0C64(Eline *e) {
    u8 idx;
    s32 val;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    val = e->stack[(s8)idx];
    e->resultSlots[0] = func_8009F74C(e, (s32)D_80085230[val]);
    return 2;
}

/** @brief Pop halfword from stack and store to @c field_0x1F8. */
s32 func_800B0CCC(Eline *e) {
    u8 idx = e->stackPtr;
    e->stackPtr = idx - 1;
    e->field_0x1F8 = *(u16 *)&e->stack[(s8)idx];
    return 2;
}

/** @brief Pop halfword from stack and store to @c radius (collision radius). */
s32 func_800B0CFC(Eline *e) {
    u8 idx = e->stackPtr;
    e->stackPtr = idx - 1;
    e->radius = *(u16 *)&e->stack[(s8)idx];
    return 2;
}

/**
 * @brief Snapshot the entity's position and a few descriptor bytes into
 *        the result-slot register file so subsequent script @c GETN
 *        opcodes can read them. Position is rounded toward zero by
 *        dividing the fixed-point @c posX/Y/Z by 4096.
 */
s32 func_800B0D2C(Eline *e) {
    e->resultSlots[0] = e->posX / 4096;
    e->resultSlots[1] = e->posY / 4096;
    e->resultSlots[2] = e->posZ / 4096;
    e->resultSlots[4] = e->field_0x241;
    e->resultSlots[5] = e->field_0x1FA;
    e->resultSlots[6] = e->savedChannel;
    return 2;
}

/**
 * @brief op070 PGETINFO — pop a party-slot id and snapshot the
 *        corresponding party-member entity's position (rounded toward
 *        zero by dividing the fixed-point @c posX/Y/Z by 4096) and a
 *        few descriptor halfwords into the result-slot register file.
 *
 * Counterpart to @c func_800B0D2C — same store pattern, but the source
 * entity is the one indexed by @c g_seedState->memberSlot[popped]
 * rather than the current Eline.
 *
 * @return 2 (continue processing).
 */
s32 func_800B0D94(Eline *e) {
    SeedState *seed = g_seedState;
    u8 idx;
    s32 popped;
    s32 entIdx;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    popped = e->stack[(s8)idx];
    entIdx = seed->memberSlot[popped];
    e->resultSlots[0] = D_80085224[entIdx].posX / 4096;
    e->resultSlots[1] = D_80085224[entIdx].posY / 4096;
    e->resultSlots[2] = D_80085224[entIdx].posZ / 4096;
    e->resultSlots[4] = D_80085224[entIdx].field_0x241;
    e->resultSlots[5] = D_80085224[entIdx].field_0x1FA;
    e->resultSlots[6] = D_80085224[entIdx].savedChannel;
    return 2;
}

/** @brief Pop a character-id and stash @c findCharacterSlot's result in @c resultSlots[0]. */
s32 func_800B0E68(Eline *e) {
    u8 idx = e->stackPtr;
    e->stackPtr = idx - 1;
    e->resultSlots[0] = findCharacterSlot(e->stack[(s8)idx]);
    return 2;
}

/**
 * @brief op??? JUNCTION — swap the active battle party with the bench
 *        roster (and re-sync the per-character GFs).
 *
 * Pops a flag word. Bit 0 selects the direction:
 *   - bit 0 set: SAVE — copy the current active party
 *     (@c g_gameState.battleParty / @c party) into the bench backup
 *     (@c g_seedState->partyOrderA / @c partyOrderB), then re-apply
 *     character-data settings via @c func_80036B90 for every character
 *     whose @c characterId is in range.
 *   - bit 0 clear: RESTORE — copy the bench backup back into the
 *     active party.
 *
 * Bit 1 (only meaningful with bit 0 set) chooses between:
 *   - bit 1 set: park @c D_800704A8 into mode-5 (active-party swap
 *     animation) with a @c 0x16-frame counter and @c unk1AB copied
 *     from the bottom 3 bits of @c stateFlags.
 *   - bit 1 clear: defer to @c func_80036D44 with those same 3 bits.
 *
 * @return 3 (advance two stack slots and continue).
 */
s32 func_800B0EBC(Eline *e) {
    s32 popped;
    s32 saveMode;
    s32 i;

    {
        u8 idx = e->stackPtr;
        e->stackPtr = idx - 1;
        popped = e->stack[(s8)idx];
    }
    saveMode = popped & 1;
    resetCardSlots(saveMode);

    if (saveMode != 0) {
        for (i = 0; i < 3; i++) {
            g_seedState->partyOrderA[i] = g_gameState.battleParty[i];
            g_seedState->partyOrderB[i] = g_gameState.mainData.party.party[i];
        }

        if (popped & 2) {
            D_800704A8.mode = 5;
            D_800704A8.counter = 0x16;
            D_800704A8.unk1AB = g_seedState->stateFlags & 7;
        } else {
            func_80036D44(g_seedState->stateFlags & 7);
        }

        for (i = 0; i < 8; i++) {
            if (g_gameState.chars[i].characterId < 8) {
                func_80036B90(i);
            }
        }
        return 3;
    }

    for (i = 0; i < 3; i++) {
        g_gameState.battleParty[i] = g_seedState->partyOrderA[i];
        g_gameState.mainData.party.party[i] = g_seedState->partyOrderB[i];
    }
    return 3;
}

/**
 * @brief Pop an entity id, look up the corresponding @c Eline in the
 *        @c D_80085230 table, and copy six locator fields (pos x/y/z,
 *        @c field_0x241, @c field_0x1FA, @c savedChannel) from that
 *        entity into the current one. Finally call @c func_8009A8E0
 *        on the global @c D_8008538C anchor to recompute derived
 *        coordinates.
 *
 * @return 2 (continue processing).
 */
s32 func_800B1034(Eline *e) {
    u8 idx;
    s32 entityId;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    entityId = e->stack[(s8)idx];
    e->posX = D_80085230[entityId]->posX;
    e->posY = D_80085230[entityId]->posY;
    e->posZ = D_80085230[entityId]->posZ;
    e->field_0x241 = D_80085230[entityId]->field_0x241;
    e->field_0x1FA = D_80085230[entityId]->field_0x1FA;
    e->savedChannel = D_80085230[entityId]->savedChannel;
    func_8009A8E0(D_8008538C);
    return 2;
}

/**
 * @brief Variant of @c func_800B1034 that resolves the source entity
 *        through the active-party @c memberSlot[] table — pop a
 *        party-slot id, look up @c g_seedState->memberSlot[party] for
 *        the field-entity index, and copy six locator fields
 *        (pos x/y/z, @c field_0x241, @c field_0x1FA, @c savedChannel)
 *        into the current entity. Calls @c func_8009A8E0(D_8008538C)
 *        afterwards to recompute derived state.
 *
 * @return 2 (continue processing).
 */
s32 func_800B10F8(Eline *e) {
    SeedState *seed = g_seedState;
    u8 idx;
    s32 popped;
    s32 entIdx;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    popped = e->stack[(s8)idx];
    entIdx = seed->memberSlot[popped];
    e->posX = D_80085224[entIdx].posX;
    e->posY = D_80085224[entIdx].posY;
    e->posZ = D_80085224[entIdx].posZ;
    e->field_0x241 = D_80085224[entIdx].field_0x241;
    e->field_0x1FA = D_80085224[entIdx].field_0x1FA;
    e->savedChannel = D_80085224[entIdx].savedChannel;
    func_8009A8E0(D_8008538C);
    return 2;
}

/**
 * @brief op12D ACTORMODE — pop a mode byte and toggle two flag bits
 *        in the entity's @c flags word, then invoke @c func_800A97E4
 *        with the entity's @c field_0x256 and a mode-specific
 *        (mask, param) tuple to update derived actor state.
 *
 *        Mode  flags          mask  param
 *        ----  -----          ----  -----
 *          0   |= 0x02000000  0x0F    1
 *          1   &= ~0x02000000 0x0F    0
 *          2   &= ~0x01000000 0x22   (unused)
 *          3   |= 0x01000000  0x21   (unused)
 *      other   no flag change, call still fires
 *
 * @return 2 (continue processing).
 */
s32 func_800B11BC(Eline *e) {
    s32 mode = POP(e);
    s32 a1_val;
    s32 a2_val;

    switch (mode) {
    case 0:
        e->flags |= 0x02000000;
        a1_val = 0xF;
        a2_val = 1;
        break;
    case 1:
        e->flags &= ~0x02000000;
        a1_val = 0xF;
        a2_val = 0;
        break;
    case 2:
        e->flags &= ~0x01000000;
        a1_val = 0x22;
        break;
    case 3:
        e->flags |= 0x01000000;
        a1_val = 0x21;
        break;
    }
    func_800A97E4(e->field_0x256, a1_val, a2_val, 0);
    return 2;
}

/**
 * @brief op0A3 MOVIEREADY — peek the top two stack slots
 *        (a @c movieIndex and a @c priorityMode) and kick off an
 *        asynchronous CD read of the corresponding movie data into
 *        the @c 0x801E8000 frame buffer.
 *
 *        @c priorityMode picks one of three priorities:
 *          0 -> 8, 1 -> 0xA, anything else -> 6.
 *        The first invocation (active for this script group) seeds the
 *        movie subsystem (@c func_800A59D0, then @c func_80038868 with
 *        @c D_800C2D14[0..1] as the LBA/size pair) and marks
 *        @c D_800DE4FD as in-flight. Subsequent invocations poll
 *        @c func_800393C8 — they return @c 1 to keep the script
 *        blocked until the read completes, then commit the movie
 *        entry @c (movieIndex + 1) via @c func_801E8104 and pop both
 *        stack slots before returning @c 2.
 *
 * @return 1 while CD read is pending, 2 once committed.
 */
s32 func_800B12A4(Eline *e) {
    s32 priority;
    s32 entryIdx;

    priority = e->stack[(s8)e->stackPtr];
    entryIdx = e->stack[(s8)e->stackPtr - 1] + 1;

    switch (priority) {
    case 0:  priority = 8;   break;
    case 1:  priority = 0xA; break;
    default: priority = 6;   break;
    }

    if ((e->activeMask >> e->scriptGroup) & 1) {
        D_8007064B = 1;
        func_800A59D0();
        func_80038868(D_800C2D14[0], D_800C2D14[1], 0x801E8000, 0);
        D_800DE4FD[0] = 1;
    }

    if (func_800393C8() != 0) {
        return 1;
    }

    D_800DE4FD[0] |= 0x2;
    func_801E8000(priority);
    D_800DE4EC = func_801E8104(D_800C2D14[entryIdx * 2],
                               D_800C2D14[entryIdx * 2 + 1],
                               1,
                               D_8005F13C);
    *(s32 *)&g_seedState->pad50[0] = 0;
    e->stackPtr -= 2;
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object5", func_800B13EC);

INCLUDE_ASM("asm/field/nonmatchings/fe_object5", func_800B14C8);

/**
 * Calls func_801E8B84, returns 1 if result is nonzero, else 2.
 *
 * @param a0 Pointer to the script/object structure.
 * @return 1 if func_801E8B84 returns nonzero, 2 otherwise.
 */
s32 func_800B158C(u8 *a0) {
    s32 result;
    result = func_801E8B84(a0);
    if (result != 0) {
        return 1;
    }
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object5", func_800B15BC);

/**
 * @brief op164 SPUSYNC — peek top-of-stack; if positive and below the
 *        SPU's free-sample threshold (@c D_800772B8), pop and return 2
 *        (advance). If non-positive, defer to @c func_801E8B98 (an SPU
 *        readiness query) and pop only when it reports ready. Otherwise
 *        return 1 to block the script.
 *
 * Counterpart to SPUREADY — used right after a sample upload to wait
 * for the SPU to actually have the requested sample loaded.
 */
s32 opHandler_SPUSYNC(Eline *e) {
    s32 top = e->stack[(s8)e->stackPtr];
    if (top > 0) {
        if ((u32)top >= D_800772B8) {
            return 1;
        }
    } else {
        if (func_801E8B98() != 0) {
            return 1;
        }
    }
    e->stackPtr--;
    return 2;
}

/**
 * Returns 2, indicating continue processing.
 *
 * @param a0 Pointer to the script/object structure (unused).
 * @return 2 (continue processing).
 */
s32 func_800B1730(u8 *a0) {
    return 2;
}

/**
 * @brief Pop two values and stage a battle via @c loadBattleCmd. Top of
 *        stack supplies a flag byte (OR'd with @c 1), and the value
 *        below it supplies the encounter id. The returned handle is
 *        stashed in @c D_800DE878 for later use.
 */
s32 func_800B1738(Eline *e) {
    u8 idx;
    s32 val1, val2;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    val1 = e->stack[(s8)idx];
    e->stackPtr = idx - 2;
    val2 = e->stack[(s8)(idx - 1)];
    *(s32 *)D_800DE878 = loadBattleCmd(D_800C5FB0, val2, val1 | 1);
    return 2;
}

/**
 * Returns 2, indicating continue processing.
 *
 * @param a0 Pointer to the script/object structure (unused).
 * @return 2 (continue processing).
 */
s32 func_800B17A0(u8 *a0) {
    return 2;
}

/**
 * @brief Wait-on-CD-read opcode body — block until @c func_800393C8
 *        (a CD poll) returns @c 0, then advance.
 */
s32 func_800B17A8(Eline *e) {
    if (func_800393C8() == 0) {
        return 2;
    }
    return 1;
}

/**
 * @brief op0CF — reset SPU volume state.
 *
 * Issues @c sndCmdF1, restores both active sound handles to volume
 * @c 0x7F, resets the stored @c musicVolume / @c sfxVolume to
 * @c 0x7F, sets the master volume back to max, and resets the SEQ
 * tempo to @c 0x80.
 *
 * @return 2 (continue processing).
 */
s32 func_800B17D8(void) {
    sndCmdF1();
    if (g_seedState->soundHandle0 != -1) {
        sndCmdC0(g_seedState->soundHandle0, 0x7F);
    }
    if (g_seedState->soundHandle1 != -1) {
        sndCmdC0(g_seedState->soundHandle1, 0x7F);
    }
    g_seedState->musicVolume = 0x7F;
    g_seedState->sfxVolume = 0x7F;
    sndSetMasterVolume(0x7F);
    sndSeqSetTempo(0x80);
    return 2;
}

/**
 * @brief op0CB SETBATTLEMUSIC — pop a music ID and store as the next
 *        battle's music selector.
 *
 * @return 2 (continue processing).
 */
s32 opHandler_SETBATTLEMUSIC(Eline *e) {
    g_seedState->battleMusicId = (u8)POP(e);
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object5", func_800B18A4);

/**
 * @brief Toggle @c fieldCF from the inverse of @c stateFlags bit 0x400,
 *        then clear that bit. Helper for the music-state machine.
 */
void func_800B19D4(void) {
    if (g_seedState->stateFlags & 0x400) {
        g_seedState->fieldCF = 0;
    } else {
        g_seedState->fieldCF = 1;
    }
    g_seedState->stateFlags &= ~0x400;
}

/**
 * @brief op0B4 MUSICCHANGE — if @c D_800DE8C8[8] is set (= a sound-bank
 *        change was queued), clear the flag, copy the staged bank into
 *        @c audioChannel0State, toggle the bank, start the new music
 *        track, save the SPU handle, issue the @c sndCmdC0 with the
 *        current music id, and re-arm via @c func_800B19D4. Otherwise
 *        return 2 (no-op).
 *
 * @return 2 if no change was queued, 3 once the new track is started.
 */
s32 opHandler_MUSICCHANGE(void) {
    u8 *p = D_800DE8C8;
    if (p[8] == 0) {
        return 2;
    }
    p[8] = 0;
    g_seedState->audioChannel0State = g_seedState->nextSoundBank;
    g_seedState->soundHandle0 = sndCmd10(toggleSoundBank());
    sndCmdC0(0, g_seedState->musicVolume);
    func_800B19D4();
    return 3;
}

/**
 * @brief op141 — start the next music track using the bank currently
 *        selected by @c g_seedState->soundBankSelector, save the
 *        returned handle in @c soundHandle0, then issue @c sndCmdC0
 *        with the current @c musicId and re-arm via
 *        @c func_800B19D4.
 *
 * @return 3 (advance PC after deferred wait).
 */
s32 func_800B1AA0(void) {
    u8 *p;
    if ((s8)g_seedState->soundBankSelector == 0) {
        p = D_8005F388;
    } else {
        p = D_80063388;
    }
    g_seedState->soundHandle0 = sndCmd10(p);
    sndCmdC0(0, g_seedState->musicVolume);
    func_800B19D4();
    return 3;
}

/**
 * @brief op144 — variant of @c MUSICCHANGE that also takes a stack
 *        argument. If @c D_800DE8C8[8] is set, pop the argument,
 *        clear the flag, perform the bank-swap dance, and start the
 *        new track via @c sndCmd12 (instead of @c sndCmd10) with the
 *        popped value as second argument.
 *
 * @return 2 if no swap queued, 3 once the new track is launched.
 */
s32 func_800B1B10(Eline *e) {
    s32 popped = POP(e);
    u8 *flag = D_800DE8C8;
    if (flag[8] == 0) {
        return 2;
    }
    flag[8] = 0;
    g_seedState->audioChannel0State = g_seedState->nextSoundBank;
    g_seedState->soundHandle0 = sndCmd12(toggleSoundBank(), popped);
    sndCmdC0(0, g_seedState->musicVolume);
    func_800B19D4();
    return 3;
}

/**
 * @brief MUSICCHANGE variant taking two stack args. Pop @c b (top) and
 *        @c a (below), and once the staged-bank flag is armed, call
 *        @c sndCmd14 with the toggled bank handle, @c a masked to
 *        26 bits, and @c b passed through.
 *
 * @return 2 if no bank swap was queued, 3 once the new track starts.
 */
s32 func_800B1BB8(Eline *e) {
    s32 b = POP(e);
    s32 a = POP(e);
    s32 aMasked = a & 0x3FFFFFF;
    u8 *flag = D_800DE8C8;
    if (flag[8] == 0) {
        return 2;
    }
    flag[8] = 0;
    g_seedState->audioChannel0State = g_seedState->nextSoundBank;
    g_seedState->soundHandle0 = sndCmd14(toggleSoundBank(), aMasked, b);
    func_800B19D4();
    return 3;
}

/**
 * @brief Music-channel variant: pop @c first (top, the fade-ramp in
 *        frames) and @c second (the priority byte), then once the
 *        staged-bank flag is armed, call @c sndCmd1A with the toggled
 *        bank handle, @c first shifted left by one (= ramp*2 frames),
 *        and @c second masked to 7 bits.
 *
 * @return 2 if no bank swap was queued, 3 once the new track starts.
 */
s32 func_800B1C7C(Eline *e) {
    s32 first = POP(e);
    s32 firstShifted = first << 1;
    s32 second = POP(e);
    s32 secondMasked = second & 0x7F;
    u8 *flag = D_800DE8C8;
    if (flag[8] == 0) {
        return 2;
    }
    flag[8] = 0;
    g_seedState->audioChannel0State = g_seedState->nextSoundBank;
    g_seedState->soundHandle0 = sndCmd1A(toggleSoundBank(), firstShifted, secondMasked);
    func_800B19D4();
    return 3;
}

/**
 * @brief Variant of @c MUSICCHANGE for the SFX channel: pop a tempo /
 *        priority byte, swap the staged sound bank into @c
 *        audioChannel1State, start the new track on bank 0/1 via
 *        @c sndCmd19 (passing the popped value masked to 7 bits), save
 *        the returned handle in @c soundHandle1, and re-arm via
 *        @c func_800B19D4.
 *
 * @return 2 if no bank-swap was queued, 3 once the new SFX track starts.
 */
s32 func_800B1D40(Eline *e) {
    s32 masked = POP(e) & 0x7F;
    u8 *flag = D_800DE8C8;
    u8 *bank;
    if (flag[8] == 0) {
        return 2;
    }
    flag[8] = 0;
    g_seedState->audioChannel1State = g_seedState->nextSoundBank;
    if (g_seedState->soundBankSelector != 0) {
        bank = D_8005F388;
    } else {
        bank = D_80063388;
    }
    g_seedState->soundHandle1 = sndCmd19(bank, masked);
    func_800B19D4();
    return 3;
}

/** @brief Pop a flag value and feed it to @c sndSetEngineFlag. */
s32 func_800B1DF4(Eline *e) {
    u8 idx;

    idx = e->stackPtr;
    e->stackPtr = idx - 1;
    sndSetEngineFlag(e->stack[(s8)idx]);
    return 2;
}

/**
 * @brief op0BF MUSICSTOP — pop a channel index and silence the
 *        corresponding SPU sound handle, then mark both the handle
 *        and the audio-channel state byte as inactive (-1).
 *
 * @return 2 (continue processing).
 */
s32 opHandler_MUSICSTOP(Eline *e) {
    s32 ch = POP(e) & 1;
    s32 handle = (&g_seedState->soundHandle0)[ch];
    s8 *p;
    if (handle != -1) {
        sndCmd11(handle);
        (&g_seedState->soundHandle0)[ch] = -1;
        p = (s8 *)g_seedState + ch;
        p[0xC7] = -1;
    }
    return 2;
}

/** @brief Stash @c sndGetStatus into @c resultSlots[0]. */
s32 func_800B1ED4(Eline *e) {
    e->resultSlots[0] = sndGetStatus(e);
    return 2;
}

/**
 * @brief Call @c func_80012FEC and split its packed 32-bit return into
 *        the high halfword (@c resultSlots[0]) and zero-extended low
 *        halfword (@c resultSlots[1]).
 */
s32 func_800B1F04(Eline *e) {
    s32 result;
    result = func_80012FEC(e);
    e->resultSlots[1] = result;
    e->resultSlots[0] = result >> 16;
    e->resultSlots[1] = *(u16 *)&e->resultSlots[1];
    return 2;
}

/**
 * @brief op0C0 MUSICVOL — pop a volume and channel index, ramp the
 *        corresponding SPU sound handle to that volume, and persist
 *        the new value in @c g_seedState->musicVolume /
 *        @c g_seedState->sfxVolume (the @c [musicVolume,sfxVolume]
 *        adjacent-byte pair indexed by @c channel&1).
 *
 * @return 2 (continue processing).
 */
s32 opHandler_MUSICVOL(Eline *e) {
    s32 vol = POP(e);
    s32 ch = POP(e) & 1;
    u8 *p;
    sndCmdC1((&g_seedState->soundHandle0)[ch], 0x10, vol);
    p = (u8 *)g_seedState + ch;
    p[0xC5] = vol;
    return 2;
}

/**
 * @brief op0C1 MUSICVOLTRANS — pop a volume, fade ramp, and channel
 *        index; ramp the SPU channel's volume over @c ramp*2 frames
 *        and persist the new volume in @c g_seedState->musicVolume /
 *        @c sfxVolume.
 *
 * @return 2 (continue processing).
 */
s32 opHandler_MUSICVOLTRANS(Eline *e) {
    s32 vol = POP(e);
    s32 ramp = POP(e);
    s32 ch = POP(e) & 1;
    u8 *p;
    sndCmdC1((&g_seedState->soundHandle0)[ch], ramp << 1, vol);
    p = (u8 *)g_seedState + ch;
    p[0xC5] = vol;
    return 2;
}

/**
 * @brief 4-arg variant of @c MUSICVOLTRANS. Pop @c vol (top), @c depth,
 *        @c ramp, and a channel selector; ramp the SPU channel's volume
 *        toward @c vol with the extended parameters and persist the new
 *        volume in @c g_seedState->musicVolume / @c sfxVolume (indexed
 *        by @c channel&1, the adjacent-byte pair at offset @c 0xC5).
 *
 * @return 2 (continue processing).
 */
s32 func_800B2090(Eline *e) {
    s32 vol = POP(e);
    s32 depth = POP(e);
    s32 ramp = POP(e);
    s32 ch = POP(e) & 1;
    u8 *p;
    sndCmdC2((&g_seedState->soundHandle0)[ch], ramp << 1, depth, vol);
    p = (u8 *)g_seedState + ch;
    p[0xC5] = vol;
    return 2;
}

/**
 * Calls sndGetMaxVolume with argument 1, returns 1 if result is nonzero, else 2.
 *
 * @param a0 Pointer to the script/object structure (unused).
 * @return 1 if sndGetMaxVolume returns nonzero, 2 otherwise.
 */
s32 func_800B2158(u8 *a0) {
    s32 result;
    result = sndGetMaxVolume(1);
    if (result == 0) {
        return 2;
    }
    return 1;
}

/**
 * @brief Wait for the SPU to finish its current command, then keep
 *        retrying @c sndUploadSamples(D_8005F13C, 1) until it returns
 *        a non-error value, then mark @c D_800DE8D5 as ready.
 *
 * Helper for the music-load family — synchronously stages the next
 * sample bank into SPU RAM.
 */
void func_800B2188(void) {
    while (sndGetEngineState() != 0) {
    }
    while (sndUploadSamples(D_8005F13C, 1) == -1) {
    }
    D_800DE8D5 = 1;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object5", func_800B21E0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object5", func_800B2248);

/**
 * @brief op0BC EFFECTPLAY — pop 4 SFX parameters and start playback.
 *
 * @return 2 (continue processing).
 */
s32 opHandler_EFFECTPLAY(Eline *e) {
    s32 d = POP(e);
    s32 c = POP(e);
    s32 b = POP(e);
    s32 a = POP(e);
    sndPlaySfx(a, b, c, d);
    return 2;
}
