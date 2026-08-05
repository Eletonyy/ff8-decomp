#include "common.h"
#include "gamestate.h"
#include "game.h"
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
    memzero16((s32 *)D_80077BA8, 4);
}

/**
 * @brief Give stocked magic to the party.
 *
 * Distributes @p quantity copies of spell @p magicId across all present
 * characters (@c exists bit 0): first tops up every existing stack of the
 * spell to the 100 cap, carrying any remainder onward; whatever remains
 * (or the full amount when no character holds the spell) is placed in the
 * first empty magic slot of the first present character that does not
 * already know the spell. Silently drops the spell when everyone who could
 * take it is full.
 *
 * @param magicId  Magic spell ID to add (see MAGIC_* defines).
 * @param quantity Number of copies to add.
 */
void func_800C048C(s32 magicId, s32 quantity) {
    s32 total;
    s32 i;
    s32 j;

    total = 0;
    for (i = 0; i < CHARACTER_COUNT; i++) {
        if (g_gameState.chars[i].exists & CHAR_FLAG_PRESENT) {
            for (j = 0; j < MAGIC_SLOT_COUNT; j++) {
                if (g_gameState.chars[i].magic[j].magicId == magicId) {
                    total = g_gameState.chars[i].magic[j].quantity;
                    total += quantity;
                    if (total > 100) {
                        g_gameState.chars[i].magic[j].quantity = 100;
                        quantity = total - 100;
                        break;
                    }
                    g_gameState.chars[i].magic[j].quantity = total;
                    return;
                }
            }
        }
    }

    if (total == 0) {
        for (i = 0; i < CHARACTER_COUNT; i++) {
            if (g_gameState.chars[i].exists & CHAR_FLAG_PRESENT) {
                for (j = 0; j < MAGIC_SLOT_COUNT; j++) {
                    if (g_gameState.chars[i].magic[j].magicId == 0) {
                        g_gameState.chars[i].magic[j].magicId = magicId;
                        g_gameState.chars[i].magic[j].quantity = quantity;
                        return;
                    }
                }
            }
        }
    } else {
        for (i = 0; i < CHARACTER_COUNT; i++) {
            if (g_gameState.chars[i].exists & CHAR_FLAG_PRESENT) {
                /* Skip characters already holding the spell. A goto is the
                 * only spelling that matches: measured goto-free forms
                 * (scan flag, `if (j < 32) continue`, folded loop condition)
                 * all change codegen. */
                for (j = 0; j < MAGIC_SLOT_COUNT; j++) {
                    if (g_gameState.chars[i].magic[j].magicId == magicId) {
                        goto next_char;
                    }
                }
                for (j = 0; j < MAGIC_SLOT_COUNT; j++) {
                    if (g_gameState.chars[i].magic[j].magicId == 0) {
                        g_gameState.chars[i].magic[j].magicId = magicId;
                        g_gameState.chars[i].magic[j].quantity = quantity;
                        return;
                    }
                }
            }
next_char:;
        }
    }
}

/**
 * @brief Apply and clear character slot 7's stocked magic.
 *
 * Walks all 32 magic slots of @c g_gameState.chars[7]; each stocked spell
 * (@c magicId @c != @c 0) is applied via @c func_800C048C, then the whole
 * magic list is zeroed. @c D_80077C40 aliases @c &g_gameState.chars[7].magic
 * (same address, 64 bytes); @c memzero16 clears it in 16-byte units.
 */
void func_800C0634(void) {
    s32 i;

    for (i = 0; i < MAGIC_SLOT_COUNT; i++) {
        s32 magicId = g_gameState.chars[7].magic[i].magicId;
        s32 quantity = g_gameState.chars[7].magic[i].quantity;
        if (magicId != 0) {
            func_800C048C(magicId, quantity);
        }
    }
    memzero16((s32 *)D_80077C40, 4);
}
