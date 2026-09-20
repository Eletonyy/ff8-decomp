#ifndef MENUSHOP_H
#define MENUSHOP_H

#include "gamestate.h"
#include "menushop2.h"

#define ITEM_PRICE_COUNT 200

typedef struct {
    u8 pad0[4];
    u8 characterId; /**< 0x04: character id who uses this weapon. */
    u8 pad5[2];     /* 0x05 */
    u8 hit;         /**< 0x07: weapon hit. */
    u8 pad8[4];     /* 0x08 */
} WeaponInfo; /* 12 bytes */

typedef struct {
    u16 nameId;     /**< 0x00: index of the weapon name. */
    u8 pad3;        /* 0x02 */
    u8 basePrice;   /**< 0x03: weapon base price. */
    u8 items[8];    /**< 0x04: items required to craft the weapon (id - quantity). */
} WeaponRecipe; /* 12 bytes */

extern WeaponInfo D_8007C3B8[28]; /**< Weapon attributes. */
extern WeaponRecipe D_801E9BA0[33]; /**< Junk shop weapon recipes (mwepon.bin content). */
extern u8 D_801E9D2C[68]; /**< Weapon names (mwepon.msg content). */
extern u8 D_801EB088[ITEM_PRICE_COUNT]; /**< Item quantities. */
extern u8 D_801EB150[8]; /**< weapon ID's listed at junk shop for the selected character. */
extern s32 D_801EB2E4;
extern s32 D_801EB2E8;

void func_801E5C08(s32);
s32 func_801E5D28(void);
s32 func_801E77EC(s32, s32, s32, s32, s32);
u8* func_801E7CFC(s32);
void func_801E7D30(u8*, u8*);
s32 func_801E7E1C(s32);
s32 func_801E7E68(s32, u32);
s32 func_801E7F4C(s32, s32);
s32 func_801E8058(s32);
void func_801E8134(s32, s32);
void func_801E816C(s32, s32);

#endif /* MENUSHOP_H */
