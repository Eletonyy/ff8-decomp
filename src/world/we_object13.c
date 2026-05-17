#include "common.h"
#include "gamestate.h"
#include "battle.h"
#include "field.h"
#include "sound.h"
#include "psxsdk/libcd.h"
#include "psxsdk/libetc.h"

/** Initializes the CD subsystem: clears callbacks and pauses the drive. */
void func_800C40F8(void) {
    CdSyncCallback(NULL);
    CdReadyCallback(NULL);
    CdControl(9, NULL, NULL);
}

/**
 * @brief Multi-sector CD streaming controller.
 *
 * Drives the block-based read pipeline used by the world engine for streaming
 * data off disc. @c buffers points at a NULL-terminated array of destination
 * buffers (one per buffer slot); @c blocksPerBuf is the per-buffer block
 * count (reloaded into @c remaining when advancing to the next buffer). Each
 * tick decrements @c remaining while reading the @c blockIdx-th block of the
 * current buffer.
 */
typedef struct StreamState {
    u8 *volatile *buffers;       /* 0x00: NULL-terminated array of destination buffers (slots are volatile pointers) */
    u32 pad_04;
    volatile s32 blocksPerBuf;   /* 0x08: blocks to read into each buffer (reload for remaining) */
    u32 pad_0C;
    volatile s32 remaining;      /* 0x10: blocks remaining in current buffer */
    volatile s32 field_14;       /* 0x14 */
    volatile s32 field_18;       /* 0x18: VSync() snapshot */
    volatile s32 expectedSeq;    /* 0x1C: expected sequence number */
    volatile s32 blockIdx;       /* 0x20: current block index within buffer */
    volatile u8 bufIdx;          /* 0x24: current buffer index */
    volatile u8 status;          /* 0x25: status flag (0 idle, 2 error/done) */
} StreamState;

extern StreamState D_800E3E70;
extern void (*D_800E3E60)(s32, void *);
extern SeedState *g_seedState;

extern void func_80047C3C(u8 *msg);
extern u8   D_800987C0;

/**
 * @brief Per-frame tick of the multi-sector CD streaming engine.
 *
 * On @p event == 1: services the in-flight read pipeline. While
 * @c D_800E3E70.remaining is positive, pulls the just-finished sector's
 * header via @c CdGetSector and validates the decoded sequence number
 * (from @c CdPosToInt) against @c expectedSeq. On mismatch, logs
 * "CdRead: sector error" and aborts the buffer by setting @c remaining to
 * -1. After verify, if @c remaining is still positive, queues the next
 * block read at @c buffers[bufIdx] + blockIdx*0x800 (0x200 words = one
 * sector), decrements @c remaining, and advances the sequence/block indices.
 *
 * On any other @p event: aborts the in-flight buffer by setting
 * @c remaining to -1.
 *
 * Trailing bookkeeping: if @c remaining went negative, transitions
 * @c status to 2 (error/done). If @c remaining hit zero, advances
 * @c bufIdx; if the next slot is @c NULL the stream is exhausted so it
 * resets the reader via @c func_800C4450 and fires the registered
 * completion callback with @c (2, ctx). Otherwise reloads @c remaining from
 * @c blocksPerBuf and starts the next buffer.
 */
void func_800C4130(u8 event, void *ctx) {
    if (event == 1) {
        StreamState *s = &D_800E3E70;
        u8 header[0x10];

        if (s->remaining > 0) {
            CdGetSector(header, 3);
            if (CdPosToInt((CdlLOC *)header) != s->expectedSeq) {
                func_80047C3C(&D_800987C0);
                s->remaining = -1;
            }
            if (s->remaining > 0) {
                CdGetSector(s->buffers[s->bufIdx] + s->blockIdx * 0x800, 0x200);
                s->remaining--;
                s->expectedSeq++;
                s->blockIdx++;
            }
        }
    } else {
        D_800E3E70.remaining = -1;
    }

    if (D_800E3E70.remaining < 0) {
        D_800E3E70.status = 2;
    } else if (D_800E3E70.remaining == 0) {
        D_800E3E70.bufIdx++;
        if (D_800E3E70.buffers[D_800E3E70.bufIdx] == NULL) {
            func_800C4450();
            if (D_800E3E60 != NULL) {
                D_800E3E60(2, ctx);
            }
        } else {
            D_800E3E70.blockIdx = 0;
            D_800E3E70.remaining = D_800E3E70.blocksPerBuf;
        }
    }
}

/**
 * @brief Begin streaming the next CD buffer (or restart after seek).
 *
 * Disables any in-flight read by clearing both CD callbacks, checks the CD
 * status, and either re-seeks to the saved sequence number (when @p doSeek
 * is set) or resumes from the drive's current position. Installs
 * @c func_800C4130 as the ready callback and kicks off a mode-6
 * multi-sector read.
 *
 * @param doSeek When non-zero, re-seek to the saved @c expectedSeq before
 *               starting; when zero, start from the drive's current spot.
 * @return Number of blocks queued in the new buffer (@c blocksPerBuf -
 *         @c blockIdx); @c -1 if the CD subsystem reported an error.
 */
s32 func_800C42D8(s32 doSeek) {
    u8 stackBuf[16];

    CdSyncCallback(NULL);
    CdReadyCallback(NULL);

    if (CdStatus() & 0x10) {
        CdControlF(1, NULL);
        D_800E3E70.field_18 = VSync(-1);
        D_800E3E70.remaining = -1;
        return D_800E3E70.remaining;
    }

    if (doSeek != 0) {
        u8 *pPos = &stackBuf[8];
        CdControl(9, NULL, NULL);
        CdIntToPos(D_800E3E70.expectedSeq, (CdlLOC *)pPos);
        if (CdControl(2, pPos, NULL) == 0) {
            D_800E3E70.remaining = -1;
            return D_800E3E70.remaining;
        }
    }

    CdFlush();
    stackBuf[0] = 0xA0;
    if (CdMode() != 0xA0) {
        if (CdControl(0xE, &stackBuf[0], NULL) == 0) {
            D_800E3E70.remaining = -1;
            return D_800E3E70.remaining;
        }
    }

    D_800E3E70.expectedSeq = CdPosToInt((CdlLOC *)CdLastPos());
    CdReadyCallback((CdlCB)func_800C4130);
    CdControlF(6, NULL);
    D_800E3E70.remaining = D_800E3E70.blocksPerBuf - D_800E3E70.blockIdx;
    D_800E3E70.field_14 = VSync(-1);
    return D_800E3E70.remaining;
}

/** Resets playback state and reinitializes subsystem.
 * @return Always returns 1.
 */
s32 func_800C4450(void) {
    D_800E3E70.remaining = 0;
    D_800E3E70.status = 0;
    func_800C40F8();
    return 1;
}

/**
 * @brief Initialize the multi-sector streaming engine and start the first
 *        buffer.
 *
 * Initializes the @c StreamState header from the caller's arguments and
 * kicks off the first read via @c func_800C42D8.
 *
 * @param startSeq      Starting sector sequence number (logical block).
 * @param blocksPerBuf  Blocks to read into each buffer slot (reload value
 *                      for @c remaining when advancing).
 * @param buffers       NULL-terminated array of destination buffer pointers.
 * @return @c 1 if streaming kicked off successfully, @c 0 on failure.
 *
 * @verbatim
 * s32 func_800C4480(s32 startSeq, s32 blocksPerBuf, u8 **buffers) {
 *     if (CdSync(1, NULL) == 0)
 *         return 0;
 *
 *     D_800E3E70.buffers = buffers;
 *     D_800E3E70.blocksPerBuf = blocksPerBuf;
 *     D_800E3E70.bufIdx = 0;
 *     D_800E3E70.expectedSeq = startSeq;
 *     D_800E3E70.remaining = 0;
 *     D_800E3E70.field_14 = 0;
 *     D_800E3E70.field_18 = VSync(-1);
 *     D_800E3E70.blockIdx = 0;
 *     D_800E3E70.status = 1;
 *     CdSyncCallback(NULL);
 *     CdReadyCallback(NULL);
 *     if (CdStatus() & 0xC0)
 *         CdControlB(9, NULL, NULL);
 *
 *     return func_800C42D8(1) > 0;
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object13", func_800C4480);

/**
 * @brief Query streaming status; optionally block until the current buffer
 *        completes.
 *
 * @param nowait When non-zero, return immediately with the current state;
 *               when zero, busy-wait until the buffer queue drains or
 *               @c status leaves the @c streaming state.
 * @retval  1   Streaming, current buffer slot still active.
 * @retval  0   Idle, or current buffer slot drained.
 * @retval -1   Error / reset state, or @c status changed mid-wait.
 *
 * @verbatim
 * s32 func_800C4558(s32 nowait) {
 *     StreamState *s = &D_800E3E70;
 *     s32 status = s->status;
 *
 *     if (status != 1) {
 *         if (status < 2)  return 0;
 *         if (status == 2) return -1;
 *         return 0;
 *     }
 *     if (nowait != 0) {
 *         if (s->buffers[s->bufIdx] == NULL) return 0;
 *         return 1;
 *     }
 *     if (s->buffers[s->bufIdx] == NULL) return 0;
 *     while (s->status == 1) {
 *         if (s->buffers[s->bufIdx] == NULL) return 0;
 *     }
 *     return -1;
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object13", func_800C4558);

/** Exchanges the callback pointer, returning the previous value.
 * @param newVal New callback value to store.
 * @return Previous callback value.
 */
s32 func_800C4634(s32 newVal) {
    s32 old = (s32)D_800E3E60;
    D_800E3E60 = (void (*)(s32, void *))newVal;
    return old;
}

/** Clears all playback state and callback pointer. */
void func_800C4644(void) {
    D_800E3E60 = NULL;
    D_800E3E70.buffers = NULL;
    D_800E3E70.pad_04 = 0;
    D_800E3E70.blocksPerBuf = 0;
    D_800E3E70.pad_0C = 0;
    D_800E3E70.remaining = 0;
    D_800E3E70.field_14 = 0;
    D_800E3E70.field_18 = 0;
    D_800E3E70.expectedSeq = 0;
    D_800E3E70.blockIdx = 0;
    D_800E3E70.bufIdx = 0;
    D_800E3E70.status = 0;
}

/**
 * @brief World-engine SeeD level-up tick — same logic as the field_engine
 *        @c updateSeedLevel (@c func_800BD3A8).
 *
 * Snapshots @c seedExp, sums kill counts across the 8 character slots,
 * adjusts @c seedExp by @c (totalKills - prevKillSum) - 10 (clamped to
 * @c [100, 3100]), then pays salary into @c gil from the per-rank
 * salary table at @c g_seedSalaryTable[level] (capped at 99,999,999
 * gil). When neither @c stateFlags bit @c 0x10 nor bit @c 0x1000 is
 * set, fires the level-up notification (@c func_800316D4 + 3 rank-up
 * SFX). Stores @c totalKills as the new @c prevKillSum.
 */
void func_800C4688(void) {
    s16 totalKills = 0;
    s32 i;
    s32 salary;

    g_seedState->prevSeedExp = g_seedState->seedExp;

    for (i = 0; i < 8; i++)
        totalKills += g_gameState.chars[i].kills;

    g_seedState->seedExp = totalKills - g_seedState->prevKillSum - 10 + g_seedState->seedExp;

    if ((s16)g_seedState->seedExp < 100)
        g_seedState->seedExp = 100;
    else if ((s16)g_seedState->seedExp >= 0xC1C)
        g_seedState->seedExp = 0xC1C;

    /* Reg-allocation hack: this dead lookup keeps `i`'s register binding
       alive long enough that gcc 2.7.2 places `salary` in the same slot
       the original toolchain used. Without it, the allocator picks one
       register higher and the byte-match is lost. */
    i = g_seedSalaryTable[((s16)g_seedState->seedExp) / 100];
    salary = g_seedSalaryTable[(s16)g_seedState->seedExp / 100];
    g_gameState.mainData.party.gil += salary * 10;
    if (g_gameState.mainData.party.gil > 0x5F5E0FEu)
        g_gameState.mainData.party.gil = 0x5F5E0FF;

    if (!(g_seedState->stateFlags & 0x0010)) {
        if (!(g_seedState->stateFlags & 0x1000)) {
            s32 oldLevel = (s16)g_seedState->prevSeedExp / 100;
            s32 newLevel = (s16)g_seedState->seedExp / 100;

            func_800316D4(oldLevel, newLevel,
                          g_seedSalaryTable[oldLevel] * 10,
                          g_seedSalaryTable[newLevel] * 10);

            g_seedState->levelUpDisplayTimer = 150;
            sndPlaySfx(0x5B, 0, 0x80, 0x7F);
            sndPlaySfx(0x5C, 0, 0x80, 0x7F);
            sndPlaySfx(0x5D, 0, 0x80, 0x7F);
        }
    }

    g_seedState->prevKillSum = totalKills;
}

/**
 * @brief Tick all active GFs' HP up by 1 toward their battle max.
 *
 * Iterates the 16 GF save entries. For each unlocked GF (@c exists bit 0)
 * with non-zero HP, if the current HP is below the GF's battle max (at
 * @c g_battleChars.gfEntries[i].hp), it's incremented by 1. Likely runs
 * once per field step to slowly regenerate GF HP while walking.
 *
 * @note Match requires reading @c g_gameState.gfs[i].hp directly at the
 *       compare AND increment — not through the cached @c hp local.
 */
void func_800C48C0(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (g_gameState.gfs[i].exists & 1) {
            u16 hp = g_gameState.gfs[i].hp;
            if (hp != 0) {
                if (g_gameState.gfs[i].hp < (s16)g_battleChars.gfEntries[i].hp) {
                    g_gameState.gfs[i].hp = g_gameState.gfs[i].hp + 1;
                }
            }
        }
    }
}

/**
 * @brief Tick each active party member's HP up by 1 toward their battle regen cap.
 *
 * Iterates the 3 party slots. For each occupied slot with its field-status
 * bit 0 set, increments the character's @c currentHp by 1 as long as it
 * stays below the slot's @c hpRegenCap (stored in the BattleCharData at
 * offset 0x174). Paired with @c func_800C48C0 which does the same for GF HP.
 *
 * @note Match requires reading @c g_gameState.mainData.party.party[i] and
 *       @c g_gameState.chars[charIdx].currentHp directly at each use site
 *       (not through cached locals) — identical pattern to func_800C48C0.
 */
void func_800C492C(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        u8 charIdx = g_gameState.mainData.party.party[i];
        if (charIdx != 0xFF) {
            if (g_battleChars.chars[i].fieldStatusByte & 1) {
                u16 hp = g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp;
                if (hp != 0) {
                    if ((s32)g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp <
                        (s32)g_battleChars.chars[i].hpRegenCap) {
                        g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp =
                            g_gameState.chars[g_gameState.mainData.party.party[i]].currentHp + 1;
                    }
                }
            }
        }
    }
}

/**
 * @brief Tick the currently-learning Angelo trick's points; unlock + play SFX at 0.
 *
 * When Rinoa (CHAR_RINOA = 4) is in the battle party, decrements the
 * learning-points counter for the currently-learning Angelo trick (index
 * at @c mainData.party.trickLearning). When the counter reaches 0 and the
 * trick isn't already completed, sets the completed bit and plays the
 * learn SFX (0x83). Paired with other field-tick helpers.
 */
void func_800C49CC(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_gameState.battleParty[i] == CHAR_RINOA) {
            u8 idx = g_gameState.mainData.party.trickLearning;
            if (g_gameState.mainData.limitBreaks.angeloPoints[idx] != 0) {
                g_gameState.mainData.limitBreaks.angeloPoints[idx]--;
            } else {
                u8 mask = g_gameState.mainData.limitBreaks.angeloCompleted;
                if (!((mask >> idx) & 1)) {
                    g_gameState.mainData.limitBreaks.angeloCompleted = mask | (1 << idx);
                    sndPlaySfx(0x83, 0, 0x80, 0x7F);
                }
            }
            return;
        }
    }
}

/** Scans all 256 entries and processes active ones. */
void func_800C4A74(void) {
    s32 i;
    s32 result;
    for (i = 0; i < 256; i++) {
        result = getPackedField2Bit(i) - 1;
        if ((u32)result < 2) {
            if (fieldRandom() & 1) {
                func_800383B8(i, result);
            }
        }
    }
}

extern s32 D_80082C14;

/**
 * @brief Per-step world-engine tick — duplicate of @c func_800BD804 from
 *        the field engine.
 *
 * Drives every per-step accumulator on the field state for the world map:
 *   - @c stepCounter (mirrored to @c D_80082C14).
 *   - @c packedFlagsStepAcc — fires @c func_800C4A74 every @c 0x2800 steps.
 *   - @c hpRegenStepAcc — fires GF and party HP regen every @c 8 steps.
 *   - @c seedExpStepAcc — fires the SeeD level-up tick every @c 0x6000 steps,
 *     then clamps @c seedExp to @c [100, 0xC1C].
 *   - @c levelUpDisplayTimer — counts down each step; fires
 *     @c setTransitionPhase7 the frame it reaches @c 0.
 *   - @c angeloLearnStepAcc — fires the Angelo trick learn tick every
 *     @c 0x250 steps.
 *
 * The HP regen, SeeD level-up, timer and Angelo branches each gate on
 * @c D_8007809A or @c stateFlags bits.
 *
 * @param stepDelta  Step delta to add to all accumulators this tick.
 *
 * @verbatim
 * void func_800C4AE4(s32 stepDelta) {
 *     g_seedState->hpRegenStepAcc += stepDelta;
 *     g_seedState->stepCounter += stepDelta;
 *     g_seedState->angeloLearnStepAcc += stepDelta;
 *     g_seedState->packedFlagsStepAcc = (u16)(g_seedState->packedFlagsStepAcc + stepDelta);
 *     D_80082C14 = g_seedState->stepCounter;
 *     if (g_seedState->packedFlagsStepAcc >= 0x2800u) {
 *         g_seedState->packedFlagsStepAcc = 0;
 *         func_800C4A74();
 *     }
 *     if (g_seedState->hpRegenStepAcc >= 8) {
 *         g_seedState->hpRegenStepAcc = 0;
 *         func_800C48C0();
 *         func_800C492C();
 *     }
 *     if (D_8007809A & 1) return;
 *     if (!(g_seedState->stateFlags & 8)) {
 *         g_seedState->seedExpStepAcc += stepDelta;
 *         if ((u32)g_seedState->seedExpStepAcc >= 0x6000u) {
 *             g_seedState->seedExpStepAcc = 0;
 *             func_800C4688();
 *         }
 *         if ((s16)g_seedState->seedExp < 100)         g_seedState->seedExp = 100;
 *         else if ((s16)g_seedState->seedExp >= 0xC1C) g_seedState->seedExp = 0xC1C;
 *     }
 *     if ((s16)g_seedState->levelUpDisplayTimer >= 0) {
 *         if ((s16)g_seedState->levelUpDisplayTimer == 0) setTransitionPhase7();
 *         g_seedState->levelUpDisplayTimer--;
 *     }
 *     if (D_8007809A & 0x10) return;
 *     if ((u32)g_seedState->angeloLearnStepAcc >= 0x250u) {
 *         g_seedState->angeloLearnStepAcc = 0;
 *         func_800C49CC();
 *     }
 * }
 * @endverbatim
 */
INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object13", func_800C4AE4);
