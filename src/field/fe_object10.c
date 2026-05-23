#include "common.h"
#include "field.h"
#include "gamestate.h"
#include "character.h"
#include "battle.h"
#include "sound.h"
#include "cd.h"
#include "field/fe_object10.h"

extern SeedState   *g_seedState;
extern CdReadState  D_8008A3D8;
extern u8           D_8007809A;
extern s32          D_80082C14;
extern u16         *D_800852F0;
extern s32          func_80037AEC(u8 *header, u16 *table, s32 **outBase);

/**
 * @brief Pop value from script stack and branch to one of two handlers.
 *
 * Calls func_800C03BC if popped value is nonzero, func_800C03D8 if zero.
 * @param entity Script entity context.
 * @return 2.
 */
s32 opHandler_DISABLEANGELO(FieldEntity *entity) {
    u8 idx = entity->stackIdx;
    entity->stackIdx = idx - 1;
    if (entity->stack[(s8)idx] != 0) {
        func_800C03BC();
    } else {
        func_800C03D8();
    }
    return 2;
}

/**
 * @brief Fill a 3-halfword rect/offset buffer from a direction code.
 *
 * Writes the rect's third entry to @c 0xCF (the fixed sub-mode word)
 * and zeroes the second; the first becomes @c 0, @c 0x20, or
 * @c -0x20 depending on the direction code (0, 1, or 2). Used by
 * the look/turn opcodes to feed @c func_800A9434.
 *
 * @param dir Direction code (0, 1, or 2).
 * @param out Output buffer; the function writes @c out[0..2] (halfwords).
 */
void func_800BD250(s32 dir, s16 *out) {
    out[2] = 0xCF;
    out[1] = 0;
    switch (dir) {
    case 0:
        out[0] = 0;
        break;
    case 1:
        out[0] = 0x20;
        break;
    case 2:
        out[0] = -0x20;
        break;
    }
}

s32 func_800BD2AC(FieldEntity *entity) {
    u8 idx = entity->stackIdx;
    s16 buf[4];
    entity->stackIdx = idx - 1;
    func_800BD250(entity->stack[(s8)idx], buf);
    func_800A9434(((Eline *)entity)->field_0x256, 0x30, 1, (u8 *)buf, 0x1E);
    return 2;
}

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
    func_800BD250(entity->stack[(s8)idx], buf);
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
 * @param stepDelta Steps to add (typically @c 1 per processed step event).
 */
void func_800BD804(s32 stepDelta) {
    s32 minSeedExp = 100;
    g_seedState->stepCounter += stepDelta;
    g_seedState->hpRegenStepAcc += stepDelta;
    g_seedState->packedFlagsStepAcc = (u16)(g_seedState->packedFlagsStepAcc + stepDelta);
    g_seedState->angeloLearnStepAcc += stepDelta;
    D_80082C14 = (u32)g_seedState->stepCounter;
    if (g_seedState->packedFlagsStepAcc >= 0x2800u) {
        g_seedState->packedFlagsStepAcc = 0;
        func_800BD794();
    }
    if ((u32)g_seedState->hpRegenStepAcc >= 8u) {
        g_seedState->hpRegenStepAcc = 0;
        func_800BD5E0();
        func_800BD64C();
    }
    if (D_8007809A & 1) return;
    if (!(g_seedState->stateFlags & 8)) {
        g_seedState->seedExpStepAcc += stepDelta;
        if ((u32)g_seedState->seedExpStepAcc >= 0x6000u) {
            g_seedState->seedExpStepAcc = 0;
            updateSeedLevel();
        }
        if ((s16)g_seedState->seedExp < minSeedExp)  g_seedState->seedExp = minSeedExp;
        else if ((s16)g_seedState->seedExp >= 0xC1C) g_seedState->seedExp = 0xC1C;
    }
    if ((s16)g_seedState->levelUpDisplayTimer >= 0) {
        if ((s16)g_seedState->levelUpDisplayTimer == 0) setTransitionPhase7();
        g_seedState->levelUpDisplayTimer--;
    }
    if (D_8007809A & 0x10) return;
    if ((u32)g_seedState->angeloLearnStepAcc >= 0x250u) {
        g_seedState->angeloLearnStepAcc = 0;
        func_800BD6EC();
    }
}

/**
 * @brief Field-engine per-frame tick — drives the script VM for every
 *        field-entity array and processes per-step accumulators.
 *
 * Stores @p stepDelta into the field status block and toggles its busy
 * byte. If the pad-init bit (@c D_800DE4FD[0] & 2) is armed, captures the
 * @c func_801E8B58 status into @c g_seedState->padInitStatus. Bails out
 * if a CD read is in flight (@c D_8008A3D8.flags bit 0).
 *
 * On entry, mirrors party gil/dreamGil from @c g_gameState to
 * @c g_seedState, mirrors the audio-channel marker @c fieldD1 into
 * @c D_800704A8.unk1AB, and fades out per-slot SFX whose ambient
 * trigger has expired (@c sfxStartMask bit set without
 * @c sfxEntryMask). Then calls @c getPackedField2Bit for the active
 * dispatcher slot and stores the result into @c D_800704A8.packedFlagSlot.
 *
 * Runs four script-VM dispatch loops in order:
 *   - @c D_80085224 / @c D_80085388  — full @c Eline pool (stride 0x264);
 *     each tick processes SFX-trigger 6/7 plus up to 16 opcode steps.
 *   - @c D_8008538C / @c D_800852F8 — @c FieldEntityB pool (stride 0x1A0);
 *     6 SFX triggers (groups 2..7) gated by @c D_800704BD.
 *   - @c D_80085384 / @c D_80085228 — @c FieldEntityC pool (stride 0x18C);
 *     2 SFX triggers (groups 6/7) gated by @c D_800704A8.unk015.
 *   - @c D_800852F4 / @c D_80085391 — @c FieldEntityD pool (stride 0x1B4);
 *     plain script tick, finalized by @c func_800B2BA0.
 *
 * Each inner VM iteration reads an opcode via @c func_80037B7C, dispatches
 * @c g_fieldOpcodeTable[opcode + 0x12], and processes the return bits:
 *   - bit 0 (0x1) — terminate this entity's tick (zeros the iteration counter)
 *   - bit 1 (0x2) — advance @c pc, set @c activeMask bit
 *   - bit 2 (0x4) — keep @c activeMask bit (else clear it)
 *
 * @param stepDelta Step delta added by @c func_800BD804 to per-step
 *                  accumulators (passed through @c D_800DE8C8.stepDelta).
 */
void func_800BD9C4(s32 stepDelta) {
    s32 unused[16];
    s32 sp50;
    s32 sp54;
    s32 s1;
    u8 *fs;

    D_800DE8C8[0] = stepDelta;
    fs = (u8 *)D_800DE8C8;
    fs[0xB] = !fs[0xB];
    if (D_800DE4FD[0] & 2) {
        g_seedState->padInitStatus = func_801E8B58();
    }
    if (D_8008A3D8.flags & 1) return;

    func_800BD804(D_800704A8.fieldStepDelta);
    g_seedState->gilMirror      = g_gameState.mainData.party.gil;
    g_seedState->dreamGilMirror = g_gameState.mainData.party.dreamGil;
    D_800704A8.unk1AB = g_seedState->fieldD1;

    {
        s32 i;
        for (i = 0; i < getMaxBattleEntities(); i++) {
            if ((g_seedState->sfxEntryMask >> i) & 1) continue;
            if (!((g_seedState->sfxStartMask >> i) & 1)) continue;
            if (D_800704A8.ambientFlags & 0xC0) {
                if (getSfxField28(i)) {
                    if (!((g_seedState->sfxActiveMask >> i) & 1)) {
                        fadeOutSfxSlow(i);
                    }
                }
            }
            if (getSfxField1C(i)) continue;
            if (!getSfxField28(i)) continue;
            g_seedState->sfxStartMask &= ~(1 << i);
        }
    }

    {
        u8 packed = (u8)getPackedField2Bit(g_seedState->fieldF2);
        SystemState *sys = &D_800704A8;
        sys->packedFlagSlot = packed;
    }

    /* Block A: full-Eline pool (stride 0x264). */
    {
        Eline *e = D_80085224;
        D_800DE4FC = 0;
        if (D_80085388 != 0) {
            do {
                s1 = 0x10;
                func_800B9288(e);

                if (D_800704A8.unk015 != 0) {
                    e->triggerSfx7 = 0;
                    e->unk248 = 0;
                } else {
                    if (e->triggerSfx7 != 0) {
                        if (e->triggerSfx7 == 2) {
                            e->flags |= 0x20;
                        }
                        func_800AE8B4(e, 7, (u16)(e->rangeLo + 2));
                        e->triggerSfx7 = 0;
                    }
                    if (e->unk248 != 0) {
                        func_800AE8B4(e, 6, (u16)(e->rangeLo + 3));
                        e->unk248 = 0;
                    }
                }

                if (!(e->flags & 1)) {
                    do {
                        s32 ret;
                        func_80037B7C(&D_80085380[e->pc], &sp50, &sp54);
                        ret = g_fieldOpcodeTable[sp50 + 0x12](e, sp54);
                        if (!(ret & 4)) {
                            e->activeMask &= ~(1 << e->scriptGroup);
                        }
                        if (ret & 2) {
                            e->pc++;
                            e->activeMask |= (1 << e->scriptGroup);
                        }
                        if (ret & 1) s1 = 0;
                        else s1--;
                        if (s1 == 0) break;
                    } while (!(e->flags & 1));
                }

                func_800B6854(e);
                D_800DE4FC++;
                e++;
            } while (D_800DE4FC < D_80085388);
        }
    }

    /* Block B: FieldEntityB pool (stride 0x1A0). */
    {
        FieldEntityB *eb = D_8008538C;
        D_800DE4FC = 0;
        if (D_800852F8 != 0) {
            do {
                s1 = 0x10;
                if (eb->activeMarker != 0 && !D_800704BD) {
                    if (eb->trigger7 != 0) {
                        if (eb->trigger7 == 2) eb->flags |= 0x20;
                        func_800AE8B4(eb, 7, (u16)(eb->rangeLo + 2));
                        eb->trigger7 = 0;
                    }
                    if (eb->trigger6 != 0) {
                        func_800AE8B4(eb, 6, (u16)(eb->rangeLo + 3));
                        eb->trigger6 = 0;
                    }
                    if (eb->trigger5 != 0) {
                        func_800AE8B4(eb, 5, (u16)(eb->rangeLo + 4));
                        eb->trigger5 = 0;
                    }
                    if (eb->trigger4 != 0) {
                        func_800AE8B4(eb, 4, (u16)(eb->rangeLo + 5));
                    }
                    if (eb->trigger3 != 0) {
                        func_800AE8B4(eb, 3, (u16)(eb->rangeLo + 6));
                        eb->trigger3 = 0;
                    }
                    if (eb->trigger2 != 0) {
                        func_800AE8B4(eb, 2, (u16)(eb->rangeLo + 7));
                        eb->trigger2 = 0;
                    }
                } else {
                    eb->trigger7 = 0;
                    eb->trigger6 = 0;
                    eb->trigger5 = 0;
                    eb->trigger3 = 0;
                    eb->trigger2 = 0;
                }

                do {
                    s32 ret;
                    func_80037B7C(&D_80085380[eb->pc], &sp50, &sp54);
                    ret = g_fieldOpcodeTable[sp50 + 0x12](eb, sp54);
                    if (!(ret & 4)) {
                        eb->activeMask &= ~(1 << eb->scriptGroup);
                    }
                    if (ret & 2) {
                        eb->pc++;
                        eb->activeMask |= (1 << eb->scriptGroup);
                    }
                    if (ret & 1) s1 = 0;
                    else s1--;
                } while (s1 != 0);

                eb++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_800852F8);
        }
    }

    /* Block C: FieldEntityC pool (stride 0x18C). */
    {
        FieldEntityC *ec = D_80085384;
        D_800DE4FC = 0;
        if (D_80085228 != 0) {
            do {
                s1 = 0x10;
                if (ec->activeMarker != 0 && !D_800704A8.unk015) {
                    if (ec->trigger6 != 0) {
                        func_800AE8B4(ec, 6, (u16)(ec->rangeLo + 2));
                        ec->trigger6 = 0;
                    }
                    if (ec->trigger7 != 0) {
                        func_800AE8B4(ec, 7, (u16)(ec->rangeLo + 3));
                        ec->trigger7 = 0;
                    }
                } else {
                    ec->trigger6 = 0;
                    ec->trigger7 = 0;
                }

                do {
                    s32 ret;
                    func_80037B7C(&D_80085380[ec->pc], &sp50, &sp54);
                    ret = g_fieldOpcodeTable[sp50 + 0x12](ec, sp54);
                    if (!(ret & 4)) {
                        ec->activeMask &= ~(1 << ec->scriptGroup);
                    }
                    if (ret & 2) {
                        ec->pc++;
                        ec->activeMask |= (1 << ec->scriptGroup);
                    }
                    if (ret & 1) s1 = 0;
                    else s1--;
                } while (s1 != 0);

                ec++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_80085228);
        }
    }

    /* Block D: FieldEntityD pool (stride 0x1B4). */
    {
        FieldEntityD *ed = D_800852F4;
        D_800DE4FC = 0;
        if (D_80085391 != 0) {
            do {
                s1 = 0x10;
                do {
                    s32 ret;
                    func_80037B7C(&D_80085380[ed->pc], &sp50, &sp54);
                    ret = g_fieldOpcodeTable[sp50 + 0x12](ed, sp54);
                    if (!(ret & 4)) {
                        ed->activeMask &= ~(1 << ed->scriptGroup);
                    }
                    if (ret & 2) {
                        ed->pc++;
                        ed->activeMask |= (1 << ed->scriptGroup);
                    }
                    if (ret & 1) s1 = 0;
                    else s1--;
                } while (s1 != 0);

                func_800B2BA0(ed);
                ed++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_80085391);
        }
    }
}

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
    D_800DE4E0 = (u16 *)(header + 8);
    D_800DE4E4 = header + h->offset4;
    D_800DE4E8 = header + h->offset6;
    D_800DE8C0 = b3;
}

/**
 * @brief Entity-array compaction after event-script rebind.
 *
 * Re-binds the event preamble via @c func_800BE30C(header), then sweeps
 * the @c D_80085224 entity pool and keeps only entries whose @c flags
 * bit @c 0x2 is set. Each kept @c Eline is copied to the next compacted
 * slot at the head of the array, tagged with its new index in
 * @c field_0x256, and registered in @c D_80085230 (NULL for dropped
 * entries). @c D_80085388 is set to the new entry count.
 *
 * @param header Event preamble forwarded to @c func_800BE30C.
 * @return Pointer to the first free @c Eline slot past the compacted set.
 */
Eline *func_800BE36C(u8 *header) {
    func_800BE30C(header);
    {
        s32 srcIdx = 0;
        s32 dstIdx = 0;
        Eline *src = D_80085224;
        Eline *dst = src;

        if (D_80085388 != 0) {
            do {
                D_80085230[srcIdx] = NULL;
                if (src->flags & 2) {
                    D_80085230[srcIdx] = dst;
                    *dst = *src;
                    dst->field_0x256 = (u8)dstIdx;
                    dst++;
                    dstIdx++;
                }
                src++;
                srcIdx++;
            } while (srcIdx < D_80085388);
        }

        D_80085388 = (u8)dstIdx;
        return dst;
    }
}

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
        e++;
    }
    return 0;
}

/**
 * @brief Script-table relinker — flattens per-group bytecode ranges
 *        into a single contiguous slice of @c D_80085380.
 *
 * For each script-group entry @c k in @p table (the @c [lo,hi] pair list),
 * if the corresponding @c D_800DE4E4 marker has bit @c 0x8000 set OR
 * @c func_800BE44C(k) returns non-zero, copies all @c D_800DE4E8 entries
 * @c [orig..hi) into @c D_80085380 starting at the current write cursor,
 * and overwrites @c table[k] with the new slice index. Skipped groups
 * keep their original @c table[k] value.
 *
 * @param header Forwarded to @c func_80037AEC for pool sizing.
 * @param table  Group @c (lo,hi) array — also receives the rewritten lo values.
 * @return Pointer to the first free slot past the compacted slice.
 */
s32 *func_800BE4B0(u8 *header, u16 *table) {
    s32 count;
    s32 dstIdx;
    s32 k;

    D_800852F0 = table;
    count = func_80037AEC(header, table, &D_80085380);

    dstIdx = 0;
    for (k = 0; k < count - 1; k++) {
        if (!(((u16 *)D_800DE4E4)[k] & 0x8000) && !func_800BE44C(k)) continue;
        {
            u16 orig = D_800852F0[k];
            D_800852F0[k] = (u16)dstIdx;
            do {
                D_80085380[dstIdx] = ((s32 *)D_800DE4E8)[orig];
                orig++;
                dstIdx++;
            } while (orig < D_800852F0[k + 1]);
        }
    }

    return &D_80085380[dstIdx];
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object10", func_800BE5E4);

/**
 * @brief Initialize the Block B field-entity pool (stride 0x1A0).
 *
 * Block B entities are smaller field actors (0x1A0 bytes each) but share
 * the script-VM header (offsets 0x000..0x187) with @c Eline, so we type
 * them as @c Eline* and walk with manual stride. Memsets the buffer,
 * then for each entry pulls a packed (upper:9, lower:7) value from
 * @c D_800DE4E0, derives @c rangeLo / @c rangeHi / @c pc, and registers
 * the entity in @c D_80085230 at indices @c [D_80085388, D_80085388+count).
 * pc is masked with @c 0x7FFF. Returns the post-init free-slot pointer.
 *
 * @param buf Entity buffer (Block B pool base).
 * @return Pointer past the last initialized Block B slot.
 */
Eline *func_800BE7F4(Eline *buf) {
    Eline *e = buf;
    func_800396E0(buf, D_800852F8 * 0x1A0);
    {
        s32 k = 0;
        u16 new_var;

        if (D_800852F8 == 0) return e;

        do {
            u16 packed;
            u16 upper;
            u16 lower;
            u16 pcVal;

            D_80085230[D_80085388 + k] = e;
            e->flags = 0x20000000;
            e->stackPtr = -1;

            packed = *D_800DE4E0;
            D_800DE4E0++;
            upper = packed >> 7;
            new_var = upper;
            lower = packed & 0x7F;
            e->rangeLo = new_var;
            e->rangeHi = lower;
            upper = e->rangeLo;
            e->rangeHi = (new_var + lower) + 1;

            pcVal = D_800852F0[upper];
            e->activeMask = 0xFF;
            e->groupRanges[0] = 0xFFFF;
            e->groupRanges[1] = 0xFFFF;
            e->groupRanges[2] = 0xFFFF;
            e->groupRanges[3] = 0xFFFF;
            e->groupRanges[4] = 0xFFFF;
            e->groupRanges[5] = 0xFFFF;
            e->groupRanges[6] = 0xFFFF;
            e->pc = pcVal & 0x7FFF;
            e->groupRanges[7] = 0xFFFF;

            e = (Eline *)((u8 *)e + 0x1A0);
            k++;
        } while (k < D_800852F8);

        return e;
    }
}

/**
 * @brief Initialize the @c FieldEntityD pool (Block D entities, stride 0x1B4).
 *
 * Memsets the supplied buffer for @c D_80085391 entities, then for each
 * entry pulls a packed (upper:9, lower:7) value from @c D_800DE4E0,
 * derives @c rangeLo / @c rangeHi / @c pc, and registers the entity in
 * @c D_80085230 at indices @c [D_80085388+D_800852F8, ...). pc is masked
 * with @c 0x7FFF. Returns the post-init free-slot pointer.
 *
 * @param buf Entity buffer (becomes the @c FieldEntityD pool base).
 * @return Pointer past the last initialized @c FieldEntityD slot.
 */
FieldEntityD *func_800BE924(FieldEntityD *buf) {
    FieldEntityD *e = buf;

    func_800396E0(buf, D_80085391 * 0x1B4);
    {
        s32 k = 0;
        u16 groupIdx;

        if (D_80085391 == 0) return e;

        do {
            u16 packed;
            u16 upper;
            u16 lower;
            u16 pcVal;

            D_80085230[D_80085388 + D_800852F8 + k] = (Eline *)e;
            e->flags = 0x80001000;
            e->stackPtr = -1;

            packed = *D_800DE4E0;
            D_800DE4E0++;
            upper = packed >> 7;
            groupIdx = upper;
            lower = packed & 0x7F;
            e->rangeLo = groupIdx;
            e->rangeHi = lower;
            upper = e->rangeLo;
            e->rangeHi = (groupIdx + lower) + 1;

            pcVal = D_800852F0[upper];
            e->activeMask = 0xFF;
            e->unk18A = 0x40;
            e->groupRanges[0] = 0xFFFF;
            e->groupRanges[1] = 0xFFFF;
            e->groupRanges[2] = 0xFFFF;
            e->groupRanges[3] = 0xFFFF;
            e->groupRanges[4] = 0xFFFF;
            e->groupRanges[5] = 0xFFFF;
            e->groupRanges[6] = 0xFFFF;
            e->groupRanges[7] = 0xFFFF;
            e->unk188 = -1;
            e->pc = pcVal & 0x7FFF;
            e->unk1AC = 0x80;
            e->unk1AB = 0x80;
            e->unk1AA = 0x80;

            e++;
            k++;
        } while (k < D_80085391);

        return e;
    }
}

/**
 * @brief Initialize the @c FieldEntityC pool (Block C entities, stride 0x18C).
 *
 * Memsets the supplied buffer for @c D_80085228 entities, then for each
 * entry pulls a packed (upper:9, lower:7) value from @c D_800DE4E0,
 * derives @c rangeLo / @c rangeHi / @c pc, and registers the entity in
 * @c D_80085230 at indices @c [D_80085388+D_800852F8+D_80085391, ...).
 * pc is masked with @c 0x7FFF; @c activeMarker is set to 1.
 *
 * @param buf Entity buffer (becomes the @c FieldEntityC pool base).
 * @return Pointer past the last initialized @c FieldEntityC slot.
 */
FieldEntityC *func_800BEA84(FieldEntityC *buf) {
    FieldEntityC *e = buf;

    func_800396E0(buf, D_80085228 * 0x18C);
    {
        s32 k = 0;
        u16 groupIdx;

        if (D_80085228 == 0) return e;

        do {
            u16 packed;
            u16 upper;
            u16 lower;
            u16 pcVal;

            D_80085230[D_80085388 + D_800852F8 + D_80085391 + k] = (Eline *)e;
            e->flags = 0x40000000;
            e->stackPtr = -1;

            packed = *D_800DE4E0;
            D_800DE4E0++;
            upper = packed >> 7;
            groupIdx = upper;
            lower = packed & 0x7F;
            e->rangeLo = groupIdx;
            e->rangeHi = lower;
            upper = e->rangeLo;
            e->rangeHi = (groupIdx + lower) + 1;

            pcVal = D_800852F0[upper];
            e->activeMask = 0xFF;
            e->groupRanges[0] = 0xFFFF;
            e->groupRanges[1] = 0xFFFF;
            e->groupRanges[2] = 0xFFFF;
            e->groupRanges[3] = 0xFFFF;
            e->groupRanges[4] = 0xFFFF;
            e->groupRanges[5] = 0xFFFF;
            e->groupRanges[6] = 0xFFFF;
            e->groupRanges[7] = 0xFFFF;
            e->pc = pcVal & 0x7FFF;
            e->activeMarker = 1;

            e++;
            k++;
        } while (k < D_80085228);

        return e;
    }
}

/**
 * @brief Pre-tick script-VM pump for all 4 entity pools.
 *
 * Iterates pools in order D, C, B, A (large stride first). For each
 * entity, runs the script VM: calls @c func_800393C8 (yield-poll),
 * fetches the next opcode via @c func_80037B7C, dispatches
 * @c g_fieldOpcodeTable[opcode + 0x12] with the arg from
 * @c func_80037B7C, and processes the return bits exactly like
 * @c func_800BD9C4's per-iter dispatch (bit 2 keeps the @c activeMask
 * bit; bit 1 advances @c pc and sets the bit). Unlike the main tick,
 * the inner loop runs unbounded until the script yields with
 * opcode 6 + arg < 9 (dialog/wait), at which point the entity is
 * considered "paused" and the loop advances to the next entity.
 *
 * @note Mirrors @c func_800BD9C4's structure but with the inner loop
 *       gated by the (op == 6 && arg < 9) yield instead of an
 *       iteration counter.
 */
void func_800BEBD0(void) {
    s32 sp10;
    s32 sp14;

    /* Block D: D_800852F4 entities (stride 0x1B4). */
    {
        FieldEntityD *e = D_800852F4;
        D_800DE4FC = 0;
        if (D_80085391 != 0) {
            do {
                while (1) {
                    s32 ret;
                    func_800393C8();
                    func_80037B7C(&D_80085380[e->pc], &sp10, &sp14);
                    if (sp10 == 6 && sp14 < 9) break;
                    ret = g_fieldOpcodeTable[sp10 + 0x12](e, sp14);
                    if (!(ret & 4)) {
                        e->activeMask &= ~(1 << e->scriptGroup);
                    }
                    if (ret & 2) {
                        e->pc++;
                        e->activeMask |= (1 << e->scriptGroup);
                    }
                }
                e++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_80085391);
        }
    }

    /* Block C: D_80085384 entities (stride 0x18C). */
    {
        FieldEntityC *e = D_80085384;
        D_800DE4FC = 0;
        if (D_80085228 != 0) {
            do {
                while (1) {
                    s32 ret;
                    func_800393C8();
                    func_80037B7C(&D_80085380[e->pc], &sp10, &sp14);
                    if (sp10 == 6 && sp14 < 9) break;
                    ret = g_fieldOpcodeTable[sp10 + 0x12](e, sp14);
                    if (!(ret & 4)) {
                        e->activeMask &= ~(1 << e->scriptGroup);
                    }
                    if (ret & 2) {
                        e->pc++;
                        e->activeMask |= (1 << e->scriptGroup);
                    }
                }
                e++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_80085228);
        }
    }

    /* Block B: D_8008538C entities (stride 0x1A0). */
    {
        FieldEntityB *e = D_8008538C;
        D_800DE4FC = 0;
        if (D_800852F8 != 0) {
            do {
                while (1) {
                    s32 ret;
                    func_800393C8();
                    func_80037B7C(&D_80085380[e->pc], &sp10, &sp14);
                    if (sp10 == 6 && sp14 < 9) break;
                    ret = g_fieldOpcodeTable[sp10 + 0x12](e, sp14);
                    if (!(ret & 4)) {
                        e->activeMask &= ~(1 << e->scriptGroup);
                    }
                    if (ret & 2) {
                        e->pc++;
                        e->activeMask |= (1 << e->scriptGroup);
                    }
                }
                e++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_800852F8);
        }
    }

    /* Block A: D_80085224 entities (full Eline, stride 0x264). */
    {
        Eline *e = D_80085224;
        D_800DE4FC = 0;
        if (D_80085388 != 0) {
            do {
                while (1) {
                    s32 ret;
                    func_800393C8();
                    func_80037B7C(&D_80085380[e->pc], &sp10, &sp14);
                    if (sp10 == 6 && sp14 < 9) break;
                    ret = g_fieldOpcodeTable[sp10 + 0x12](e, sp14);
                    if (!(ret & 4)) {
                        e->activeMask &= ~(1 << e->scriptGroup);
                    }
                    if (ret & 2) {
                        e->pc++;
                        e->activeMask |= (1 << e->scriptGroup);
                    }
                }
                e++;
                D_800DE4FC++;
            } while (D_800DE4FC < D_80085388);
        }
    }
}

/**
 * @brief Reset each entity in all 4 pools to its NEXT script-group entry.
 *
 * For each entity (Block A first, then C, B, D), sets @c scriptGroup to
 * @c 0, clears the stack (@c stackPtr = -1), points @c groupRanges[0]
 * and @c pc at @c D_800852F0[rangeLo + 1] + 1 (= start of the next
 * script group, one past), and (for Blocks C/B/D) resets the entity's
 * @c activeMask to @c 0xFF.
 */
void func_800BF080(void) {
    /* Block A */
    {
        Eline *e = D_80085224;
        u16 val;
        s32 k = 0;
        for (k = 0; k < D_80085388; k++, e++) {
            e->scriptGroup = 0;
            val = D_800852F0[e->rangeLo + 1] + 1;
            e->groupRanges[0] = val;
            e->pc = val;
            e->stackPtr = -1;
        }
    }

    /* Block C */
    {
        FieldEntityC *e = D_80085384;
        u16 val;
        s32 k = 0;
        for (k = 0; k < D_80085228; k++, e++) {
            e->scriptGroup = 0;
            val = D_800852F0[e->rangeLo + 1] + 1;
            e->stackPtr = -1;
            e->groupRanges[0] = val;
            e->pc = val;
            e->activeMask = 0xFF;
        }
    }

    /* Block B */
    {
        FieldEntityB *e = D_8008538C;
        u16 val;
        s32 k = 0;
        for (k = 0; k < D_800852F8; k++, e++) {
            e->scriptGroup = 0;
            val = D_800852F0[e->rangeLo + 1] + 1;
            e->stackPtr = -1;
            e->groupRanges[0] = val;
            e->pc = val;
            e->activeMask = 0xFF;
        }
    }

    /* Block D */
    {
        FieldEntityD *e = D_800852F4;
        u16 val;
        s32 k = 0;
        for (k = 0; k < D_80085391; k++, e++) {
            e->scriptGroup = 0;
            val = D_800852F0[e->rangeLo + 1] + 1;
            e->stackPtr = -1;
            e->groupRanges[0] = val;
            e->pc = val;
            e->activeMask = 0xFF;
        }
    }
}

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

/**
 * @brief Per-entity dispatch refresh for the Eline pool (Block A).
 *
 * For each entity in @c D_80085224, picks the appropriate initial draw
 * path: if @p a0 != 0 AND the entity's @c flags bit @c 0x4 is clear,
 * runs @c func_800BF230 (which copies geometry state into the render
 * slot); otherwise calls @c func_800B912C with the entity's
 * @c field_0x24F and ORs in @c flags bit @c 0x2000. Then fires
 * @c func_800A97E4 dispatches based on bit flags:
 *   - 0x100000  → cmd 0x2E with the per-entity counter slot
 *   - 0x200000  → cmd 0x2F with the entity's @c field_0x256
 *   - 0x80000   → cmd 0x27 with arg3 = @c field_0x263
 * Always copies @c flags-bit-8 into the render slot's @c unk60, plus
 * @c field_0x220 into @c unk62 and @c field_0x257 into @c unk61, and
 * issues @c func_800A97E4(field_0x256, 0x10, &entity->unk18A, 0).
 * Two more conditional dispatches follow:
 *   - 0x1000000 → cmd 0x21
 *   - 0x2000000 → cmd 0xF with arg2 = 1
 *
 * @param a0 Dispatch flag — non-zero enables the @c func_800BF230 path
 *           for entities without flags-bit-4 set.
 */
void func_800BF28C(s32 a0) {
    Eline *e = D_80085224;
    D_800DE4FC = 0;
    if (D_80085388 != 0) {
        do {
            s32 flags;
            if (a0 != 0 && !(e->flags & 4)) {
                func_800BF230(e);
            } else {
                func_800B912C(e, e->field_0x24F);
                e->flags |= 0x2000;
            }
            flags = e->flags;
            if (flags & 0x100000) {
                func_800A97E4(D_800DE4FC, 0x2E, 0, 0);
            } else if (flags & 0x200000) {
                func_800A97E4(e->field_0x256, 0x2F, 0, 0);
            } else if (flags & 0x80000) {
                func_800A97E4(e->field_0x256, 0x27, 0, e->field_0x263);
            }
            if (e->flags & 8) {
                D_800D9630[D_800DE4FC]->unk60 = 1;
            } else {
                D_800D9630[D_800DE4FC]->unk60 = 0;
            }
            D_800D9630[D_800DE4FC]->unk62 = e->field_0x220;
            D_800D9630[D_800DE4FC]->unk61 = e->field_0x257;
            func_800A97E4(e->field_0x256, 0x10, (s32)&e->unk18A, 0);
            if (e->flags & 0x1000000) {
                func_800A97E4(e->field_0x256, 0x21, 0, 0);
            }
            if (e->flags & 0x2000000) {
                func_800A97E4(e->field_0x256, 0xF, 1, 0);
            }
            D_800DE4FC++;
            e++;
        } while (D_800DE4FC < D_80085388);
    }
}

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
