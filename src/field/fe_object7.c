#include "common.h"
#include "battle.h"
#include "gamestate.h"
#include "field.h"
#include "psxsdk/libgte.h"


/** @brief Battle encounter setup parameters (at D_80082C90). */
typedef struct {
    /* 0x00 */ s32 encounterPtr;
    /* 0x04 */ u8 field_04;
    /* 0x05 */ u8 field_05;
    /* 0x06 */ u8 field_06;
    /* 0x07 */ u8 field_07;
    /* 0x08 */ u8 field_08;
    /* 0x09 */ u8 field_09;
    /* 0x0A */ u8 pad0A[2];
    /* 0x0C */ u8 result;
} EncounterParams;

extern u8 D_8007064C;
extern s16 D_8007737C;
extern Eline *D_80085224;
extern Eline *D_80085230[];
extern u8 D_800773C0;
extern u8 D_80082C11;
extern EncounterParams D_80082C90;
extern u8 D_80082C10;
extern u8 D_8005630C[];
extern s32 D_8005F13C;
extern s16 D_800704B2;
extern u8 D_800DE8D0;
extern s16 D_800DE4D0;
extern s8 D_800DE4D2;
extern s8 D_800DE4D3;
extern s8 D_800DE4D4;
extern Eline *D_800DE4F0;
extern Eline *D_800DE4F4;
extern Eline *D_800DE4F8;

/**
 * @brief Pop a key item ID and store its value.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B542C(Eline *eline) {
    eline->resultSlots[0] = getKeyItemValue(POP(eline));
    return 2;
}

/**
 * @brief Table index 0x14C handler — initiate battle encounter.
 *
 * Pops encounter parameters from the bytecode stack, sets up the battle
 * transition (fade, sound shutdown, animation cleanup), then waits for
 * battle completion. On the return pass (entity inactive), reads the
 * battle result.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 on first pass (battle started), 2 on return (result ready).
 */
s32 func_800B5480(Eline *eline) {
    u8 *params;
    s32 result;
    s32 i;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        EncounterParams *params = &D_80082C90;
        params->field_08 = POP_BYTE(eline);
        params->field_06 = POP_BYTE(eline);
        params->field_07 = POP_BYTE(eline);
        params->field_09 = POP_BYTE(eline);
        params->field_04 = POP_BYTE(eline);
        params->encounterPtr = POP(eline);
        params->field_05 = POP_BYTE(eline);

        result = sumItemQuantities(params);
        eline->resultSlots[0] = result;
        eline->resultSlots[1] = 0;

        if (result >= 5) {
            if (!(g_seedState->stateFlags & 0x10)) {
                initBattleTransition();
            }

            D_800704A8.mode = 8;
            D_800704A8.counter = 0;

            while (sndGetStatus() == 2) {
                func_800393C8();
            }

            for (i = 0; i < 2; i++) {
                clearAnimEntryActive(i);
            }

            setCameraVibrateState(0);

            if (g_seedState->soundHandle0 == -1) {
                sndCmd11(0);
            }

            sndCmd40();
            func_80037FB0(0, 0x46, D_8005F13C);

            while (g_seedState->soundLoadComplete == 0) {
                func_800393C8();
            }

            D_80082C11 = g_seedState->soundBankSelector;
            D_8005F11C = sndCmd10(toggleSoundBank());
            sndCmdC0(0, 0x7F);
            sndStopPlayback();
            sndCmdF1();
            sndSetMasterVolume(0x7F);
        }

        return 1;
    } else {
        if (D_80082C90.result == 3) {
            eline->resultSlots[1] = -1;
        } else {
            eline->resultSlots[1] = D_80082C90.result;
        }
        return 2;
    }
}

extern u8 D_800DE880[];

/**
 * @brief Copy a null-terminated string to the D_800DE880 buffer.
 *
 * @param src Source string.
 * @return Pointer to D_800DE880 (the copied string).
 */
u8 *func_800B574C(u8 *src) {
    u8 *dst = D_800DE880;

    while (*src) {
        *dst++ = *src++;
    }
    *dst = *src;

    return D_800DE880;
}

/**
 * @brief Search the D_800DE880 buffer for a marker byte and offset the
 *        byte following each match.
 *
 * @param search Marker byte to search for (masked to u8).
 * @param offset Value to add to the byte after each match.
 * @return Pointer to D_800DE880.
 */
u8 *func_800B578C(s32 search, s32 offset) {
    u8 *ptr = D_800DE880;

    if (*ptr) {
        search &= 0xFF;
        do {
            if (*ptr == search) {
                ptr++;
                *ptr += offset;
            }
            ptr++;
        } while (*ptr);
    }

    return D_800DE880;
}


/**
 * @brief Process message control codes for party member display.
 *
 * Scans the D_800DE880 buffer interpreting control codes:
 *   - 0x02: count separator, truncate at maxCount+1
 *   - 0x03: insert party member name offset, check junctioned ability
 *   - 0x06: check ability ownership and status flag
 *
 * @param maxCount Maximum number of entries to show.
 * @param abilityId Ability ID to check for junctioned/owned status.
 * @return Pointer to D_800DE880.
 */
u8 *func_800B57E8(s32 maxCount, s32 abilityId) {
    u8 *ptr = D_800DE880;
    s32 count = 0;
    s32 charId;
    s32 memberIdx;
    u8 *base;

    if (*ptr) {
        base = (u8 *)&g_gameState;
        memberIdx = 0;

        do {
            switch (*ptr) {
            case 2:
                if (count >= maxCount + 1) {
                    *ptr = 0;
                    return D_800DE880;
                }
                count++;
                break;
            case 3:
                ptr++;
                *ptr += base[0xD38 + memberIdx];
                charId = func_80037C6C(base[0xD38 + memberIdx]);
                if (hasJunctionedAbility(charId, abilityId)) {
                    ptr += 2;
                    *ptr = 5;
                    ptr++;
                    *ptr = 0x41;
                }
                memberIdx++;
                break;
            case 6:
                charId = func_80037C6C(base[0xD38 + memberIdx]) & 0xFF;
                if (!func_80021108(base[0xAF4 + charId], abilityId)) {
                    if (g_battleChars.chars[charId].fieldStatusByte & 2) {
                        ptr++;
                        *ptr = 0x27;
                    }
                }
                break;
            default:
                break;
            }
            ptr++;
        } while (*ptr);
    }

    return D_800DE880;
}

/**
 * @brief Recalculate party stats and check if any member has fieldStatusByte bit 1 set.
 *
 * Sets D_80082C10 from WorldContext field_0xF3 (if flag 0x800 is active),
 * calls recalcPartyStats(), then checks each party slot.
 *
 * @return 1 if any active party member has fieldStatusByte bit 1 set, 0 otherwise.
 */
s32 func_800B5990(void) {
    s32 i;

    if (g_seedState->stateFlags & 0x800) {
        D_80082C10 = g_seedState->fieldF3;
    } else {
        D_80082C10 = 0;
    }

    recalcPartyStats();

    for (i = 0; i < 3; i++) {
        if (g_gameState.mainData.party.party[i] != 0xFF) {
            if (g_battleChars.chars[i].fieldStatusByte & 2) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Draw point interaction handler (8-state machine).
 *
 * Manages the full draw-point interaction: prompt, ability check,
 * party member selection, magic draw, result display, and used-flag update.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while processing, 2 when complete.
 */
s32 func_800B5A30(Eline *eline) {
    s32 fieldIdx;
    s32 tableResult;
    u8 *text;
    u32 dims;
    s32 i;
    s16 rect[4];

    fieldIdx = PEEK(eline) - 1;
    tableResult = lookupFieldTable(fieldIdx);

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x204 = 0;
    }

    switch (eline->field_0x204) {
    case 0:
        g_seedState->fieldF2 = fieldIdx;
        setSfxEntryVolume(6, 0x1000);
        setSfxEntityType(6, 6);

        if (func_800B5990()) {
            i = getPackedField2Bit(fieldIdx);
            if (i == 3 || getPackedField2Bit(fieldIdx) == 2) {
                text = func_800B574C(func_8003974C(D_8005630C, 3));
            } else {
                text = func_800B574C(func_8003974C(D_8005630C, 2));
                text = func_800B578C(0xC, tableResult & 0x3F);
                sndPlaySfx(0x42, 0, 0x80, 0x7F);
            }
        } else {
            text = func_800B574C(func_8003974C(D_8005630C, 7));
        }

        initSfxPlayback(6, text);
        dims = func_8002E680(text);
        rect[2] = (dims & 0xFFFF) + 0x10;
        rect[3] = (dims >> 16) + 0x11;
        rect[0] = 0xA0 - rect[2] / 2;
        rect[1] = 0x70 - rect[3] / 2;
        func_8002E064(6, rect);
        startSfxSlow(6);
        setSfxGlobalFlag(6);
        g_seedState->sfxStartMask |= 0x40;

        if (tableResult & 0x80) {
            D_800DE4D0 = 0x14;
        } else {
            D_800DE4D0 = 0xA;
        }

        if (getPackedField2Bit(fieldIdx) == 1) {
            D_800DE4D0 /= 2;
        }

        i = fieldRandom() & 0xFF;
        D_800DE4D0 = D_800DE4D0 * (i + 0x80) / 512 + 1;
        eline->field_0x204++;
        break;

    case 1:
        if (g_seedState->sfxStartMask & 0x40) {
            return 1;
        }

        i = getPackedField2Bit(fieldIdx);
        if (i == 3 || getPackedField2Bit(fieldIdx) == 2) {
            eline->resultSlots[0] = 0;
            eline->stackPtr--;
            return 2;
        }

        if (func_800B5990()) {
            eline->field_0x204++;
            break;
        }

        eline->resultSlots[0] = 0;
        eline->stackPtr--;
        return 2;

    case 2:
        for (D_800DE4D4 = 0; D_800DE4D4 < 3; D_800DE4D4++) {
            if (g_gameState.battleParty[D_800DE4D4] == 0xFF) {
                break;
            }
        }

        text = func_800B574C(func_8003974C(D_8005630C, 4));
        text = func_800B57E8(D_800DE4D4, tableResult & 0x3F);
        dims = func_8002E680(text);
        rect[2] = (dims & 0xFFFF) + 0x30;
        rect[3] = (dims >> 16) + 0x11;
        rect[0] = 0xA0 - rect[2] / 2;
        rect[1] = 0x70 - rect[3] / 2;
        func_8002E064(6, rect);
        func_8002D784(6, text, 1, D_800DE4D4 + 1, 2, 1);
        startSfxSlow(6);
        setSfxGlobalFlag(6);
        g_seedState->sfxStartMask |= 0x40;
        g_seedState->sfxActiveMask |= 0x40;
        eline->field_0x204++;
        break;

    case 3:
        setSfxGlobalFlag(6);
        D_800DE4D2 = func_8002CE84(6);
        if ((s8)D_800DE4D2 < 0) {
            return 1;
        }
        fadeOutSfxSlow(6);
        g_seedState->sfxActiveMask &= ~0x40;

        if ((s8)D_800DE4D2 == 0) {
            eline->resultSlots[0] = 0;
            eline->stackPtr--;
            return 2;
        }

        D_800DE4D2--;
        eline->field_0x204++;
        break;

    case 4:
        D_800DE4D3 = func_80037C6C(g_gameState.battleParty[D_800DE4D2]);
        if (getSfxField1C(6)) {
            return 1;
        }

        if (g_battleChars.chars[D_800DE4D3].fieldStatusByte & 2) {
            func_800A455C(g_seedState->memberSlot[D_800DE4D2]);
            sndPlaySfx(0x43, 0, 0x80, 0x7F);
            eline->field_0x204++;
            break;
        }

        eline->field_0x204 += 2;
        break;

    case 5:
        if (func_800A48CC()) {
            return 1;
        }
        eline->field_0x204++;
        break;

    case 6:
        if (g_battleChars.chars[D_800DE4D3].fieldStatusByte & 2) {
            for (i = 0; i < D_800DE4D0; i++) {
                if (func_800211B4(g_gameState.mainData.party.party[D_800DE4D3], tableResult & 0x3F)) {
                    break;
                }
            }

            func_8002E1B4(7, i);

            if (i != 0) {
                text = func_800B574C(func_8003974C(D_8005630C, 5));
            } else {
                text = func_800B574C(func_8003974C(D_8005630C, 6));
                eline->field_0x204 = 0;
            }
        } else {
            text = func_800B574C(func_8003974C(D_8005630C, 6));
            eline->field_0x204 = 0;
        }

        func_800B578C(3, g_gameState.battleParty[D_800DE4D2]);
        text = func_800B578C(0xC, tableResult & 0x3F);
        initSfxPlayback(6, text);
        dims = func_8002E680(text);
        rect[2] = (dims & 0xFFFF) + 0x10;
        rect[3] = (dims >> 16) + 0x11;
        rect[0] = 0xA0 - rect[2] / 2;
        rect[1] = 0x70 - rect[3] / 2;
        func_8002E064(6, rect);
        startSfxSlow(6);
        setSfxGlobalFlag(6);
        g_seedState->sfxStartMask |= 0x40;
        eline->field_0x204++;
        break;

    case 7:
        if (g_seedState->sfxStartMask & 0x40) {
            return 1;
        }

        if (tableResult & 0x40) {
            func_800383B8(fieldIdx, 2);
        } else {
            func_800383B8(fieldIdx, 3);
        }

        eline->stackPtr--;
        return 2;
    }

    return 1;
}

/**
 * @brief Set up a scripted camera/effect using eline position data.
 *
 * Sets WorldContext field_0xF0 to 1 (active), pops a byte parameter
 * into field_0xF1, then calls func_800A4500 with the eline's position
 * fields and func_800A4550 with field_0xF1 | field_0x58.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6210(Eline *eline) {
    g_seedState->fieldF0 = 1;
    g_seedState->fieldF1 = POP_BYTE(eline);
    func_800A4500(eline->posX, eline->posY, eline->posZ);
    func_800A4550(g_seedState->fieldF1 | g_seedState->field58);
    return 2;
}

/**
 * @brief Pop a byte from the stack, decrement, and store to WorldContext field_0xF2.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B629C(Eline *eline) {
    g_seedState->fieldF2 = POP_BYTE(eline);
    g_seedState->fieldF2--;
    return 2;
}

/**
 * @brief Activate a system slot by index from the script stack.
 *
 * Pops a value, masks to 4 bits (0-15), and sets the corresponding
 * slotActive entry in the system state to 1.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B62E8(Eline *eline) {
    D_800704A8.slotActive[POP(eline) & 0xF] = 1;
    return 2;
}

/**
 * @brief Deactivate a system slot by index from the script stack.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6328(Eline *eline) {
    D_800704A8.slotActive[POP(eline) & 0xF] = 0;
    return 2;
}

/**
 * @brief Activate a system slot with the eline's field_0x256 value | 0x80.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6364(Eline *eline) {
    D_800704A8.slotActive[POP(eline) & 0xF] = eline->field_0x256 | 0x80;
    return 2;
}

/**
 * @brief Pop a value; if nonzero call func_800C0384 (set bit 0x20),
 *        otherwise call func_800C03A0 (clear bit 0x20).
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B63A4(Eline *eline) {
    if (POP(eline)) {
        func_800C0384();
    } else {
        func_800C03A0();
    }
    return 2;
}

/**
 * @brief Delegate to func_800C03F4 and return.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6400(Eline *eline) {
    func_800C03F4(eline);
    return 2;
}

/**
 * @brief Clear field_0x204 if the entity's script group is active.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6420(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->field_0x204 = 0;
    }
    return 2;
}

/**
 * @brief Pop a value from the stack and store to D_8007737C.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6448(Eline *eline) {
    D_8007737C = POP(eline);
    return 2;
}

/**
 * @brief Pop a value and store to both D_80082C0A and WorldContext field_0xB6.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6478(Eline *eline) {
    D_80082C0A = g_seedState->fieldB6 = POP(eline);
    do {} while (0);
    return 2;
}

/**
 * @brief Set system mode to 3 (if idle) and pop scene ID + counter.
 *
 * @param eline Pointer to the event line (script context).
 * @return 3 (special return — triggers mode transition).
 */
s32 func_800B64B0(Eline *eline) {
    if (D_800704A8.mode == 0) {
        D_800704A8.mode = 3;
    }
    D_80082C0A = POP(eline);
    D_800704A8.counter = POP(eline);
    return 3;
}

/**
 * @brief Store D_80082C0F into the eline result field.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B6524(Eline *eline) {
    eline->resultSlots[0] = D_80082C0F;
    return 2;
}

/**
 * @brief If D_800DE8D0 is set, enable flag 0x400; otherwise clear field_0xCF.
 *
 * @return 2 (continue processing).
 */
s32 func_800B653C(void) {
    if (D_800DE8D0) {
        g_seedState->stateFlags |= 0x400;
    } else {
        g_seedState->fieldCF = 0;
    }
    return 2;
}

/**
 * @brief Set field_0xCF to 1 and clear flag 0x400.
 *
 * @return 2 (continue processing).
 */
s32 func_800B6588(void) {
    SeedState *ctx = g_seedState;

    ctx->fieldCF = 1;
    ctx->stateFlags &= ~0x400;
    return 2;
}

/** @brief No-op handler. Returns 2 (continue). */
s32 func_800B65B0(Eline *eline) {
    return 2;
}

/**
 * @brief Set system mode to 4 (game render) and yield.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 (yield).
 */
s32 func_800B65B8(Eline *eline) {
    D_800704A8.mode = 4;
    return 1;
}

/** @brief Yield handler. Returns 1 (wait). */
s32 func_800B65CC(Eline *eline) {
    return 1;
}

/**
 * @brief Pop disc number from stack, set WorldContext and global state.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B65D4(Eline *eline) {
    g_seedState->expectedDiscId = POP_BYTE(eline);
    D_800773C0 = g_seedState->expectedDiscId - 1;
    setDiscNumber(g_seedState->expectedDiscId);
    return 2;
}

/**
 * @brief Save active message state for async restore.
 *
 * If the async-pending flag (0x10000000) is set and msgActive is 1,
 * backs up current message position/window/channel to the saved fields,
 * clears msgActive, and sets the deferred flag (0x10000).
 * Inverse of func_800B66A8 (restore).
 *
 * @param eline Pointer to the event line (script context).
 */
void func_800B663C(Eline *eline) {
    if (!(eline->flags & 0x10000000)) {
        return;
    }
    if (eline->msgActive != 1) {
        return;
    }

    eline->field_0x1C0 = eline->msgTextPtr;
    eline->field_0x1C4 = eline->msgPosX;
    eline->field_0x1C8 = eline->msgPosY;
    eline->field_0x21C = eline->windowId;
    eline->field_0x202 = eline->savedChannel;
    eline->msgActive = 0;
    eline->flags |= 0x10000;
}

/**
 * @brief Restore a pending async message display.
 *
 * If both the async-pending flag (0x10000000) and the deferred-message
 * flag (0x10000) are set, restores the saved message parameters from
 * the backup fields (field_0x1C0..0x1C8, field_0x21C, field_0x202),
 * reactivates the message, and clears the deferred flag. If the
 * async-message flag (0x40000) is still set, re-calls func_800B6738
 * to reinitialize the display.
 *
 * @param eline Pointer to the event line (script context).
 */
void func_800B66A8(Eline *eline) {
    if (!(eline->flags & 0x10000000)) {
        return;
    }
    if (!(eline->flags & 0x10000)) {
        return;
    }
    eline->msgTextPtr = eline->field_0x1C0;
    eline->msgPosX = eline->field_0x1C4;
    eline->msgPosY = eline->field_0x1C8;
    eline->windowId = eline->field_0x21C;
    eline->savedChannel = eline->field_0x202;
    eline->msgActive = 1;
    eline->msgState = 0;

    if ((eline->flags = eline->flags & ~0x10000) & 0x40000) {
        func_800B6738(eline);
    }
}

/**
 * @brief Initialize message display.
 *
 * Compares savedChannel against a threshold derived from D_800704B2
 * to select between two message source indices (field_0x250 vs
 * field_0x251). If the selected source differs from field_0x24E,
 * calls func_800B912C to set it up and marks the 0x2000 flag.
 * Clears field_0x1DA and sets the async-message flag (0x40000).
 *
 * @param eline Pointer to the event line (script context).
 */
void func_800B6738(Eline *eline) {
    s32 threshold = (D_800704B2 * 69020) >> 9;

    if (eline->savedChannel >= threshold) {
        if (eline->field_0x24E != eline->field_0x251) {
            func_800B912C(eline, eline->field_0x251);
            eline->flags |= 0x2000;
        }
    } else {
        if (eline->field_0x24E != eline->field_0x250) {
            func_800B912C(eline, eline->field_0x250);
            eline->flags |= 0x2000;
        }
    }

    eline->field_0x1DA = 0;
    eline->flags |= 0x40000;
}

/**
 * @brief Finalize message display.
 *
 * If the async-message flag (0x40000) is set, calls func_800B912C
 * with field_0x24F, then sets bit 0x2000 and clears the async flag.
 * Always clears field_0x240.
 *
 * @param eline Pointer to the event line (script context).
 */
void func_800B67F4(Eline *eline) {
    if (eline->flags & 0x40000) {
        func_800B912C(eline, eline->field_0x24F);
        eline->flags = (eline->flags | 0x2000) & ~0x40000;
    }
    eline->field_0x240 = 0;
}

/**
 * @brief Check if message display is complete and clean up.
 *
 * If the message-pending flag (0x20000) is set in the eline's flags
 * and the message state has reached 2 (complete), calls func_800B67F4
 * to finalize and clears the flag.
 *
 * @param eline Pointer to the event line (script context).
 */
void func_800B6854(Eline *eline) {
    if ((eline->flags & 0x20000) && eline->msgState == 2) {
        func_800B67F4(eline);
        eline->flags &= ~0x20000;
    }
}

/**
 * @brief Table index 0x04F handler — set message channel.
 *
 * Pops a channel value from the bytecode stack and stores it to both
 * savedChannel and msgChannel fields of the eline.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B68B8(Eline *eline) {
    u16 channel = POP(eline);

    eline->savedChannel = channel;
    eline->msgChannel = channel;
    return 2;
}

/**
 * @brief MES opcode 0x3E handler — show dialog message on screen.
 *
 * Checks if the entity is active, then pops window ID, Y position,
 * X position, and message text pointer from the bytecode stack. Saves
 * the current message channel, initializes message state, and calls
 * func_800B6738 to set up the message display. If message state reaches
 * 2 (complete), calls func_800B67F4 to finalize.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 if message display is complete, 1 otherwise (yield).
 */
s32 opHandler_MES(Eline *eline) {
    s32 new_var;
    u16 saved;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        saved = eline->msgChannel;
        eline->msgActive = 1;
        eline->msgState = 0;

        eline->windowId = POP(eline);
        eline->msgPosY = POP(eline) << 12;
        eline->msgPosX = POP(eline) << 12;
        new_var = POP(eline);
        eline->savedChannel = saved;
        eline->msgTextPtr = new_var << 12;

        func_800B6738(eline);
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        return 2;
    }
    return 1;
}

/**
 * @brief Show a message positioned at a map entity's location.
 *
 * On the first pass (entity active): pops window ID, saves channel,
 * calls func_800B6738 to init display. On subsequent ticks: reads
 * the entity's position from D_80085230[PEEK] and updates the message
 * coordinates each frame. When message state reaches 2, finalizes
 * and drops the entity index from the stack.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while message is active, 2 when complete.
 */
s32 func_800B69E8(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive = 1;
        eline->msgState = 0;
        eline->windowId = POP(eline);
        eline->savedChannel = eline->msgChannel;
        func_800B6738(eline);
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        eline->stackPtr--;
        return 2;
    }

    eline->msgTextPtr = D_80085230[PEEK(eline)]->posX;
    eline->msgPosX = D_80085230[PEEK(eline)]->posY;
    eline->msgPosY = D_80085230[PEEK(eline)]->posZ;
    return 1;
}

/**
 * @brief Show a message tracking a party member entity's position.
 *
 * Like func_800B69E8 but looks up the entity index through
 * g_seedState->memberSlot[] first, then reads position from
 * D_80085224[idx]. Updates message coordinates each frame.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while message is active, 2 when complete.
 */
s32 func_800B6B20(Eline *eline) {
    u8 idx;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive = 1;
        eline->msgState = 0;
        eline->windowId = POP(eline);
        eline->savedChannel = eline->msgChannel;
        func_800B6738(eline);
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        eline->stackPtr--;
        return 2;
    }

    idx = g_seedState->memberSlot[PEEK(eline)];
    eline->msgTextPtr = D_80085224[idx].posX;
    eline->msgPosX = D_80085224[idx].posY;
    eline->msgPosY = D_80085224[idx].posZ;
    return 1;
}

/**
 * @brief ASK opcode handler — display a yes/no prompt.
 *
 * Sets up the message display with position and window ID from
 * the bytecode stack, initializes the prompt state fields,
 * then polls for completion.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while prompt is active, 2 when answered.
 */
s32 func_800B6C28(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive = 1;
        eline->msgState = 0;
        eline->windowId = POP(eline);
        eline->savedChannel = eline->msgChannel;
        eline->msgPosY = POP(eline) << 12;
        eline->msgPosX = POP(eline) << 12;
        eline->msgTextPtr = POP(eline) << 12;
        eline->field_0x262 = 0;
        eline->field_0x240 = 1;
        eline->field_0x1DA = 0;
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        return 2;
    }

    return 1;
}

/**
 * @brief Display a positioned message (variant without yes/no prompt setup).
 *
 * Like func_800B6C28 (ASK prompt) but without the field_0x262/field_0x240
 * initialization. Pops window ID, Y position, X position, and message text
 * pointer from the bytecode stack; saves the current message channel;
 * resets message state. Then polls for completion.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while message is active, 2 when complete.
 */
s32 func_800B6D24(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive = 1;
        eline->msgState = 0;
        eline->windowId = POP(eline);
        eline->savedChannel = eline->msgChannel;
        eline->msgPosY = POP(eline) << 12;
        eline->msgPosX = POP(eline) << 12;
        eline->msgTextPtr = POP(eline) << 12;
        eline->field_0x1DA = 0;
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        return 2;
    }

    return 1;
}

/**
 * @brief Position-tracking message variant (clears field_0x1DA, no display init).
 *
 * Like func_800B69E8 but clears field_0x1DA instead of calling func_800B6738.
 * Pops window ID and saves the channel on the first pass; afterwards, updates
 * the message coordinates from D_80085230[PEEK]'s position each frame.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while message is active, 2 when complete.
 */
s32 func_800B6E18(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive = 1;
        eline->msgState = 0;
        eline->windowId = POP(eline);
        eline->savedChannel = eline->msgChannel;
        eline->field_0x1DA = 0;
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        eline->stackPtr--;
        return 2;
    }

    eline->msgTextPtr = D_80085230[PEEK(eline)]->posX;
    eline->msgPosX = D_80085230[PEEK(eline)]->posY;
    eline->msgPosY = D_80085230[PEEK(eline)]->posZ;
    return 1;
}

/**
 * @brief Party-member position-tracking message variant (clears field_0x1DA).
 *
 * Like func_800B6B20 but clears field_0x1DA instead of calling func_800B6738.
 * Looks up a party slot through g_seedState->memberSlot[], then updates the
 * message coordinates from D_80085224[idx]'s position each frame.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while message is active, 2 when complete.
 */
s32 func_800B6F4C(Eline *eline) {
    u8 idx;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->msgActive = 1;
        eline->msgState = 0;
        eline->windowId = POP(eline);
        eline->savedChannel = eline->msgChannel;
        eline->field_0x1DA = 0;
    }

    if (eline->msgState == 2) {
        func_800B67F4(eline);
        eline->stackPtr--;
        return 2;
    }

    idx = g_seedState->memberSlot[PEEK(eline)];
    eline->msgTextPtr = D_80085224[idx].posX;
    eline->msgPosX = D_80085224[idx].posY;
    eline->msgPosY = D_80085224[idx].posZ;
    return 1;
}

/**
 * @brief Pending-message MES variant — set flag 0x20000, init display, return 3.
 *
 * Unconditionally initializes the message with 4 stack values and sets the
 * pending-message flag (0x20000). Unlike opHandler_MES, there is no activeMask
 * guard or msgState==2 check — it schedules the display and yields with 3.
 *
 * @param eline Pointer to the event line (script context).
 * @return 3 (message scheduled, deferred completion).
 */
s32 func_800B7050(Eline *eline) {
    s32 new_var;
    u16 saved;

    saved = eline->msgChannel;
    eline->msgActive = 1;
    eline->flags |= 0x20000;
    eline->msgState = 0;

    eline->windowId = POP(eline);
    eline->msgPosY = POP(eline) << 12;
    eline->msgPosX = POP(eline) << 12;
    new_var = POP(eline);
    eline->savedChannel = saved;
    eline->msgTextPtr = new_var << 12;

    func_800B6738(eline);
    return 3;
}

/**
 * @brief Pending message variant with entity-position tracking.
 *
 * Sets the pending-message flag, pops a window ID, then reads initial message
 * coordinates from D_80085230[idx]'s position (two PEEKs + one POP for the
 * final Y). Returns 3 to indicate deferred completion.
 *
 * @param eline Pointer to the event line (script context).
 * @return 3 (message scheduled, deferred completion).
 */
s32 func_800B711C(Eline *eline) {
    eline->msgActive = 1;
    eline->flags |= 0x20000;
    eline->msgState = 0;
    eline->windowId = POP(eline);
    eline->savedChannel = eline->msgChannel;
    func_800B6738(eline);

    eline->msgTextPtr = D_80085230[PEEK(eline)]->posX;
    eline->msgPosX = D_80085230[PEEK(eline)]->posY;
    eline->msgPosY = D_80085230[POP(eline)]->posZ;
    return 3;
}

/**
 * @brief Pending message variant with party-member position tracking.
 *
 * Sets the pending-message flag, pops a window ID, looks up a party slot
 * through g_seedState->memberSlot[POP], then reads initial coordinates from
 * D_80085224[idx]'s position. Returns 3 for deferred completion.
 *
 * @param eline Pointer to the event line (script context).
 * @return 3 (message scheduled, deferred completion).
 */
s32 func_800B7228(Eline *eline) {
    u8 idx;

    eline->msgActive = 1;
    eline->flags |= 0x20000;
    eline->msgState = 0;
    eline->windowId = POP(eline);
    eline->savedChannel = eline->msgChannel;
    func_800B6738(eline);

    idx = g_seedState->memberSlot[POP(eline)];
    eline->msgTextPtr = D_80085224[idx].posX;
    eline->msgPosX = D_80085224[idx].posY;
    eline->msgPosY = D_80085224[idx].posZ;
    return 3;
}

/**
 * @brief Pending ASK-prompt variant — leaf function, all 4 POPs, returns 3.
 *
 * Sets the pending-message flag and pops window ID, Y, X, and text pointer
 * from the stack. Also initializes the prompt-specific state fields
 * (field_0x262=0, field_0x240=1, field_0x1DA=0) like func_800B6C28, but
 * unconditionally (no activeMask guard) and returns 3.
 *
 * @param eline Pointer to the event line (script context).
 * @return 3 (prompt scheduled, deferred completion).
 */
s32 func_800B7310(Eline *eline) {
    eline->msgActive = 1;
    eline->flags |= 0x20000;
    eline->msgState = 0;
    eline->windowId = POP(eline);
    eline->savedChannel = eline->msgChannel;
    eline->msgPosY = POP(eline) << 12;
    eline->msgPosX = POP(eline) << 12;
    eline->msgTextPtr = POP(eline) << 12;
    eline->field_0x1DA = 0;
    eline->field_0x262 = 0;
    eline->field_0x240 = 1;
    return 3;
}

/**
 * @brief Pending MES variant — leaf function, all 4 POPs, returns 3.
 *
 * Sets the pending-message flag and pops window ID, Y, X, and text pointer
 * from the stack. Returns 3 immediately (no display init call).
 *
 * @param eline Pointer to the event line (script context).
 * @return 3 (message scheduled, deferred completion).
 */
s32 func_800B73D4(Eline *eline) {
    eline->msgActive = 1;
    eline->flags |= 0x20000;
    eline->msgState = 0;
    eline->windowId = POP(eline);
    eline->savedChannel = eline->msgChannel;
    eline->msgPosY = POP(eline) << 12;
    eline->msgPosX = POP(eline) << 12;
    eline->msgTextPtr = POP(eline) << 12;
    eline->field_0x1DA = 0;
    return 3;
}

/**
 * @brief Poll whether the current message has finished displaying.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 if msgState == 2 (message complete), 1 otherwise (still displaying).
 */
s32 func_800B7490(Eline *eline) {
    if (eline->msgState == 2) {
        return 2;
    }
    return 1;
}

/**
 * @brief Clean up message state for a referenced entity (pops its index).
 *
 * Looks up an entity from D_80085230[POP], and if the pending-message flag
 * (0x10000000) is set and the entity's message is currently active, clears
 * it, tears down the display via func_800B912C, and repurposes the flags
 * (clear 0xF800 range, set 0x2000).
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B74B0(Eline *eline) {
    s32 idx = POP(eline);

    if (D_80085230[idx]->flags & 0x10000000) {
        if (D_80085230[idx]->msgActive != 1) {
            return 2;
        }
        D_80085230[idx]->msgActive = 0;
        func_800B912C(D_80085230[idx], D_80085230[idx]->field_0x24F);
        D_80085230[idx]->flags &= 0xFFFF07FF;
        D_80085230[idx]->flags |= 0x2000;
    }
    return 2;
}

/**
 * @brief Clean up message state for a referenced party-member entity.
 *
 * Pops a party slot, looks up the entity through g_seedState->memberSlot[],
 * then (if the message is currently active) clears it, tears down the display,
 * and marks the flags (clear 0xF800 range and set 0x2000).
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B7578(Eline *eline) {
    u8 idx = g_seedState->memberSlot[POP(eline)];

    if (D_80085224[idx].msgActive == 1) {
        D_80085224[idx].msgActive = 0;
        func_800B912C(&D_80085224[idx], D_80085224[idx].field_0x24F);
        D_80085224[idx].flags = (D_80085224[idx].flags & 0xFFFF07FF) | 0x2000;
    }
    return 2;
}

/**
 * @brief Clear the 0x10000 flag bit if the entity is active.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 (continue processing).
 */
s32 func_800B7640(Eline *eline) {
    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        eline->flags &= ~0x10000;
    }
    return 1;
}

/**
 * @brief Pop a byte off the stack into field_0x262.
 *
 * @param eline Pointer to the event line (script context).
 * @return 2 (continue processing).
 */
s32 func_800B7674(Eline *eline) {
    eline->field_0x262 = POP_BYTE(eline);
    return 2;
}

/**
 * @brief 2D distance from the saved message position to the entity's current position.
 *
 * Both coordinate pairs are in fixed-point (Q20.12). The per-axis deltas
 * are truncated toward zero via signed divide-by-4096 before being squared.
 * Used by the scrolling-message handler to scale the channel advance by
 * how far the entity has travelled since the message started.
 *
 * @param self Pointer to the event line (script context).
 * @return Integer distance between the two positions.
 */
s32 func_800B76A4(Eline *self) {
    s32 dx = (self->msgTextPtr - self->posX) / 4096;
    s32 dy = (self->msgPosX - self->posY) / 4096;
    dx = dx * dx;
    dy = dy * dy;
    return SquareRoot0(dx + dy);
}

/**
 * @brief Scrolling positioned-message handler with per-frame channel advance.
 *
 * On the first pass, pops 5 values (field_0x204, window, Y, X, text pointer),
 * initializes display state, and sets up prompt flags. Each subsequent frame,
 * advances the sound channel toward the target (field_0x204) at a rate scaled
 * by the distance travelled (via func_800B76A4) and the global tempo
 * D_800704B2. When the message completes, saves the current channel to
 * field_0x202 and finalizes via func_800B67F4.
 *
 * @param self Pointer to the event line (script context).
 * @return 1 while message is animating, 2 when complete.
 */
s32 func_800B7718(Eline *self) {
    s32 delta = 0;

    if ((self->activeMask >> self->scriptGroup) & 1) {
        self->msgActive = 1;
        self->msgState = 0;
        self->savedChannel = self->msgChannel;
        self->field_0x204 = POP(self);
        self->windowId = POP(self);
        self->msgPosY = POP(self) << 12;
        self->msgPosX = POP(self) << 12;
        self->msgTextPtr = POP(self) << 12;
        self->field_0x262 = 0;
        self->field_0x240 = 1;
    }

    if (self->msgState == 2) {
        self->field_0x202 = self->savedChannel;
        func_800B67F4(self);
        return 2;
    }

    delta = (s32)(self->field_0x204 - self->savedChannel) * D_800704B2;
    delta = delta / func_800B76A4(self);
    self->savedChannel += delta;
    func_800B6738(self);
    return 1;
}

/**
 * @brief Start a proximity-triggered message anchored at another entity.
 *
 * Computes the planar distance from self to target (Q20.12 fixed-point),
 * then picks one of two sound-channel scalings and one of two script-param
 * byte offsets (field_0x250 vs field_0x251) based on whether the squared
 * distance exceeds 0x3F47F (~509 units). Issues the chosen command via
 * func_800B912C, primes the message at the target's position, and sets
 * flag 0x2001.
 *
 * @param self Pointer to the event line (script context).
 * @param target The entity whose position anchors the message.
 */
void func_800B788C(Eline *self, Eline *target) {
    s32 dx, dy, distSq;

    dx = (target->posX - self->posX) / 4096;
    dy = (target->posY - self->posY) / 4096;
    dx = dx * dx;
    dy = dy * dy;
    dx = dx + dy;
    distSq = dx;

    if (distSq > 0x3F47F) {
        self->savedChannel = (u32)(D_800704B2 * 25375) >> 6;
        func_800B912C(self, self->field_0x251);
    } else {
        self->savedChannel = (u32)(D_800704B2 * 17255) >> 7;
        func_800B912C(self, self->field_0x250);
    }
    self->msgActive = 1;
    self->msgState = 0;
    self->windowId = 0x60;
    self->msgTextPtr = target->posX;
    self->msgPosX = target->posY;
    self->msgPosY = target->posZ;
    self->flags |= 0x2001;
}

/**
 * @brief Proximity-anchored message from the OTHER two party members.
 *
 * On the active frame, locates the party slot whose @c memberSlot
 * matches @c eline->field_0x255 (the "speaker"), then sets up
 * proximity messages from the other two slots back at the speaker
 * via @c func_800B788C. Saves the chosen entity pointers in
 * @c D_800DE4F0 / @c D_800DE4F4, kicks off the speaker's own animation
 * via @c func_800B912C, and marks flag @c 0x2000.
 *
 * On subsequent inactive frames, waits for both saved entities to
 * reach @c msgState == 2 with a fresh field_0x24E/24F mismatch — for
 * each, triggers @c func_800B912C and sets flag @c 0x8000. Once both
 * have completed, calls @c func_8009ECA4 and clears msgActive + flag
 * bit @c 0x1 on each.
 *
 * @param eline Script context (the speaker).
 * @return 1 while waiting, 2 once both other-party messages finish.
 */
s32 func_800B79C8(Eline *eline) {
    s32 i;
    s32 idx;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        for (i = 0; i < 3; i++) {
            if (g_gameState.battleParty[i] == eline->field_0x255) {
                break;
            }
        }

        D_800DE4F8 = 0;
        D_800DE4F4 = 0;
        D_800DE4F0 = 0;

        switch (i) {
        case 0:
            if (g_seedState->memberSlot[1] != 0xFF) {
                idx = g_seedState->memberSlot[1];
                D_800DE4F0 = &D_80085224[idx];
                func_800B788C(&D_80085224[idx], eline);
            }
            if (g_seedState->memberSlot[2] != 0xFF) {
                idx = g_seedState->memberSlot[2];
                D_800DE4F4 = &D_80085224[idx];
                func_800B788C(&D_80085224[idx], eline);
            }
            break;
        case 1:
            if (g_seedState->memberSlot[0] != 0xFF) {
                idx = g_seedState->memberSlot[0];
                D_800DE4F0 = &D_80085224[idx];
                func_800B788C(&D_80085224[idx], eline);
            }
            if (g_seedState->memberSlot[2] != 0xFF) {
                idx = g_seedState->memberSlot[2];
                D_800DE4F4 = &D_80085224[idx];
                func_800B788C(&D_80085224[idx], eline);
            }
            break;
        case 2:
            if (g_seedState->memberSlot[0] != 0xFF) {
                idx = g_seedState->memberSlot[0];
                D_800DE4F0 = &D_80085224[idx];
                func_800B788C(&D_80085224[idx], eline);
            }
            if (g_seedState->memberSlot[1] != 0xFF) {
                idx = g_seedState->memberSlot[1];
                D_800DE4F4 = &D_80085224[idx];
                func_800B788C(&D_80085224[idx], eline);
            }
            break;
        }

        func_800B912C(eline, eline->field_0x24F);
        eline->flags |= 0x2000;
    } else {
        if (D_800DE4F0 != NULL && D_800DE4F0->msgState == 2
                && D_800DE4F0->field_0x24E != D_800DE4F0->field_0x24F) {
            func_800B912C(D_800DE4F0, D_800DE4F0->field_0x24F);
            D_800DE4F0->flags |= 0x8000;
        }
        if (D_800DE4F4 != NULL && D_800DE4F4->msgState == 2
                && D_800DE4F4->field_0x24E != D_800DE4F4->field_0x24F) {
            func_800B912C(D_800DE4F4, D_800DE4F4->field_0x24F);
            D_800DE4F4->flags |= 0x8000;
        }

        if (D_800DE4F0 == NULL || D_800DE4F0->msgState == 2) {
            if (D_800DE4F4 == NULL || D_800DE4F4->msgState == 2) {
                func_8009ECA4();
                if (D_800DE4F0) {
                    D_800DE4F0->flags &= ~1;
                    D_800DE4F0->msgActive = 0;
                }
                if (D_800DE4F4) {
                    D_800DE4F4->flags &= ~1;
                    D_800DE4F4->msgActive = 0;
                }
                return 2;
            }
        }
    }
    return 1;
}

/**
 * @brief Start a proximity-anchored message at an explicit XYZ position.
 *
 * Sister of @c func_800B788C, but takes the anchor coordinates directly
 * as args rather than via a target entity. Computes the planar distance
 * from @c eline to @c (x, y) (Q20.12 fixed-point), picks one of two
 * sound-channel scalings and one of two script-param byte offsets
 * (field_0x251 for "far" ≥ ~509 units, field_0x250 for "near") based on
 * the squared distance threshold @c 0x3F47F. Issues the chosen command
 * via @c func_800B912C, primes the message at the supplied position,
 * and sets flag @c 0x2000. Window ID is fixed to @c 8.
 *
 * @param eline Script context.
 * @param x     Anchor X (Q19.12 fixed-point).
 * @param y     Anchor Y (Q19.12 fixed-point).
 * @param z     Anchor Z (Q19.12 fixed-point).
 */
void func_800B7D44(Eline *eline, s32 x, s32 y, s32 z) {
    s32 dx, dy, distSq;

    dx = (x - eline->posX) / 4096;
    dy = (y - eline->posY) / 4096;
    dx = dx * dx;
    dy = dy * dy;
    dx = dx + dy;
    distSq = dx;

    if (distSq > 0x3F47F) {
        eline->savedChannel = (u32)(D_800704B2 * 25375) >> 6;
        func_800B912C(eline, eline->field_0x251);
    } else {
        eline->savedChannel = (u32)(D_800704B2 * 17255) >> 7;
        func_800B912C(eline, eline->field_0x250);
    }

    eline->msgActive = 1;
    eline->msgState = 0;
    eline->windowId = 8;
    eline->msgTextPtr = x;
    eline->msgPosX = y;
    eline->msgPosY = z;
    eline->flags |= 0x2000;
}

/**
 * @brief 3-party walk opcode handler.
 *
 * Pops 9 stack values (3 sets of XYZ coordinates), shifts each into Q19.12
 * fixed-point, and dispatches movement (via func_800B7D44) to up to 3 active
 * party-member entities looked up through g_seedState->memberSlot[0..2].
 * Sets the movement-active flag (bit 0) on each non-self entity. On
 * subsequent ticks (when the activeMask bit is clear), waits for all
 * dispatched entities to reach msgState == 2 (movement complete), then
 * clears msgActive and the active flag and returns 2.
 *
 * @param eline Pointer to the event line (script context).
 * @return 1 while still moving, 2 when all entities have completed.
 */
s32 func_800B7E78(Eline *eline) {
    s32 z2, y2, x2, z1, y1, x1, z0, y0, x0;
    Eline *e0, *e4, *e8;

    if ((eline->activeMask >> eline->scriptGroup) & 1) {
        D_800DE4F8 = 0;
        D_800DE4F4 = 0;
        D_800DE4F0 = 0;

        z2 = POP(eline) << 12;
        y2 = POP(eline) << 12;
        x2 = POP(eline) << 12;
        z1 = POP(eline) << 12;
        y1 = POP(eline) << 12;
        x1 = POP(eline) << 12;
        z0 = POP(eline) << 12;
        y0 = POP(eline) << 12;
        x0 = POP(eline) << 12;

        if (g_seedState->memberSlot[0] != 0xFF) {
            D_800DE4F0 = &D_80085224[g_seedState->memberSlot[0]];
            func_800B7D44(D_800DE4F0, x0, y0, z0);
            if (g_seedState->memberSlot[0] != eline->field_0x256) {
                D_800DE4F0->flags |= 1;
            }
        }

        if (g_seedState->memberSlot[1] != 0xFF) {
            D_800DE4F4 = &D_80085224[g_seedState->memberSlot[1]];
            func_800B7D44(D_800DE4F4, x1, y1, z1);
            if (g_seedState->memberSlot[1] != eline->field_0x256) {
                D_800DE4F4->flags |= 1;
            }
        }

        if (g_seedState->memberSlot[2] != 0xFF) {
            D_800DE4F8 = &D_80085224[g_seedState->memberSlot[2]];
            func_800B7D44(D_800DE4F8, x2, y2, z2);
            if (g_seedState->memberSlot[2] != eline->field_0x256) {
                D_800DE4F8->flags |= 1;
            }
        }

        return 1;
    }

    if (D_800DE4F0->msgState == 2 && D_800DE4F0->field_0x24E != D_800DE4F0->field_0x24F) {
        func_800B912C(D_800DE4F0, D_800DE4F0->field_0x24F);
        D_800DE4F0->flags |= 0x8000;
    }
    if (D_800DE4F4->msgState == 2 && D_800DE4F4->field_0x24E != D_800DE4F4->field_0x24F) {
        func_800B912C(D_800DE4F4, D_800DE4F4->field_0x24F);
        D_800DE4F4->flags |= 0x8000;
    }
    if (D_800DE4F8->msgState == 2 && D_800DE4F8->field_0x24E != D_800DE4F8->field_0x24F) {
        func_800B912C(D_800DE4F8, D_800DE4F8->field_0x24F);
        D_800DE4F8->flags |= 0x8000;
    }

    if (D_800DE4F0 == 0 || D_800DE4F0->msgState == 2) {
        if (D_800DE4F4 == 0 || D_800DE4F4->msgState == 2) {
            if (D_800DE4F8 == 0 || D_800DE4F8->msgState == 2) {
                e0 = D_800DE4F0;
                if (e0 != 0) {
                    e0->msgActive = 0;
                    e0->flags &= ~1;
                }
                e4 = D_800DE4F4;
                if (e4 != 0) {
                    e4->msgActive = 0;
                    e4->flags &= ~1;
                }
                e8 = D_800DE4F8;
                if (e8 != 0) {
                    e8->msgActive = 0;
                    e8->flags &= ~1;
                }
                return 2;
            }
        }
    }
    return 1;
}

/**
 * @brief Positioned-message opcode handler with stored arg.
 *
 * On the active frame, starts a new positioned message: writes
 * @c msgActive = 2, @c msgState = 0, then pops three values from the
 * bytecode stack — a halfword (window/format ID, stored to
 * @c field_0x1D8), and two s32 coords that are scaled into Q19.12 and
 * written to @c msgPosX and @c msgTextPtr. Finally stores the
 * dispatcher arg into @c field_0x1FC.
 *
 * On subsequent inactive frames, waits for @c msgState == 2; once
 * complete, clears @c msgActive and returns 3 (skip-to-next-entity).
 *
 * @param eline Script context.
 * @param a1    Opcode argument (stored as halfword to field_0x1FC).
 * @return 1 while running, 3 once the message has been read.
 */
s32 func_800B8344(Eline *eline, s32 a1) {
    if (!((eline->activeMask >> eline->scriptGroup) & 1)) {
        if (eline->msgState != 2) {
            return 1;
        }
        eline->msgActive = 0;
        return 3;
    }
    do {
        eline->msgActive = 2;
        eline->msgState = 0;
        eline->field_0x1D8 = POP(eline);
        eline->msgPosX = POP(eline) << 12;
        eline->msgTextPtr = POP(eline) << 12;
        eline->field_0x1FC = a1;
    } while (0);
    return 1;
}

/**
 * @brief Positioned-message opcode handler — 4-pop variant (full XYZ).
 *
 * Sister of @c func_800B8344 but pops an extra coordinate: pops one
 * halfword (window/format ID, stored to @c field_0x1D8) plus three
 * s32 values that become @c msgPosY, @c msgPosX, and @c msgTextPtr
 * (each scaled into Q19.12). Stores the dispatcher arg into
 * @c field_0x1FC. Same inactive-frame contract: wait for
 * @c msgState == 2 then clear @c msgActive and return 3.
 *
 * @param eline Script context.
 * @param a1    Opcode argument (stored as halfword to field_0x1FC).
 * @return 1 while running, 3 once the message has been read.
 */
s32 func_800B83FC(Eline *eline, s32 a1) {
    if (!((eline->activeMask >> eline->scriptGroup) & 1)) {
        if (eline->msgState != 2) {
            return 1;
        }
        eline->msgActive = 0;
        return 3;
    }
    do {
        eline->msgActive = 2;
        eline->msgState = 0;
        eline->field_0x1D8 = POP(eline);
        eline->msgPosY = POP(eline) << 12;
        eline->msgPosX = POP(eline) << 12;
        eline->msgTextPtr = POP(eline) << 12;
        eline->field_0x1FC = a1;
    } while (0);
    return 1;
}

/**
 * @brief Positioned-message opcode handler — anchor at a party member.
 *
 * Variant of the @c func_800B8344 / @c func_800B83FC pattern that
 * copies the anchor coordinates from a party-member entity instead
 * of taking them as args. Pops a window/format halfword (stored to
 * @c field_0x1D8) and a slot index, looks up the entity through
 * @c g_seedState->memberSlot[], and copies the entity's @c posX /
 * @c posY / @c posZ to @c msgTextPtr / @c msgPosX / @c msgPosY plus
 * @c field_0x1FA to @c field_0x1FC.
 *
 * @param eline Script context.
 * @return 1 while running, 3 once the message has been read.
 */
s32 func_800B84D8(Eline *eline) {
    if (!((eline->activeMask >> eline->scriptGroup) & 1)) {
        if (eline->msgState != 2) {
            return 1;
        }
        eline->msgActive = 0;
        return 3;
    }
    do {
        s32 m;
        eline->msgActive = 2;
        eline->msgState = 0;
        eline->field_0x1D8 = POP(eline);
        m = g_seedState->memberSlot[POP(eline)];
        eline->msgTextPtr = D_80085224[m].posX;
        eline->msgPosX = D_80085224[m].posY;
        eline->msgPosY = D_80085224[m].posZ;
        eline->field_0x1FC = D_80085224[m].field_0x1FA;
    } while (0);
    return 1;
}

/**
 * @brief Pop a word from the bytecode stack and store its low byte to
 *        @c D_800704A8.unk1AE.
 *
 * The @c volatile cast on the load is required to prevent gcc 2.7.2
 * from narrowing the @c lw to @c lbu — the target reads a full s32
 * even though only the low byte is stored.
 *
 * @param eline Script context.
 * @return 2 (advance PC).
 */
s32 func_800B85C8(Eline *eline) {
    D_800704A8.unk1AE = *(volatile s32 *)&POP(eline);
    return 2;
}

/**
 * @brief Positioned-message opcode handler — variant with animation
 *        kick and "msgActive = 3" semantics.
 *
 * On the active frame: sets @c msgActive = 3 (distinct from the
 * @c msgActive = 2 variants in @c func_800B8344 / @c func_800B83FC),
 * @c windowId = 1, @c msgState = 0; pops a signed halfword and
 * dispatches @c func_800B912C with it (animation trigger); marks
 * @c flags |= 0x2000; pops three coords (Q19.12) into @c msgPosY /
 * @c msgPosX / @c msgTextPtr; stores the opcode arg into @c field_0x1FC.
 *
 * On inactive frames: waits for @c msgState == 2 and returns 2 once
 * the message has been read (no return 3 here — the message stays
 * available rather than skipping the entity).
 *
 * @param eline Script context.
 * @param a1    Opcode argument (stored as halfword to field_0x1FC).
 * @return 1 while running, 2 once read.
 */
s32 func_800B85F8(Eline *eline, s32 a1) {
    if (!((eline->activeMask >> eline->scriptGroup) & 1)) {
        if (eline->msgState == 2) {
            eline->msgActive = 0;
            return 2;
        }
        return 1;
    }
    do {
        eline->msgActive = 3;
        eline->windowId = 1;
        eline->msgState = 0;
        func_800B912C(eline, (s16)POP(eline));
        eline->flags |= 0x2000;
        eline->msgPosY = POP(eline) << 12;
        eline->msgPosX = POP(eline) << 12;
        eline->msgTextPtr = POP(eline) << 12;
        eline->field_0x1FC = a1;
    } while (0);
    return 1;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object7", func_800B8710);

/**
 * @brief 9-pop positioned-message handler — sets up message position
 *        plus three "saved" coords and three additional unk slots.
 *
 * On the active frame: @c msgActive = 4 (distinct from sibling
 * handlers), @c windowId = 1, @c msgState = 0; then pops nine s32
 * values, each scaled into Q19.12, into a contiguous region of the
 * Eline:
 *   - @c msgPosY  / @c msgPosX  / @c msgTextPtr   (current msg coords)
 *   - @c field_0x1C8 / @c field_0x1C4 / @c field_0x1C0 (saved msg coords)
 *   - @c unk1B0 / @c unk1AC / @c unk1A8 (three extra slots)
 * and finally stores the dispatcher arg into @c field_0x1FC.
 *
 * On inactive frames: wait for @c msgState == 2 and return 2 once
 * the message has been read.
 *
 * @param eline Script context.
 * @param a1    Opcode argument (stored as halfword to field_0x1FC).
 * @return 1 while running, 2 once read.
 */
s32 func_800B8824(Eline *eline, s32 a1) {
    if (!((eline->activeMask >> eline->scriptGroup) & 1)) {
        if (eline->msgState == 2) {
            eline->msgActive = 0;
            return 2;
        }
        return 1;
    }
    do {
        eline->msgActive = 4;
        eline->windowId = 1;
        eline->msgState = 0;
        eline->msgPosY = POP(eline) << 12;
        eline->msgPosX = POP(eline) << 12;
        eline->msgTextPtr = POP(eline) << 12;
        eline->field_0x1C8 = POP(eline) << 12;
        eline->field_0x1C4 = POP(eline) << 12;
        eline->field_0x1C0 = POP(eline) << 12;
        eline->unk1B0 = POP(eline) << 12;
        eline->unk1AC = POP(eline) << 12;
        eline->unk1A8 = POP(eline) << 12;
        eline->field_0x1FC = a1;
    } while (0);
    return 1;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object7", func_800B89C0);

/**
 * @brief Pop three halfwords and broadcast each to a pair of fields.
 *
 * Pops three u16 values from the bytecode stack and stores each one to
 * two adjacent halfword fields (current+target pattern for animation
 * interpolation): @c (1EC,1EE), @c (1E6,1E8), and @c (1E0,1E2). Also
 * stores the dispatcher arg into @c field_0x1FC, clears @c unk245 /
 * @c msgState / @c field_0x1F4. Leaf function.
 *
 * @param eline Script context.
 * @param a1    Opcode argument (stored as halfword to field_0x1FC).
 * @return 2 (advance PC).
 */
s32 func_800B8B58(Eline *eline, s32 a1) {
    do {
        u16 a, b, c;
        a = POP(eline);
        eline->field_0x1EC = a;
        eline->field_0x1EE = a;
        b = POP(eline);
        eline->field_0x1E6 = b;
        eline->field_0x1E8 = b;
        c = POP(eline);
        eline->field_0x1FC = a1;
        eline->unk245 = 0;
        eline->msgState = 0;
        eline->field_0x1F4 = 0;
        eline->field_0x1E0 = c;
        eline->field_0x1E2 = c;
    } while (0);
    return 2;
}

/**
 * @brief Pop seven halfwords into animation fields with @c unk245 = 1.
 *
 * Sister of @c func_800B8B58 but with one more parameter direction:
 * sets @c unk245 = 1, pops seven u16 values into @c field_0x1F2,
 * @c 1F0, @c 1EA, @c 1E4, @c 1EE, @c 1E8, @c 1E2 (last), and zeros
 * @c field_0x1F4. Leaf function.
 *
 * @param eline Script context.
 * @param a1    Ignored.
 * @return 2 (advance PC).
 */
s32 func_800B8BE0(Eline *eline, s32 a1) {
    do {
        eline->unk245 = 1;
        eline->field_0x1F2 = POP(eline);
        eline->field_0x1F0 = POP(eline);
        eline->field_0x1EA = POP(eline);
        eline->field_0x1E4 = POP(eline);
        eline->field_0x1EE = POP(eline);
        eline->field_0x1E8 = POP(eline);
        eline->field_0x1E2 = POP(eline);
        eline->field_0x1F4 = 0;
    } while (0);
    return 2;
}

/**
 * @brief Pop seven halfwords into animation fields with @c unk245 = 2.
 *
 * Identical to @c func_800B8BE0 except @c unk245 is set to @c 2 (a
 * different sub-mode marker). Same pop sequence and field layout.
 *
 * @param eline Script context.
 * @param a1    Ignored.
 * @return 2 (advance PC).
 */
s32 func_800B8CD4(Eline *eline, s32 a1) {
    do {
        eline->unk245 = 2;
        eline->field_0x1F2 = POP(eline);
        eline->field_0x1F0 = POP(eline);
        eline->field_0x1EA = POP(eline);
        eline->field_0x1E4 = POP(eline);
        eline->field_0x1EE = POP(eline);
        eline->field_0x1E8 = POP(eline);
        eline->field_0x1E2 = POP(eline);
        eline->field_0x1F4 = 0;
    } while (0);
    return 2;
}

/**
 * @brief Animation handler — rotate current XY/Z fields into "saved"
 *        slots and pop new values in (unk245 = 1 variant).
 *
 * Sister of @c func_800B8E74 with @c unk245 = 1. Same shape and
 * matching scaffold — see @c func_800B8E74 for the reg-alloc trick.
 *
 * @param eline Script context.
 * @param a1    Ignored.
 * @return 2 (advance PC).
 */
s32 func_800B8DC8(Eline *eline, s32 a1) {
    s32 s1E4 = eline->field_0x1E4;
    s32 s1F0 = eline->field_0x1F0;
    s32 s1EA;
    eline->unk245 = 1;
    eline->field_0x1F2 = POP(eline);
    s1EA = eline->field_0x1EA;
    eline->field_0x1E2 = s1E4;
    eline->field_0x1EE = s1F0;
    eline->field_0x1E8 = s1EA;
    s1EA = (s8)(eline->stackPtr--);
    eline->field_0x1F0 = ((s32 *)eline)[s1EA];
    s1F0 = (s8)(eline->stackPtr--);
    eline->field_0x1EA = ((s32 *)eline)[s1F0];
    eline->field_0x1E4 = POP(eline);
    eline->field_0x1F4 = 0;
    return 2;
}

/**
 * @brief Animation handler — rotate current XY/Z fields into "saved"
 *        slots and pop new values in.
 *
 * Sister of @c func_800B8DC8 (which uses @c unk245 = 1). Pre-reads
 * the current @c field_0x1E4 / @c 1F0 / @c 1EA into temporaries,
 * stores them into the "saved" trio (@c 1E2 / @c 1EE / @c 1E8),
 * then pops four fresh values into @c 1F2 / @c 1F0 / @c 1EA /
 * @c 1E4 and clears @c field_0x1F4. Sets @c unk245 = 2.
 *
 * The middle two POPs are split into "capture stackPtr index, then
 * index the array" — this forces gcc 2.7.2 to keep @c a1 / @c a2 in
 * the order the target wants (a2 = saved_1E4, a1 = saved_1F0). The
 * normal inline POP form makes gcc swap them.
 *
 * @param eline Script context.
 * @param a1    Ignored.
 * @return 2 (advance PC).
 */
s32 func_800B8E74(Eline *eline, s32 a1) {
    s32 s1E4 = eline->field_0x1E4;
    s32 s1F0 = eline->field_0x1F0;
    s32 s1EA;
    eline->unk245 = 2;
    eline->field_0x1F2 = POP(eline);
    s1EA = eline->field_0x1EA;
    eline->field_0x1E2 = s1E4;
    eline->field_0x1EE = s1F0;
    eline->field_0x1E8 = s1EA;
    s1EA = (s8)(eline->stackPtr--);
    eline->field_0x1F0 = ((s32 *)eline)[s1EA];
    s1F0 = (s8)(eline->stackPtr--);
    eline->field_0x1EA = ((s32 *)eline)[s1F0];
    eline->field_0x1E4 = POP(eline);
    eline->field_0x1F4 = 0;
    return 2;
}

/**
 * @brief Return 2 if @c unk245 == 3, otherwise return 1.
 *
 * Probes the animation sub-mode byte set by the sibling opcode
 * handlers (@c func_800B8BE0 / @c func_800B8CD4 / @c func_800B8DC8 /
 * @c func_800B8E74). Used by scripts to wait until the animation
 * reaches sub-mode 3.
 *
 * @param eline Script context.
 * @return 2 if @c unk245 == 3 (advance), 1 otherwise (yield).
 */
s32 func_800B8F20(Eline *eline) {
    if (eline->unk245 == 3) {
        return 2;
    }
    return 1;
}

/**
 * Sets the global byte D_8007064C to 1, returns 2.
 *
 * @param a0 Pointer to the script/object structure (unused).
 * @return 2 (continue processing).
 */
s32 func_800B8F3C(u8 *a0) {
    D_8007064C = 1;
    return 2;
}

/**
 * Clears the global byte D_8007064C, returns 2.
 *
 * @param a0 Pointer to the script/object structure (unused).
 * @return 2 (continue processing).
 */
s32 func_800B8F50(u8 *a0) {
    D_8007064C = 0;
    return 2;
}

/**
 * Calls func_8009E660 and returns 2.
 *
 * @param a0 Pointer to the script/object structure.
 * @return 2 (continue processing).
 */
s32 func_800B8F60(u8 *a0) {
    func_8009E660(a0);
    return 2;
}

/** @brief Compare D_800704A8 entries. Returns 1 if different, 2 if same. */
s32 func_800B8F80(u8 *a0) {
    u8 *base = (u8 *)&D_800704A8;
    u16 val1 = *(u16 *)(base + 0x106);
    u16 val2 = *(u16 *)(base + 0x104);
    if (val1 == val2) {
        return 2;
    }
    return 1;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object7", func_800B8FA8);

/** @brief Pop byte from stack and store to offset 0x240. Returns 2. */
s32 func_800B9000(u8 *a0) {
    u8 idx = *(u8 *)(a0 + 0x184);
    *(u8 *)(a0 + 0x184) = idx - 1;
    *(u8 *)(a0 + 0x240) = *(u8 *)(a0 + (s8)idx * 4);
    return 2;
}

INCLUDE_ASM("asm/field/nonmatchings/fe_object7", func_800B9030);
