#include "common.h"
#include "gamestate.h"
#include "field/fe_object12.h"

extern u8 D_80077BA8[];

INCLUDE_ASM("asm/field/nonmatchings/fe_object12", func_800C00C8);

/** @brief Sets bit 0x20 of the partyLockFlag. */
void func_800C0384(void) {
    g_gameState.mainData.partyLockFlag |= 0x20;
}

/** @brief Clears bit 0x20 of the partyLockFlag. */
void func_800C03A0(void) {
    g_gameState.mainData.partyLockFlag &= ~0x20;
}

/** @brief Sets bit 0x10 of the partyLockFlag. */
void func_800C03BC(void) {
    g_gameState.mainData.partyLockFlag |= 0x10;
}

/** @brief Clears bit 0x10 of the partyLockFlag. */
void func_800C03D8(void) {
    g_gameState.mainData.partyLockFlag &= ~0x10;
}

/** @brief Sets bit 0x02 of the partyLockFlag. */
void func_800C03F4(void) {
    g_gameState.mainData.partyLockFlag |= 0x02;
}

/**
 * @brief Test whether the player's inventory holds a given item ID.
 *
 * Linear scan over all @ref ITEM_SLOT_COUNT slots of
 * @c g_gameState.mainData.itemSlots, comparing each slot's @c id.
 *
 * @param itemId Item ID to search for.
 * @return @c 1 if any slot holds @p itemId, otherwise @c 0.
 */
s32 func_800C0410(s32 itemId) {
    s32 i;

    for (i = 0; i < ITEM_SLOT_COUNT; i++) {
        if (g_gameState.mainData.itemSlots[i].id == itemId) {
            return 1;
        }
    }
    return 0;
}

/**
 * Copies 0x40 bytes from D_80077BA8 - 0x98 to D_80077BA8 using memcopy,
 * then calls memzero16 with D_80077BA8 and mode 4.
 */
void func_800C0448(void) {
    memcopy(D_80077BA8, D_80077BA8 - 0x98, 0x40);
    memzero16(D_80077BA8, 4);
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object12", func_800C048C);

INCLUDE_ASM("asm/field/nonmatchings/fe_object12", func_800C0634);
