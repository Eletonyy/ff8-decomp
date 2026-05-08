/**
 * @file bc_object1.c
 * @brief Battle scene orchestration and entity management.
 *
 * This file contains functions for managing battle entities within the
 * battle_code overlay. It handles:
 * - Entity state initialization and transitions (state machine at D_800ED148)
 * - Sound/SFX command queuing via a 16-entry task queue (D_800EE24B)
 * - Battle animation triggers and status effect application
 * - Random number generation via a shuffle buffer (D_800EEBA8/D_800EEBB0)
 * - Damage clamping and probability checks
 * - Music/sound playback coordination
 *
 * The primary data structure is the battle state array at D_800ED148, with
 * 0xD0-byte stride per entity (up to 7 entities). Entity fields include
 * state (0x4), flags (0x18), animation params (0x84-0x92), linked index
 * (0xCB), and various control bytes at large offsets (0x5C2, 0x12E8-0x1319).
 */
#include "common.h"
#include "battle.h"
#include "gf.h"


/* FIXME: D_800E19BC is conceptually an array of (s32 sector, s32 length)
   pairs (8-byte stride, two s32s per entry) used as CdRead arguments by
   the music/sound loader. The two callers below index it with `idx * 2`
   and `idx * 2 + 1` rather than `entries[idx].sector` / `.length`,
   because gcc 2.7.2 only emits the original binary's dual-pointer
   addressing pattern from the flat s32[] form — every typed-struct or
   typedef'd-2D-array variant we tried collapsed to a single pointer
   with two offsets and broke the match. The magic 2 / +1 should go
   away if/when a struct-typed access form that produces the same
   codegen is found. */
extern u8 D_80082C0A[];
extern u8 D_80082C0F[];
extern s16 D_8005F11C;

extern u8 D_80098030[];

extern u32 jtbl_80098018[];

void func_80027448(void);
void func_8009A254(void);
void func_8009A308(void);
void func_8009A38C(void);
void func_8009A3BC(void);
void func_8009A42C(s32, s32);
void func_8009A4A4(void);
void func_8009A528(s32, s32);
void func_8009A638(void);
void func_8009A6A8(s32);
void func_8009A74C(void);
void func_8009A8B4(s32);
void func_8009A928(void);
void func_8009A990(s32);
void func_8009AA2C(void);
void func_8009AAC4(s32);
void func_8009AB54(s32);
void func_8009ABE4(void);
void func_8009ABFC(void);
void func_8009AC14(void);
void func_8009AC34(void);
void func_8009AC68(void);
void func_8009ACB4(void);
void func_8009ACEC(void);
void func_8009AD7C(void);
void func_8009AE08(s32);
void func_8009AE9C(void);
void func_8009AF14(s32);
s32 func_8009AF3C(s32, s32, s32, s32, s32);
void func_8009AF98(s32);
s32 func_8009AFF0(s32);
void func_8009B088(s32, s32, s32, s32);
void func_8009B0F8(s32);
SoundCmd *func_8009B134(s32, s32, s32);
s32 func_8009B15C(void);
void func_8009B198(s32);
void func_8009B208(u8 *, u8 *, s32);
s32 func_8009B238(u8 *, s32);
s32 func_8009B270(u8 *, s32);
s32 func_8009B2A4(u8 *, u8 *, s32);
void func_8009B320(s32, u8 *, u8 *);
s32 func_8009B390(u8 *, s32);
s32 func_8009B3D0(s32);
void func_8009B428(void);
void func_8009B478(void);
void func_8009B520(void);
void func_8009B59C(s32, s32 *, s32 *);
void func_8009B5C4(s32, s32, s32, s32);
void func_8009B654(void);
void func_8009B690(void);
void func_8009B6B0(void);
s32 func_8009B74C(s32, s32);
s32 func_8009B79C(s32, s32);
s32 func_8009B7BC(s32);
s32 func_8009B7F4(s32, s32);
void func_8009B878(s32, u16 *, s32 *, s32);
void func_8009B924(s32, s32, s32);
s32 func_8009BA5C(s32, s32);

void func_800393C8(void);
s32 func_80042634(s32);
void func_8009B6D0(s32, s32);
void func_800A30E4(void);
void func_800A6288(s32);
void func_800A62B0(void);
void func_800A1CFC(s32);
void func_800A1AB8(s32, s32, s32);
void func_800A240C(s32, s32, s32);
void func_800A59AC(s32, s32, s32);
void func_800A69BC(void);
void func_800A6724(void);
void func_800A6D30(void);
void func_800A7B48(void);
void func_800A7884(void);
void func_800A853C(void);
void func_800A864C(void);
void func_800A86F0(s32);
void func_800A94E0(void);
void func_800A97D4(void);
void func_800A79A0(void);
s32 func_800AE6C0(void);
s32 func_800AE730(void);
s32 func_800AE788(void);
void func_800AECD4(void);
void func_800AED30(void);
void func_800AEC04(void);
void func_800AED9C(void);
void func_800AEB50(void);
void func_800AF8A4(s32);

s32 func_800B0668(s32, s32);


s32 func_800B1930(s32);
void func_800B1ACC(void);
void func_800B2024(void);
void func_800B2084(void);
void func_800B25E4(void);
void func_800B26B8(void);
SoundCmd *func_800B8564(s32, s32);
s32 func_800CEDA4(void);
void setCameraVibrateIntensity(s32);
void setCameraVibrateState(s32);
void sndCmdC1(s32, s32, s32);
void func_80038868(s32, s32, s32, s32);
void cdRead(s32, s32, s32, s32);
void cdReadSync(s32, s32, s32, s32);
void memcopy(s32, s32, s32);
void func_800D0F74(void);

/**
 * @brief Battle initialization entry point.
 *
 * Initializes the battle state, config, and all battle subsystems.
 * Loads the active entity data, merges status bits from the entity
 * into the battle config, then calls the chain of setup functions
 * for animations, sound, camera, and AI.
 */
void func_80099FE8(void) {
    s32 i;

    func_80027448();
    func_8009B428();

    ((BattleSystemHeader *)&D_800ED148)->unk5C3 = 1;
    ((BattleSystemHeader *)&D_800ED148)->unk4 = 0;
    ((BattleSystemHeader *)&D_800ED148)->unk12EC = 0xFF;
    ((BattleSystemHeader *)&D_800ED148)->unkC = 0xFF;
    g_battleConfig.result = BATTLE_RESULT_UNDETERMINED;
    ((BattleSystemHeader *)&D_800ED148)->unk1319 = 0xFF;

    for (i = 0; i < 3; i++) {
        g_battleConfig.unk4[i] = 0xFF;
    }

    func_8009B198(func_80042634(-1));
    func_800B25E4();
    func_8009B6D0(g_battleConfig.battleSceneId, D_800EDE24);
    {
        u8 *base = (u8 *)(D_800EDE24 - 0xCDC);
        if ((base[0xCDD] & 0xE0) == 0) {
            g_battleConfig.unk2 |= base[0xCDD] & (~0x10);
        } else {
            g_battleConfig.unk2 &= 0xFF1F;
            g_battleConfig.unk2 |= base[0xCDD] & (~0x10);
        }
    }
    func_800A94E0();
    func_800A86F0(1);
    func_800A86F0(2);
    func_800A86F0(0);
    func_800A6724();
    func_800A853C();
    func_800A7884();
    func_800A864C();
    func_800A30E4();
    func_8009A38C();
    func_8009A3BC();
    func_8009B134(9, 0x80, 0);
    func_8009A4A4();
    func_8009AF14((void *)func_8009ABFC);
}

/**
 * @brief Transition to battle active state.
 *
 * Calls animation setup, entity init, position calculation, and queues
 * sound/SFX commands. Sets entity state to 2 (active).
 */
void func_8009A160(void) {
    volatile BattleSystem *state;
    func_800A7B48();
    func_800A6D30();
    func_8009A638();
    func_8009A928();
    func_8009A74C();
    func_8009AF14(&func_800D0F74);
    func_800A69BC();
    func_8009B134(0x70, 0x80, 0);
    func_8009AF14(&func_8009ABE4);
    state = &D_800ED148;
    state->entities[0].state = 2;
}

/**
 * @brief Transition to post-battle state.
 *
 * Sets animation flag 0x12EA, calls victory/result handlers,
 * initializes idle animations, and sets entity state to 4.
 * Volatile base pointer prevents maspsx from moving sb stores
 * into jal delay slots.
 */
void func_8009A1E0(void) {
    s32 base = (s32)&D_800ED148;
    s32 one = 1;

    *(volatile u8 *)(base + 0x12EA) = one;
    func_800AE6C0();
    func_800A97D4();
    func_8009A254();
    *(volatile u8 *)(base + 0x5C2) = one;
    func_800B1ACC();
    func_800B2084();
    func_800B2024();
    *(volatile s32 *)(base + 0x4) = 4;
}

/**
 * @brief Initialize idle animations for all active entities.
 *
 * Iterates 7 entities (stride 0xD0). For each with flags (0x8C) having
 * bits 0x1 and 0x10 set but not 0x80, triggers idle animation via
 * func_800A59AC.
 */
void func_8009A254(void) {
    volatile BattleEntity *units = (volatile BattleEntity *)&D_800ED148;
    s32 i;
    for (i = 0; i < 7; i++) {
        if (units[i].controlFlags & 1) {
            if (units[i].controlFlags & 0x10) {
                if (!(units[i].controlFlags & 0x80)) {
                    func_800A59AC(i, 0, 0);
                }
            }
        }
    }
}

/**
 * @brief Get battle animation active flag from D_800ED148+0x12E9.
 * @return The byte at offset 0x12E9 in the battle state array.
 */
u32 func_8009A2E0(void) {
    return D_800ED148.unk12E9;
}

/**
 * @brief Get battle animation sequence flag from D_800ED148+0x12EA.
 * @return The byte at offset 0x12EA in the battle state array.
 */
u32 func_8009A2F4(void) {
    return D_800ED148.unk12EA;
}

/**
 * @brief Drain pending animation commands while the sequence flag is set.
 *
 * If unk12EA is set, repeatedly checks func_800CEDA4 (returns nonzero while
 * commands remain): sets unk12E9 to 1 and yields via func_8009B690 each
 * iteration. On loop exit, clears unk12E9, yields once more, then runs
 * func_800B26B8 to finalize the animation phase.
 */
void func_8009A308(void) {
    if (D_800ED148.unk12EA != 0) {
        while (func_800CEDA4() != 0) {
            D_800ED148.unk12E9 = 1;
            func_8009B690();
        }
    }
    D_800ED148.unk12E9 = 0;
    func_8009B690();
    func_800B26B8();
}

/**
 * @brief Queue a battle music command.
 *
 * Plays sound ID 0x3EA at volume 0x80, then stores D_800EDE24 value
 * into the returned command buffer.
 */
void func_8009A38C(void) {
    SoundCmd *cmd = func_8009B134(0x3EA, 0x80, 0);
    cmd->unk0 = D_800EDE24[0];
}

/**
 * @brief Play two startup sound effects.
 *
 * Plays sound ID 1 and sound ID 0x3EB, both at volume 0x80.
 */
void func_8009A3BC(void) {
    func_8009B134(1, 0x80, 0);
    func_8009B134(0x3EB, 0x80, 0);
}

/**
 * @brief Conditionally apply vibration feedback.
 *
 * If bit 2 of D_80082C0A is set, calls setCameraVibrateIntensity(0x1000) and
 * setCameraVibrateState(1) to trigger controller vibration.
 */
void func_8009A3F4(void) {
    if (*(u16 *)D_80082C0A & 4) {
        setCameraVibrateIntensity(0x1000);
        setCameraVibrateState(1);
    }
}

/**
 * @brief Queue a hit-reaction sound for a specific entity.
/**
 * @brief Queue an attack-hit sound for a slot with a custom hit-type lookup.
 *
 * Plays sound 0x66 (channel 0x80) anchored at the battle unit in slot @p idx
 * and writes both the slot index and the @p off-th entry of the hit-type
 * table at @c D_800ED158+0xD04 into the returned command buffer.
 *
 * @param idx Battle-unit slot index.
 * @param off Hit-type table index.
 */
void func_8009A42C(s32 idx, s32 off) {
    SoundCmd *cmd = func_8009B134(0x66, 0x80, (s32)&D_800ED158.slots[idx]);
    u8 *hitType = &D_800ED158.unkD04[off];
    cmd->unk0 = idx;
    cmd->unk2.hword = *hitType;
}

/**
 * @brief Queue hit-reaction sounds for all active party members.
 *
 * Iterates 8 slots checking D_800ED148+0xD5C. For each active slot,
 * calls func_8009A42C with sequential sound IDs starting at 3.
 */
void func_8009A4A4(void) {
    s32 cmdId = 3;
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800ED148.unkD5C[i]) {
            func_8009A42C(cmdId, i);
            cmdId++;
        }
    }
}

/**
 * @brief Test if a specific bit is set in a bitmask.
 * @param a0 The bitmask value.
 * @param a1 The bit position to test.
 * @return 1 if the bit is set, 0 otherwise.
 */
s32 func_8009A514(s32 a0, s32 a1) {
    s32 mask = 1 << a1;
    s32 val = a0 & mask;
    return val != 0;
}

/**
 * @brief Stage an entity attack: snapshot, play sound 0x67, set hit type and position.
 *
 * Snapshots entity state via func_8009AFF0 and func_800A1CFC, then plays
 * sound 0x67 targeting the entity. Stores idx + flag (cmd->unk2.b.lo = 1,
 * .b.hi = 1 if entity flags bit 1 set, else 0). Then copies the @p off-th
 * hit-type byte (D_800ED148.unkD14) into entity->unkCB and the @p off-th
 * position ((*(BattleVec3u (*)[0x8])((u8 *)&D_800ED148 + 0xCE4))) into entity->pos.
 * @param idx Entity slot index.
 * @param off Source entity index for the hit-type / position lookup.
 *
 * Best clean (struct-only) attempt:
 * @code
 * void func_8009A528(s32 idx, s32 off) {
 *     BattleEntity *units = (BattleEntity *)&D_800ED148;
 *     BattleEntity *entity = &units[idx];
 *     SoundCmd *cmd;
 *     func_8009AFF0(idx);
 *     func_800A1CFC(idx);
 *     cmd = func_8009B134(0x67, 0x80, (s32)&D_800ED158.slots[idx]);
 *     cmd->unk0 = idx;
 *     cmd->unk2.b.lo = 1;
 *     cmd->unk2.b.hi = (entity->flags & 2) ? 1 : 0;
 *     entity->unkCB = D_800ED148.unkD14[off];
 *     entity->pos.x = (*(BattleVec3u (*)[0x8])((u8 *)&D_800ED148 + 0xCE4))[off].x;
 *     entity->pos.y = (*(BattleVec3u (*)[0x8])((u8 *)&D_800ED148 + 0xCE4))[off].y;
 *     entity->pos.z = (*(BattleVec3u (*)[0x8])((u8 *)&D_800ED148 + 0xCE4))[off].z;
 * }
 * @endcode
 * Permuter ~67% — same gcc-combine and `s1 -= 0x10` / split-across-call
 * issues as func_8009A6A8.
 */
INCLUDE_ASM("asm/ovl/battle_code/nonmatchings/bc_object1", func_8009A528);

/**
 * @brief Queue attack animations for all active party members.
 *
 * Iterates 8 slots checking D_800ED148+0xD5C. For each active slot,
 * calls func_8009A528 with sequential command IDs starting at 3.
 */
void func_8009A638(void) {
    s32 cmdId = 3;
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800ED148.unkD5C[i]) {
            func_8009A528(cmdId, i);
            cmdId++;
        }
    }
}

/**
 * @brief Queue a return-to-position animation for an entity.
 *
 * Snapshots animation state via func_800A240C / func_8009AFF0 / func_800A1AB8
 * with the entity's status / unk18 / unk28 fields, then plays sound 0x67
 * with position bytes cleared.
 * @param idx Entity slot index.
 *
 * Best clean (struct-only) attempt — 84.24% match:
 * @code
 * void func_8009A6A8(s32 idx) {
 *     BattleEntity *units = (BattleEntity *)&D_800ED148;
 *     BattleEntity *entity = &units[idx];
 *     SoundCmd *cmd;
 *     func_800A240C(idx, entity->unk28, (s32)&entity->status);
 *     func_8009AFF0(idx);
 *     func_800A1AB8(idx, entity->status, entity->unk18);
 *     cmd = func_8009B134(0x67, 0x80, (s32)&D_800ED158.slots[idx]);
 *     cmd->unk0 = idx;
 *     cmd->unk2.b.lo = 0;
 *     cmd->unk2.b.hi = 0;
 * }
 * @endcode
 * Mismatches: gcc collapses entity into a single saved register where the
 * original keeps base + offset separate (4 saved s-regs in target), and
 * the target reuses `addiu s1, s1, 0x10` to derive &D_800ED158 from
 * &D_800ED148 — both differences require magic byte offsets in the C to
 * reproduce.
 */
INCLUDE_ASM("asm/ovl/battle_code/nonmatchings/bc_object1", func_8009A6A8);

/**
 * @brief Lay out party-side battle slots and play the start-of-encounter sound.
 *
 * Counts active entities in the first 3 slots (those with @c unkCB != 0xFF),
 * plays sound 0xD at volume 0x80, then iterates the same 3 slots calling
 * func_8009A6A8 for each active one. For each, clears @c pos.y and sets
 * @c pos.x / @c pos.z from a lookup table chosen by the active count:
 * @c D_800E3CA4 (count == 1), @c D_800E3CA8 (count == 2),
 * @c D_800E3CB0 (count == 3). Other counts leave the position fields alone.
 *
 * Best clean (struct-only) attempt:
 * @code
 * void func_8009A74C(void) {
 *     BattleEntity *units = (BattleEntity *)&D_800ED148;
 *     s32 active_count = 0;
 *     s32 pos_idx;
 *     s32 i;
 *     for (i = 0; i < 3; i++) {
 *         if (units[i].linkedIdx != 0xFF) active_count++;
 *     }
 *     func_8009B134(0xD, 0x80, 0);
 *     pos_idx = 0;
 *     for (i = 0; i < 3; i++) {
 *         if (units[i].linkedIdx != 0xFF) {
 *             func_8009A6A8(i);
 *             units[i].pos.y = 0;
 *             if (active_count == 2) {
 *                 units[i].pos.x = D_800E3CA8[pos_idx * 2];
 *                 units[i].pos.z = D_800E3CA8[pos_idx * 2 + 1];
 *                 pos_idx++;
 *             } else if (active_count == 1) {
 *                 units[i].pos.x = D_800E3CA4[0];
 *                 units[i].pos.z = D_800E3CA4[1];
 *             } else if (active_count == 3) {
 *                 units[i].pos.x = D_800E3CB0[pos_idx * 2];
 *                 units[i].pos.z = D_800E3CB0[pos_idx * 2 + 1];
 *                 pos_idx++;
 *             }
 *         }
 *     }
 * }
 * @endcode
 * Permuter ~69%; depends on func_8009A6A8 matching first.
 */
INCLUDE_ASM("asm/ovl/battle_code/nonmatchings/bc_object1", func_8009A74C);

/**
 * @brief Queue a damage/heal visual effect sound for an entity slot.
 *
 * Plays sound 0x66 (channel 0x80) targeting the entity at slot @p idx
 * (208-byte stride into @c D_800ED158) and stores the slot index plus
 * the linked-entity byte at +0xBB as the sound's command parameters.
 *
 * @param idx Entity slot index.
 */
void func_8009A8B4(s32 idx) {
    SoundCmd *cmd = func_8009B134(0x66, 0x80, (s32)&D_800ED158.slots[idx]);
    cmd->unk0 = idx;
    cmd->unk2.hword = *(u8 *)&D_800ED158.slots[idx].linkedIdx2;
}

/**
 * @brief Queue damage sounds for all entities with linked targets.
 *
 * Iterates 3 entity slots. For each with CB != 0xFF, calls
 * func_8009A8B4 to queue the damage sound.
 */
void func_8009A928(void) {
    volatile BattleEntity *units = (volatile BattleEntity *)&D_800ED148;
    s32 i;

    for (i = 0; i < 3; i++) {
        if (units[i].linkedIdx != 0xFF) {
            func_8009A8B4(i);
        }
    }
}

/**
 * @brief Drain a single pending trigger keyed against the given target.
 *
 * Walks the battle-unit slots looking for the next slot whose `key`
 * field matches `target`. When found, dispatches func_800A59AC with
 * the slot's `type` (skipping the call when type == 2 and the previous
 * slot's status bit 0 is set), then clears the slot's type/key fields
 * and exits. Slot 0 is the "self" slot and is never matched against.
 * @param target Trigger key to find and clear.
 */
void func_8009A990(s32 target) {
    s32 i;
    BattleEntity *p;

    i = 0;
    p = (BattleEntity *)&D_800ED148;
top:
    if (p[1].slot8.byteView.trigKey == target) {
        if (((u8 *)&p[1])[0x7] != 0) {
            if (((u8 *)&p[1])[0x7] == 2) {
                if (!(p[0].status & 1)) {
                    func_800A59AC(i, ((u8 *)&p[1])[0x7], 0);
                }
            } else {
                func_800A59AC(i, ((u8 *)&p[1])[0x7], 0);
            }
            p[1].slot8.byteView.trigKey = 0;
            ((u8 *)&p[1])[0x7] = 0;
            return;
        }
    }
    p++;
    i++;
    if (i < 7) goto top;
}

/**
 * @brief Process all pending deferred damage triggers.
 *
 * If D_80082C0F is zero, checks animation state and processes each
 * pending trigger type via func_8009A990.
 */
void func_8009AA2C(void) {
    s32 sentinel;
    s32 i;

    if (D_80082C0F[0] != 0) {
        return;
    }

    if (D_800ED148.unk12F9 != 1) {
        sentinel = 0xFF;
        if (func_800AE730() == sentinel) {
            return;
        }
        if (func_800AE788() == sentinel) {
            return;
        }
    }
    {
        s32 count = D_800ED148.unk12F8;
        if (count != 0) {
            i = 0;
            do {
                func_8009A990(i);
                count = D_800ED148.unk12F8;
                i++;
            } while (i < count);
        }
    }
}

/**
 * @brief Timer-based sound effect callback.
 *
 * Decrements a timer at offset 0x8 in the task entry. When timer
 * reaches zero, plays sound 0x4 at volume 0xF0, and conditionally
 * plays a timed sound via sndCmdC1 if condition 0x1BD == 3.
 * @param a0 Task queue slot index (multiplied by 0x10).
 */
void func_8009AAC4(s32 a0) {
    s32 off = a0 << 4;
    s32 base = (s32)D_800EE28C;
    u8 *entry = (u8 *)(off + base);

    if (*(u16 *)(entry + 0x8) == 0) {
        func_8009B134(4, 0xF0, 0);
        if (*(u8 *)(base + 0x1BD) == 3) {
            sndCmdC1(D_8005F11C, 0x3C, 0);
        }
        *(u8 *)(entry + 0xF) = 1;
    }
    *(u16 *)(entry + 0x8) = *(u16 *)(entry + 0x8) - 1;
}

/**
 * @brief Schedule a timer-based sound effect.
 *
 * Queues func_8009AAC4 as a task callback via func_8009B3D0, then
 * stores the given duration into the task's timer field.
 * @param a0 Duration in frames for the timer.
 */
void func_8009AB54(s32 a0) {
    s32 dur = a0;
    s32 off;
    s32 base;
    off = func_8009B3D0((s32)func_8009AAC4) << 4;
    base = (s32)D_800EE28C;
    *(u16 *)(off + base + 0x8) = dur;
}

/**
 * @brief Conditionally run startup sequence if battle state is idle.
 *
 * If D_800ED148 word 0 is zero, calls a chain of initialization
 * functions: func_800AECD4, func_800AED30, func_800AEC04,
 * func_800AED9C, func_800AEB50.
 */
void func_8009AB98(void) {
    if (D_800ED148.entities[0].unk0 == 0) {
        func_800AECD4();
        func_800AED30();
        func_800AEC04();
        func_800AED9C();
        func_800AEB50();
    }
}

/**
 * @brief Set entity state to 3 (victory/win).
 *
 * Writes value 3 to D_800ED148 offset 0x4 (entity state field).
 */
void func_8009ABE4(void) {
    D_800ED148.entities[0].state = 3;
}

/**
 * @brief Set entity state to 1 (active/ready).
 *
 * Writes value 1 to D_800ED148 offset 0x4 (entity state field).
 *
 */
void func_8009ABFC(void) {
    D_800ED148.entities[0].state = 1;
}

/**
 * @brief Wrapper for func_800A30E4.
 *
 * Simple pass-through call to the animation processing function.
 */
void func_8009AC14(void) {
    func_800A30E4();
}

/**
 * @brief Reset battle state and reinitialize animation engine.
 *
 * Clears entity state word, calls func_8009AA2C (deferred triggers),
 * func_800A30E4 (animation), and func_800A79A0 (state reset).
 */
void func_8009AC34(void) {
    *(s32 *)&D_800ED148.entities[0].unk0 = 0;
    func_8009AA2C();
    func_800A30E4();
    func_800A79A0();
}

/**
 * @brief Full reset with entity-specific reinitialization.
 *
 * Clears entity state, calls func_8009AA2C, func_800A30E4,
 * func_800A79A0, then reads entity index from offset 0xF and
 * calls func_800AF8A4 with it.
 */
void func_8009AC68(void) {
    D_800ED148.entities[0].unk0 = 0;
    func_8009AA2C();
    func_800A30E4();
    func_800A79A0();
    func_800AF8A4(D_800ED148.entities[0].entityRef);
}

/**
 * @brief Conditional reset based on entity activity check.
 *
 * Reads entity type from D_800ED157, checks via func_800B1930.
 * If inactive, calls func_8009AC68 for full reset.
 */
void func_8009ACB4(void) {
    s32 a0 = D_800ED157[0];
    if (func_800B1930(a0) == 0) {
        func_8009AC68();
    }
}

/**
 * @brief Initialize battle entities for a new round.
 *
 * Sets control bytes (0x12E8=2, 0xD=0, 0x12FD=1, 0x12EA=0, 0x5C2=0),
 * then iterates 3 entity slots calling func_800A6288 and clearing
 * bit 31 of flags word (0x18). Finally calls func_800A62B0.
 */
void func_8009ACEC(void) {
    s32 i = 0;
    s32 mask = 0x7FFFFFFF;
    register s32 tmp asm("$2") = (s32)&D_800ED148;
    s32 base;
    REGALLOC_BARRIER(tmp);
    base = tmp;

    *(u8 *)(base + 0x12E8) = 2;
    *(u8 *)(base + 0xD) = 0;
    *(u8 *)(base + 0x12FD) = 1;
    *(u8 *)(base + 0x12EA) = 0;
    *(u8 *)(base + 0x5C2) = 0;

    for (; i < 3; i++) {
        func_800A6288(i);
        *(volatile s32 *)(base + 0x18) = *(volatile s32 *)(base + 0x18) & mask;
        base += 0xD0;
    }
    func_800A62B0();
}

/**
 * @brief Set battle round timer based on speed setting.
 *
 * Reads D_800EE449 (speed setting 0-3) and maps to frame counts:
 * 0 or 3 -> 60, 1 -> 30, 2 -> 40. Schedules a timer via func_8009AB54
 * (passing frames - 15) and stores the frame count into D_800ED148.entities[0].timer.
 */
void func_8009AD7C(void) {
    s32 frames;
    switch (D_800EE449[0]) {
        case 0:
            frames = 60;
            break;
        case 1:
            frames = 30;
            break;
        case 2:
            frames = 40;
            break;
        case 3:
            frames = 60;
            break;
    }
    func_8009AB54(frames - 15);
    D_800ED148.entities[0].timer = frames;
}

/**
 * @brief Dispatch a battle transition command.
 *
 * Codes 5..10 map to a fixed set of transition entry points. Code 5 marks
 * the battle state field at offset 0 with value 1 (signalling a primed
 * transition); codes 6..10 each schedule one of five callbacks via
 * func_8009AF14 to be run as the next phase. Out-of-range codes are
 * silently ignored.
 * @param cmd Transition code in the range 5..10.
 */
void func_8009AE08(s32 cmd) {
    switch (cmd) {
        case 5:
            *(s32 *)&D_800ED148.entities[0].unk0 = 1;
            break;
        case 6:
            func_8009AF14((s32)func_8009AC14);
            break;
        case 7:
            func_8009AF14((s32)func_8009AC34);
            break;
        case 8:
            func_8009AF14((s32)func_8009ACB4);
            break;
        case 9:
            func_8009AF14((s32)func_8009AC68);
            break;
        case 10:
            func_8009AF14((s32)func_8009ACEC);
            break;
    }
}

/**
 * @brief Update battle phase transition sound.
 *
 * If not in phase 2 and the current sound flag (unk12ED) differs from the
 * last played flag (unk12EE), plays one of two phase transition sounds at
 * volume 0xF0: sound 0x6C if unk12ED == 1, else sound 0x6D. Always copies
 * the current flag into the last-played slot.
 */
void func_8009AE9C(void) {
    if (D_800ED148.unk12E8 != 2) {
        if (D_800ED148.unk12ED != D_800ED148.unk12EE) {
            if (D_800ED148.unk12ED == 1) {
                func_8009B134(0x6C, 0xF0, 0);
            } else {
                func_8009B134(0x6D, 0xF0, 0);
            }
        }
    }
    D_800ED148.unk12EE = D_800ED148.unk12ED;
}

/**
 * @brief Schedule a callback function as a battle task.
 *
 * Wraps the given function pointer with sound ID 0xA at volume 0x80
 * by calling func_8009B134, effectively queueing it for execution.
 * @param a0 Function pointer to schedule as a task callback.
 */
void func_8009AF14(s32 a0) {
    func_8009B134(0xA, 0x80, a0);
}

/**
 * @brief Queue a custom sound command with full parameters.
 *
 * Plays sound ID 8 at volume a3, then stores entity ID, flags,
 * and extra parameter into the command buffer.
 * @param a0 Entity data pointer for the command target.
 * @param a1 Entity ID to store in command.
 * @param a2 Flag byte 1.
 * @param a3 Volume / priority.
 * @param stack_arg Extra flag byte 2 (5th argument on stack).
 */
s32 func_8009AF3C(s32 a0, s32 a1, s32 a2, s32 a3, s32 stack_arg) {
    s32 entity_id = a1;
    s32 flag1 = a2;
    SoundCmd *cmd;
    s32 flag2 = stack_arg;

    cmd = func_8009B134(8, a3, a0);
    cmd->unk0 = entity_id;
    cmd->unk2.b.lo = flag1;
    cmd->unk2.b.hi = flag2;
}

/**
 * @brief Queue a status effect animation command.
 *
 * Snapshots entity state via func_8009AFF0, then plays sound 0x75
 * at volume 0x80 targeting the entity. Stores entity index.
 * @param a0 Entity slot index.
 */
void func_8009AF98(s32 a0) {
    s32 idx = a0;
    s32 offset;
    SoundCmd *cmd;

    func_8009AFF0(idx);
    offset = ((idx * 2 + idx) * 4 + idx) * 16;
    cmd = func_8009B134(0x75, 0x80, offset + (s32)&D_800ED158);
    cmd->unk0 = idx;
}

/**
 * @brief Snapshot entity animation state before a command.
 *
 * Copies status (0x90) to backup (0x92) and flags (0x18) to backup
 * (0x1C) for the given entity. If entity index >= 3, checks linked
 * entity data flags and conditionally clears bit 6 of status and
 * bit 0x2000 of flags.
 * @param a0 Entity slot index.
 */
s32 func_8009AFF0(s32 a0) {
    s32 base = (s32)&D_800ED148;
    s32 offset = ((a0 * 2 + a0) * 4 + a0) * 16;
    u8 *entity = (u8 *)(offset + base);
    s32 flags;
    u8 *linked;
    s32 linkedFlags;

    *(u16 *)(entity + 0x92) = *(u16 *)(entity + 0x90);
    flags = *(s32 *)(entity + 0x18);
    *(s32 *)(entity + 0x1C) = flags;

    if (a0 >= 3) {
        linked = *(u8 **)(*(s32 *)(entity + 0x10));
        linkedFlags = *(u8 *)(linked + 0xF7);
        if (linkedFlags & 1) {
            *(u16 *)(entity + 0x92) = *(u16 *)(entity + 0x92) & 0xFFBF;
        }
        linkedFlags = *(u8 *)(linked + 0xF7);
        if (linkedFlags & 2) {
            *(s32 *)(entity + 0x1C) = *(s32 *)(entity + 0x1C) & ~0x2000;
        }
    }
}

/**
 * @brief Queue an effect command with directional flag.
 *
 * Snapshots state, then plays sound 0x76 or 0x77 based on direction
 * flag, at volume 0xF0. Stores entity ID and two flag bytes.
 * @param a0 Entity slot index.
 * @param a1 Direction flag (0 = sound 0x76, nonzero = sound 0x77).
 * @param a2 Flag byte 1.
 * @param a3 Flag byte 2.
 */
void func_8009B088(s32 a0, s32 a1, s32 a2, s32 a3) {
    s32 idx = a0;
    s32 dir = a1;
    s32 flag1 = a2;
    s32 flag2 = a3;
    s32 snd;
    SoundCmd *cmd;

    func_8009AFF0(idx);
    snd = 0x77;
    if (dir == 0) {
        snd = 0x76;
    }
    cmd = func_8009B134(snd, 0xF0, 0);
    cmd->unk0 = idx;
    cmd->unk2.b.lo = flag1;
    cmd->unk2.b.hi = flag2;
}

/**
 * @brief Conditionally play a timed sound effect.
 *
 * If bit 1 of D_80082C0A is clear, plays a timed sound via
 * sndCmdC1 using D_8005F11C as the sound ID.
 * @param a0 Duration parameter for the timed sound.
 */
void func_8009B0F8(s32 a0) {
    if (!(*(u16 *)D_80082C0A & 2)) {
        sndCmdC1(D_8005F11C, a0, 0);
    }
}

/**
 * @brief Queue a sound command and return a buffer for caller to fill.
 *
 * Sign-extends @p cmd from 16 bits, masks @p vol to 8 bits, then dispatches
 * via func_800B8564 to allocate a slot in the sound-command queue. The
 * returned pointer addresses a small command buffer whose layout depends on
 * @p cmd; the caller fills in entity / parameter words as appropriate.
 *
 * @param cmd Sound-command id (sign-extended s16).
 * @param vol Volume (low 8 bits).
 * @param entry Caller-supplied context pointer / value.
 * @return Pointer to the command buffer, or NULL if the queue is full.
 */
SoundCmd *func_8009B134(s32 cmd, s32 vol, s32 entry) {
    return func_800B8564((s16)cmd, vol & 0xFF);
}

/**
 * @brief Get next random value from shuffle buffer.
 *
 * Reads the current index from D_800EEBB0, uses it to index into
 * D_800EEBA8, increments the value at that position, then returns
 * the corresponding byte from D_80098030.
 * @return Random byte value from the lookup table.
 */
s32 func_8009B15C(void) {
    s32 idx = D_800EEBB0[0];
    u8 *entry = D_800EEBA8 + idx;
    s32 val = *entry;
    *entry = val + 1;
    return D_80098030[val];
}

/**
 * @brief Initialize shuffle buffer with random permutation.
 *
 * Clears D_800EEBB0, fills 8-entry shuffle buffer D_800EEBA8 with
 * sequential random values from func_8009B15C, then sets D_800EEBB0
 * to the final value & 7.
 * @param a0 Initial seed value stored into each buffer entry.
 */
void func_8009B198(s32 a0) {
    s32 buf;
    s32 i;

    D_800EEBB0[0] = 0;
    i = 0;
    buf = (s32)D_800EEBA8;

    do {
        *(u8 *)(i + buf) = a0;
        a0 = func_8009B15C() & 0xFF;
        i++;
    } while (i < 8);

    D_800EEBB0[0] = func_8009B15C() & 7;
}

/**
 * @brief Clear a color buffer array and set terminator.
 *
 * Zeros out a0[0..2] for each of a2 entries (4-byte stride), then
 * stores 0xFF into a1[0] as a terminator.
 * @param a0 Pointer to color buffer entries.
 * @param a1 Pointer to terminator byte.
 * @param a2 Number of entries to clear.
 */
void func_8009B208(u8 *a0, u8 *a1, s32 a2) {
    s32 i = 0;
    if (a2 > 0) {
top:
        a0[2] = 0;
        a0[1] = 0;
        a0[0] = 0;
        asm("");
        i++;
        if (i < a2) {
            a0 += 4;
            goto top;
        }
    }
    *a1 = 0xFF;
}

/**
 * @brief Find first entry with value 0xFF in a buffer.
 *
 * Scans up to a1 entries (4-byte stride) looking for a0[0] == 0xFF.
 * Returns the index (as u8) of the matching entry.
 * @param a0 Pointer to buffer entries.
 * @param a1 Maximum number of entries to scan.
 * @return Index of the first 0xFF entry, or 0 if not found.
 */
s32 func_8009B238(u8 *a0, s32 a1) {
    s32 i;
    for (i = 0; i < a1; i++) {
        if (a0[0] == 0xFF) {
            return (u8)i;
        }
        a0 += 4;
    }
    return 0;
}

/**
 * @brief Find first empty (inactive) entry in a buffer.
 *
 * Scans up to a1 entries (4-byte stride) looking for one where
 * both a0[0] and a0[1] are zero. Returns the entry index.
 * @param a0 Pointer to buffer entries.
 * @param a1 Maximum number of entries to scan.
 * @return Index of the first empty entry.
 */
s32 func_8009B270(u8 *a0, s32 a1) {
    s32 i;
    for (i = 0; i < a1; i++) {
        if (a0[1] == 0xFF) {
            return i;
        }
        a0 += 4;
    }
}

/**
 * @brief Allocate a task queue slot and link it.
 *
 * Finds an empty slot via func_8009B390, writes the source entry
 * data into it, and updates the doubly-linked list pointers.
 * @param a0 Pointer to task entry array.
 * @param a1 Pointer to head index.
 * @param a2 Number of slots.
 * @return Allocated slot index (as u8).
 */
s32 func_8009B2A4(u8 *table, u8 *head, s32 count) {
    s32 slot = (u8)func_8009B390(table, count);
    u8 *entry = (u8 *)(slot * 4 + (s32)table);

    entry[0] = *head;
    entry[1] = 0xFF;
    entry[2] = 0;

    if (*head != 0xFF) {
        table[*head * 4 + 1] = slot;
    }

    *head = slot;
    return slot;
}

/**
 * @brief Remove a task entry and relink neighbors.
 *
 * Given slot index a0, updates forward/backward links of adjacent
 * entries. Clears the removed entry. If the removed entry's forward
 * link is not 0xFF, updates that entry's backward link. Stores
 * new head if needed.
 * @param a0 Slot index to remove (masked to u8).
 * @param a1 Pointer to task entry array.
 * @param a2 Pointer to head index storage.
 */
void func_8009B320(s32 a0, u8 *a1, u8 *a2) {
    u8 *entry;
    s32 sentinel;
    s32 fwd;

    entry = (u8 *)((a0 & 0xFF) * 4 + (s32)a1);
    sentinel = 0xFF;

    fwd = entry[0];
    if (fwd != sentinel) {
        a1[fwd * 4 + 1] = entry[1];
    }
    fwd = entry[1];
    if (fwd != sentinel) {
        a1[fwd * 4] = entry[0];
    } else {
        *a2 = entry[0];
    }
    {
        u8 *e2 = (u8 *)((a0 & 0xFF) * 4 + (s32)a1);
        e2[2] = 0;
        e2[1] = 0;
        e2[0] = 0;
    }
}

/**
 * @brief Find first active entry in a linked buffer.
 *
 * Scans up to a1 entries (4-byte stride). An entry is active if
 * either byte 0 or byte 1 is nonzero. Returns the index of the
 * first inactive (both zero) entry found, or falls through.
 * @param a0 Pointer to buffer entries.
 * @param a1 Maximum number of entries to scan.
 * @return Index of the first inactive entry.
 */
s32 func_8009B390(u8 *a0, s32 a1) {
    s32 i;
    for (i = 0; i < a1; i++) {
        if (a0[0] == 0 && a0[1] == 0) {
            return (u8)i;
        }
        a0 += 4;
    }
}

/**
 * @brief Queue a task into the battle command queue.
 *
 * Allocates a slot in D_800EE24B (16 entries, head at +0x1F3),
 * multiplies slot index by 16 for the data offset, stores the
 * callback pointer, and returns the slot index as s16.
 *
 * @param arg0 Callback function pointer.
 * @return Allocated slot index (sign-extended to s16).
 */
s32 func_8009B3D0(s32 arg0) {
    s32 base;
    unsigned long slot;
    base = (s32)D_800EE24B;
    slot = func_8009B2A4((u8 *)base, (u8 *)(base + 0x1F3), 0x10);
    *((s32 *)((((s32)D_800EE24B) + ((2 * slot) * 8)) + 0x41)) = arg0;
    return (s16)slot;
}

/**
 * @brief Reset the task queue and associated data.
 *
 * Clears all 16 entries in D_800EE24B via func_8009B208, then
 * iterates 16 slots zeroing out extended data at offset 0x1153
 * (with -0x10 stride, descending).
 */
void func_8009B428(void) {
    register s32 base asm("$16") = D_800EE24B;
    s32 i;

    func_8009B208((u8 *)base, (u8 *)(base + 0x1F3), 0x10);

    i = 15;
    base += -0x1013;
    do {
        *(u8 *)(base + 0x1153) = 0;
        i--;
        base += -0x10;
    } while (i >= 0);
}

/**
 * @brief Execute pending tasks from the task queue.
 *
 * Scans 16 queue entries for the first active one (byte at 0x1103
 * == 0xFF). Loads and calls the callback function pointer from the
 * corresponding data slot via jalr. Continues processing linked
 * entries until terminator (0xFF) or abort flag (0x12F6).
 */
void func_8009B478(void) {
    s32 i = 0;
    s32 sentinel = 0xFF;
    s32 base = (s32)&D_800ED148;
    s32 base2;
    s32 entry;

    do {
        if (*(u8 *)(base + 0x1103) == sentinel) {
            goto found;
        }
        i++;
        base += 4;
    } while (i < 16);
    return;

loop:
    i = *(u8 *)(entry + 0x1104);
found:
    base2 = (s32)&D_800ED148;
    {
        s32 off;
        void (*fn)(s32);

        fn = (void (*)(s32))*(s32 *)(base2 + i * 16 + 0x1144);
        fn(i);
        off = i * 4;
        entry = off + base2;
        if (*(u8 *)(entry + 0x1104) == 0xFF) {
            return;
        }
        if (*(u8 *)(base2 + 0x12F6) != 0xFF) {
            goto loop;
        }
    }
}

/**
 * @brief Process completed tasks and remove them from queue.
 *
 * Iterates 16 queue slots. For each with completion flag == 1
 * (at offset 0x1153), clears the flag and calls func_8009B320
 * to unlink and free the slot.
 */
void func_8009B520(void) {
    s32 i = 0;
    s32 one = 1;
    s32 base = (s32)&D_800ED148;
    s32 ptr = base + ptr - ptr;

top:
    if (*(u8 *)(ptr + 0x1153) == one) {
        *(u8 *)(ptr + 0x1153) = 0;
        func_8009B320(i & 0xFF, (u8 *)(base + 0x1103), (u8 *)(base + 0x12F6));
    }
    i++;
    if (i < 16) {
        ptr += 0x10;
        goto top;
    }
}

/**
 * @brief Read a pair of s32 values from the @c D_800E19BC lookup table.
 *
 * The table holds (sector, size) or similar word pairs; this helper
 * fetches the pair at index @p idx into the caller's output pointers.
 *
 * @param idx Table index.
 * @param out1 Output pointer for the first word.
 * @param out2 Output pointer for the second word.
 */
void func_8009B59C(s32 idx, s32 *out1, s32 *out2) {
    /* FIXME: magic `* 2` / `+ 1` — D_800E19BC is logically an array of
       (sector, length) pairs; see decl above for why we can't use a
       struct-typed access here without breaking the match. */
    *out1 = D_800E19BC[idx * 2];
    *out2 = D_800E19BC[idx * 2 + 1];
}

/**
 * @brief Issue a CD or memory read for a track and arm the completion callback.
 *
 * Looks up a (sector, length) pair from D_800E19BC[idx*2..idx*2+1] and
 * dispatches the read: cdRead when direction is 0 (load from disc), or
 * func_80038868 otherwise (alternate transfer path). Both calls register
 * func_8009B654 as the completion callback. The user data and the data
 * length are cached in D_800ED148.unk128C / unk12D8 for the callback to
 * pick up later.
 * @param idx Index into D_800E19BC (each entry is two s32s).
 * @param dst Destination buffer / target address.
 * @param dir Direction flag (0 = cdRead, else func_80038868).
 * @param userData Opaque value forwarded to the callback via unk128C.
 */
void func_8009B5C4(s32 idx, s32 dst, s32 dir, s32 userData) {
    /* FIXME: magic `* 2` / `+ 1` — same issue as func_8009B59C: the table
       is conceptually CdReadEntry[] but only the flat s32[] form keeps
       gcc's dual-pointer addressing intact. */
    s32 sector = D_800E19BC[idx * 2];
    s32 length = D_800E19BC[idx * 2 + 1];
    if (dir == 0) {
        cdRead(sector, length, dst, (s32)func_8009B654);
    } else {
        func_80038868(sector, length, dst, (s32)func_8009B654);
    }
    D_800ED148.unk128C = userData;
    D_800ED148.unk12D8 = length;
}

/**
 * @brief Music playback completion callback.
 *
 * Called when music playback finishes. If the stored callback at
 * D_800ED148+0x128C is non-null, calls it with the stored data
 * pointer from D_800ED148+0x12D8.
 */
void func_8009B654(void) {
    s32 callback = D_800ED148.unk128C;
    if (callback != 0) {
        ((void (*)(s32))callback)(D_800ED148.unk12D8);
    }
}

/**
 * @brief Yield execution (wrapper for func_800393C8).
 *
 * Calls the system yield/wait function to give up the current
 * execution timeslice.
 */
void func_8009B690(void) {
    func_800393C8();
}

/**
 * @brief Yield execution (identical wrapper for func_800393C8).
 *
 * Second yield wrapper, functionally identical to func_8009B690.
 * Both exist as separate symbols for different call contexts.
 */
void func_8009B6B0(void) {
    func_800393C8();
}

/**
 * @brief Load and play a sound bank segment.
 *
 * Computes VRAM page from a0 (shifted left 7), reads base pointer
 * from D_800E19B4, calls cdReadSync to load the segment, then
 * calls memcopy to start playback.
 *
 * @param a0 Sound bank index (u16).
 * @param a1 Playback parameters pointer.
 */
void func_8009B6D0(s32 a0, s32 a1) {
    s32 temp;
    s32 temp2;
    s32 new_var;
    a0 = (a0 & 0xFFFF) << 7;
    new_var = a0;
    temp = new_var;
    if (new_var < 0) {
        temp = new_var + 0x7FF;
    }
    cdReadSync(*(s32 *)D_800E19B4 + (temp >> 11), 0x800, 0x801C0000, 0);
    memcopy((new_var & 0x7FF) | 0x801C0000, a1, 0x80);
}

/**
 * @brief Probability check using random shuffle buffer.
 *
 * Computes (a0 * 255) / a1 as threshold, gets a random value from
 * func_8009B15C, and returns 1 if the random value is less than
 * the threshold (and threshold is nonzero), else 0.
 * @param a0 Numerator for probability (0-255 range).
 * @param a1 Denominator for probability.
 * @return 1 if random check passes, 0 otherwise.
 */
s32 func_8009B74C(s32 a0, s32 a1) {
    s32 threshold = (a0 * 255) / a1;
    s32 rnd = func_8009B15C();
    if (threshold != 0 && (u32)threshold >= (u32)rnd) {
        return 1;
    }
    return 0;
}

/**
 * @brief Wrapper for probability check (func_8009B74C).
 * @param a0 Numerator.
 * @param a1 Denominator.
 * @return Forwarded result of @c func_8009B74C.
 */
s32 func_8009B79C(s32 a0, s32 a1) {
    return func_8009B74C(a0, a1);
}

/**
 * @brief Random value in range [1, a0] using modulo.
 *
 * Gets a random value from func_8009B15C, computes modulo a0,
 * and adds 1.
 * @param a0 Upper bound (exclusive before +1).
 * @return Random value in [1, a0].
 */
s32 func_8009B7BC(s32 a0) {
    s32 mod = a0;
    return func_8009B15C() % mod + 1;
}

/**
 * @brief Clamp a damage value to valid range.
 *
 * If D_800EE4C1 == 0xED (special battle mode) and the entity at
 * slot a1 has field 0x28 == 0, returns 0. Otherwise, checks
 * D_800EE456 bit 3: if set, max is 60000 (0xEA60); else max is
 * 9999 (0x270F). Returns clamped value: max if a0 > max, 0 if
 * a0 < 0, else a0.
 * @param a0 Raw damage value.
 * @param a1 Entity slot index.
 * @return Clamped damage value.
 */
s32 func_8009B7F4(s32 a0, s32 a1) {
    s32 max;

    if (D_800EE4C1[0] == 0xED) {
        s32 base = (s32)&D_800ED148;
        s32 offset = ((a1 * 2 + a1) * 4 + a1) * 16;
        if (*(s32 *)(base + offset + 0x28) == 0) {
            return 0;
        }
    }
    max = 9999;
    if (D_800EE456[0] & 8) {
        max = 60000;
    }
    if (a0 > max) {
        return max;
    }
    if (a0 < 0) {
        return 0;
    }
    return a0;
}

/**
 * @brief Apply status effect flags to entity based on linked data.
 *
 * For entities with index >= 3, checks linked entity data at offset
 * 0xF7 for status immunity flags. Bit 0 controls the 0x40 flag on
 * status (u16 at a1), bit 1 controls the 0x2000 flag on ability
 * flags (s32 at a2). The a3 parameter determines set vs clear.
 * @param a0 Entity slot index.
 * @param a1 Pointer to entity status u16.
 * @param a2 Pointer to entity ability flags s32.
 * @param a3 Mode: 0 = clear flags, nonzero = set flags.
 */
void func_8009B878(s32 a0, u16 *a1, s32 *a2, s32 a3) {
    s32 base;
    s32 offset;
    u8 *linked;
    s32 status;
    s32 flags;

    if (a0 < 3) {
        return;
    }
    base = (s32)&D_800ED148;
    offset = ((a0 * 2 + a0) * 4 + a0) * 16;
    linked = *(u8 **)(*(s32 *)(offset + base + 0x10));
    status = *a1;
    if (status & 0x40) {
        if (*(u8 *)(linked + 0xF7) & 1) {
            if (a3 != 0) {
                *a1 = status | 0x40;
            } else {
                *a1 = status & 0xFFBF;
            }
        }
    }
    flags = *a2;
    if (flags & 0x2000) {
        if (*(u8 *)(linked + 0xF7) & 2) {
            if (a3 == 0) {
                *a2 = flags & ~0x2000;
            } else {
                *a2 = flags | 0x2000;
            }
        }
    }
}

/**
 * @brief Apply status effects from a bitmask to an entity.
 *
 * Iterates 14 status bits. For each bit set in a2, calls
 * func_800B0668 to check if the effect should be removed; if so,
 * clears that bit. Then iterates again calling func_800B0600 for
 * remaining set bits. Finally calls func_8009B878 to apply linked
 * entity flags.
 * @param a0 Entity slot index.
 * @param a1 Bitmask of status effects to clear from entity.
 * @param a2 Bitmask of status effects to apply.
 */
void func_8009B924(s32 slot, s32 clearMask, s32 applyMask) {
    BattleEntity *entities;
    s32 bit;
    s32 i;

    ((BattleEntity *)&D_800ED148)[slot].status &= ~clearMask;

    for (bit = 1, i = 0; i < 14; i++, bit <<= 1) {
        if (applyMask & bit) {
            if (func_800B0668(slot, bit))
                applyMask &= ~bit;
        }
    }

    ((BattleEntity *)&D_800ED148)[slot].flags &= ~applyMask;

    for (bit = 1, i = 0; i < 14; i++, bit <<= 1) {
        if (applyMask & bit)
            func_800B0600(slot, bit);
    }

    func_8009B878(slot,
        &((BattleEntity *)&D_800ED148)[slot].status,
        (s32 *)&((BattleEntity *)&D_800ED148)[slot].flags, 1);
}

/**
 * @brief Get combined status flags for an entity from ability data.
 *
 * Reads ability flags for the given entity slot from g_gfData,
 * queries two flag lookup functions, OR's results together.
 * If bit 15 is set in the combined result, returns it masked to u16;
 * otherwise returns defaultStatus masked to u16.
 * @param slot Entity slot index.
 * @param defaultStatus Default status value if no ability flags apply.
 * @return Combined or default status flags (u16).
 */
s32 func_8009BA5C(s32 slot, s32 defaultStatus) {
    unsigned short result;

    result = func_800B0F9C(g_gfData.subTableS[slot].abilityFlags);
    result |= func_800B0F7C(g_gfData.subTableS[slot].abilityFlags);

    if (result & 0x8000)
        return (u16)result;
    return (u16)defaultStatus;
}
