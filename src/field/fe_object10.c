#include "common.h"
#include "field.h"
#include "gamestate.h"
#include "character.h"
#include "battle.h"
#include "sound.h"
#include "field/fe_object10.h"

extern SeedState *g_seedState;

/**
 * @brief Pop value from script stack and branch to one of two handlers.
 *
 * Calls func_800C03BC if popped value is nonzero, func_800C03D8 if zero.
 * @param entity Script entity context.
 * @return 2.
 */
s32 func_800BD1F4(FieldEntity *entity) {
    u8 *a0 = (u8 *)entity;
    u8 idx = entity->stackIdx;
    entity->stackIdx = idx - 1;

    if (*(s32 *)(a0 + (s8)idx * 4) != 0) {
        func_800C03BC();
    } else {
        func_800C03D8();
    }
    return 2;
}

/**
 * @brief Set camera direction parameters based on direction code.
 *
 * Sets halfword at a1+4 to 0xCF and a1+2 to 0. Based on a0:
 * 0 -> a1+0 = 0, 1 -> a1+0 = 0x20, 2 -> a1+0 = -0x20.
 *
 * @param a0 Direction code (0, 1, or 2).
 * @param a1 Output parameter buffer.
 */
void func_800BD250(s32 a0, u8 *a1) {
    *(u16 *)(a1 + 4) = 0xCF;
    *(u16 *)(a1 + 2) = 0;
    switch (a0) {
    case 0:
        *(u16 *)(a1 + 0) = 0;
        break;
    case 1:
        *(u16 *)(a1 + 0) = 0x20;
        break;
    case 2:
        *(s16 *)(a1 + 0) = -0x20;
        break;
    }
}

s32 func_800BD2AC(FieldEntity *entity) { u8 *a0 = (u8 *)entity; u8 idx = entity->stackIdx; s16 buf[4]; entity->stackIdx = idx - 1; func_800BD250(*(s32 *)(a0 + (s8)idx * 4), (u8 *)buf); func_800A9434(*(u8 *)(a0 + 0x256), 0x30, 1, (u8 *)buf, 0x1E); return 2; }

/**
 * @brief Variant of @c func_800BD2AC that negates the rect/offset triple.
 *
 * Same shape as @c func_800BD2AC: pop one stack value, call @c func_800BD250
 * to fill @c buf with a 3-halfword rect from the direction code, then
 * dispatch @c func_800A9434 with cmd @c 0x30 and sub-cmd @c 1. The
 * difference is the three leading halfwords are sign-negated before the
 * dispatch — used when the source direction needs to be inverted.
 */
s32 func_800BD318(FieldEntity *entity) {
    Eline *eline = (Eline *)entity;
    u8 idx = entity->stackIdx;
    s16 buf[4];
    entity->stackIdx = idx - 1;
    func_800BD250(entity->stack[(s8)idx], (u8 *)buf);
    buf[0] = -buf[0];
    buf[1] = -buf[1];
    buf[2] = -buf[2];
    func_800A9434(eline->field_0x256, 0x30, 1, (u8 *)buf, 0x1E);
    return 2;
}

/**
 * @brief SeeD level-up tick: update SeeD experience and pay salary based on rank.
 *
 * Called from the step accumulator (@c func_800BD804) when the step counter at
 * @c state[0x8] overflows @c 0x6000. Performs:
 *   1. Snapshots current SeeD experience to @c prevSeedExp.
 *   2. Sums kill counts across all 8 characters.
 *   3. Adjusts @c seedExp by @c (totalKills - prevKillSum) - 10 (clamped to
 *      @c [100, 3100]). SeeD level @c = seedExp / 100, so 100..199 = lvl 1,
 *      500..599 = lvl 5, 2500..2599 = lvl 25, capping at 31 (exp = 3100).
 *   4. Pays salary: @c gil += @c g_seedSalaryTable[level] * 10 (capped at
 *      99,999,999).
 *   5. If state isn't muted (flags 0x10 and 0x1000 both clear), triggers the
 *      level-up notification: palette transition (@c func_800316D4 with old/new
 *      rank and old/new salary) plus three rank-up sound effects.
 *   6. Stores @c totalKills as the new @c prevKillSum baseline.
 */
void updateSeedLevel(void) {
    s16 totalKills = 0;
    s32 i;
    s32 salary;

    g_seedState->prevSeedExp = g_seedState->seedExp;

    for (i = 0; i < 8; i++)
        totalKills += g_gameState.chars[i].kills;

    g_seedState->seedExp = totalKills - g_seedState->prevKillSum - 10 + g_seedState->seedExp;

    if ((s16)g_seedState->seedExp < 100)
        g_seedState->seedExp = 100;
    else if ((s16)g_seedState->seedExp >= 0xC1C)
        g_seedState->seedExp = 0xC1C;

    /* Reg-allocation hack: this dead lookup keeps `i`'s register binding
       alive long enough that gcc 2.7.2 places `salary` in the same slot
       the original toolchain used. Without it, the allocator picks one
       register higher and the byte-match is lost. */
    i = g_seedSalaryTable[((s16)g_seedState->seedExp) / 100];
    salary = g_seedSalaryTable[(s16)g_seedState->seedExp / 100];
    g_gameState.mainData.party.gil += salary * 10;
    if (g_gameState.mainData.party.gil > 0x5F5E0FEu)
        g_gameState.mainData.party.gil = 0x5F5E0FF;

    if (!(g_seedState->stateFlags & 0x0010)) {
        if (!(g_seedState->stateFlags & 0x1000)) {
            s32 oldLevel = (s16)g_seedState->prevSeedExp / 100;
            s32 newLevel = (s16)g_seedState->seedExp / 100;

            func_800316D4(oldLevel, newLevel,
                          g_seedSalaryTable[oldLevel] * 10,
                          g_seedSalaryTable[newLevel] * 10);

            g_seedState->levelUpDisplayTimer = 150;
            sndPlaySfx(0x5B, 0, 0x80, 0x7F);
            sndPlaySfx(0x5C, 0, 0x80, 0x7F);
            sndPlaySfx(0x5D, 0, 0x80, 0x7F);
        }
    }

    g_seedState->prevKillSum = totalKills;
}

/**
 * @brief Tick all active GFs' HP up by 1 toward their battle max.
 *
 * Identical body to @c func_800C48C0 in we_object13. Iterates the 16
 * GF save entries; for each unlocked GF (@c exists bit 0) with
 * non-zero HP, if the current HP is below the GF's battle max (at
 * @c g_battleChars.gfEntries[i].hp), increments it by 1. Runs once
 * per field step to slowly regenerate GF HP while walking.
 */
void func_800BD5E0(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (g_gameState.gfs[i].exists & 1) {
            u16 hp = g_gameState.gfs[i].hp;
            if (hp != 0) {
                if (g_gameState.gfs[i].hp < (s16)g_battleChars.gfEntries[i].hp) {
                    g_gameState.gfs[i].hp = g_gameState.gfs[i].hp + 1;
                }
            }
        }
    }
}

/**
 * @brief Tick each active party member's HP up by 1 toward their battle regen cap.
 *
 * Identical body to @c func_800C492C in we_object13. Iterates the 3
 * party slots. For each occupied slot with its field-status bit 0 set,
 * increments the character's @c currentHp by 1 as long as it stays
 * below the slot's @c hpRegenCap. Paired with @c func_800BD5E0 which
 * does the same for GF HP.
 */
void func_800BD64C(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        u8 charIdx = g_gameState.mainData.party.party[i];
        if (charIdx != 0xFF) {
            if (g_battleChars.chars[i].fieldStatusByte & 1) {
                u16 hp = g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp;
                if (hp != 0) {
                    if ((s32)g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp <
                        (s32)g_battleChars.chars[i].hpRegenCap) {
                        g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp =
                            g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp + 1;
                    }
                }
            }
        }
    }
}

/**
 * @brief Tick the currently-learning Angelo trick's points; unlock + play SFX at 0.
 *
 * Identical body to @c func_800C49CC in we_object13. When Rinoa
 * (@c CHAR_RINOA = 4) is in the battle party, decrements the
 * learning-points counter for the currently-learning Angelo trick
 * (index at @c mainData.party.trickLearning). When the counter
 * reaches 0 and the trick isn't already completed, sets the completed
 * bit and plays the learn SFX (@c 0x83).
 */
void func_800BD6EC(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_gameState.battleParty[i] == CHAR_RINOA) {
            u8 idx = g_gameState.mainData.party.trickLearning;
            if (g_gameState.mainData.limitBreaks.angeloPoints[idx] != 0) {
                g_gameState.mainData.limitBreaks.angeloPoints[idx]--;
            } else {
                u8 mask = g_gameState.mainData.limitBreaks.angeloCompleted;
                if (!((mask >> idx) & 1)) {
                    g_gameState.mainData.limitBreaks.angeloCompleted = mask | (1 << idx);
                    sndPlaySfx(0x83, 0, 0x80, 0x7F);
                }
            }
            return;
        }
    }
}

void func_800BD794(void) { s32 i = 0; do { s32 status = getPackedField2Bit(i) - 1; if ((u32)status < 2) { if (fieldRandom() & 1) { func_800383B8(i, status); } } i++; } while (i < 0x100); }

/**
 * @brief Field step tick — accumulates @c stepDelta into all per-step counters.
 *
 * Mirror of @c func_800C4AE4 in we_object13. Drives every per-step
 * accumulator on @c g_seedState for the field map:
 *   - @c stepCounter (mirrored to @c D_80082C14).
 *   - @c packedFlagsStepAcc — fires @c func_800BD794 every @c 0x2800 steps.
 *   - @c hpRegenStepAcc — fires GF and party HP regen every @c 8 steps.
 *   - @c seedExpStepAcc — fires the SeeD level-up tick every @c 0x6000 steps,
 *     then clamps @c seedExp to @c [100, 0xC1C].
 *   - @c levelUpDisplayTimer — counts down each step; fires
 *     @c setTransitionPhase7 the frame it reaches @c 0.
 *   - @c angeloLearnStepAcc — fires the Angelo trick learn tick every
 *     @c 0x250 steps.
 *
 * @note Currently @c INCLUDE_ASM — near-match at 88.87% pending permuter
 *       to resolve gcc 2.7.2 register-allocation/scheduling quirks (mine
 *       uses @c a1 for the @c g_seedState pointer; target uses @c a0).
 *
 * @verbatim
 * void func_800BD804(s32 stepDelta) {
 *     g_seedState->hpRegenStepAcc += stepDelta;
 *     g_seedState->stepCounter += stepDelta;
 *     g_seedState->angeloLearnStepAcc += stepDelta;
 *     g_seedState->packedFlagsStepAcc = (u16)(g_seedState->packedFlagsStepAcc + stepDelta);
 *     D_80082C14 = g_seedState->stepCounter;
 *     if (g_seedState->packedFlagsStepAcc >= 0x2800u) {
 *         g_seedState->packedFlagsStepAcc = 0;
 *         func_800BD794();
 *     }
 *     if ((u32)g_seedState->hpRegenStepAcc >= 8u) {
 *         g_seedState->hpRegenStepAcc = 0;
 *         func_800BD5E0();
 *         func_800BD64C();
 *     }
 *     if (D_8007809A & 1) return;
 *     if (!(g_seedState->stateFlags & 8)) {
 *         g_seedState->seedExpStepAcc += stepDelta;
 *         if ((u32)g_seedState->seedExpStepAcc >= 0x6000u) {
 *             g_seedState->seedExpStepAcc = 0;
 *             updateSeedLevel();
 *         }
 *         if ((s16)g_seedState->seedExp < 100)         g_seedState->seedExp = 100;
 *         else if ((s16)g_seedState->seedExp >= 0xC1C) g_seedState->seedExp = 0xC1C;
 *     }
 *     if ((s16)g_seedState->levelUpDisplayTimer >= 0) {
 *         if ((s16)g_seedState->levelUpDisplayTimer == 0) setTransitionPhase7();
 *         g_seedState->levelUpDisplayTimer--;
 *     }
 *     if (D_8007809A & 0x10) return;
 *     if ((u32)g_seedState->angeloLearnStepAcc >= 0x250u) {
 *         g_seedState->angeloLearnStepAcc = 0;
 *         func_800BD6EC();
 *     }
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BD804);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BD9C4);

/**
 * Returns the value of the global byte D_800DE4FD.
 *
 * @return The value of D_800DE4FD.
 */
u8 func_800BE264(void) {
    return D_800DE4FD[0];
}

/** @brief Call func_801E8B84 if D_800DE4FD bit 1 is set. Returns result or 0. */
s32 func_800BE274(void) {
    if (*(u8 *)D_800DE4FD & 2) {
        return func_801E8B84();
    }
    return 0;
}

/** @brief Call func_801E8804 if D_800DE4FD bit 1 is set. */
void func_800BE2AC(void) {
    if (*(u8 *)D_800DE4FD & 2) {
        func_801E8804();
    }
}

/** @brief Call func_801E888C if D_800DE4FD bit 1 is set. */
void func_800BE2DC(void) {
    if (*(u8 *)D_800DE4FD & 2) {
        func_801E888C();
    }
}

/**
 * @brief Parse a script-event header at @c header and stash its fields globally.
 *
 * The 8-byte preamble at @c header is split into:
 *   - byte[0..2] → @c D_800DE8D9 / @c D_800DE8DA / @c D_800DE8D8 (event flags)
 *   - byte[3] → @c D_800DE8C0 (held across the pointer setup, written last)
 *   - bytecode pointer @c D_800DE4E0 = @c header + 8
 *   - sub-table pointers @c D_800DE4E4 / @c D_800DE4E8 (relative offsets at +4 / +6)
 */
void func_800BE30C(u8 *header) {
    EventHeader *h = (EventHeader *)header;
    u8 b3;
    D_800DE8D9 = h->field0;
    D_800DE8DA = h->field1;
    D_800DE8D8 = h->field2;
    b3 = h->field3;
    D_800DE4E0 = header + 8;
    D_800DE4E4 = header + h->offset4;
    D_800DE4E8 = header + h->offset6;
    D_800DE8C0 = b3;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BE36C);

/**
 * @brief Test if @c val falls within any active entity's @c [rangeLo, rangeHi].
 *
 * Scans all active Elines (@ref D_80085224, count @ref D_80085388);
 * returns 1 on the first entity whose @c rangeLo @c <= @c val @c <= @c rangeHi,
 * otherwise 0. Used by script-trigger dispatch to test whether the
 * popped value matches any registered range.
 */
s32 func_800BE44C(s32 val) {
    s32 i;
    Eline *e = &D_80085224[0];
    u8 count = D_80085388;

    for (i = 0; i < count; i++) {
        if (val >= e->rangeLo && val <= e->rangeHi) {
            return 1;
        }
        e = (Eline *)((u8 *)e + 0x264);
    }
    return 0;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BE4B0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BE5E4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BE7F4);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BE924);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BEA84);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BEBD0);

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BF080);

/**
 * Re-issue motion command @c 0xD to the active spatial entity, then
 * mirror @c field_0x206 into the entity's render slot @c unk52. Used
 * by the dispatch table to keep the renderer in sync after an
 * animation update.
 */
void func_800BF230(FieldEntity *entity) {
    Eline *eline = (Eline *)entity;
    func_800AA46C(eline->field_0x256, 0xD, eline->field_0x24E, 0);
    D_800D9630[eline->field_0x256]->unk52 = eline->field_0x206;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BF28C);

extern s32 getMaxBattleEntities(void);

/**
 * @brief Restore SFX/anim channels and dialog state after a load.
 *
 * Calls @c func_800BF28C(1) to do early setup, then iterates active
 * SFX slots restoring playback for any with their @c sfxStartMask
 * bit set, then iterates anim slots dispatching to @c setupAnimEntry
 * or @c setupAnimEntryFull based on the @c flag field. Finally
 * restores the dialog state from @c g_seedState and clears the
 * D_800704A8 mode-slot[0] to a default state mirroring the active
 * entity index.
 *
 * @note @c D_80085398[].flag is treated as @c s16 (signed compare with
 *       the literals @c 1 and @c 2); reading it as @c u16 would emit
 *       @c lhu instead of the @c lh the target uses.
 */
void func_800BF4A4(void) {
    u16 buf[2];
    s32 i;

    func_800BF28C(1);

    for (i = 0; i < getMaxBattleEntities(); i++) {
        if ((g_seedState->sfxStartMask >> i) & 1) {
            initSfxPlayback(i, (u8 *)D_80085300[i].payload);
            func_8002E064(i, (s16 *)&D_80085300[i]);
            startSfxSlow(i);
            setSfxGlobalFlag(i);
        }
        setSfxEntityType(i, D_80085300[i].type);
        setSfxEntryVolume(i, D_80085300[i].volume);
    }

    for (i = 0; i < 2; i++) {
        buf[0] = D_80085398[i].fieldE;
        buf[1] = D_80085398[i].fieldC;
        switch (D_80085398[i].flag) {
        case 1:
            setupAnimEntry(i, D_80085398[i].fieldA, (s32)buf,
                           D_80085398[i].field8, D_80085398[i].field6,
                           D_80085398[i].field4);
            break;
        case 2:
            setupAnimEntryFull(i, D_80085398[i].fieldA, (s32)buf,
                               D_80085398[i].field8, D_80085398[i].field6,
                               D_80085398[i].field4, D_80085398[i].field2);
            break;
        }
    }

    if (D_800704A8.dialogCount == D_800704A8.dialogTimer) {
        g_seedState->dialogStateMirror = 0;
    }
    if ((s16)g_seedState->dialogStateMirror == 0) {
        D_800704A8.dialogState = 2;
        D_800704A8.dialogCount = 0x10;
        D_800704A8.dialogTimer = 0xFF;
        D_800704A8.field_0x10E = 0xFF;
        D_800704A8.field_0x110 = 0xFF;
        D_800704A8.field_0x112 = 0xFF;
    } else {
        D_800704A8.dialogState = g_seedState->dialogStateMirror;
        D_800704A8.dialogCount = g_seedState->fieldDC;
        D_800704A8.dialogTimer = g_seedState->fieldDA;
        D_800704A8.field_0x10E = g_seedState->fieldDE;
        D_800704A8.field_0x110 = g_seedState->fieldE0;
        D_800704A8.field_0x112 = g_seedState->fieldE2;
        D_800704A8.field_0x114 = g_seedState->fieldE4;
        D_800704A8.field_0x116 = g_seedState->fieldE6;
        D_800704A8.field_0x118 = g_seedState->fieldE8;
        D_800704A8.field_0x11A = g_seedState->fieldEA;
        D_800704A8.field_0x11C = g_seedState->fieldEC;
        D_800704A8.field_0x11E = g_seedState->fieldEE;
    }

    D_800704A8.slots[0].mode = 0;
    D_800704A8.slots[0].submode = 0;
    D_800704A8.slots[0].param = D_800704A8.entityIndex[0];
}
