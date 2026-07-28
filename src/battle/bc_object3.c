#include "common.h"
#include "gamestate.h"
#include "battle.h"

extern u8 D_800786D8[];
extern u8 D_800EE424[];
extern u8 D_800EE43C[];
extern u8 D_800EE462[];
extern u8 D_80077E58[];
void func_800A5948(s32, s32);
void func_800A18E0(s32);
void func_800A589C(s32);
void func_800A6288(s32);
void func_800DEAA4(s32, s32);
void func_800E1880(s32, s32);
void func_800AF8A4(s32);
s32 func_800AE568(s32);
s32 func_800AE64C(s32);
s32 func_800A4F28(s32, s32, s32);
void func_800A6184(s32 a0, s32 a1, s32 a2, u16 a3); // bc obj4
extern u8 D_800ED156[];

/**
 * @brief Clear entity status bits 3 and 2 at offset 0x8C, then call cleanup.
 *
 * Computes the entity address at D_800ED148 + a0 * 0xD0. Clears bits 3 and 2
 * of the word at entity offset 0x8C, then calls func_800A6288.
 *
 * @param a0 Entity index (stride 0xD0).
 */
void func_800A18E0(s32 a0) {
    BattleSystem *entityBase;
    s32 entity;
    s32 mask;
    entityBase = &D_800ED148;
    entity = (s32)entityBase + a0 * 0xD0;
    mask = ~0x8;
    *(volatile s32 *)(entity + 0x8C) &= mask;
    mask = ~0x4;
    *(volatile s32 *)(entity + 0x8C) &= mask;
    func_800A6288(a0);
}

/**
 * @brief Initialize entity and display state for the given index.
 *
 * Calls func_800A18E0 and func_800A589C with the entity index,
 * then clears the word at entity offset 0x24 and copies it to
 * the display structure at offset 0x184.
 *
 * @param a0 Entity index (stride 0xD0 for entity, 0x1D0 for display).
 */
void func_800A1940(s32 a0) {
    u8 *displayBase;
    BattleSystem *entityBase;
    s32 display;
    s32 entity;
    s32 offset = 0x24;
    func_800A18E0(a0);
    func_800A589C(a0);
    displayBase = (u8 *)&g_battleChars;
    entityBase = &D_800ED148;
    entity = (s32)entityBase + a0 * 0xD0;
    display = (s32)displayBase + a0 * 0x1D0;
    *(volatile s32 *)(entity + offset) = 0;
    *(s32 *)(display + 0x184) = *(volatile s32 *)(entity + 0x24);
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A19BC);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A1AB8);

/**
 * @brief Clear entity field 0x24 and status bits 3 and 2 at offset 0x8C,
 * then call func_800A589C.
 *
 * Computes entity address at D_800ED148 + a0 * 0xD0. Clears the word at
 * entity offset 0x24, then clears bits 3 and 2 of entity offset 0x8C,
 * and calls func_800A589C with the entity index.
 *
 * @param a0 Entity index (stride 0xD0).
 */
void func_800A1C98(s32 a0) {
    BattleSystem *entityBase;
    s32 entity;
    s32 mask;
    entityBase = &D_800ED148;
    entity = (s32)entityBase + a0 * 0xD0;
    *(volatile s32 *)(entity + 0x24) = 0;
    mask = ~0x8;
    *(volatile s32 *)(entity + 0x8C) &= mask;
    mask = ~0x4;
    *(volatile s32 *)(entity + 0x8C) &= mask;
    func_800A589C(a0);
}

/**
 * @brief Check entity flags and trigger reset if flagged.
 *
 * Computes the entity at D_800ED148 + a0 * 0xD0. If bit 16 of the word
 * at +0x18 is set, or bit 0 of the halfword at +0x90 is set, calls
 * func_800A1C98 and func_800A84CC with the entity index.
 *
 * @param a0 Entity index (stride 0xD0 in D_800ED148).
 */
void func_800A1CFC(s32 a0) {
    volatile u8 *base = (u8 *)&D_800ED148;
    u8 *entity = (u8 *)base + a0 * 0xD0;
    if ((*(s32 *)(entity + 0x18) & 0x10000) ||
        (*(u16 *)(entity + 0x90) & 1)) {
        func_800A1C98(a0);
        func_800A84CC(a0);
    }
}

/**
 * @brief Subtract delta from a halfword value, clamping to zero.
 *
 * @param unused0 Unused parameter.
 * @param unused1 Unused parameter.
 * @param delta Amount to subtract.
 * @param ptr Pointer to structure; halfword at offset 0x18 is modified.
 */
void func_800A1D78(s32 unused0, s32 unused1, s32 delta, u8 *ptr) {
    s16 val = *(u16 *)(ptr + 0x18) - delta;
    *(u16 *)(ptr + 0x18) = val;
    if (val <= 0) {
        *(u16 *)(ptr + 0x18) = 0;
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A1DA0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A1E04);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A1EC8);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A1F74);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2008);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A20AC);

/**
 * @brief Count active party entities with display flag bit 0 set.
 *
 * Iterates through 3 party slots checking D_800ED148 (stride 0xD0) for
 * active flag bit 0 at +0x8C, and g_battleChars (stride 0x1D0) for display
 * flag bit 0 at +0x1B2. Returns count of entities with both flags set.
 *
 * @return Number of active displayed party members (0-3).
 */
s32 func_800A2150(void) {
    s32 count = 0;
    s32 i = 0;
    u8 *display = (u8 *)&g_battleChars;
    u8 *entity = (u8 *)&D_800ED148;

    do {
        if ((*(s32 *)(entity + 0x8C) & 1) && (*(u16 *)(display + 0x1B2) & 1)) {
            count++;
        }
        display += 0x1D0;
        i++;
        entity += 0xD0;
    } while (i < 3);

    return count;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A21B0);

/**
 * @brief Check if entity ability slot is available.
 *
 * Loads the ability index from entity[0xDA] at D_800ED148 + a0 * 0xD0,
 * computes ability_index * 60, and calls func_8009B79C to check
 * availability with mask 0xFF.
 *
 * @param a0 Entity slot index.
 * @return 1 if ability slot is available, 0 otherwise.
 */
s32 func_800A2310(s32 a0) {
    u8 *base = (u8 *)&D_800ED148;
    u8 *entity = base + a0 * 0xD0;
    u8 val = entity[0xDA];
    return func_8009B79C((s32)val * 60, 0xFF) != 0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2360);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A240C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2480);

/**
 * @brief Call func_8009B924 with constants 0x7A and 0x030E77FF.
 *
 * @param a0 First argument passed through.
 */
void func_800A2520(s32 a0) {
    func_8009B924(a0, 0x7A, 0x030E77FF);
}

/**
 * @brief Call func_8009B924 with constants 0x7E and 0x0180560D.
 *
 * @param a0 First argument passed through.
 */
void func_800A2548(s32 a0) {
    func_8009B924(a0, 0x7E, 0x0180560D);
}

/**
 * @brief Call func_8009B924 with constants 0x37E and 0x038E7FFF.
 *
 * @param a0 First argument passed through.
 */
void func_800A2570(s32 a0) {
    func_8009B924(a0, 0x37E, 0x038E7FFF);
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2598);

void func_800A2638(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    if (arg2 == 0) {
        if (!(arg1 & 1) && (arg0 != 0)) {
            D_800ED148.entities[arg3].unk9E = 0;
            return;
        }
        
        D_800ED148.entities[arg3].unk9E = 1;
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A26A0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2724);

/**
 * @brief Check battle status flags and optionally store adjusted value.
 *
 * Reads D_800786D8[0] and tests bit flags. If bit 0 is clear, returns 0.
 * If bit 1 is set, returns 1. Otherwise, stores D_800786D8[0x2D] + 2
 * into *a0 as a halfword and returns 2.
 *
 * @param a0 Pointer to halfword destination (written only if returning 2).
 * @return 0, 1, or 2 depending on flag state.
 */
s32 func_800A2CE4(s16 *a0) {
    u8 byte = D_800786D8[0];
    if (byte & 1) {
        if (byte & 2) {
            return 1;
        }
        *a0 = D_800786D8[0x2D] + 2;
        return 2;
    }
    return 0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2D24);

/**
 * @brief Look up a byte from a two-level table.
 *
 * Indexes into D_800ED148 by a0*208, reads a byte at offset 0xDA,
 * then uses (byte-1)*2 to index into D_80078E00 at offset 0x4CFD.
 *
 * @param a0 Entity index (stride 208).
 * @return Byte value from the second-level table.
 */
s32 func_800A2E04(s32 a0) {
    u8 *table = (u8 *)&D_80078E00;
    u8 *base = (u8 *)&D_800ED148;
    s32 val = *(u8 *)(base + a0 * 208 + 0xDA);
    val = (val - 1) * 2;
    return *(u8 *)(table + val + 0x4CFD);
}

/**
 * @brief Look up a halfword value from a table using entity ability data.
 *
 * Calls func_8009B7BC(2) to get a base offset, then indexes into
 * D_800ED148 entity table at stride 0xD0 to read ability byte at +0xDA.
 * Combines that with the base offset and a0 to read a halfword from
 * offset 0x150.
 *
 * @param a0 Table base pointer or offset.
 * @param a1 Entity index (stride 0xD0).
 * @return Halfword value from computed table location.
 */
s32 func_800A2E48(s32 a0, s32 a1) {
    s32 table = a0;
    s32 idx = a1;
    s32 val = func_8009B7BC(2);
    u8 *base = (u8 *)&D_800ED148;
    s32 ability = *(u8 *)(base + idx * 0xD0 + 0xDA);
    ability += val;
    ability <<= 1;
    ability += table;
    return *(u16 *)(ability + 0x150);
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2EB8);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2EF8);

/**
 * @brief Look up ability flag and value, then call func_800E1880.
 *
 * Reads battle state index from D_800ED148[0x1324], entity index from
 * D_800ED148[0xF]. Looks up flag byte from D_80078E00 table (stride 24,
 * offset 0x4802), checks bit 0x10. Also reads entity ability byte at
 * offset 0xDA (stride 0xD0), uses it to index D_80078E00 at offset 0x4D03.
 * Calls func_800E1880 with inverted flag and the lookup result.
 */
void func_800A2F54(void) {
    u8 *table = (u8 *)&D_80078E00;
    u8 *base = (u8 *)&D_800ED148;
    u8 idx1 = base[0x1324];
    u8 idx2 = base[0xF];
    u8 *entry = table + (s32)idx1 * 24;
    u8 *entity = base + (s32)idx2 * 0xD0;
    s32 flag = entry[0x4802] & 0x10;
    u8 val = entity[0xDA];
    func_800E1880(flag == 0, (s32)table[val + 0x4D03]);
}

/**
 * @brief Look up ability data and dispatch via func_800DEAA4.
 *
 * Reads entity index from D_800ED148[0xF], computes entity pointer
 * (stride 0xD0), reads ability byte at entity+0xDA, then indexes
 * into D_80078E00 table at offset 0x4CFC. Calls func_800DEAA4 with
 * base[0x131A] and the table lookup result.
 */
INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A2FC8);

/**
 * @brief Call func_800DEA58 with D_800EE462 byte value.
 */
void func_800A302C(void) {
    func_800DEA58(*(u8 *)D_800EE462);
}

/**
 * @brief Add an entry to the command table at D_800ED148+0xEFC.
 *
 * Reads the current table index from D_800ED148[0x1302], computes the
 * entry pointer (stride 24), and stores the provided fields.
 *
 * @param a0 First byte field (offset 0).
 * @param a1 Second byte field (offset 1).
 * @param a2 Halfword field (offset 4).
 * @param a3 Third byte field (offset 2).
 * @param arg5 Halfword field (offset 6).
 */
void func_800A3054(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5) {
    u8 *base = (u8 *)&D_800ED148;
    s32 idx;
    u8 *entry;
    idx = base[0x1302];
    base += 0xEFC;
    entry = base + idx * 24;
    entry[0] = a0;
    entry[1] = a1;
    *(u16 *)(entry + 4) = a2;
    entry[2] = a3;
    entry[3] = 0;
    *(u16 *)(entry + 6) = arg5;
}

/**
* enemyId is the id of the enemy you are using draw on (3-5) 
*/

s32 func_800A3094(s32 targetId, s32 arg1) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800EE9E8.subEntries[targetId - 3].array0[i].unk0 == arg1) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief Clear two battle state bytes at D_800ED148 offsets 0x5C0 and 0x5C1.
 */
void func_800A30E4(void) {
    u8 *base = (u8 *)&D_800ED148;
    base[0x5C1] = 0;
    base[0x5C0] = 0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A30F8);

/**
 * @brief Initialize battle state: set flag, clear params, and store constants.
 *
 * @param a Byte pointer to clear.
 * @param b Byte pointer to set to 0xA.
 * @param c Halfword pointer to clear.
 * @param d Halfword pointer to set to 9.
 */
void func_800A42B4(u8 *a, u8 *b, u16 *c, u16 *d) {
    *(u8 *)D_800EE43C = 1;
    *a = 0;
    *d = 9;
    *b = 0xA;
    *c = 0;
}

/**
 * @brief Initialize battle state and call func_800A432C.
 *
 * Rearranges parameters: a1-a3+stack become a0-a3 for func_800A42B4,
 * then original a0 is passed to func_800A432C.
 *
 * @param a0 Argument for func_800A432C.
 * @param a1 Byte pointer (becomes a0 for func_800A42B4).
 * @param a2 Byte pointer (becomes a1 for func_800A42B4).
 * @param a3 Halfword pointer (becomes a2 for func_800A42B4).
 * @param stack0 Halfword pointer (becomes a3 for func_800A42B4).
 */
void func_800A42DC(s32 a0, u8 *a1, u8 *a2, u16 *a3, u16 *stack0) {
    func_800A42B4(a1, a2, a3, stack0);
    func_800A432C(a0);
}

/**
 * @brief Store a value to the global D_800EE424.
 *
 * @param value Value to store.
 */
void func_800A4320(s32 value) {
    *(s32 *)D_800EE424 = value;
}

/**
 * @brief Call getMenuString and store the result to D_800EE424.
 *
 * @param a0 Argument passed to getMenuString.
 */
void func_800A432C(s32 a0) {
    *(s32 *)D_800EE424 = getMenuString(a0);
}

void func_800A4350(s32 arg0, s32 arg1) {
    BattleCharData* var_v1;
    s32 i;

    var_v1 = &g_battleChars.chars[arg0];
    if (var_v1->currentHp == 0) {
        for(i = 0; i < 16; i++){
            if (var_v1->itemSlots[i].field0 == arg1) {
                var_v1->itemSlots[i].field4 |= 2;
                return;
            }
        }
    }
}

/**
 * @brief Check entity status bit and trigger effect if clear.
 *
 * Checks bit 0 of the halfword at entity[0x90] (stride 0xD0 from
 * D_800ED148). If the bit is clear, calls func_800AF8A4 and
 * func_8009B134(0x72, 0xF0, 0), then stores a0 as a halfword at
 * the returned address.
 *
 * @param a0 Entity index (stride 0xD0).
 */


void func_800A43C0(s32 arg0) {
    if (!(D_800ED148.entities[arg0].status & 1)) {
        func_800AF8A4(arg0);
        func_8009B134(114, 240, 0)->unk0 = arg0;
    }
}

void func_800A4434(u32 arg0, s32 unused, TaskEntry* arg2, BattleCharData* arg3) {
    func_800A4350(arg0, g_battleChars.chars[arg0].field01D);
    D_800ED148.entities[arg0].flags &= 0x7FFFFFFF; // ~(1 << BATTLE_ENTITY_FLAG_BIT_31)
    arg3->field01C &= 0xFE;
    D_800ED148.entities[arg0].controlFlags &= ~CTRL_FLAG_400;
    recalcAllGfStats();
    arg2->done = 1;
}

void func_800A44FC(s32 arg0) {
    TaskEntry* temp_s0;
    BattleCharData* temp_s1;

    temp_s0 = &D_800ED148.taskData[arg0];
    temp_s1 = &g_battleChars.chars[temp_s0->unkC];
    g_gameState.gfs[temp_s1->field01D - 64].hp = temp_s1->currentHp;

    if ((D_800ED148.entities[temp_s0->unkC].controlFlags & 0x400)) {
        return;
    }
    
    if (D_800ED148.entities[temp_s0->unkC].flags < 0) {
        if (temp_s1->field014 != 0) {
            return;
        }
        
        func_800A6184(temp_s0->unkC, temp_s0->unkD, temp_s0->unkE, func_8009BB3C(temp_s0->unkE));
    }
    
    else {
        func_800A43C0(temp_s0->unkC);
    }
    
    func_800A4434(temp_s0->unkC, temp_s1->field01D, temp_s0, temp_s1);
}

void func_800A4618(s16 arg0, s32 arg1, u32 arg2, s32 arg3, s32 arg4) {
    BattleCharData* temp_s0;
    TaskEntry* temp_v0;
    BattleEntity* temp_v1;
    s32 temp_v0_2;
    
    temp_s0 = &g_battleChars.chars[arg1];
    temp_v0 = &D_800ED148.taskData[func_8009B3D0(func_800A44FC)];
    temp_v1 = &D_800ED148.entities[arg1];
    
    temp_v0->unkC = arg1;
    temp_v0->unkD = arg2;
    temp_v0->unkE = arg3;
    temp_v0->unkA = arg4;
    temp_s0->field01D = arg3;
    temp_s0->field016 = arg0 * 4;
    temp_s0->field014 = arg0 * 4;
    
    temp_v0_2 = g_gameState.gfs[arg3 - 64].hp;
    temp_s0->currentHp = temp_v0_2;
    temp_v1->hpDisplay = temp_v0_2;
    temp_s0->unk01A = g_battleChars.gfEntries[arg3 - 64].hp; 
    temp_v1->flags |= 0x80000000;
    temp_s0->field01C |= 1;
}

/**
 * @brief Call one of two processing functions based on entity count.
 *
 * If the input (masked to 16 bits) is less than 8, calls func_800A980C;
 * otherwise calls func_800A9888. Returns the lower 16 bits of the result.
 *
 * @param count Entity count (u16).
 * @return Result masked to 16 bits.
 */
u16 func_800A475C(u16 count) {
    s32 result;
    if (count < 8) {
        result = func_800A980C();
    } else {
        result = func_800A9888();
    }
    return (u16)result;
}

s32 func_800A4798(u32 arg0, s32 arg1) { // arg0 is always 0-6
    s32 i;
    s32 count = 0;
    u16 mask = 1 << arg0;
    
    for (i = 0; i < arg1; i++) {
        if (mask & D_800ED148.array12B8[i]) {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Call func_800A4798 for each of 7 entities, storing result at offset 0xD9.
 *
 * Iterates over 7 entities (stride 0xD0) in D_800ED148, calling func_800A4798
 * with the entity index and a0, storing the result byte at offset 0xD9.
 *
 * @param a0 Parameter passed through to func_800A4798.
 */
void func_800A47E4(s32 a0) {
    s32 i = 0;
    u8 *base = (u8 *)&D_800ED148;
    do {
        base[0xD9] = func_800A4798(i, a0);
        i++;
        base += 0xD0;
    } while (i < 7);
}

s32 func_800A4844(s32 arg0) {
    s32 status = D_800ED148.entities[arg0].status;
    s32 flags = D_800ED148.entities[arg0].flags;
    
    if((status & 0x25) || (flags & 0x02004009)) {
        return 0;
    }
    
    return 1;
}

s32 func_800A4898(s32 arg0) {
    if (D_800ED148.entities[arg0].linkedIdx != 255) {
        if ((g_battleChars.chars[arg0].statusFlags & 0x10) && (func_800A4844(arg0) != 0)) {
            return arg0;
        }

        return 255;
    }
    
    return 254;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A493C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A4A74);

/**
 * @brief Unpack a 16-bit value into entity fields.
 *
 * Stores the lower 13 bits as a halfword at D_800ED148.unk12E0,
 * and the upper 3 bits (shifted right 13) as a byte at D_800ED148.unk130F.
 *
 * @param arg0 Packed 16-bit value.
 */
void func_800A4B68(s32 arg0) {
    s16 tmp = arg0;
    s32 tmp2 = arg0 & 0xE000;
    D_800ED148.unk12E0 = (tmp &= 0x1FFF);
    tmp = ((u32)tmp2) >> 0xD;
    D_800ED148.unk130F = (s8)tmp;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A4B88);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A4C84);

/**
 * @brief Append a value to the battle command queue and set the current command.
 *
 * Increments the queue index at D_800ED148[0x5C0], stores the value at the
 * computed queue slot (stride 20), and also stores it at D_800ED148[0x1305].
 *
 * @param a0 Command value to store.
 */
void func_800A4DD4(s32 a0) {
    u8 *base = (u8 *)&D_800ED148;
    u8 idx = base[0x5C0];
    base[0x5C0] = idx + 1;
    *(u8 *)(base + idx * 20 + 0x5D4) = a0;
    base[0x1305] = a0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A4E08);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A4EA0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A4F28);

/**
 * @brief Extract set bit positions from a 16-bit mask into a buffer.
 *
 * Iterates over bits 0-15. For each set bit, stores the bit position
 * to *dst and increments dst. Returns the count of set bits (masked to 8 bits).
 *
 * @param mask 16-bit bitmask to scan.
 * @param dst Destination buffer for bit positions.
 * @return Number of set bits found (0-16), masked to u8.
 */
s32 func_800A4FC4(s32 mask, u8 *dst) {
    s32 count = 0;
    s32 bit = 1;
    s32 pos = 0;
    do {
        if ((mask & bit) & 0xFFFF) {
            *dst++ = pos;
            count++;
        }
        pos++;
        bit <<= 1;
    } while (pos < 16);
    return count & 0xFF;
}

s32 func_800A5004(void) {
    if ((D_800ED148.unk12F2 == 0) || (D_800ED148.unk12F0 == 0)) {
        return 1;
    }
    
    if(D_800ED148.unk130C != 0) {
        return 1;
    }
    
    if (D_800EE4C0.unk1 == 16) {
        if (D_800ED148.entities[D_800EE4C0.unk0].status & 1) {
            goto x;
        }

        return 1;
    }

    
    if (D_800ED148.unk12F0 == 1) {
        if (!(D_800ED148.entities[D_800EE4C0.unk0].flags & 0x20000) && !(D_800ED148.entities[D_800EE4C0.unk0].flags & 0x40000)) {
            return 0;
        }
    }
    
    if ((D_800ED148.unk12F0 == 2) && !((D_800ED148.entities[D_800EE4C0.unk0].flags & 0x40000))) {
      return 0;
    }

    if ((D_800ED148.entities[D_800EE4C0.unk0].status & 0x35)) {
        return 0;    
    }
    
    if ((D_800ED148.entities[D_800EE4C0.unk0].flags & 0x4009)) {
        x:
        return 0;
    }
    
    return 1;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A517C);

void func_800A5210(s32 arg0) {
    Struct_func_800A5210* temp_v1;

    temp_v1 = &D_800ED148.Array844[D_800ED148.unk5C1++];
    temp_v1->unk0 = arg0;
    temp_v1->unk1 = D_800EE4C0.unk4;
    temp_v1->unk2 = D_800EE4C0.flags5;
    temp_v1->unk3 = D_800EE4C0.flags6;
    temp_v1->unk4 = D_800EE4C0.unk1E;
    temp_v1->unk6 = D_800EE4C0.unk0C;
    temp_v1->unk8 = D_800EE4C0.unk10;
    temp_v1->unkC = D_800EE4C0.unk7;
    temp_v1->unkD = D_800EE4C0.unk08;
    temp_v1->unkE = D_800EE4C0.unk09;
    temp_v1->unkF = D_800EE4C0.unkA;
    temp_v1->unk10 = D_800EE4C0.unk20;
    temp_v1->unk12 = D_800EE4C0.unk14;
    temp_v1->unk14 = D_800EE4C0.unk18;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A52E4);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A53C4);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5454);

/**
 * @brief Store a timer value into an entity entry at D_800ED158.
 *
 * Computes (D_80077E58[0] + 1) * 4000, stores it at entry offset 0x10,
 * and clears offset 0x14. The entry is at D_800ED158 + a0 * 208.
 *
 * @param a0 Entity index (stride 208).
 */
INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A554C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A559C);

/**
 * @brief Clear the word at offset 0x24 of a battle entity.
 *
 * @param idx Entity index (stride 0xD0).
 */
void func_800A565C(s32 idx) {
    u8 *base = (u8 *)&D_800ED148;
    u8 *entity;
    asm("");
    entity = base + idx * 0xD0;
    *(volatile s32 *)(entity + 0x24) = 0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5688);

/**
 * @brief Copy entity timer data to display structure for party members.
 *
 * For party members (index < 3), copies two words at offsets +0x10 and +0x14
 * from D_800ED158 (entity data, stride 0xD0) to g_battleChars (display data,
 * stride 0x1D0) at offsets +0x180 and +0x184.
 *
 * @param idx Entity index.
 */
void func_800A5778(s32 idx) {
    u8 *src = (u8 *)&D_800ED158 + idx * 0xD0;
    u8 *dst = (u8 *)&g_battleChars + idx * 0x1D0;

    if (idx < 3) {
        *(s32 *)(dst + 0x184) = *(s32 *)(src + 0x14);
        *(s32 *)(dst + 0x180) = *(s32 *)(src + 0x10);
    }
}

/**
 * @brief Set up entity battle data and process action.
 *
 * Reads the entity index from D_800ED148[0x12F2], computes the entity
 * data pointer (stride 44 at offset 0xD64) and status pointer (offset 0x1100),
 * calls func_8009B320 to initialize them, then func_800A5948 to process.
 *
 * @param a0 Battle action parameter.
 */
void func_800A57E0(s32 a0) {
    u8 *base = (u8 *)&D_800ED148;
    u8 *entityBase = base + 0xD64;
    s32 idx = base[0x12F2];
    base += 0x1100;
    func_8009B320(a0, entityBase + idx * 44, base + idx);
    func_800A5948(a0, idx);
}

/**
 * @brief Count how many entries in two tables match the given value.
 *
 * @param a0 Value to match.
 * @return Total number of matching entries across both tables.
 */
s32 func_800A584C(s32 a0) {
    s32 count = 0;
    s32 i = 0;
    s32 ptr = (s32)&D_800ED148;
top:
    if (*(u8 *)(ptr + 0xFF8) == a0) {
        count++;
    }
    if (*(u8 *)(ptr + 0xEF0) == a0) {
        count++;
    }
    i++;
    ptr += 0x18;
    if (i < 11) goto top;
    return count;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A589C);

void func_800A5948(s32 arg0, s32 arg1) {
    s32 i;
    s32 j;

    for (i = 0; i < 2; i++) {
        D_800ED148.arrayDE8[arg1][arg0][i].unk0 = 255;
        
        for (j = 2; j > -1; j--) {
            D_800ED148.arrayDE8[arg1][arg0][i].unk6[j] = 0;
        }
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A59AC);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5A7C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5AF4);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5BC4);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5C48);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object3", func_800A5F24);
