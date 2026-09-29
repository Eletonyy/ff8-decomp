#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "btl_color.h"
#include "btl_anim.h"
#include "gamestate.h"
#include "numstr.h"

/* --- Externs (sorted by address) --- */

extern u8 g_battleSfxTable[];              /* 0x80052A34 — SPU command table */
extern s32 D_80083870; /* 0x80083870 — peak stream 0 value of the running commands */
extern s32 D_80083874; /* 0x80083874 — peak stream 1 value of the running commands */
extern BattleCmdEntry g_battleCmdTable[];   /* 0x80083878 — battle command entries (4 × 0x24) */
extern s32 g_battleTimer;               /* 0x80083750 — battle timer */

/* --- Private functions --- */

static void buildRgbGpuColor(s32 r, s32 g, s32 b);
static BattleCmdEntry *getBattleCmdTable(void);
static BattleCmdEntry *findBestBattleCmd(s32 threshold);
static s32 func_80030A54(CmdStream *stream);
static void disableSoundReverb(s32 mask);
static s32 remapButtonIndex(s32 index);

/**
 * @brief Reset a dialog's typing progress: @c typedChars, @c typingRow and @c typingLine.
 * @param entry The dialog.
 */
void resetDialogTyping(Dialog *entry) {
    entry->typedChars = 0;
    entry->typingLine = 0;
    entry->typingRow = 0;
}


/* ========================================================================
 * resetDialogTyping above likely belongs in numstr.c rather than here: it
 * clears exactly the three typing fields that numstr's nextDialogChar
 * advances.
 * ======================================================================== */


/**
 * @brief Draw one glyph of the font table as a run of sprites.
 *
 * Emits every cell of glyph @p idx of the @c D_80052A68 font table as one
 * @c TSPRT, tinted with @p color and linked into @p ot; an @p idx past the
 * table's glyph count draws nothing. Per cell: the u/v/CLUT word is the cell's
 * own plus the font CLUT; the texture page is the font page with the cell's
 * blend rate; the colour word gets the cell's semi-transparency bit and is
 * forced to a SPRT code; width/height are copied and the cell's signed offsets
 * are added to (@p x, @p y).
 *
 * @note The emitter of drawTextIcon (dialog.c) with drawDialogMarker's colour
 * masking and the same @c (u8) narrowing of the blend rate. @c p is taken from
 * @p head only after the glyph-count check: set before it, the table address
 * gets a register of its own instead of the cell cursor's.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param head First free packet.
 * @param idx Glyph index into @c D_80052A68.
 * @param x Left edge of the glyph.
 * @param y Top edge of the glyph.
 * @param color Colour word the cells are tinted with.
 * @return The first free packet after the ones written.
 */
void *func_8002FF34(void *ot, void *head, s32 idx, s32 x, s32 y, s32 color) {
    GlyphTable *table;
    GlyphCell *cell;
    TSPRT *p;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    s32 n;

    table = &D_80052A68;
    cell = (GlyphCell *)table;
    if (idx >= table->glyphCount) {
        return head;
    }
    p = head;
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (GlyphCell *)((u8 *)cell + word);

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & GLYPH_UVCLUT_MASK;
        val += getClut(GLYPH_CLUT_X, GLYPH_CLUT_Y) << 16;
        setGlyphUVClut(p, val);

        val = (word >> GLYPH_ABR_SHIFT) & GLYPH_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, GLYPH_TPAGE_X, GLYPH_TPAGE_Y);

        val = word >> GLYPH_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
        val &= SPRT_RGB_MASK;
        val |= SPRT_CODE;
        setGlyphRGBC(p, val);

        word = cell->metrics;
        val = word & GLYPH_WH_MASK;
        setGlyphWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    return p;
}


/**
 * @brief Build a packed grayscale GPU color and store to g_gpuColor.
 * @param intensity Scalar intensity value (divided by 32, masked to 8 bits).
 */
void buildGrayscaleGpuColor(s32 intensity) {
    intensity /= 32;
    intensity &= 0xFF;
    g_gpuColor = intensity | (intensity << 8) | (intensity << 16) | SPRT_CODE;
}


/**
 * @brief Build a packed RGB GPU color and store to g_gpuColor.
 * @param r Red intensity (divided by 32, masked to 8 bits).
 * @param g Green intensity.
 * @param b Blue intensity.
 */
static void buildRgbGpuColor(s32 r, s32 g, s32 b) {
    r /= 32;
    g /= 32;
    b /= 32;
    r &= 0xFF;
    g &= 0xFF;
    b &= 0xFF;
    g_gpuColor = r | (g << 8) | (b << 16) | SPRT_CODE;
}


/**
 * @brief Draw one glyph of the font table as a run of sprites, with a CLUT offset.
 *
 * The emitter of drawTextIcon (dialog.c), tinted with @p color, with @p clut
 * added to every cell's CLUT: the menus pass CLUT ids such as @c (row << 6) + 2
 * to draw a glyph through another palette row. There is no glyph-count check.
 *
 * @note @p p is the packet cursor itself; copying it to a local first moves the
 * copy out of the prologue.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param p First free packet.
 * @param idx Glyph index into @c D_80052A68.
 * @param x Left edge of the glyph.
 * @param y Top edge of the glyph.
 * @param color Colour word the cells are tinted with.
 * @param clut CLUT id added to each cell's CLUT.
 * @return The first free packet after the ones written.
 */
void *func_800300F8(void *ot, TSPRT *p, s32 idx, s32 x, s32 y, s32 color, s32 clut) {
    GlyphTable *table;
    GlyphCell *cell;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    s32 n;

    table = &D_80052A68;
    cell = (GlyphCell *)table;
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (GlyphCell *)((u8 *)cell + word);

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & GLYPH_UVCLUT_MASK;
        val += getClut(GLYPH_CLUT_X, GLYPH_CLUT_Y) << 16;
        val += clut << 16;
        setGlyphUVClut(p, val);

        val = (word >> GLYPH_ABR_SHIFT) & GLYPH_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, GLYPH_TPAGE_X, GLYPH_TPAGE_Y);

        val = word >> GLYPH_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
        setGlyphRGBC(p, val);

        word = cell->metrics;
        val = word & GLYPH_WH_MASK;
        setGlyphWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    return p;
}


/** @brief Call buildGrayscaleGpuColor with the default parameter value 0x1000. */
void setDefaultGpuColor(void) { buildGrayscaleGpuColor(0x1000); }


/** @brief Empty stub -- no operation. */
void btlColorStub0234(void) {
}


/* ========================================================================
 * Likely file boundary: the countdown timer display starts here. Each block
 * of this file ends with an init that initBattleAnimSystem calls in address
 * order (btlColorStub0234 for the glyph and colour code above), which
 * suggests each block was its own file.
 * ======================================================================== */


/**
 * @brief Set the HUD brightness, the grey level of the countdown, the SeeD rank
 * notification and the gauges.
 * @param brightness 0x1000 is full brightness.
 */
void setHudBrightness(s32 brightness) {
    g_battleAnims.countdown.brightness = brightness;
}


/**
 * @brief Show or hide the countdown timer.
 *
 * Showing it restarts the colon blink from the current countdown value.
 *
 * @param visible Nonzero to show the countdown.
 */
void setCountdownVisible(unsigned int visible) {
    CountdownDisplay *disp = &g_battleAnims.countdown;

    disp->visible = visible;
    if (visible != 0) {
        disp->blinkFrames = 0;
        disp->lastSeconds = (s8)g_gameState.mainData.countdownTimer;
    }
}


/**
 * @brief Set where the countdown timer is drawn.
 * @param x Left edge of the minutes.
 * @param y Top edge.
 */
void setCountdownPosition(s32 x, s32 y) {
    CountdownDisplay *disp = &g_battleAnims.countdown;

    disp->x = x;
    disp->y = y;
}


/**
 * @brief Step the countdown's colon blink by one frame.
 *
 * Counts frames up to 0x40 and restarts from 0 whenever the low byte of
 * g_gameState.mainData.countdownTimer changes, so the colon, which is drawn
 * while the count is under half a second, blinks once per second.
 */
void updateCountdownBlink(void) {
    CountdownDisplay *disp = &g_battleAnims.countdown;
    s32 counter;
    s32 clamped;
    s32 gsVal;
    s32 curVal;

    counter = disp->blinkFrames;
    counter++;
    clamped = 0x40;
    if (counter < 0x41U) {
        clamped = counter;
    }
    disp->blinkFrames = clamped;
    gsVal = g_gameState.mainData.countdownTimer;
    curVal = disp->lastSeconds;
    gsVal &= 0xFF;
    counter = gsVal;
    if (counter != curVal) {
        disp->blinkFrames = 0;
        disp->lastSeconds = counter;
    }
}


/**
 * @brief Draw the countdown timer as MM:SS glyphs, then the draw-environment packets.
 *
 * Draws nothing while the countdown is hidden. Otherwise it shows the countdown
 * less one second, clamped to 0x1797 seconds, at the position and brightness in
 * @c g_battleAnims.countdown. A blank leading minutes digit is skipped, and the colon
 * shows for the first half second of each second (30 frames on NTSC, 25 on PAL).
 *
 * @param ot Ordering-table slot the glyphs are linked into.
 * @param pkt First free packet.
 * @return The first free packet after everything drawn.
 *
 * @see https://decomp.me/scratch/CEsOX
 */
u8 *func_800302DC(void *ot, u8 *pkt) {
    u8 buf[24];
    u8 *out;
    u32 color;
    s32 timer;
    CountdownDisplay *disp;
    s32 x;
    s32 y;
    s32 i;
    s32 threshold;
    s32 ret;
    s32 c;

    out = pkt;
    ret = GetVideoMode();

    threshold = 30;
    if (ret == MODE_PAL) {
        threshold = 25;
    }

    disp = &g_battleAnims.countdown;
    if (disp->visible == 0) {
        return out;
    }

    color = disp->brightness;
    x = disp->x;
    timer = g_gameState.mainData.countdownTimer;
    y = disp->y;

    color >>= 5;
    color &= 0xFF;

    {
        u32 lo;
        u32 hi;

        lo = color | (color << 8);
        hi = color << 16;
        color = lo | hi;
    }

    color |= SPRT_CODE;
    timer--;
    timer = (timer < 0) ? 0 : ((timer >= 0x1798) ? 0x1797 : timer);

    intToDecStringShort(timer / 60, buf, 0x70);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        if (i != 3 || c != 0x70) {
            out = func_8002FF34(ot, out, c, x, y, color);
        }
        x += 10;
    }

    x++;
    if (disp->blinkFrames < threshold) {
        out = func_8002FF34(ot, out, 0x7A, x, y, color);
    }

    x += 7;
    intToDecStringShort(timer % 60, buf, 0x70);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        out = func_8002FF34(ot, out, c, x, y, color);
        x += 10;
    }

    return emitDrawEnvPackets(ot, out);
}


/**
 * @brief Draw the countdown timer as MM:SS glyphs.
 *
 * The same display as func_800302DC, read from the same state: the countdown
 * (clamped to 0x1797 seconds) is drawn at the position, brightness and blink phase kept
 * in @c g_battleAnims.countdown, the leading minutes digit is skipped when blank and the
 * colon shows while the blink counter is below 30. Unlike func_800302DC it shows
 * the countdown as is, uses the NTSC blink threshold whatever the video mode and
 * leaves the draw-environment packets to the caller.
 *
 * @param ot Ordering-table slot the glyphs are linked into.
 * @param pkt First free packet.
 * @return The first free packet after the glyphs.
 */
u8 *func_80030518(P_TAG *ot, u8 *pkt) {
    u8 buf[16];
    u8 *out;
    u32 color;
    s32 timer;
    CountdownDisplay *disp;
    s32 x;
    s32 y;
    s32 i;
    s32 c;

    out = pkt;
    disp = &g_battleAnims.countdown;
    if (disp->visible == 0) {
        return out;
    }

    color = disp->brightness;
    x = disp->x;
    timer = g_gameState.mainData.countdownTimer;
    y = disp->y;

    color >>= 5;
    color &= 0xFF;

    {
        u32 lo;
        u32 hi;

        lo = color | (color << 8);
        hi = color << 16;
        color = lo | hi;
    }

    color |= SPRT_CODE;
    timer = (timer < 0) ? 0 : ((timer >= 0x1798) ? 0x1797 : timer);

    intToDecStringShort(timer / 60, buf, 0x70);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        if (i != 3 || c != 0x70) {
            out = func_8002FF34(ot, out, c, x, y, color);
        }
        x += 10;
    }

    x++;
    if (disp->blinkFrames < 30) {
        out = func_8002FF34(ot, out, 0x7A, x, y, color);
    }

    x += 7;
    intToDecStringShort(timer % 60, buf, 0x70);

    for (i = 3; i < 5; i++) {
        c = buf[i];
        out = func_8002FF34(ot, out, c, x, y, color);
        x += 10;
    }

    return out;
}


/**
 * @brief Hide the countdown timer and reset its position and blink, with full brightness (0x1000).
 */
void resetCountdownDisplay(void) {
    CountdownDisplay *disp = &g_battleAnims.countdown;

    disp->visible = 0;
    disp->brightness = 0x1000;
    disp->x = 0;
    disp->y = 0;
    disp->lastSeconds = 0;
    disp->blinkFrames = 0;
}


/* ========================================================================
 * Likely file boundary: controller vibration (the "BattleCmd" code) starts
 * here, after the countdown's resetCountdownDisplay.
 * ======================================================================== */


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
 * @param data     Pointer to packed command data block (offset table + streams).
 * @param idx      Index into the offset table.
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


/* ========================================================================
 * Likely file boundary: the sound helpers start here, after the vibration
 * code's initBattleCmdEntries.
 * ======================================================================== */


/**
 * @brief Send an SPU command from the battle SFX table.
 *
 * Looks up a command byte from g_battleSfxTable and sends it to the
 * SPU via sndKeyOn.
 *
 * @param idx Index into g_battleSfxTable.
 */
void sendSpuCommand(s32 idx) {
    sndKeyOn(g_battleSfxTable[idx]);
}


/**
 * @brief Play a sound effect from the battle SFX table.
 *
 * Looks up a sound ID from g_battleSfxTable and plays it via sndPlaySfx
 * with default volume (0x80) and pan (0x7F).
 *
 * @param idx Index into the g_battleSfxTable sound table.
 */
void playSoundEffect(s32 idx) {
    sndPlaySfx(g_battleSfxTable[idx], 0, 0x80, 0x7F);
}


/**
 * @brief Configure sound reverb channels based on a bitmask.
 *
 * Reads hardware state via func_80047384, optionally pauses/resumes audio
 * hardware. Mutes master volume, then enables reverb on channels indicated
 * by bits 0-2 of @p mask. If @p mask is 7, enables reverb on channel 0 (all).
 *
 * @param mask Bitmask of reverb channels to enable (bits 0, 1, 2).
 */
void enableSoundReverb(s32 mask) {
    s32 hwState = func_80047384();

    if (!(hwState & 4)) {
        func_800472E4();
    }
    sndSetMasterVolume(0);
    if (mask == 7) {
        sndEnableReverb(0);
    } else {
        if (mask & 1) {
            sndEnableReverb(1);
        }
        if (mask & 2) {
            sndEnableReverb(2);
        }
        if (mask & 4) {
            sndEnableReverb(3);
        }
    }
    if (!(hwState & 4)) {
        func_800472F4();
    }
}


/**
 * @brief Disable sound reverb channels based on a bitmask and restore volume.
 *
 * Reads hardware state via func_80047384, optionally pauses/resumes audio
 * hardware. Disables reverb on channels indicated by bits 0-2 of @p mask
 * (all of them via channel 0 when it is 7), then restores master volume to 0x7F.
 *
 * @param mask Bitmask of reverb channels to disable (bits 0, 1, 2).
 */
static void disableSoundReverb(s32 mask) {
    s32 hwState = func_80047384();

    if (!(hwState & 4)) {
        func_800472E4();
    }
    if (mask == 7) {
        sndDisableReverb(0);
    } else {
        if (mask & 1) {
            sndDisableReverb(1);
        }
        if (mask & 2) {
            sndDisableReverb(2);
        }
        if (mask & 4) {
            sndDisableReverb(3);
        }
    }
    sndSetMasterVolume(0x7F);
    if (!(hwState & 4)) {
        func_800472F4();
    }
}


/* ========================================================================
 * Possible file boundary: the button remap starts here. The sound helpers
 * above have no init, so this cut rests on the change of topic and on the
 * overlays' pad readers calling applyButtonRemapTranslation without its
 * prototype.
 * ======================================================================== */


/**
 * @brief Translate pad button bits through the player's custom button mapping.
 *
 * Returns @p bitmask unchanged unless the Config menu's controller setting is
 * Customize (CONFIG_CONTROLLER). Then each of the 12 remappable buttons in the
 * low bits becomes the button its g_gameState.config.buttons entry names
 * (1-based; 0 drops it), and the D-pad bits (0xF000) pass through.
 *
 * @param bitmask Pad button bits.
 * @return The translated button bits.
 */
u16 applyButtonRemapTranslation(u16 bitmask) {
    s32 top;
    u8* table;
    s32 result;
    s32 i;

    if (!(g_gameState.config.flags & CONFIG_CONTROLLER)) {
        return bitmask;
    }

    top = bitmask & 0xF000;
    table = g_gameState.config.buttons;
    result = 0;
    bitmask &= 0xFFF;

    for (i = 0; i < 12; i++) {
        if ((bitmask >> i) & 1) {
            s32 remap = *table++;
            if (remap) {
                result |= 1 << (remap - 1);
            }
        } else {
            table++;
        }
    }

    return top | result;
}


/**
 * @brief Remap a button index through the controller button table.
 *
 * If CONFIG_CONTROLLER is set and @p index is within the 12-button range,
 * returns the remapped button value minus 1. Otherwise returns the index
 * unchanged.
 *
 * @param index Button index to remap.
 * @return Remapped index, or original if remapping inactive or out of range.
 */
static s32 remapButtonIndex(s32 index) {
    if ((g_gameState.config.flags & CONFIG_CONTROLLER) && index < 12) {
        return g_gameState.config.buttons[index] - 1;
    }
    return index;
}


/**
 * @brief Reverse-lookup a logical button index to its physical button.
 *
 * Inverse of remapButtonIndex. Given a logical index, searches the
 * controller remap table for which physical button maps to it.
 * Returns the index unchanged if remapping is inactive or out of range,
 * or -1 if no physical button maps to the given logical index.
 *
 * @param index Logical button index to reverse-lookup.
 * @return Physical button index, original index if inactive, or -1 if not found.
 */
s32 reverseButtonRemap(s32 index) {
    u8* table;
    s32 i;
    u8 remap;

    if (!(g_gameState.config.flags & CONFIG_CONTROLLER) || index >= 12) {
        return index;
    }

    table = g_gameState.config.buttons;
    index++;

    for (i = 0; i < 12; i++) {
        remap = *table++;
        if (remap == index) {
            return i;
        }
    }

    return -1;
}


/** @brief Empty stub -- no operation. */
void btlColorStub1044(void) {
}

