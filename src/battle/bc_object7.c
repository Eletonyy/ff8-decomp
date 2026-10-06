#include "common.h"
#include "battle.h"
#include "gamestate.h"
#include "kernel.h"
#include "psxsdk/libetc.h"
#include "battle/bc_object7.h"
u8* getMenuString(s32);

extern u8 D_800EE490[];
extern u8 D_800EEBE8[];

void func_800AF254(void) {
    func_800AF740();
    
    switch (g_battleConfig.result) {
        case 2:
            g_gameState.mainData.fieldCE2++;
            g_vsyncRate = 5;
            break;
            
        case 4:
            g_gameState.mainData.fieldCDC++;
            if (D_800ED148.unkCDD & 0x10) {
                g_vsyncRate = 100;
            }
                
            else {
                g_vsyncRate = 5;
            }
            
            break;
            
        case 1:
        case 3:
            g_gameState.mainData.fieldCE0++;
            g_vsyncRate = 100;
            break;
            
        case 5:
            g_vsyncRate = 100;
            break;
    }
    
    sndCmdF1();
    g_renderMode = 0;
    VSync(2);
    DrawSync(0);
    func_800D0B24();
}

s32 func_800AF358(s32 arg0, s32 arg1, s32 arg2) {
    s32 i;

    if (arg1 == 0) {
        return 0;
    }

    if (arg2 == 0) {
        for (i = 0; i < 32; i++) {
            if (g_battleChars.chars[arg0].magicSlots[i].unk0 == arg1) {
                if (g_battleChars.chars[arg0].magicSlots[i].unk1 < 100) {
                    g_battleChars.chars[arg0].magicSlots[i].unk1++;
                    return 0;
                }
                
                return 1;
            }
        }
        
        for (i = 0; i < 32; i++) {
            if (g_battleChars.chars[arg0].magicSlots[i].unk0 == 0) {
                g_battleChars.chars[arg0].magicSlots[i].unk0 = arg1;
                g_battleChars.chars[arg0].magicSlots[i].unk1++;
                func_800229FC(arg0);
                return 0;
            }
        }
        
        return 1;
    }
    
    for (i = 0; i < 32; i++) {
        if (g_battleChars.chars[arg0].magicSlots[i].unk0 == arg1) {
            if (g_battleChars.chars[arg0].magicSlots[i].unk1 != 0) {
                g_battleChars.chars[arg0].magicSlots[i].unk1--;
                
                if (g_battleChars.chars[arg0].magicSlots[i].unk1 == 0) {
                    g_battleChars.chars[arg0].magicSlots[i].unk0 = 0;
                    func_800229FC(arg0);
                    return 255;
                }
                
                return 0;
            }
            
            return 255;
        }
    }
    
    return 255;
}

s32 func_800AF4BC(s32 arg0, s32 arg1) {
    s32 i;

    if (arg0 == 0) {
        return 0;
    }
    
    if (arg1 == 0) {
        for (i = 0; i < 32; i++) {
            if (D_800EE9E8.animSlots[i].id == arg0) {
                if (D_800EE9E8.animSlots[i].value < 100) {
                    D_800EE9E8.animSlots[i].value++;
                    return 0;
                }
                
                return 1;
            }
        }
      
  
        for (i = 0; i < 32; i++) {
            if (D_800EE9E8.animSlots[i].id == 0) {
                D_800EE9E8.animSlots[i].id = arg0;
                D_800EE9E8.animSlots[i].value++;
                func_800A8578();
                return 0;
            }
        }

        return 1;
    }

    
    else {
        for (i = 0; i < 32; i++) {
            if (D_800EE9E8.animSlots[i].id == arg0) {
                if (D_800EE9E8.animSlots[i].value != 0) {
                    D_800EE9E8.animSlots[i].value--;
            
                    if ((D_800EE9E8.animSlots[i].value) == 0) {
                        D_800EE9E8.animSlots[i].id = 0;
                    }
                    
                    return 0;
                }
                
                return 1;
            }
        }
    }
    
    return 1;
}

void func_800AF5E0(s32 arg0, s32 arg1, ItemSlot* arg2) {
    ItemSlot* slot;
    s32 i;
    
    if (arg0 == 0) {
        return;
    }

    slot = arg2;
    for (i = 0; i < 198; i++, slot++) {
        if (slot->id == arg0) {
            slot->count = arg1;
            return;
        }
    }

    slot = arg2;
    for (i = 0; i < 198; i++, slot++) {
        if (slot->id == 0) {
            slot->id = arg0;
            slot->count = arg1;
            return;
        }
    }    
}

/**
 * @brief Re-init the 32-slot anim init table at @c D_800EE9E8 by feeding
 *        each slot's @c (id, value) pair plus @c D_80077EBC into
 *        @c func_800AF5E0.
 */
void func_800AF654(void) {
    ItemSlot* item = g_gameState.mainData.itemSlots;
    s32 i;
    for (i = 0; i < 32; i++) {
        func_800AF5E0(D_800EE9E8.animSlots[i].id, D_800EE9E8.animSlots[i].value, item);
    }
}

/**
 * @brief Copy entity animation data to lookup table and clear a flag.
 *
 * Computes entity pointer from D_800ED158 + a0*0xD0. Reads an index
 * byte from g_gameState + a0 + 0xAF4, multiplies by 0x98 to find a
 * table entry at g_gameState + 0x490. Copies entity halfword at 0x18
 * to the table entry. Clears bit 5 of entity halfword at 0x80 and
 * stores the result at table entry + 0x96.
 *
 * @param a0 Entity index (stride 0xD0).
 */
void func_800AF6BC(s32 arg0) {
    CharacterData* partyMember;
    BattleEntity* entity;

    entity = &D_800ED148.entities[arg0];
    partyMember = &g_gameState.chars[g_gameState.mainData.party.partyMembers[arg0]];
    
    partyMember->currentHp = entity->currentHp;
    partyMember->statusFlags = entity->status &= ~STATUS_BERSERK;
    func_800AE4A0(arg0);
}

/**
 * @brief For each of the 3 party slots, mirror the entity's display status
 *        into the matching @c BattleCharData and refresh its anim table entry.
 *
 * Walks @c D_800ED148.entities[0..2] (BattleSystem block) — for any slot whose
 * @c comFileId is not 0xFF, calls @c func_800AF6BC(i) (which copies the
 * entity's animation halfwords into the per-character anim cache) and then
 * mirrors @c entity->status into @c g_battleChars.chars[i].displayStatus.
 * Finishes by calling @c func_800AF654 to rebuild the global anim list.
 */
void func_800AF740(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800ED148.entities[i].comFileId != 255) {
            func_800AF6BC(i);
            g_battleChars.chars[i].displayStatus = D_800ED148.entities[i].status;
        }
    }
    
    func_800AF654();
}

void func_800AF7C4(void) {
    s32 i;
    s32 j;
    s32 k;
    CharacterData* temp_a3;
    BattleCharData* character;
    
    for (i = 0; i < 3; i++) {
        if (D_800ED148.entities[i].comFileId == 255) {
            continue;
        }
        
        temp_a3 = &g_gameState.chars[g_gameState.mainData.party.partyMembers[i]];
        character = &g_battleChars.chars[i];
        
        for (j = 0; j < 32; j++) {
            temp_a3->magic[j].magicId  = character->magicSlots[j].unk0;
            temp_a3->magic[j].quantity = character->magicSlots[j].unk1;
        }
        
        for (k = 0; k < 20; k++) {
            for (j = 0; j < 32; j++) {
                if (temp_a3->junctions[k] == temp_a3->magic[j].magicId) {
                    goto skip;
                }
            }
             
            temp_a3->junctions[k] = 0;
            skip:
        }
    }
}

/**
 * @brief Clear two flag bits in the entity's @c controlFlags and bracket
 *        the update with @c func_800A565C / @c func_800A5778 calls.
 *
 * @param a0 Entity index into @c D_800ED148.entities.
 */
void func_800AF8A4(s32 a0) {
    func_800A565C(a0);
    D_800ED148.entities[a0].controlFlags &= ~0x8;
    D_800ED148.entities[a0].controlFlags &= ~0x4;
    func_800A5778(a0);
}

s32 func_800AF918(s32 arg0, u8* arg1) {
    s32 bit;
    s32 i;
    s32 count;

    count = 0;
    bit = 1;

    for (i = 0; i < 16; i++) {
        if (g_gameState.chars[g_gameState.mainData.party.partyMembers[arg0]].junctedGfs & bit) {
            *arg1++ = i;
            count++; // counts how many elements of arg1 have a value inside
        }
        
        bit <<= 1;
    }
        
    return count;
}

/**
 * @brief Read the byte at offset 0x14F of the entity's linked data block.
 *
 * @param a0 Entity index into @c D_800ED148.entities.
 * @return Byte at @c (*entities[a0].linkedPtr)[0x14F].
 */
s32 func_800AF988(s32 a0) {
    return (*D_800ED148.entities[a0].entityData)->unk14F;
}

/**
 * @brief Clamp a 16-bit unsigned value to a maximum of 60000.
 *
 * @param a0 Input value (low 16 bits used).
 * @return min(a0 & 0xFFFF, 60000).
 */
u32 func_800AF9C4(u16 arg0) {
    
    if (arg0 > 60000) {
        return 60000;
    }
    
    return arg0;
}

u16 func_800AF9E8(s32 arg0, s32 arg1) {
    s32 result;
    s32 temp_v1;
    BattleEntity* entity; 
    
    temp_v1 = (*D_800ED148.entities[arg0].entityData)->unk100;
    entity = &D_800ED148.entities[arg0];
    result = (entity->level * 5 * temp_v1) / arg1 - temp_v1;
    
    if (temp_v1 == 0) {
        result = 0;
    } 
    
    else if (result < 1) {
        result = 1;
    }
    
    return result;
}

u16 func_800AFA64(s32 arg0) {
    BattleEntity* temp_s0;
    BattleEntityData* temp_s1;
    s32 result;

    temp_s1 = *D_800ED148.entities[arg0].entityData;
    temp_s0 = &D_800ED148.entities[arg0];
    
    if (temp_s1->unk102 == 0) {
        result = 0;
    } 
    
    else if (temp_s0->maxHp == temp_s0->currentHp) {
        result = 0;
    } 
    
    else {
        result = ((temp_s0->level * 5 * temp_s1->unk102) / func_800A6DD8() - temp_s1->unk102) * (temp_s0->maxHp - temp_s0->currentHp) / temp_s0->maxHp;

        if (temp_s1->unk102 == 0) {
            result = 0;
        } 
        
        else if (result < 1) {
            result = 1;
        }
    }
    
    return result;
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object7", func_800AFB5C);

void func_800AFD0C(void) {
    s32 i;
    s32 j;
    s32 idx;
    u16 tmp;
    u16 temp_s2;
    u8 sp10[16];
    
    for (i = 3; i < 7; i++) {
        if (!(D_800ED148.entities[i].status & 1)) {
            g_battleChars.unk574[0] = func_800AF9C4(g_battleChars.unk574[0] + func_800AF9C4(func_800AFA64(i)));
        }
    }
    
    tmp = g_battleChars.unk574[0];
    for (i = 0; i < 3; i++) {
        if ((D_800ED148.entities[i].status & 1) || (D_800ED148.entities[i].status & 4)) {
            g_battleChars.unk574[i] = 0;
            g_battleChars.unk57A[i] = 0;
        } 
        
        else {
            g_battleChars.unk574[i] = tmp;
        }
    }

    temp_s2 = g_battleChars.unk5C0[0];
    g_battleChars.unk5C0[0] = 0;
    for (i = 0; i < 3; i++) {
        if (!(D_800ED148.entities[i].controlFlags & 1) || ((D_800ED148.entities[i].status & 1) && (D_800ED148.entities[i].status & 4))) {
              continue;  
        }
        
        idx = func_800AF918(i, sp10);
        if (idx != 0) {
            u16 div = tmp / idx;
            for (j = 0; j < idx; j++) {
                if (g_gameState.gfs[sp10[j]].hp != 0) {
                    g_battleChars.unk580[sp10[j]] = div;
                    g_battleChars.unk5C0[sp10[j]] = temp_s2;
                } 
                
                else {
                    g_battleChars.unk5A0[sp10[j]] = 0;
                }
            }
        }
    }
    
    if (g_battleConfig.unk2 & 8) {
        func_800A9490();
    }
}

/**
 * @brief Return the name of non-junctionable GF attack @p a0.
 *
 * @param a0 Index into @c g_kernel.nonJunctionableGfAttacks.
 */
u8* func_800AFF30(s32 a0) {
  return resolveKernelPtr(g_kernel.nonJunctionableGfAttacks[a0].nameOffset, g_kernel.nonJunctionableGfAttacksText);
}

/**
 * @brief Return the name of junctionable GF @p a0 - 0x40.
 *
 * @param a0 Index into @c g_kernel.junctionableGfs, offset by @c 0x40.
 */
u8* func_800AFF70(s32 a0) {
    return resolveKernelPtr(g_kernel.junctionableGfs[a0 - 64].nameOffset, g_kernel.junctionableGfsText);
}

/**
 * @brief Return the name of enemy attack @p a0.
 *
 * @param a0 Index into @c g_kernel.enemyAttacks.
 */
u8* func_800AFFB4(s32 a0) {
    return resolveKernelPtr(g_kernel.enemyAttacks[a0].nameOffset, g_kernel.enemyAttacksText);
}

/**
 * @brief Call getMenuString with argument 0xA.
 */
void func_800AFFF4(void) {
    getMenuString(0xA);
}

/**
 * @brief Call getMenuString with argument 0xC.
 */
void func_800B0014(void) {
    getMenuString(0xC);
}

/**
 * @brief Call getMenuString with argument 0xD.
 */
void func_800B0034(void) {
    getMenuString(0xD);
}

/**
 * @brief Call getMenuString with argument 0xE.
 */
void func_800B0054(void) {
    getMenuString(0xE);
}

/**
 * @brief Return the first word of the data linked from a battle entity.
 *
 * @param idx Entity index into D_800ED148.entities.
 * @return First s32 word at @c entities[idx].linkedPtr.
 */
BattleEntityData* func_800B0074(s32 idx) {
    return *D_800ED148.entities[idx].entityData;
}

/**
 * @brief Call getMenuString with argument 0xF.
 */
void func_800B00A8(void) {
    getMenuString(0xF);
}

/**
 * @brief Call getMenuString with argument 0x10.
 */
void func_800B00C8(void) {
    getMenuString(0x10);
}

// returns 1-A as a string value
INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object7", func_800B00E8);
/*
u8 func_800B00E8(s32 arg0) {
    switch (arg0) {
        case 0:
            return getMenuString(0xB)[1];
        case 1:
            return getMenuString(0xB)[2];
        case 2:
            return getMenuString(0xB)[3];
        case 3:
            return getMenuString(0xB)[4];
        case 4:
            return getMenuString(0xB)[5];
        case 5:
            return getMenuString(0xB)[6];
        case 6:
            return getMenuString(0xB)[7];
        case 7:
            return getMenuString(0xB)[8];
        case 8:
            return getMenuString(0xB)[9];
        case 9:
            return getMenuString(0xB)[10];
    }
}
*/


/**
 * @brief Copy a null-terminated string from src to dst.
 *
 * @param dst Destination buffer.
 * @param src Source string.
 */
void func_800B01E8(u8 *dst, u8 *src) {
    u8 ch;
    do {
        ch = *src++;
        *dst++ = ch;
    } while (ch != 0);
}

/**
 * @brief Copy string with optional terminator replacement.
 *
 * Copies bytes from src to dst until a null byte is found, counting
 * the number of non-null bytes copied (added to initial len). After
 * copying, if the terminator byte (masked to 8 bits) equals 7, returns
 * the length. Otherwise, overwrites the null with the terminator byte
 * and returns length + 1.
 *
 * @param a0 Destination buffer.
 * @param a1 Source buffer (as integer).
 * @param a2 Initial length counter.
 * @param a3 Terminator byte (only low 8 bits used).
 * @return Final length of written data.
 */
s32 func_800B0204(u8* arg0, u8* arg1, s32 arg2, u8 arg3) {
    while (*arg0++ = *arg1++) {
        arg2++;
    }
    
    if (arg3 == 7) {
       return arg2;
    }
    
    *(arg0 - 1) = arg3;
    return arg2 + 1;
}

/**
 * @brief Build a string in D_800EEBE8 from two parts using func_800B0204.
 *
 * Writes the first part with a1 as length byte, then appends the
 * second part starting at the returned offset.
 *
 * @param a0 First part data.
 * @param a1 Length/type byte for first part (masked to 8 bits).
 * @param a2 Second part data.
 * @return Pointer to D_800EEBE8 buffer.
 */
u8* func_800B0248(u8* a0, u8 a1, u8* a2) {
    u8 *buf = D_800EEBE8;
    s32 offset = func_800B0204(buf, a0, 0, a1);
    func_800B0204(buf + offset, a2, offset, 0);
    return buf;
}
// split

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object7", func_800B02AC);

/**
 * @brief Copy a string to D_800EE490 and return the buffer pointer.
 *
 * @param src Source string to copy.
 * @return Pointer to D_800EE490.
 */
u8 *func_800B0328(u8 *src) {
    u8 *dst = D_800EE490;
    func_800B01E8(dst, src);
    return dst;
}

/**
 * @brief Return the description of Rinoa limit break (part 1) @p a0.
 *
 * @param a0 Index into @c g_kernel.rinoaLimitBreaks1.
 */
u8* func_800B0360(s32 a0) {
    return resolveKernelPtr(g_kernel.rinoaLimitBreaks1[a0].descOffset, g_kernel.rinoaLimitBreaks1Text);
}

INCLUDE_ASM("asm/ovl/battle/nonmatchings/bc_object7", func_800B0398);

u8 func_800B0414(u32 arg0, u8* arg1) {
    s32 i;
    u8* temp_a0;
    u8* temp_v1;
    

    for (i = 0; i < 5; i++) {
        temp_a0 = arg1 + (4 - i);
        *temp_a0 = arg0 % 10;
        arg0 /= 10;
    }
    
    for (i = 0; i < 4; i++) {
        temp_v1 = arg1 + i;
        if (*temp_v1 != 0) {
            return (5 - i);
        }
        
        *temp_v1 = 10;
    }
    
    return 1;
}

u8* func_800B04A0(u32 arg0, u8* arg1) {
    s32 i;
    u8 sp10[8];
    u8* str;

    str = arg1;
    for (i = 5 - func_800B0414(arg0, sp10); i < 5; i++) {
        if (sp10[i] != 10) { // if its A, skips
            *str++ = func_800B00E8(sp10[i]);
        } 
    }
    
    *str = 0;
    return arg1;
}

static s32 func_800B054C(u32 arg0) {
    s32 i;

    for (i = 0; i < 32; i++) {
        if (arg0 == 1) {
            return i;
        }

        arg0 >>= 1;
    }
    
    return i;
}

/**
 * @brief Store scaled animation value at entity's bit position offset.
 *
 * Calls func_800B054C to find the lowest set bit in a1. If the result
 * is less than 14, computes a scale factor from @c GameConfig.battleSpeed and the
 * status's kernel timer, multiplies them, and stores the result at the
 * entity's bit-indexed halfword slot.
 *
 * @param a0 Entity index (stride 0xD0).
 * @param a1 Bitmask to find lowest set bit.
 */
void func_800B0574(s32 arg0, u32 arg1) {
    s32 temp_v0;
    
    temp_v0 = func_800B054C(arg1);
    if (temp_v0 < 14) {
        u8 val = g_kernel.misc.statusTimers[temp_v0];
        s32 temp = ((g_gameState.config.battleSpeed + 1) * 4);
        D_800ED148.entities[arg0].perBit[temp_v0] = val * temp;
    }
}

/**
 * @brief Store the reset sentinel @c -0x457 in the entity's per-bit
 *        halfword slot indexed by the lowest set bit of @p a1.
 *
 * @param a0 Entity index into @c D_800ED148.entities.
 * @param a1 Bitmask whose lowest set bit selects the slot in @c timers.
 */
void func_800B0600(s32 a0, s32 a1) {
    s32 bitPos = func_800B054C(a1);
    if (bitPos < 14) {
        D_800ED148.entities[a0].perBit[bitPos] = -0x457;
    }
}

/**
 * @brief Test whether the entity's per-bit halfword slot (selected by the
 *        lowest set bit of @p a1) currently holds the reset sentinel.
 *
 * @param a0 Entity index into @c D_800ED148.entities.
 * @param a1 Bitmask whose lowest set bit selects the slot in @c timers.
 * @return 1 if @c timers[bitPos] == -0x457, 0 otherwise.
 */
s32 func_800B0668(s32 a0, s32 a1) {
    s32 bitPos = func_800B054C(a1);
    if (bitPos < 14) {
        if (D_800ED148.entities[a0].perBit[bitPos] == -0x457) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Process entity ability and trigger state transitions.
 *
 * Masks @p arg0 to 16 bits and calls @c func_800A4C84. If @c sys->unkE
 * is zero, transitions to state 5, calls @c func_800AE524 with the
 * preceding entry index (@c sys->unk5C0 - 1), clears that entry's
 * @c unk10 byte, then transitions to state 6.
 *
 * @param arg0 Entity bitmask (16-bit).
 */
void func_800B06DC(u16 arg0) {
    func_800A4C84(arg0);
    if (D_800ED148.header.unkE == 0) {
        func_8009AE08(5);
        func_800AE524(D_800ED148.unk5C0 - 1);
        D_800ED148.entries[D_800ED148.unk5C0 - 1].unk11 = 0;
        func_8009AE08(6);
    }
}

/**
 * @brief Set up extended parameters and call two processing functions.
 *
 * Saves the 16-bit truncation of a3, calls func_800A30F8 with 7 args
 * (a0, a1, a2 passed through, a3 zeroed, plus a0, truncated a3, and 0
 * on the stack), then calls func_800B06DC with the truncated a3 value.
 *
 * @param a0 First parameter (also passed as 5th arg).
 * @param a1 Second parameter passed through.
 * @param a2 Third parameter passed through.
 * @param a3 Fourth parameter (16-bit truncated, passed as 6th arg).
 */
void func_800B0754(s32 a0, s32 a1, s32 a2, u16 a3) {
    func_800A30F8(a0, a1, a2, 0, a0, a3, 0);
    func_800B06DC(a3);
}

/**
 * @brief Handle special battle action flags for an entity.
 *
 * If @p a1 has bit @c 0x400 set, calls @c func_800A59AC with mode 5 and
 * returns 1. If @p a1 has bit @c 0x1000 set, sets bit 2 in the entity's
 * @c status field and calls @c func_800A2520. Otherwise returns 0.
 *
 * @param a0 Entity index into @c D_800ED148.entities.
 * @param a1 Action flags bitmask.
 * @return 1 if bit @c 0x400 action taken, 0 otherwise.
 */
s32 func_800B0794(s32 a0, s32 a1) {
    if (a1 & 0x400) {
        func_800A59AC(a0, 5, 0);
        return 1;
    }

    if (a1 & 0x1000) {
        D_800ED148.entities[a0].status |= 4;
        func_800A2520(a0);
    }

    return 0;
}

void func_800B0808(s32 arg0, u8* arg1) {
    s32 temp_s0;
    s32 temp_s1;

    temp_s0 = func_800B0248(arg0, 7U, getMenuString(0x1B));
    temp_s1 = func_800B0248(temp_s0, *getMenuString(0xB), arg1);

    func_800A4320(func_800B02AC(func_800B0248(temp_s1, *getMenuString(0xB), getMenuString(0x74))));
    func_800AD960();
}

void func_800B08AC(s32 arg0, s32 arg1) {
    s32 var_s0;
    
    if (arg0 < 3) {
        var_s0 = getBattleCharName();
    } 
    
    else {
        var_s0 = func_800B0074(arg0);
    }

    if (arg1 & 0x40) {
        func_800B0808(var_s0, getMenuString(0x4F));
    }
    
    if (arg1 & 0x20) {
        func_800B0808(var_s0, getMenuString(0x4E));
    }
    
    if (arg1 & 0x80) {
        func_800B0808(var_s0, getMenuString(0x50));
    }
}

/**
 * @brief Call func_800A59AC with a1=6 and a2=0.
 *
 * @param a0 First argument passed through.
 */
void func_800B095C(s32 a0) {
    func_800A59AC(a0, 6, 0);
}

void func_800B0980(s32 arg0, s16 arg1, s32 arg2, s16 arg3) {
    if ((arg0 & 0x10) && (arg3 != 0)) {
        s16 div = arg1 / arg3;
        if ((div % (60 / arg3)) == 0) {
            func_800B095C(arg2);
        }
    }
}

void func_800B09F0(s32 arg0) {
    s32 i;
    s32 val;
    s32 flag;
    BattleEntity* entity;

    entity = &D_800ED148.entities[arg0];
    for (i = 0; i < 14; i++) {
        if (entity->perBit[i] == -0x457) {
            continue;
        }
        
        flag = 1;
        flag <<= i;
        if (entity->perBit[i] <= 0) {
            entity->perBit[i] = -0x457;
            if (func_800B0794(arg0, flag) != 0) {
                entity->flags &= ~flag;
                return;
            }
            
            func_800B08AC(arg0, flag);
            entity->flags &= ~flag;
            func_800A240C(arg0, D_800ED148.entities[arg0].currentHp, &D_800ED148.entities[arg0].status);
            func_8009AFF0(arg0);
            
            if (arg0 < 3) {
                func_800A1AB8(arg0, D_800ED148.entities[arg0].status, D_800ED148.entities[arg0].flags);
            } 
            
            else {
                func_800A1CFC(arg0);
            }
            
            func_8009AF98(arg0);
        }
            
        else {
            val = 2;
            if (entity->flags & 2) {
                val = 3;
            }
            
            if (entity->flags & 4) {
                val = 1;
            }
            
            if ((entity->flags & 9) == 9) {
                if (i != 3) {
                    val = 0;
                }
            }
            
            else {
                if (entity->flags & 8) {
                    if (i != 3) {
                        val = 0;
                    }
                }
                
                if (entity->flags & 1) {
                    if (i != 0) {
                        val = 0;
                    }
                }
            }
            
            func_800B0980(flag, entity->perBit[i], arg0, val);
            entity->perBit[i] -= val;
        }  
    }
}

/**
 * @brief Process entities whose @c status has neither bit 0 nor bit 2 set.
 *
 * Walks @c D_800ED148.entities[0..6]; for each slot whose @c status & 5
 * is zero, calls @c func_800B09F0(i).
 */
void func_800B0C08(void) {
    s32 i;
    for (i = 0; i < 7; i++) {
        if ((D_800ED148.entities[i].status & 5) == 0) {
            func_800B09F0(i);
        }
    }
}

s8 func_800B0C68(s32 arg0, s32 arg1) {
    BattleCharData* var_v1;
    s32 i;
    
    for (i = 0; i < 32; i++) {
        var_v1 = &g_battleChars.chars[arg0];
        if (var_v1->magicSlots[i].unk0 == arg1) {
            return var_v1->magicSlots[i].unk1;
        }
    }
    
    return 0;
}

s32 func_800B0CC4(s32 arg0, s32 arg1) {
    s32 i;
    s32 count;
    BattleCharData* character;
    
    count = 0;
    character = &g_battleChars.chars[arg0];
    
    if (arg1 == 0) {
        for (i = 0; i < 32; i++) {
            if (character->magicSlots[i].unk0 != 0) {
                count++;
            }
        }
        
        if (count == 0) {
            arg1 = 255;
        }
            
        else {
            arg1 = func_8009B15C() % count;
            
            while(1) {
                if (character->magicSlots[arg1].unk0 != 0) {
                    arg1 = character->magicSlots[arg1].unk0;
                    break;
                    
                }
                
                arg1++;
                arg1 &= 0x1F;
            }
        }
    }
    
    return arg1;
}

s32 func_800B0D8C(s32 arg0, s32 arg1) {
    BattleCharData* var_v1;
    s32 i;

    for (i = 0; i < 4; i++) {
        var_v1 = &g_battleChars.chars[arg0];
        if (var_v1->cmdSlots[i].cmdType == arg1) {
            return 0;
        }
    }

    return 255;
}

/**
 * @brief Dispatch call based on @ref SEALED_FLAG_02 in g_battleConfig.unk8.
 *
 * If @ref SEALED_FLAG_02 is set, passes 0xFF to func_800B0CC4.
 * Otherwise calls func_800B0D8C with a0 and mode 2, then passes
 * the result to func_800B0CC4.
 *
 * @param a0 Entity parameter for func_800B0D8C and func_800B0CC4.
 */
s32 func_800B0DDC(s32 a0) {
    s32 val;
    if (g_battleConfig.unk8 & SEALED_FLAG_02) {
        val = 255;
    } 
    
    else {
        val = func_800B0D8C(a0, 2);
    }
    return func_800B0CC4(a0, val);
}

s32 func_800B0E30(s32 arg0) {
    s32 count;
    s32 i;
    s32 id;

    count = 0;
    if (arg0 == 0) {
        for (i = 0; i < 32; i++) {
            if ((D_800EE9E8.animSlots[i].id != 0) && (g_kernel.battleItems[D_800EE9E8.animSlots[i].id].randomSelect & 1)) {
                count++;
            }
        }
        
        if (count == 0) {
            arg0 = 255;
        }
            
        else {
            arg0 = func_8009B15C() % count;
            while (1) {
                if ((D_800EE9E8.animSlots[arg0].id != 0) && (g_kernel.battleItems[D_800EE9E8.animSlots[arg0].id].randomSelect & 1)) {
                    arg0 = D_800EE9E8.animSlots[arg0].id;
                    break;
                }
                
                arg0++;
                arg0 &= 0x1F;
            }
        }  
    }

    return arg0;
}

/**
 * @brief Dispatch call based on @ref SEALED_FLAG_01 in g_battleConfig.unk8.
 *
 * If @ref SEALED_FLAG_01 is set, passes 0xFF to func_800B0E30.
 * Otherwise calls func_800B0D8C with a0 and mode 4, then passes
 * the result to func_800B0E30.
 *
 * @param a0 Entity parameter for func_800B0D8C.
 */
s32 func_800B0F3C(s32 a0) {
    s32 val;
    if (g_battleConfig.unk8 & SEALED_FLAG_01) {
        val = 255;
    } 
    
    else {
        val = func_800B0D8C(a0, 4);
    }

    return func_800B0E30(val);
}

/**
 * @brief Convert ability flag bits to GF compatibility bitmask.
 *
 * Bit 0 of the input maps to bit 14 (0x4000) of the result,
 * and bit 1 maps to bit 13 (0x2000).
 *
 * @param arg0 Ability flags.
 * @return Bitmask with bits 14 and/or 13 set.
 */
s32 func_800B0F7C(s32 arg0) {
    s32 temp_v1;
    int new_var;
    s32 var_v0;

    temp_v1 = (arg0 & 1) << 0xE;
    new_var = arg0 & 2;
    var_v0 = temp_v1;
    if (new_var) {
        var_v0 = temp_v1 | 0x2000;
        var_v0 = temp_v1;
        var_v0 = var_v0 | 0x2000;
    }
    return var_v0;
}

u16 func_800B0F9C(s32 arg0) {
    switch (arg0 & 0x30) {
        case 0:
            if (arg0 & 0x40) {
                return func_800AA4E8();
            }
            return func_800AA4E0();
            
        case 16:
            if (arg0 & 0x40) {
                return func_800A9888();
            }
            return func_800A980C();
        
        case 32:
            return func_800AA4F0();
    }
}

u16 func_800B1050(s32 arg0) {
    switch (arg0 & 0x30) {
        case 0:
            if (arg0 & 0x40) {
                return func_800AA4E0();
            }
            return func_800AA4E8();

        case 16:
            if (arg0 & 0x40) {
                return func_800A980C();
            }
            return func_800A9888();

        case 32:
            return func_800AA4F0();
    }
}

/**
 * @brief Compute combined ability flags for the spell record at the given ID.
 *
 * Reads the spell's target info, passes it to func_800B1050 and
 * func_800B0F7C, and returns the OR of both results masked to 16 bits.
 *
 * @param a0 Spell ID (index into g_kernel.magic).
 * @return Combined 16-bit ability flags.
 */
u16 func_800B1104(s32 a0) {
    return func_800B1050(g_kernel.magic[a0].targetInfo) | func_800B0F7C(g_kernel.magic[a0].targetInfo);
}

/**
 * @brief Resolve the action ID and flags for one of the player's command slots.
 *
 * Picks a deterministic-but-pseudorandom variant via func_8009B15C() % 3 and
 * dispatches on the command type stored at g_battleChars.chars[selfIdx].cmdSlots[cmdIdx].
 *
 * - cmd 1 / 12 (Attack-like): writes only *outFlags (no ID resolved).
 * - cmd 2 (Magic): resolves a spell ID via func_800B0DDC; combined element/status flags
 *   are read from g_kernel.magic[id].targetInfo via func_800B0F9C/F7C/1104.
 * - cmd 4 (GF/Item): resolves an ability ID via func_800B0F3C, calls func_800AF4BC(id, 1)
 *   to consume a charge, then reads flags from g_kernel.battleItems[id].targetInfo via
 *   func_800B1050/F9C/F7C.
 *
 * @param selfIdx     Party slot index into g_battleChars.chars (0..2).
 * @param cmdIdx      Command slot index (0..3) within the chosen char.
 * @param outId       Output: resolved action ID, or 0xFF on lookup failure.
 * @param outFlags    Output: combined 16-bit element/status flags.
 * @return The command type that was dispatched, or 0 if no match / lookup failed.
 */
s32 func_800B115C(s32 selfIdx, s32 cmdIdx, s32 *outId, u16 *outFlags) {
    u8 var = func_8009B15C() % 3;
    s32 cmd = g_battleChars.chars[selfIdx].cmdSlots[cmdIdx].cmdType;
    s32 result;

    *outId = 0;

    switch (cmd) {
        case 1:
        case 12:
            if (var != 0) {
                *outFlags = func_800A980C();
            } 
            
            else {
                *outFlags = func_800A9888();
            }
            
            return cmd;
        case 2:
            result = func_800B0DDC(selfIdx);
            *outId = result;
            if (result == 255) {
                return 0;
            }

            if (var != 0) {
                *outFlags = func_800B1104(result);
            } 
            
            else {
                *outFlags = func_800B0F9C(g_kernel.magic[result].targetInfo) | func_800B0F7C(g_kernel.magic[*outId].targetInfo);
            }
            return cmd;

        case 4:
            result = func_800B0F3C(selfIdx);
            *outId = result;
            if (result == 255) {
                return 0;
            }
            func_800AF4BC(result, 1);
            if (var != 0) {
                *outFlags = func_800B1050(g_kernel.battleItems[*outId].targetInfo) | func_800B0F7C(g_kernel.battleItems[*outId].targetInfo);
            } 
            
            else {
                *outFlags = func_800B0F9C(g_kernel.battleItems[*outId].targetInfo) | func_800B0F7C(g_kernel.battleItems[*outId].targetInfo);
            }
            
            return cmd;
    }

    return 0;
}

void func_800B13A0(s32 arg0, s32* arg1, s32* arg2, u16* arg3) {
    u32 result;
    
    result = func_8009B15C() % 4;
    while((*arg1 = func_800B115C(arg0, result, arg2, arg3)) == 0) {
        result++;
        result %= 4;
    }
}

s32 func_800B1438(s32 arg0) {
    s32 i;
    s32 result;
    s32 count;
    BattleCharData* character;

    count = 0;
    character = &g_battleChars.chars[arg0];
    
    for (i = 0; i < 32; i++) {
        if ((character->magicSlots[i].unk0 != 0) && (g_kernel.magic[character->magicSlots[i].unk0].targetInfo & 0x40)) {
            count++;
        }
    }
    
    if (count == 0) {
        result = 255;
    }
        
    else {
        result = func_8009B15C() % 32;
        while (1) {            
            if ((character->magicSlots[result].unk0 != 0) && (g_kernel.magic[character->magicSlots[result].unk0].targetInfo & 0x40)) {
                result = character->magicSlots[result].unk0;
                break;
            }
            
            result++;
            result &= 0x1F;
        }
    }
        
    return result;
}

void func_800B1564(s32 arg0, s32* arg1, s32* arg2, s16* arg3) {
    if ((*arg2 = func_800B1438(arg0)) != 255) {
        *arg1 = 2;
        *arg3 = func_800B0F9C(g_kernel.magic[*arg2].targetInfo) 
              | func_800B0F7C(g_kernel.magic[*arg2].targetInfo);
        return;
    }
    
    *arg1 = 1;
    *arg2 = 0;
    *arg3 = func_800A9888();
}