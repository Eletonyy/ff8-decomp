#include "common.h"
#include "psxsdk/libgpu.h"
#include "battle.h"
#include "input/button_remap.h"
#include "dialog.h"
#include "btl_entity.h"
#include "drawbar.h"

extern BattleDisplayEntity g_battleEntities[];
extern s32 D_800834CC;
extern u8 g_digitBaseCode;
extern DisplayListBuf *D_800834C0;
extern u16 D_80052974[];

u8 *func_8002BC6C(u32 *ot, s32 idx, u8 *head);
u8 *func_8002BE48(u32 *ot, u8 *head);


INCLUDE_ASM("asm/nonmatchings/btl_entity", func_8002BAA0);


/**
 * @brief Draw a window box: its frame (func_8002B3A0, fill mode 3), then its
 * background (drawWindowBackground).
 * @param ot Ordering table.
 * @param prim Primitive buffer cursor.
 * @param rect The window rect.
 * @param color Colour word.
 * @return The primitive cursor after the packets.
 */
DR_AREA *func_8002BC10(P_TAG *ot, DR_AREA *prim, RECT *rect, s32 color)
{
    DR_AREA *p;

    p = func_8002B3A0(ot, prim, rect, color, 3);

    return drawWindowBackground(ot, p, rect, color);
}


INCLUDE_ASM("asm/nonmatchings/btl_entity", func_8002BC6C);


INCLUDE_ASM("asm/nonmatchings/btl_entity", func_8002BE48);


/**
 * @brief Get the OT entry for a battle entity based on its anim speed.
 * @param idx Entity index.
 * @return Pointer to the OT entry at active display list's ot[animSpeed].
 */
s32* getEntityTablePtr(s32 idx) {
    u32 *base = D_800834C0->ot;
    s32 speed = getBattleEntityAnimSpeed(idx);
    return &base[speed];
}


/**
 * @brief Build the per-frame battle entity display list.
 *
 * For each of the 8 entity slots, runs the visibility test (activeFlag set,
 * field36 clear, field35 set) and clips the entity rect via @c clipBlitRects.
 * Each entity that passes and has a non-empty clip result sets its bit in
 * @p mask. Then @c func_8002BE48 sets up the draw area at @c ot[15], and for
 * every set bit @c func_8002BC6C is called to render that entity into the
 * display list at @p head.
 *
 * @param ot Ordering table.
 * @param head Display-list write head; advanced through nested calls.
 * @return The display-list head after all entity primitives are emitted.
 */
u8 *func_8002BF24(u32 *ot, u8 *head) {
    s32 mask = 0;
    s32 hit;
    s32 i;
    s32 bit;
    s32 slotBit;

    i = 0;
    bit = 1;
    for (; i < 8; i++) {
        BattleDisplayEntity *e = &g_battleEntities[i];
        hit = 0;
        if (e->activeFlag != 0 && e->unk36 == 0 && e->unk35 != 0) {
            hit = (clipBlitRects((BlitParams *)e) != 0);
        }
        if (hit) {
            mask |= bit << i;
        }
    }

    head = func_8002BE48(&ot[15], head);
    if (mask == 0) {
        return head;
    }

    for (i = 0; i < 8; i++) {
        slotBit = 1;
        if (mask & (slotBit << i)) {
            head = func_8002BC6C(ot, i, head);
        }
    }
    return head;
}


/**
 * @brief Dispatch a battle entity's update callback.
 *
 * If the entity has a callback set, invokes it with the entity pointer.
 *
 * @param idx Entity index.
 */
void dispatchBattleEntity(s32 idx) {
    BattleDisplayEntity *entity = &g_battleEntities[idx];
    if (entity->callback) {
        entity->callback(entity);
    }
}


/** @brief Finds the first available battle entity slot (0-7) and marks it as active.
 *  @return Slot index (0-7), or -1 if none available.
 */
s32 allocBattleEntitySlot(void) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (GetActiveFlag(i) == 0) {
            setBattleEntityActive(i, 1);
            return i;
        }
    }
    return -1;
}


/**
 * @brief Initialize all 8 battle entity slots by calling initBattleEntity on each.
 *
 * Iterates slots 0 through 7, resetting each entity's display rects, flags,
 * position fields, and brightness to default values.
 */
void initAllBattleEntities(void) {
    s32 i;
    for (i = 0; i < 8; i++) {
        initBattleEntity(i);
    }
}


/**
 * @brief Store a base address for battle entity data.
 * @param val Value to store (typically a memory address).
 */
void setBattleEntityBase(s32 val) {
    D_800834CC = val;
}


/** @brief Return the maximum number of battle entity slots (always 8). */
s32 getMaxBattleEntities(void) {
    return 8;
}


/**
 * @brief Get the FF8 font code for digit '0'.
 * @return Base character code for rendering digits in battle.
 */
u8 getDigitBaseCode(void) {
    return g_digitBaseCode;
}


/**
 * @brief Set the FF8 font code for digit '0'.
 * @param val Base character code from the menu string table.
 */
void setDigitBaseCode(u8 val) {
    g_digitBaseCode = val;
}


INCLUDE_ASM("asm/nonmatchings/btl_entity", func_8002C130);


INCLUDE_ASM("asm/nonmatchings/btl_entity", func_8002C3AC);


INCLUDE_ASM("asm/nonmatchings/btl_entity", func_8002C56C);


/**
 * @brief Map a message character code to its glyph/sprite index.
 *
 * Translates a raw character byte @p c into the index used to fetch the glyph's
 * dimensions and sprite data:
 *   - @c c @c >= @c 0x40 : looked up directly in the @ref D_80052974 table
 *     (letters and the bulk of the character set), offset by @c 0x40.
 *   - @c [0x30,0x40) : digits/symbols, mapped linearly to @c c @c + @c 0x50.
 *   - @c [0x20,0x30) : button-icon codes, remapped via @c reverseButtonRemap;
 *     a valid result is biased by @c 0x80, an invalid one yields 0.
 *   - @c c @c < @c 0x20 : control codes, which have no glyph (returns 0).
 *
 * @param c Character code from a battle message string.
 * @return Glyph/sprite index, or 0 if @p c has no printable glyph.
 */
s32 func_8002C734(s32 c) {
    if (c >= 0x40) {
        c -= 0x40;
        return D_80052974[c];
    }
    if (c >= 0x20) {
        if (c >= 0x30) {
            if (c < 0x40) {
                return c + 0x50;
            }
        } else {
            c = reverseButtonRemap(c - 0x20);
            if (c >= 0) {
                return c + 0x80;
            }
            return 0;
        }
    }
    return 0;
}


/**
 * @brief Set a dialog's text origin.
 * @param idx Dialog index.
 * @param x Text origin x in pixels (@c textX).
 * @param y Text origin y in pixels (@c textY).
 */
void setDialogTextOrigin(s32 idx, s32 x, s32 y) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->textX = x;
    entry->textY = y;
}


/**
 * @brief Set a dialog's choice lines.
 * @param idx Dialog index.
 * @param first First choice line (@c firstChoice).
 * @param last Last choice line (@c lastChoice).
 * @param cancel Line Triangle moves the cursor to, negative for none (@c cancelChoice).
 */
void setDialogChoices(s32 idx, s32 first, s32 last, s32 cancel) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->firstChoice = first;
    entry->lastChoice = last;
    entry->ctrl.bits.cancelChoice = cancel;
}


/**
 * @brief Put a dialog's choice cursor on a line (@c choiceCursor).
 * @param idx Dialog index.
 * @param val Choice line.
 */
void setDialogChoiceCursor(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->choiceCursor = val;
}


/**
 * @brief Set a dialog's draw hook.
 * @param idx Dialog index.
 * @param val Hook to run, or NULL for none.
 */
void setDialogDrawCallback(s32 idx, DialogDrawCallback val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->drawCallback = val;
}


/**
 * @brief Set a dialog's per-frame update hook.
 * @param idx Dialog index.
 * @param val Hook to run, or NULL for none.
 */
void setDialogUpdateCallback(s32 idx, DialogCallback val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->updateCallback = val;
}


/**
 * @brief Set a message window's brightness and copy it to the window's display entity.
 * @param idx Dialog index.
 * @param val Brightness (0x1000 = full).
 */
void setDialogBrightness(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->brightness = val;
    setBattleEntityBrightness(entry->entityIdx, val);
}


