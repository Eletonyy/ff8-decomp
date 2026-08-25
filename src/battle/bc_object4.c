#include "common.h"
#include "battle.h"
#include "gf.h"
#include "gamestate.h"
#include "battle/bc_object4.h"

extern void func_800E1850(void);



void func_800A6184(s32 arg0, s32 arg1, s32 arg2, u16 arg3) {
    func_800A5A7C(arg0, arg1, arg2, 0, arg3, 0, &D_800ED148.unk1244[arg0].unk0);
}

void func_800A61CC(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u16 arg5) {
    func_800A5A7C(arg0, arg1, arg2, arg4, arg5, arg3, &D_800ED148.unk1244[arg0].unk0);
}

/**
 * @brief Check entity flag 0x10 at offset 0x8C and either set bit 0x200 or call display handler.
 *
 * If the entity at D_800ED148[a0] (stride 0xD0) has bit 0x10 set in its
 * word at offset 0x8C, sets bit 0x200 in that word. Otherwise, calls
 * func_800A62DC with display parameters (0x11, 0x80, 0).
 *
 * @param a0 Entity index.
 */
void func_800A6218(s32 arg0) {
    if ((D_800ED148.entities[arg0].controlFlags & CTRL_FLAG_10)) {
        D_800ED148.entities[arg0].controlFlags |= 0x200;
    }
    
    else {
        func_800A62DC(arg0, 17, 128, 0);
    }
}

/**
 * @brief Call func_800D15B4 with display parameters and grid settings.
 *
 * @param a0 First display parameter.
 * @param a1 Brightness or alpha value.
 * @param a2 Size parameter.
 * @param a3 Grid configuration.
 */
void func_800A6288(s32 a0, s32 a1, s32 a2, s32 a3) {
    func_800A62DC(a0, 18, 128, 0);
}

/**
 * @brief Initialize display with default bright settings.
 *
 * Calls func_800A62DC with a0=0, a1=0x41, a2=0x80, a3=0.
 */
void func_800A62B0(void) {
    func_800A62DC(0, 65, 128, 0);
}

void func_800A62DC(s32 a0, s32 a1, s32 a2, s32 a3) {
    func_800D15B4(a0, a1, a2, a3);
}

s32 func_800A62FC(void) {
    return D_800ED148.unk12EB;
}

/**
 * @brief Initialize battle entity state and process active entries.
 *
 * Sets up entity flags, copies status bytes to D_800EE4C0, stores
 * the parameter to D_800EEBC8, then iterates through active entity
 * entries calling func_800A09D0 and func_800A5210 for each.
 *
 * @param a0 Value to store at D_800EEBC8.
 */
void func_800A6310(u8 arg0) {
    s32 i;
    BattleEntry* entry;
    SubEntry* subEntry;

    D_800ED148.unk131E = 1;
    D_800EEBC8 = arg0;
    D_800ED148.unk1300 = 0;
    D_800ED148.unk5C1 = 0;
    entry = D_800ED148.entries;
    D_800EE4C0.unk0 = entry->unk0;
    D_800EE4C0.unk1 = entry->unk1;
    D_800EE4C0.statusCode = entry->unk4;

    for (i = 0; i < entry->unk10; i++) {
        subEntry = &D_800ED148.Array844[i];
        func_800A09D0(subEntry->unk0);
        func_800A5210(subEntry->unk0);
    }
}

void func_800A63C0(s16 arg0, s16 arg1, s16 arg2, s16 arg3) {
    D_800ED148.unk1290 = arg0;
    D_800ED148.unk1292 = arg1;
    D_800ED148.unk1296 = arg3;
    D_800ED148.unk1294 = arg2;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A63DC);

/**
 * @brief Initiate battle sequence based on mode flag.
 *
 * If a0 is 0, plays sound 0xED with entity data, runs scene setup,
 * and registers handler func_800E1850. If a0 is non-zero, calls
 * func_800AA4E8 first, plays sound 0xEE, and dispatches event 8.
 *
 * @param a0 Mode flag (0 = player-initiated, non-zero = triggered).
 * @param a1 Sound parameter (masked to u16 in mode 0).
 */
void func_800A64E4(s32 arg0, u16 arg1) {
    if (arg0 == 0) {
        func_800B0754(D_800ED148.entities[0].entityRef, 237, D_800ED148.unk1324, arg1);
        decrementItemByType(D_800ED148.unk1324 + 0x65);
        func_8009AF14(func_800E1850);
        return;
    }
    
    func_800B0754(D_800ED148.entities[0].entityRef, 238, D_800ED148.unk1324, func_800AA4E8());
    func_8009AE08(8);
}

/**
 * @brief Store a byte to D_800ED148 offset 0x12EF.
 *
 * @param value Byte value to store.
 */
void func_800A6574(u8 arg0) {
    D_800ED148.unk12EF = arg0;
}

void func_800A6588(void) {
    D_800ED148.unk131C = 0;
    func_8009AE08(8);
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A65B0);

void func_800A66D0(s32 arg0) {
    if (func_8009A514(D_800ED148.unkCE3, 7 - arg0) != 0) {
        D_800ED148.unkD5C[arg0] = 1;
    }
}

/**
 * @brief Clear entity active flags and reinitialize all 8 slots.
 *
 * Clears 8 bytes at D_800ED148+0xD5C (entity active flags),
 * then calls func_800A66D0 for each slot 0-7 to re-check
 * and set active status.
 */
void func_800A6724(void) {
    s32 i;
    
   for (i = 0; i < 8; i++) {
        D_800ED148.unkD5C[i] = 0;
    }


    for (i = 0; i < 8; i++) {
        func_800A66D0(i);
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A6780);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A67FC);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A68AC);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A69BC);

/**
 * @brief Process entities 1 and 2, then call func_800A67FC with 0.
 */
void func_800A6A58(void) {
    func_800A68AC(1);
    func_800A68AC(2);
    func_800A67FC(0);
}

/**
 * @brief Process battle entities 1 and 2 via func_800A68AC.
 */
void func_800A6A88(void) {
    func_800A68AC(1);
    func_800A68AC(2);
}

/**
 * @brief Process entities 0 and 3, then call func_800A67FC with 1.
 */
void func_800A6AB0(void) {
    func_800A68AC(0);
    func_800A68AC(3);
    func_800A67FC(1);
}

/**
 * @brief Process battle entities 0 and 3 via func_800A68AC.
 */
void func_800A6AE0(void) {
    func_800A68AC(0);
    func_800A68AC(3);
}

/**
 * @brief Check if any entity in slots 3-6 has bit 2 set in indirect flag byte.
 *
 * Iterates over entity slots 3 through 6 in the D_800ED148 table (stride 0xD0).
 * For each entity whose status halfword at offset 0x90 does not have bit 0 set,
 * follows the pointer chain at offset 0x10 twice, then checks if bit 2 (0x4)
 * is set in the byte at offset 0xFE of the resulting pointer.
 *
 * @return 1 if any qualifying entity has the flag set, 0 otherwise.
 */
s32 func_800A6B08(void) {
    s32 i;

    for (i = 3; i < 7; i++) {
        if (!(D_800ED148.entities[i].status & 1) && (*D_800ED148.entities[i].entityData)->unkFE & 4) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief Check if all entities in slots 3-6 have a specific flag bit set.
 *
 * Iterates over entity slots 3 through 6 in the D_800ED148 table (stride 0xD0).
 * For each entity whose status halfword at offset 0x90 does not have bit 0 set,
 * follows the pointer chain at offset 0x10 twice, then checks if the bitmask a0
 * is set in the byte at offset 0xFE.
 *
 * @param a0 Bitmask to check against the flag byte.
 * @param a1 Value to return if all entities pass the check.
 * @return 0 if any qualifying entity lacks the flag, a1 if all pass.
 */
s32 func_800A6B6C(s32 arg0, s32 arg1) {
    s32 i;

    for (i = 3; i < 7; i++) {
        if (!(D_800ED148.entities[i].status & 1) && !((*D_800ED148.entities[i].entityData)->unkFE & arg0)) {
            return 0;
        }
    }
    
    return arg1;
}

/**
 * @brief Determine battle result based on condition and ability check.
 *
 * If a0 is 1, returns 0. Otherwise checks func_8009B74C(0x80, 0xFF)
 * to determine the return value. For a0==0, returns 3 or 4; for other
 * values, returns 1 or 2.
 *
 * @param a0 Condition selector.
 * @return Battle result code (0-4).
 */
s32 func_800A6BD0(s32 a0) {
    if (a0 == 1) {
        return 0;
    }

    if (a0 == 0) {
        if (func_8009B74C(128, 255)) {
            return 3;
        }

        return 4;
    }

    if (func_8009B74C(128, 255)) {
        return 1;
    }

    return 2;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A6C34);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A6D30);

s32 func_800A6DD8(void) {
    s32 i;
    s32 var_a2;
    s32 var_v1;

    var_a2 = 0;
    var_v1 = 0;
    
    for (i = 0; i < 3; i++) {
        if (D_800ED148.entities[i].linkedIdx != 255) {
            var_a2++;
            var_v1 += D_800ED148.entities[i].unkCC;
        }
    }
    
    return var_v1 / var_a2;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A6E2C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A6EBC);

/**
 * @brief Compute random percentage clamped to [1, 100].
 *
 * Gets a random number from func_8009B15C, divides by func_800A6E2C
 * result, takes the remainder. Clamps: 0 becomes 1, values above
 * 100 become 100.
 *
 * @return Clamped remainder in range [1, 100].
 */
INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A6F64);

/**
 * @brief Generate random value in range [1, 100].
 *
 * Calls func_8009B15C to get a random number, computes modulo 100,
 * and adds 1.
 *
 * @return Random value in [1, 100].
 */
s32 func_800A6FB8(void) {
    return func_8009B15C() % 100 + 1;
}

s32 func_800A700C(s32 arg0) {
    return func_800A6E2C() - (200 - arg0);
}

/**
 * @brief Compute adjusted value and return the lesser of it and func result.
 *
 * Calls func_800A6E2C, subtracts 100 from a0, and returns the minimum
 * of the two values.
 *
 * @param a0 Input value (100 subtracted before comparison).
 * @return Minimum of (a0 - 100) and func_800A6E2C result.
 */
s32 func_800A703C(s32 a0) {
    s32 limit = func_800A6E2C();
    a0 -= 100;
    if (a0 < limit) {
        return a0;
    }
    return limit;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7080);

/**
 * @brief Clamp func_800A7080 result to maximum 100.
 *
 * @return min(func_800A7080(), 100).
 */
/**
 * @brief Compute a value and clamp to maximum of 100.
 *
 * @return The result of func_800A7080, capped at 100.
 */
s32 func_800A7154(void) {
    s32 val = func_800A7080();
    if (val > 100) {
        return 100;
    }
    
    return val;
}

/**
 * @brief Clear five bytes around the animation-param region of a battle slot.
 *
 * Zeroes the @c animParam3 halfword (offsets @c 0x88-0x89), the first
 * byte of @c pad8A (@c 0x8A), and bytes @c 0xC7 / @c 0xC8 inside
 * @c padBC[]. Order: @c 0x8A → @c 0x89 → @c 0x88 → @c 0xC7 → @c 0xC8.
 *
 * @param slot Battle slot to clear.
 */
void func_800A7188(BattleEntityData* arg0) {
    arg0->unk8A = 0;
    arg0->unk89 = 0;
    arg0->unk88 = 0;
    arg0->unkC7 = 0;
    arg0->unkC8 = 0;
}

/**
 * @brief Clear 8 consecutive words starting at @c slot+0x24.
 *
 * Zeroes 8 @c s32 words at offsets @c 0x24, @c 0x28, @c 0x2C, @c 0x30,
 * @c 0x34, @c 0x38, @c 0x3C, @c 0x40 — i.e. @c field24, @c field28,
 * @c field2C, and the first 0x14 bytes of @c pad30[].
 *
 * @param slot Battle slot whose mid-region words are zeroed.
 */
void func_800A71A0(BattleEntityData* arg0) {
    s32 i;

    for (i = 0; i < 8; i++) {
        arg0->unk24[i] = 0;
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A71C0);

/**
 * @brief Initialize a battle slot from its corresponding @c BattleCharData.
 *
 * Looks up @c g_battleChars[idx] and @c D_800ED158.slots[idx], then:
 *  - If the source's @c characterId byte (offset @c 0x1C3) is @c 0xFF
 *    (slot empty), zeroes the slot's flags word at @c 0x7C, sets
 *    @c linkedIdx2 to @c 0xFF and returns.
 *  - Otherwise copies @c characterId into the slot's @c linkedIdx2,
 *    seeds the flags word at @c 0x7C with @c 0x8801, and mirrors
 *    @c displayStatus from the char data into the slot's halfword at
 *    @c 0x80.
 *  - Tests two ability/effect tables in @c D_80078E00 (at offsets
 *    @c 0x35C3 stride 12 keyed by @c charData[0x1BA], and @c 0x37A7
 *    stride 36 keyed by @c characterId); each set bit ORs an extra
 *    flag (@c 0x1000 / @c 0x100) into @c slot+0x7C.
 *  - Translates four bits of @c charData->statusFlags (@c 0x190) into
 *    flags in @c slot+0x8: @c 0x1000 → @c 0x80 (overwrites), then
 *    @c 0x4000 → @c |= @c 0x20, @c 0x2000 → @c |= @c 0x40,
 *    @c 0x8000 → @c |= @c 0x2.
 *  - Calls @c func_800A7188 (clears 5 bytes), @c func_800A71A0
 *    (zeroes 8 words), and @c func_800A554C(idx) (a stat/AI hook).
 *  - If neither @c slot+0x80 bit @c 0x1 nor @c 0x4 is set: when
 *    @c charData->statusFlags has bit @c 0x10000, copies word at
 *    @c D_800ED148.entities[idx]+0x20 into @c +0x24; otherwise calls
 *    @c func_800A559C(idx).
 *  - Finally fills the 40-byte @c slot+0x90..0xB7 region with @c 100
 *    (probably an HP-percent / stat init).
 *
 * @param idx Battle slot index (0..6 covers the 7 active slots).
 */

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7518); 
/*
void func_800A7518(s32 idx) {
    BattleCharData *charData = &g_battleChars.chars[idx];
    BattleEntity   *slot     = &D_800ED158.slots[idx];
    u8              charId;
    s32             i;

    charId = charData->characterId;
    if (charId == 0xFF) {
        slot->field64.slotInit.slotFlags = 0;
        slot->linkedIdx2                 = 0xFF;
        return;
    }

    {
        u16 dispStatus;
        slot->linkedIdx2                 = charId;
        dispStatus                       = charData->displayStatus;
        slot->field64.slotInit.slotFlags = 0x8801;
        slot->at0x80.slotDisplay         = dispStatus;
    }

    if (g_gfData.levelCurve12[charData->classId].field0B & 1) slot->field64.slotInit.slotFlags |= 0x1000;
    if (g_gfData.xpCurves36[slot->linkedIdx2].field03    & 1) slot->field64.slotInit.slotFlags |= 0x100;

    slot->slot8.initFlags = 0;
    if (charData->statusFlags & 0x1000) slot->slot8.initFlags  = 0x80;
    if (charData->statusFlags & 0x4000) slot->slot8.initFlags |= 0x20;
    if (charData->statusFlags & 0x2000) slot->slot8.initFlags |= 0x40;
    if (charData->statusFlags & 0x8000) slot->slot8.initFlags |= 0x02;

    func_800A7188(slot);
    func_800A71A0(slot);
    func_800A554C(idx);

    {
        s32 fillVal;
        u8 *p;

         //  The level-up sound test reads slot+0x80 as a 32-bit word (the write
         above was 16-bit) — pick up the wider view via the same union.  //
        if (!(slot->at0x80.word & 5)) {
            if (charData->statusFlags & 0x10000) {
                // The entity table at D_800ED148 starts 0x10 bytes before the
                   slot table at D_800ED158 — entity[idx] is computed from
                   the slot table base so gcc reuses the D_800ED158 saved-reg
                   instead of re-emitting lui+addiu for D_800ED148. //
                volatile BattleEntity *entity =
                    (volatile BattleEntity *)((u8 *)&D_800ED158 - 0x10) + idx;
                entity->field24 = entity->field20;
            } else {
                func_800A559C(idx);
            }
        }

        // Fill 40 bytes of @c slot[status..pad96+0x21] with 100, walked in
           reverse so gcc 2.7.2 emits @c sb with an immediate displacement
           (the offset of @c slot->status) and the walker decrement falls
           into the @c bgez delay slot. //
        fillVal = 100;
        i       = 0x27;
        p       = (u8 *)slot + 0x27;
        for (; i >= 0; i--) {
            p[(s32)&((BattleEntity *)0)->status] = fillVal;
            p--;
        }
    }
}
*/

/**
 * @brief Test a bit in the g_gameState bitfield at offset 0xD04.
 *
 * If a0 is nonzero, computes bit position (a0 - 1), divides by 32 to
 * find the word index and remainder, then tests that bit in the bitfield
 * at g_gameState + word_index * 4 + 0xD04.
 *
 * @param a0 1-based bit position to test. If 0, returns undefined.
 * @return 1 if the bit is set, 0 if clear.
 */
INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A774C);

/**
 * @brief Set a bit in the g_gameState bitfield at offset 0xD04.
 *
 * If a0 is nonzero, computes bit position (a0 - 1), divides by 32 to
 * find the word index and remainder, then sets that bit in the bitfield
 * at g_gameState + word_index * 4 + 0xD04.
 *
 * @param a0 1-based bit position to set. If 0, does nothing.
 */
void func_800A779C(s32 arg0) {
    s32 idx;
    s32 bit;
    s32 temp_v0;
    
    temp_v0 = arg0 - 1;
    if (arg0 != 0) {
        idx = temp_v0 / 32;
        bit = temp_v0 - (idx * 32);

        g_gameState.mainData.array210[idx] |= 1 << bit;
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A77E8);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7884);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7934);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A79A0);

/**
 * @brief Look up the per-class @c field09 byte for the entity at @p a0.
 *
 * Reads @c BattleCharData[a0].classId, then returns
 * @c g_gfData.levelCurve12[classId].field09 (offset @c 0x35C1 in the
 * shared @c D_80078E00 / @c g_gfData block).
 *
 * @param a0 Entity index (stride 0x1D0 in @c g_battleChars).
 * @return The @c field09 byte for that entity's class.
 */
u8 func_800A7A44(s32 arg0) {
    return D_80078E00.array35BD[g_battleChars.chars[arg0].classId].unk4;
}

/**
 * @brief Look up a byte attribute from g_battleChars entity table (stride 0x1D0).
 *
 * @param idx Entity index.
 * @return Byte at offset 0x1B9 within the entity entry.
 */
u8 func_800A7A8C(s32 arg0) {
    return g_battleChars.chars[arg0].unk1B9;
}

/**
 * @brief Get a byte field from an entity's nested pointer chain.
 *
 * Looks up D_800ED148[a0] (stride 208), follows the pointer at +0x10,
 * dereferences it, then returns the byte at offset +0xF6.
 *
 * @param a0 Entity index.
 * @return Byte value at the end of the pointer chain.
 */
u8 func_800A7AB8(s32 arg0) {
    return (*D_800ED148.entities[arg0].entityData)->unkF6;
}

/**
 * @brief Traverse a multi-level pointer chain from entity data.
 *
 * Indexes into D_800ED148 by a0*208, follows pointers at offsets
 * 0x14, 0x0, 0x4 (added back), 0xC (added back), and returns
 * the first byte at the final address.
 *
 * @param a0 Entity index (stride 208).
 * @return Byte value at the end of the pointer chain.
 */
s32 func_800A7AF4(s32 a0) {
    s32 base = (s32)&D_800ED148;
    s32 ptr;
    s32 p2;
    ptr = *(s32 *)(base + a0 * 208 + 0x14);
    p2 = *(s32 *)ptr;
    ptr = *(s32 *)(p2 + 4);
    ptr += p2;
    p2 = *(s32 *)(ptr + 0xC);
    ptr += p2;
    return *(u8 *)ptr;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7B48);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7C64);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7CEC);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7D8C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7EE0);
 
s32 func_800A7FB4(BattleEntityData* arg0, s32 arg1) {
    return arg0->unk160[arg1] * 10;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A7FD0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A82A0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A8320);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A8430);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A84CC);

/**
 * @brief Call func_800A84CC for each of 7 battle entities.
 */
void func_800A853C(void) {
    s32 i;
    
    for (i = 0; i < 7; i++) {
        func_800A84CC(i);
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A8578);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A864C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A86F0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A8794);

/**
 * @brief Clear 4 status bytes in an entity's sub-table.
 *
 * Computes D_800EE9B3 + a0 * 71 as base, then clears bytes
 * at offsets 0xD, 0x9, 0x5, and 0x1 (stride -4, 4 iterations).
 *
 * @param a0 Entity index (stride 71).
 */
void func_800A890C(s32 a0) {
    s32 i = 3;
    s32 offset = a0 * 71;
    s32 base = (s32)D_800EE9B3;
    u8 *ptr = (u8 *)(offset + base + 0xC);
    do {
        *(u8 *)(ptr + 1) = 0;
        i--;
        ptr -= 4;
    } while (i >= 0);
}

/**
 * @brief Check bit 0 of flags for entities 3-6 and call handler.
 *
 * Iterates over entities at indices 3 through 6 in the D_800ED148 table
 * (stride 0xD0). For each entity whose flags word at offset 0x8C has
 * bit 0 set, calls func_800A890C with the entity index.
 */
INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A8948);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A89B8);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object4", func_800A8A48);

s32 func_800A8AFC(s32 arg0) {
    BattleCharData* ptr = &g_battleChars.chars[arg0];

    func_800A8A48(ptr, 0, 35, 0);
    func_800A8A48(ptr, 1, 36, 0);
    func_800A8A48(ptr, 2, 0, 0);
    
    return 2;
}