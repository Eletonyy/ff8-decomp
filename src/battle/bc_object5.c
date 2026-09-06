#include "common.h"
#include "battle.h"
#include "gamestate.h"
#include "battle/bc_object5.h"
#include "battle/bc_object6.h"
#include "battle/bc_object7.h"

extern u8 D_800EEBE0[]; // an array containing 1s and 0s (size 7)
extern u8 D_800E3CEC[];
s32 func_800B0398(s32);
void func_800A59AC(s32, s32, s32);
s32 func_800A97A4(s32);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A8B7C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A8CA4);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A8D7C);

void func_800A8E90(BattleCharData* arg0, s32 arg1) {
    arg0->testSlots[0].unk0 = arg1;
    arg0->testSlots[0].unk2 = D_80078E00.array4484[arg1].unk5;
    arg0->testSlots[0].unk3 = D_80078E00.array4484[arg1].unk6;
    arg0->testSlots[0].unk4 = 0;
    arg0->testSlots[0].unk1 = 1;
    
    if (D_80078E00.array4484[arg1].unk7 & 0x80) {
        arg0->testSlots[0].unk4 |= 1;
    }
}

s32 func_800A8EFC(BattleCharData* arg0) {
    s32 bit;
    s32 i;
    s32 val;

    for (val = 0, bit = 1, i = 0; i < 16; bit *= 2, i++) {
        if (g_gameState.mainData.limitBreaks.quistisLimits & bit) {
            arg0->testSlots[val].unk0 = i;
            arg0->testSlots[val].unk2 = D_80078E00.array44FC[i].unk4;
            arg0->testSlots[val].unk3 = D_80078E00.array44FC[i].unk5;
            arg0->testSlots[val].unk4 = 0;
            arg0->testSlots[val].unk1 = 1;
            arg0->testSlots[val].unk4 &= 0xEF;
            
            if (D_80078E00.array44FC[i].unk6 & 0x80) {
                arg0->testSlots[val].unk4 |= 1;
            }
            
            val++;
        }
    }
    
    return val;
}

void func_800A8F98(BattleCharData* arg0, s32 arg1, s32 arg2) {
    arg0->testSlots[arg2].unk0 = arg1;
    arg0->testSlots[arg2].unk2 = D_80078E00.rows8[arg1].unk2;
    arg0->testSlots[arg2].unk3 = D_80078E00.rows8[arg1].unk3;
    arg0->testSlots[arg2].unk4 = 0;
    arg0->testSlots[arg2].unk1 = 1;
}

s32 func_800A8FDC(BattleCharData* arg0) {
    s32 current;
    s32 val = 0;

    if (!(g_gameState.mainData.partyLockFlag & 0x10)) {
        val++;
        func_800A8F98(arg0, 0, 0);
    }
    
    current = val;
    if (g_gameState.mainData.partyLockFlag & 0x20) {
        val++;
        func_800A8F98(arg0, 1, current);
    }
    
    return val;
}

s32 func_800A9064(BattleCharData* arg0) {
    arg0->testSlots[0].unk0 = 65;
    arg0->testSlots[1].unk0 = 67;
    arg0->testSlots[2].unk0 = 66;
    
    return 3;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A9084);

u8 func_800A9240(s32 arg0) {
    s32 i;
    
    for (i = 0; i < 198; i++) {
        if (g_gameState.mainData.itemSlots[i].id == arg0) {
            return g_gameState.mainData.itemSlots[i].count;
        }
    }
    
    return 0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A9284);

s32 func_800A9370(s32 arg0) {
    BattleCharData* temp_a0;
    s32 i;

    temp_a0 = &g_battleChars.chars[arg0];
    
    for (i = 0; i < 16; i++) {
        temp_a0->testSlots[i].unk4 = 0;
        temp_a0->testSlots[i].unk1 = 0;
        temp_a0->testSlots[i].unk3 = 0;
        temp_a0->testSlots[i].unk2 = 0;
        temp_a0->testSlots[i].unk0 = 0;
        temp_a0->testSlots[i].unk4 |= 0x10;
    }
    
    switch (temp_a0->characterId) {
        case 0:
        case 1:
            return 0;
            
        case 2:
            return func_800A9284(temp_a0);
            
        case 3:
            return func_800A8EFC(temp_a0);
            
        case 4:
            return func_800A8FDC(temp_a0);
            
        case 5:
            return func_800A9064(temp_a0);
            
        case 6:
            func_800A8E90(temp_a0, 0);
            return 1;
            
        case 7:         
            func_800A8E90(temp_a0, 1);
            return 1;
            
        case 8:         
            func_800A8E90(temp_a0, 2);
            return 1;
            
        case 9:   
            func_800A8E90(temp_a0, 3);
            return 1;
            
        case 10:
            func_800A8E90(temp_a0, 4);
            return 1;
    }
}


void func_800A9490(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        g_battleChars.unk57A[i] = 0;
        g_battleChars.unk574[i] = 0;
    }

    for (i = 0; i < 16; i++) {
        g_battleChars.unk5A0[i] = 0;
        g_battleChars.unk580[i] = 0;
    }
}

void func_800A94E0(void) {
    s32 i;

    func_800A9490();
    
    for (i = 0; i < 16; i++) {
        g_battleChars.unk5C0[i] = 0;
    }

    for (i = 0; i < 24; i++) {
        g_battleChars.unk5E0[i].unk1 = 0;
        g_battleChars.unk5E0[i].unk0 = 0;
    }



    for (i = 0; i < 8; i++) {
        g_battleChars.gfEntries[0].unk0[i] = 255;
    }
}

s32 func_800A9568(s32 arg0) {
    s32 i;

    for (i = 0; i < 32; i++) {
        if (D_800EE9E8.animSlots[i].id == arg0) {
            return 1; 
        }
    }
    
    return 0;
}

void func_800A95A0(s32 arg0, s32 arg1) {
    func_800B0754(arg0, 4, arg1, func_800A97FC(arg0));
    func_800AF4BC(arg1, 1);
    
    if (func_800AE390(arg1) == 0) {
        func_800AE3D4(arg1);
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A960C);

s32 func_800A972C(s32 arg0) {
    return func_800B0F9C(D_80078E00.entriesA0[arg0].unk7) | func_800B0F7C(D_80078E00.entriesA0[arg0].unk7);
}

u8* func_800A9784(u16 arg0, s32 arg1) {
    if (arg0 == 65535) {
        return D_800E3CEC;
    }
    
    return arg0 + arg1; // weird
}

s32 func_800A97A4(s32 arg0) {
    return func_8009B15C() % arg0;
}

void func_800A97D4(void) {
    s32 i;
    
    for (i = 0; i < 8; i++) {
        D_800ED148.unk1298[i] = 0;
    }
}

u16 func_800A97FC(s32 arg0) {
    return (1 << arg0);
}

u16 func_800A980C(void) {
    s32 idx;

    if (func_800AE730() == 255) {
        return 0;
    }

    do {
        idx = func_800A97A4(3);
    } while (D_800ED148.entities[idx].status & 1);
    
    return (1 << idx);
}

u16 func_800A9888(void) {
    s32 idx;

    if (func_800AE788() == 255) {
        return 8;
    }
    
    do {
        idx = func_800A97A4(4) + 3;
    } while (D_800ED148.entities[idx].status & 1);
    
    return (1 << idx);
}

u16 func_800A9904(s32 arg0) {
    return (1 << D_800ED148.entities[arg0].unk98);
}

void func_800A9938(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        D_800EEBE0[i] = ~D_800EEBE0[i] & 1;
    }
}

void func_800A9970(s32 arg0) {
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800ED148.entities[i].controlFlags & 0x100) {
            D_800EEBE0[i] = 1;
        } 
        
        else {
            D_800EEBE0[i] = 0;
        }
    }
    
    if (arg0 == 200) {
        func_800A9938();
    }
}

void func_800A99E8(s32 arg0) {
    s32 i;

    for (i = 0; i < 7; i++) {
        if (arg0 < 16) {
            if ((D_800ED148.entities[i].status >> arg0) & 1) {
                D_800EEBE0[i] = 1;
            } 
            
            else {
                D_800EEBE0[i] = 0;
            }
        }
            
        else {
            s32 result = 1 << (arg0 - 16);
            if (D_800ED148.entities[i].flags & result) {
                D_800EEBE0[i] = 1;
            } 
            
            else {
                D_800EEBE0[i] = 0;
            }
        }
    }
}

void func_800A9A6C(s32 arg0) {
    switch (arg0) {
        case 0xC8:
            D_800EEBD8 = 0;
            D_800EEBDC = 3;
            break;
        
        case 0xC9:
            D_800EEBD8 = 3;
            D_800EEBDC = 7;
            break;
        
        default:
            D_800EEBD8 = 0;
            D_800EEBDC = 7;
            break;
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A9AC0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A9C68);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800A9E08);

void func_800A9F98(void) {
    s32 i;
    
    for (i = 0; i < 7; i++) {
        if (!(D_800ED148.entities[i].controlFlags & 1)) {
            D_800EEBE0[i] = 0;
        }
    }
}

void func_800A9FDC(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        if (!(D_800ED148.entities[i].controlFlags & 1) || (D_800ED148.entities[i].status & 1)) {
            D_800EEBE0[i] = 0;
        }
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AA034);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AA368);

u16 func_800AA44C(s32 arg0) {
    s32 bit;

    if (func_800AA368(201) == 1) {
        bit = arg0;
    }
    
    else {
        do {
            bit = func_800A97A4(4) + 3;
        } while (bit == arg0 || (D_800ED148.entities[bit].status & 1));
    }
    
    return (1 << bit);
}

s32 func_800AA4E0(void) {
    return 32775;
}

s32 func_800AA4E8(void) {
    return 33016;
}

s32 func_800AA4F0(void) {
    return 33023;
}

s32 func_800AA4F8(s32 arg0) {
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800ED148.entities[i].linkedIdx == arg0) {
            return i;
        }
    }

    return 255;    
}

s32 func_800AA530(s32 arg0) {
    s32 i;

    for (i = 0; i < 7; i++) {
        if (!(D_800ED148.entities[i].status & 1) && ((D_800ED148.entities[i].linkedIdx == arg0))) {
            return i;
        }
    }
    
    return 255;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AA57C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AA68C);

s32 func_800AA71C(s32 arg0, s32 arg1) {
    if (arg0 == 0) {
        return func_800AA4F8(arg1) != 255;
    }
    
    if (arg0 == 3) {
        return func_800AA4F8(arg1) == 255;
    }
}

s32 func_800AA768(s32 arg0, s32 arg1) {
    if (arg0 == 0) {
        return func_800AA530(arg1) != 255;
    }

    if (arg0 == 3) {
        return func_800AA530(arg1) == 255;
    }
}

s32 func_800AA7B4(s32 unused, s32 arg1) {
    s32 i;
    
    for (i = 0; i < 3; i++) {
        if ((D_800ED148.entities[i].controlFlags & 1) && !(D_800ED148.entities[i].status & 1)) {
            if (arg1 == 202) {
                if (!(D_800ED148.entities[i].controlFlags & 0x100)) {
                    return i;
                }
            }
                
            else {
                if ((D_800ED148.entities[i].controlFlags & 0x100)) {
                     return i;
                }
            }
        }
    }
    
    return 255;
}

s32 func_800AA840(s32 arg0, s32 arg1) {
    if (arg0 == 0) {
        return func_800AA7B4(arg1, arg1) != 255;
    }

    if (arg0 == 3) {
        return func_800AA7B4(arg1, arg1) == 255;
    }
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AA88C);

s32 func_800AA930(s32 arg0) {
    drawSlot* var_v1;
    s32 i;

    
    var_v1 = D_800EE9E8.subEntries[arg0 - 3].array0;
    for (i = 0; i < 4; i++) {
        if (var_v1[i].unk0 > 63) {
           return 1;
        }
    }
    
    return 0;
}

s32 func_800AA980(s32 arg0, s32 arg1) {
    if (arg0 == 0) {
        return func_800AA930(arg1);
    }

    if (arg0 == 3) {
        return (~func_800AA930(arg1)) & 1;
    }
}

s32 func_800AA9C8(s32 arg0, s32 arg1) {
    if (arg0 == 0) {
        return (D_8007809A >> arg1) & 1;
    }

    if (arg0 == 3) {
        return ((D_8007809A >> arg1) & 1) ^ 1;
    }
}

s32 func_800AAA10(s32 arg0) {
    if (arg0 == 0) {
        return g_gameState.mainData.countdownTimer == 0;
    }

    if (arg0 == 3) {
        return g_gameState.mainData.countdownTimer != 0;
    }
}

s32 func_800AAA50(s32 arg0, s32 arg1, s32 bit) {
    if (arg1 == 0) {
        s32 mask = (1 << bit);
        if (arg0 & mask) {
            return 1;
        }
    }

    if (arg1 == 3) {
        s32 mask = (1 << bit);
        if (!(arg0 & mask)) {
            return 1;
        }
    }

    return 0;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AAA9C);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AAB50);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AABEC);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AACD0);

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AAD5C);

s32 func_800AAE10(s32 arg0, s32 arg1, s32 arg2) {
    if (D_800ED148.entities[arg2].controlFlags & 1) {
        if ((D_800ED148.entities[arg2].status & 1)) {
            return arg1;  
        }
        
         arg1 += func_800AAA9C(0, arg0, &D_800ED148.entities[arg2].entityData);
    }
    

    return arg1;
}

s32 func_800AAE98(s32 arg0) {
    s32 check1;
    s32 check2;
    s32 i;
    

    check1 = 0;
    check2 = 0;

    for (i = 0; i < 3; i++) {
        if ((D_800ED148.entities[i].controlFlags & 1) && !(D_800ED148.entities[i].status & 1)) {
            check2++;
        }
    }
    
    for (i = 0; i < 3; i++) {
        check1 = func_800AAE10(arg0, check1, i);

    }
   
    return check1 == check2;
}

void func_800AAF48(s32 arg0) {
    BattleEntityData* temp_v0;

    temp_v0 = func_800AA57C(200, arg0);
    temp_v0->unk18 = temp_v0->unk1C;
}

void func_800AAF70(s32 arg0, s16 arg1) {
    BattleEntityData* temp_v0;

    temp_v0 = func_800AA57C(200, arg0);
    temp_v0->unk18 += arg1;
}

void func_800AAFB8(s32 arg0) {
    TaskEntry* data;

    data = &D_800ED148.taskData[arg0];
    func_8009AF3C(data->unk4, 30, 3, 128, 0);
    data->done = 1;
}

void func_800AB008(s32 arg0) {
    s32 idx;
    
    TaskEntry* task = &D_800ED148.taskData[func_8009B3D0(&func_800AAFB8)];
    task->unk4 = func_800B0398(arg0);
}

void func_800AB054(s32 arg0) {
    TaskEntry* td = &D_800ED148.taskData[arg0];
    
    if (td->timer == 0) {
        func_8009AF3C(td->unk4, 0x1E, 3, 0xF0, 0);
        td->done = 1;
    }
    
    td->timer--;
}

void func_800AB0C0(s32 arg0, u16 arg1) {
    TaskEntry* currentEntry;

    currentEntry = &D_800ED148.taskData[func_8009B3D0(func_800AB054)];
    currentEntry->unk4 = func_800B0398(arg0);
    currentEntry->timer = arg1;
}

void func_800AB11C(s16 arg0) {
    D_800ED148.entities[arg0].controlFlags &= ~0x40;
    D_800ED148.entities[arg0].controlFlags &= ~0x80;
    D_800ED148.entities[arg0].controlFlags &= ~2;
    
    func_800AE6C0();
    func_800A59AC(arg0, 0, 0);
}

void func_800AB1AC(s32 arg0) {
    TaskEntry* temp_a0;

    temp_a0 = &D_800ED148.taskData[arg0];
    if (temp_a0->timer == 0) {
        D_800ED148.unk5C2 = 1;
        D_800ED148.unk12FD = 0;
        temp_a0->done = 1;
        return;
    }
    

    D_800ED148.unk5C2 = 0;
    D_800ED148.unk12FD = 1;
    temp_a0->timer--;
}

void func_800AB208(u16 arg0) {
    TaskEntry* currentEntry = &D_800ED148.taskData[func_8009B3D0(func_800AB1AC)];
    
    currentEntry->timer = arg0;
}

s32 func_800AB24C(void) {
    s32 i;

    for(i = 3; i < 7; i++) {
        if (!(D_800ED148.entities[i].controlFlags & 1)) {
            return i;
        }
    }
        
    return 255;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object5", func_800AB28C);

void func_800AB3C4(void) {
    D_800ED148.unk5C2 = 1;
    D_800ED148.unk12F9 = 0;
    D_800ED148.unk12FD = 0;
}

void func_800AB3E0(void) {
    D_800ED148.unk5C2 = 0;
    D_800ED148.unk12F9 = 1;
    D_800ED148.unk12FD = 1;
}

/**
 * @brief Resolve animation data and display it with position and timing.
 *
 * Looks up animation data from the entity's sub-object table at offset 0x14,
 * resolves it through func_800A9784 and func_800B0398, then calls
 * func_8009AF3C to display with the given Y position, fixed params.
 *
 * @param a0 Entity index (stride 0xD0 in D_800ED148).
 * @param a1 Sub-animation index (multiplied by 2 for table lookup).
 * @param a2 Y position for display.
 */
void func_800AB3FC(s32 a0, s32 a1, s32 a2) {
    volatile u8 *base = (u8 *)&D_800ED148;
    u8 *entity = (u8 *)base + a0 * 0xD0;
    s32 sub = *(s32 *)(entity + 0x14);
    s32 tbl = *(s32 *)sub;
    s32 offTab = *(s32 *)(tbl + 8) + tbl;
    s32 dataOff = *(s32 *)(tbl + 0xC);
    s32 result;
    a1 = a1 * 2 + offTab;
    result = func_800A9784(*(u16 *)a1, dataOff + tbl);
    result = func_800B0398(result);
    func_8009AF3C(result, a2, 3, 0xF0, 0);
}

void func_800AB488(s32 arg0, s32 arg1) {
    func_800AB3FC(arg0, arg1, 30);
}
