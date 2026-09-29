#include "common.h"
#include "battle.h"
#include "btl_anim.h"
#include "input/vibration.h"

/* --- Externs (sorted by address) --- */

extern s32 g_battleTimer; /* 0x80083750 — battle timer */
extern s32 D_80083870; /* 0x80083870 — peak stream 0 value of the running commands */
extern s32 D_80083874; /* 0x80083874 — peak stream 1 value of the running commands */
extern BattleCmdEntry g_battleCmdTable[]; /* 0x80083878 — battle command entries (4 × 0x24) */

/* --- Private functions --- */

static BattleCmdEntry *getBattleCmdTable(void);
static BattleCmdEntry *findBestBattleCmd(s32 threshold);
static s32 func_80030A54(CmdStream *stream);

/**
 * @brief Get the battle command table.
 * @return Pointer to the battle command entry array.
 */
static BattleCmdEntry *getBattleCmdTable(void) {
    return g_battleCmdTable;
}

/**
 * @brief Find the best available battle command entry.
 *
 * Scans the 4-entry battle command table for a free or low-priority slot.
 * If any entry has active == 0, returns it immediately (first-fit).
 * Otherwise, returns the entry with the lowest active value that is
 * still <= threshold. Returns NULL if no suitable entry is found.
 *
 * @param threshold Maximum active value to consider as a candidate.
 * @return Pointer to the best entry, or NULL if none found.
 */
static BattleCmdEntry *findBestBattleCmd(s32 threshold) {
    BattleCmdEntry* ptr;
    s32 best;
    s32 i;

    ptr = getBattleCmdTable();
    ptr++; ptr--; /* Regalloc: boost ptr priority */
    best = 0xFF;

    for (i = 0; i < 4; i++, ptr++) {
        if (ptr->active == 0) {
            return ptr;
        }
        if (threshold >= ptr->active && ptr->active < best) {
            best = i;
        }
    }

    if (best != 0xFF) {
        return &getBattleCmdTable()[best];
    }
    return NULL;
}


/**
 * @brief Check if any battle command entry is active.
 *
 * Scans the 4-entry battle command table. Returns 1 immediately if any
 * entry has a non-zero active flag, or 0 if all are inactive.
 *
 * @return 1 if any entry is active, 0 otherwise.
 */
s32 isAnyBattleCmdActive(void) {
    BattleCmdEntry* ptr = getBattleCmdTable();
    s32 i;

    for (i = 0; i < 4; i++, ptr++) {
        if (ptr->active != 0) {
            return 1;
        }
    }
    return 0;
}


/**
 * @brief Check if a battle command matches the expected source entity.
 *
 * Returns 0 if @p cmd is zero. Otherwise, looks up the entry at index
 * (cmd & 3), checks if it is active, then compares its sourceId against
 * (cmd >> 4).
 *
 * @param cmd Packed command: bits [1:0] = entry index, bits [15:4] = source ID.
 * @return 1 if active and source matches, 0 otherwise.
 */
s32 checkBattleCmdSource(s32 cmd) {
    BattleCmdEntry *base;
    BattleCmdEntry *entry;

    if (cmd == 0) {
        return 0;
    }
    base = getBattleCmdTable();
    entry = &base[cmd & 3];
    if (entry->active != 0) {
        if (entry->sourceId == (cmd >> 4)) {
            return 1;
        }
    }
    return 0;
}


/**
 * @brief Deactivate a battle command entry or clear all entries.
 *
 * If @p id is -1, clears all 4 entries' active flag and resets anim
 * entity params. Otherwise, validates the command via checkBattleCmdSource
 * and clears just that entry's active flag.
 *
 * @param id Packed command identifier, or -1 to clear all.
 */
void deactivateBattleCmd(s32 id) {
    BattleCmdEntry* ptr = getBattleCmdTable();
    s32 i;

    if (id == -1) {
        for (i = 0; i < 4; i++, ptr++) {
            ptr->active = 0;
        }
        setAnimEntityParams(0, 0, 0);
    } else {
        if (checkBattleCmdSource(id)) {
            ptr += id & 3;
            ptr->active = 0;
        }
    }
}


/**
 * @brief Load a command from a packed data block into a battle command entry.
 *
 * Finds a free or low-priority command slot via findBestBattleCmd, then
 * loads stream data from the packed block at @p data. The block contains
 * an offset table followed by variable-length stream pairs. Each pair
 * has a CmdStreamHeader (two u16 lengths) followed by the raw data.
 *
 * @param data Pointer to packed command data block (offset table + streams).
 * @param idx Index into the offset table.
 * @param priority Priority value for the command slot.
 * @return Packed command ID (sourceId << 4 | index), 0 if no slot, -1 if no data.
 */
s32 loadBattleCmd(u8 *data, s32 idx, s32 priority) {
    BattleCmdEntry *cmd;
    s32 offset;
    CmdStreamHeader *hdr;
    u16 len1, len2;
    u8 *base;
    u8 *block2;

    cmd = findBestBattleCmd(priority);
    if (cmd == NULL) {
        return 0;
    }

    offset = ((s32 *)data)[idx];
    data += offset;
    if (offset == 0) {
        return -1;
    }

    hdr = (CmdStreamHeader *)data;
    len1 = hdr->len1;
    len2 = hdr->len2;
    base = hdr->data;
    block2 = hdr->data + len1;

    if (len1 != 0) {
        cmd->streams[0].start = base;
        cmd->streams[0].end = base + len1;
        cmd->streams[0].cursor = -1;
        cmd->streams[0].enabled = 1;
        cmd->streams[0].length = len1;
    } else {
        cmd->streams[0].enabled = 0;
    }

    if (len2 != 0) {
        cmd->streams[1].start = block2;
        cmd->streams[1].end = block2 + len2;
        cmd->streams[1].cursor = -1;
        cmd->streams[1].enabled = 1;
        cmd->streams[1].length = len2;
    } else {
        cmd->streams[1].enabled = 0;
    }

    cmd->active = priority;
    cmd->sourceId++;
    if (cmd->sourceId >= 0x400) {
        cmd->sourceId = 1;
    }

    return (cmd->sourceId << 4) | cmd->index;
}


/**
 * @brief Read and interpolate the next value from a command stream.
 *
 * Reads keyframe pairs (value, duration) from the stream. Linearly
 * interpolates between the current value and the next over the
 * duration, advances the cursor each call, and moves to the next
 * keyframe pair when the duration expires. Returns the interpolated
 * result doubled and clamped to 0-255, or -1 if the stream is
 * disabled or exhausted (0xFF duration marker).
 *
 * Stream format: [val0][dur0][val1][dur1]...[0xFF]
 *
 * @param stream Pointer to a CmdStream.
 * @return Interpolated value (0-255), or -1 if stream ended.
 * @see https://decomp.me/scratch/oOOHt
 */
static s32 func_80030A54(CmdStream *stream) {
    u8 duration;
    u8 *ptr;
    u8 *end;

    s32 cursor;
    u16 cursor_u;

    s32 val1;
    s32 val2;
    s32 steps;
    s32 v1;

    s32 cond;

    if (!stream->enabled) {
        return -1;
    }

    ptr = stream->start;
    end = stream->end;

    cursor = stream->cursor;
    cursor_u = stream->cursor;

    if (!(ptr < end)) {
        stream->enabled = 0;
        return -1;
    }

    duration = ptr[1];

    /*
     * This must be the same variable that later becomes ptr[0],
     * so GCC keeps it in a1:
     *
     *   andi a1, v1, 0xFF
     *   beq  a1, v0, ...
     *   slt  v0, a3, a1
     */
    val1 = duration;

    if (val1 == 0xFF) {
        return -1;
    }

    cond = cursor < val1;

    val1 = ptr[0];
    val2 = ptr[2];

    cursor++;

    if (cond) {
        goto interpolate;
    }

    val1 = val2;
    stream->start = ptr + 2;
    stream->cursor = 0;

    goto clamp;

interpolate:
    steps = (u8)(duration + 1);

    val1 = val1 * (steps - cursor);
    val1 += val2 * cursor;
    val1 /= steps;

    stream->cursor = cursor_u + 1;

clamp:
    val1 <<= 1;

    if (val1 >= 0) {
        if (val1 < 0x100) {
            v1 = val1;
        } else {
            v1 = 0xFF;
        }
    } else {
        v1 = 0;
    }

    return v1;
}


/**
 * @brief Step the four battle commands' value streams by one tick.
 *
 * Every active command reads the next value of both its streams
 * (func_80030A54); a command whose streams have both ended is deactivated.
 * The largest positive values of stream 0 and stream 1 over the running
 * commands are kept in D_80083870 and D_80083874, then clamped to 0-255 and
 * passed to setAnimEntityParams for entity 0 (0 and 0 when none is running).
 */
void func_80030B2C(void) {
    BattleCmdEntry *entry;
    s32 i;
    s32 val0;
    s32 val1;
    s32 running;
    s32 anyRunning; /* never initialised: an initialiser adds an instruction the original lacks; harmless, as both peaks start at 0 */
    s32 peak0;
    s32 peak1;

    entry = getBattleCmdTable();
    D_80083870 = 0;
    D_80083874 = 0;
    for (i = 0; i < 4; i++, entry++) {
        running = 0;
        if (entry->active != 0) {
            val0 = func_80030A54(&entry->streams[0]);
            val1 = func_80030A54(&entry->streams[1]);
            if (val0 == -1 && val1 == -1) {
                entry->active = 0;
            } else {
                if (val0 > 0 && val0 > D_80083870) {
                    D_80083870 = val0;
                }
                running = 1;
                if (val1 > 0 && val1 > D_80083874) {
                    D_80083874 = val1;
                }
            }
        }
        anyRunning |= running;
    }
    if (anyRunning) {
        peak0 = D_80083870;
        peak1 = D_80083874;
    } else {
        peak0 = 0;
        peak1 = 0;
    }
    peak0 = peak0 < 0 ? 0 : (peak0 > 255 ? 255 : peak0);
    peak1 = peak1 < 0 ? 0 : (peak1 > 255 ? 255 : peak1);
    setAnimEntityParams(0, peak0, peak1);
}


/**
 * @brief Add to the battle timer and process ticks.
 *
 * Accumulates @p delta into g_battleTimer. For every 4 units accumulated,
 * calls func_80030B2C() once. The remainder is stored back.
 *
 * @param delta Amount to add to the timer.
 */
void advanceBattleTimer(s32 delta) {
    s32 counter = g_battleTimer;
    counter += delta;
top:
    if (counter >= 4) {
        func_80030B2C();
        counter -= 4;
        goto top;
    }
    g_battleTimer = counter;
}


/**
 * @brief Initialize the 4 battle command entries and reset the battle timer.
 *
 * Sets each entry's index to its slot number, clears the active flag,
 * and sets sourceId to 1. Zeroes g_battleTimer.
 */
void initBattleCmdEntries(void) {
    BattleCmdEntry* ptr = getBattleCmdTable();
    s32 i;

    for (i = 0; i < 4; i++, ptr++) {
        ptr->index = i;
        ptr->active = 0;
        ptr->sourceId = 1;
    }

    g_battleTimer = 0;
}
