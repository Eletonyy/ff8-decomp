#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "btl_color.h"
#include "btl_anim.h"
#include "gamestate.h"
#include "numstr.h"

/* --- Local type definitions --- */

typedef struct {
    s16 x; /* 0x00: gauge position */
    s16 y; /* 0x02 */
    s16 rangeStart;   /* 0x04 */
    s16 rangeEnd;     /* 0x06 */
    s16 inputStart;   /* 0x08 */
    s16 inputEnd;     /* 0x0A */
    s16 current;      /* 0x0C */
    u8 flags;         /* 0x0E */
    u8 pad;           /* 0x0F */
} AnimEntry; /* 0x10 */

/** @brief @c AnimEntry.flags bits. */
#define ANIM_ENTRY_BLINK_LOW 0x01 /**< Blink the gauge while its value is at most a quarter of the maximum. */
#define ANIM_ENTRY_SNAP 0x02 /**< updateAnimEntry sets @c current straight to the new value. */
#define ANIM_ENTRY_BLINK_ON 0x40 /**< Current blink phase: the bar is drawn white. */
#define ANIM_ENTRY_ACTIVE 0x80 /**< The entry is animated and drawn. */

/** @brief Primitive code 0x40 (flat line) in the colour word. */
#define LINE_F2_CODE 0x40000000

/** @brief A sprite that carries its own draw mode and texture window (tag length 7). */
typedef struct {
    u32 tag;
    u32 drawMode; /* GP0(E1h) */
    u32 texWindow[2]; /* GP0(E2h), then a zero word */
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u32 uvClut; /* u, v and CLUT */
    u16 w, h;
} GaugeSprt;

/** @brief Battle HUD OT buffer (smaller display list for UI elements). */
typedef struct {
    DISPENV disp; /* 0x00 */
    DRAWENV draw; /* 0x14 */
    u32 ot[2];               /* 0x70: 2-entry ordering table */
    void *pktAlloc; /* 0x78: current packet allocation pointer */
    u32 pktBase;             /* 0x7C: packet buffer start */
} HudDisplayBuf;

/**
 * @brief The on-screen countdown timer (g_countdownDisplay), drawn as MM:SS
 * from @c g_gameState.mainData.countdownTimer.
 */
typedef struct {
    u16 x; /* 0x00 */
    u8 y; /* 0x02 */
    u8 visible; /* 0x03 */
    u16 brightness; /* 0x04: g_hudBrightness, 0x1000 = full */
    u8 blinkFrames; /* 0x06: frames since lastSeconds changed, capped at 0x40 */
    u8 lastSeconds; /* 0x07: low byte of the countdown when it last changed */
} CountdownDisplay;

/**
 * @brief The SeeD rank banner (D_80083754): it slides in, rolls the rank and
 * the salary from their old values to the new ones, and slides out again.
 */
typedef struct {
    u16 state; /* 0x00 */
    u16 pad02; /* 0x02 */
    s16 roll; /* 0x04: 0 shows the old rank and salary, 0x1000 the new ones */
    s16 slide; /* 0x06: 0 is off screen and not drawn, 0x1000 is fully in */
    u8 oldRank; /* 0x08 */
    u8 newRank; /* 0x09 */
    u8 timer; /* 0x0A */
    u8 salaryBlanks; /* 0x0B: leading blanks the two salary strings share */
    u8 newRankText[3]; /* 0x0C */
    u8 oldRankText[3]; /* 0x0F */
    u8 newSalaryText[6]; /* 0x12 */
    u8 oldSalaryText[6]; /* 0x18 */
} RankBanner;

/**
 * @brief The 0x280 bytes before the RankBanner at D_80083754 (a separate
 * global), which start with g_hudBrightness. hudBrightness is read as the
 * block before the banner base to keep the retail lhu -640($s5) form.
 */
typedef struct {
    /* 0x000 */ u16 hudBrightness; /* g_hudBrightness */
    /* 0x002 */ u8 pad[0x27E]; /* to D_80083754 */
} RankBannerScratch; /* sizeof == 0x280 */

/* Load-bearing layout: size must stay 0x280 (see lhu -640($s5) access).
 * Negative-sized array typedefs fail compilation if it drifts. */
typedef char rank_banner_scratch_brightness_ok[
    (((u32)&((RankBannerScratch *)0)->hudBrightness) == 0x000) ? 1 : -1];
typedef char rank_banner_scratch_size_ok[
    (sizeof(RankBannerScratch) == 0x280) ? 1 : -1];

/* --- Externs (sorted by address) --- */

extern u8 g_battleSfxTable[];              /* 0x80052A34 — SPU command table */
extern CountdownDisplay g_countdownDisplay; /* 0x800834D0 */
extern u16 g_hudBrightness; /* 0x800834D4 */
extern RankBanner D_80083754; /* 0x80083754 */
extern AnimEntry D_80083772[];       /* 0x80083772 — animation entry table */
extern u8 D_80083756; /* 0x80083756 — SeeD salary enabled */
extern s32 D_80083870; /* 0x80083870 — peak stream 0 value of the running commands */
extern s32 D_80083874; /* 0x80083874 — peak stream 1 value of the running commands */
extern BattleCmdEntry g_battleCmdTable[];   /* 0x80083878 — battle command entries (4 × 0x24) */
extern HudDisplayBuf *D_80083918;    /* 0x80083918 — active HUD display buffer */
extern HudDisplayBuf *D_80083920[];  /* 0x80083920 — HUD display buffer pair */
extern u8 D_80083938[];              /* 0x80083938 — battle OT data */
extern u8 D_80085134[];              /* 0x80085134 — battle display buffer */
extern s32 g_battleTimer;               /* 0x80083750 — battle timer */
extern u8 D_80052A64[];

/* --- Private functions --- */

static void buildRgbGpuColor(s32 r, s32 g, s32 b);
static BattleCmdEntry *getBattleCmdTable(void);
static BattleCmdEntry *findBestBattleCmd(s32 threshold);
static s32 func_80030A54(CmdStream *stream);
static void disableSoundReverb(s32 mask);
static s32 remapButtonIndex(s32 index);
static u8 *renderBattleString(P_TAG *ot, u8 *pkt, u8 *str, s32 x, s32 y, s32 color);
static void *func_80031224(P_TAG *ot, LINE_F2 *line, s32 leftWidth, s32 rightX);
static s32 lerpRange(s32 rangeStart, s32 rangeEnd, s32 input, s32 maxOut);
static u8 *func_80031A18(P_TAG *ot, void *pkt, s32 idx, u32 color);
static void copyAnimEntryField(s32 idx, u16 *src);
static void initAnimEntry(s32 idx, s32 flags, u16 *src, s32 start, s32 end, s32 inStart, s32 inEnd);
static u8 *getBattleBuffer2(void);

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
 * @brief Set g_hudBrightness, the grey level of the countdown, the rank banner and the gauges.
 * @param brightness 0x1000 is full brightness.
 */
void setHudBrightness(s32 brightness) {
    g_hudBrightness = brightness;
}


/**
 * @brief Show or hide the countdown timer.
 *
 * Showing it restarts the colon blink from the current countdown value.
 *
 * @param visible Nonzero to show the countdown.
 */
void setCountdownVisible(unsigned int visible) {
    g_countdownDisplay.visible = visible;
    if (visible != 0) {
        g_countdownDisplay.blinkFrames = 0;
        g_countdownDisplay.lastSeconds = (s8)g_gameState.mainData.countdownTimer;
    }
}


/**
 * @brief Set where the countdown timer is drawn.
 * @param x Left edge of the minutes.
 * @param y Top edge.
 */
void setCountdownPosition(s32 x, s32 y) {
    g_countdownDisplay.x = x;
    g_countdownDisplay.y = y;
}


/**
 * @brief Step the countdown's colon blink by one frame.
 *
 * Counts frames up to 0x40 and restarts from 0 whenever the low byte of
 * g_gameState.mainData.countdownTimer changes, so the colon, which is drawn
 * while the count is under half a second, blinks once per second.
 */
void updateCountdownBlink(void) {
    s32 counter;
    s32 clamped;
    s32 gsVal;
    s32 curVal;

    counter = g_countdownDisplay.blinkFrames;
    counter++;
    clamped = 0x40;
    if (counter < 0x41U) {
        clamped = counter;
    }
    g_countdownDisplay.blinkFrames = clamped;
    gsVal = g_gameState.mainData.countdownTimer;
    curVal = g_countdownDisplay.lastSeconds;
    gsVal &= 0xFF;
    counter = gsVal;
    if (counter != curVal) {
        g_countdownDisplay.blinkFrames = 0;
        g_countdownDisplay.lastSeconds = counter;
    }
}


/**
 * @brief Draw the countdown timer as MM:SS glyphs, then the draw-environment packets.
 *
 * Draws nothing while the countdown is hidden. Otherwise it shows the countdown
 * less one second, clamped to 0x1797 seconds, at the position and brightness in
 * @c g_countdownDisplay. A blank leading minutes digit is skipped, and the colon
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

    disp = &g_countdownDisplay;
    if (disp->visible == 0) {
        return out;
    }

    color = disp->brightness;
    x = g_countdownDisplay.x;
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
 * in @c g_countdownDisplay, the leading minutes digit is skipped when blank and the
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
    disp = &g_countdownDisplay;
    if (disp->visible == 0) {
        return out;
    }

    color = disp->brightness;
    x = g_countdownDisplay.x;
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
    g_countdownDisplay.visible = 0;
    g_countdownDisplay.brightness = 0x1000;
    g_countdownDisplay.x = 0;
    g_countdownDisplay.y = 0;
    g_countdownDisplay.lastSeconds = 0;
    g_countdownDisplay.blinkFrames = 0;
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


/* ========================================================================
 * Likely file boundary: the SeeD rank banner starts here, after the button
 * remap's init, btlColorStub1044.
 * ======================================================================== */


/**
 * @brief Step the SeeD rank banner by one frame.
 *
 * - States 0-1: slide in (@c slide up to 0x1000 in steps of 0x100).
 * - States 2-3: hold for 24 frames.
 * - States 4-5: roll to the new rank (@c roll up to 0x1000 in steps of 0x155).
 * - State 6: shown, until hideRankBanner sets state 7.
 * - States 7-8: slide out (@c slide down to 0 in steps of 0x100).
 * - States 9 and 10: hidden.
 *
 * An unchanged rank skips the roll.
 */
void updateRankBanner(void) {
    u16 *state = &D_80083754.state;
    RankBanner *p = &D_80083754;

    switch (*state) {
    case 0:
        p->roll = 0;
        p->slide = 0;
        *state = 1;
    case 1:
        p->slide += 0x100;
        if (p->slide < 0x1000) {
            return;
        }
        p->slide = 0x1000;
        *state = 2;
        return;
    case 2:
        p->timer = 24;
        *state = 3;
    case 3:
        p->timer--;
        if (p->timer) {
            return;
        }
        *state = 4;
        return;
    case 4:
        if (p->newRank == p->oldRank) {
            p->roll = 0x1000;
            *state = 6;
            return;
        }
        *state = 5;
        return;
    case 5:
        p->roll += 0x155;
        if (p->roll < 0x1000) {
            return;
        }
        p->roll = 0x1000;
        *state = 6;
        return;
    case 6:
        return;
    case 7:
        *state = 8;
        return;
    case 8:
        p->slide -= 0x100;
        if (p->slide > 0) {
            return;
        }
        p->slide = 0;
        *state = 9;
        return;
    case 9:
    case 10:
        return;
    }
}


/**
 * @brief Draw a zero-terminated glyph string, 9 pixels per character.
 *
 * Character 7 is a blank: it draws nothing but still takes its 9 pixels.
 * Every other character is drawn with func_8002FF34.
 *
 * @param ot OT base pointer.
 * @param pkt Current packet buffer pointer.
 * @param str Zero-terminated glyph string.
 * @param x Left edge of the first character.
 * @param y Top edge.
 * @param color Color word passed to func_8002FF34.
 * @return Updated packet buffer pointer after rendering.
 */
static u8 *renderBattleString(P_TAG *ot, u8 *pkt, u8 *str, s32 x, s32 y, s32 color) {
    P_TAG *otBase = ot;
    u8 *ptr = str;
    s32 xPos = x;
    s32 yPos = y;
    s32 blank = 7;
    u8 ch;
    s32 col = color;

    do {
    top:
        ch = *ptr++;
        if (ch == 0) {
            return pkt;
        }
        if (ch == blank) goto skip;
        /* Emit one glyph packet and advance the cursor. */
        pkt = func_8002FF34(otBase, pkt, ch, xPos, yPos, col);
    } while (0);
skip:
    xPos = xPos + 9;
    goto top;
}


/**
 * @brief Draw two pairs of horizontal lines: y=209 in a bright grey and y=210 in a dim
 * grey, from x=0 to @p leftWidth + 50 and from @p rightX to 320.
 *
 * The greys are g_hudBrightness / 32 and / 128.
 *
 * @param ot Ordering table.
 * @param line Where to build the four LINE_F2 packets.
 * @param leftWidth The left lines end at @p leftWidth + 50.
 * @param rightX Where the right lines start.
 * @return The packet cursor after the lines.
 */
static void *func_80031224(P_TAG *ot, LINE_F2 *line, s32 leftWidth, s32 rightX) {
    u32 bright;
    u32 dim;
    s32 x; /* Regalloc: the intensity, then the left lines' x0. That later use keeps the / 128 rounding in a register of its own. */
    s32 c;
    u32 link1; /* Regalloc: one link temp per prim, each live from entry, gives the retail $t5..$t8. */
    u32 link2;
    u32 link3;
    u32 link4;

    x = g_hudBrightness;
    c = x / 32;
    bright = LINE_F2_CODE | (c << 16) | (c << 8) | c;
    c = x / 128;
    dim = LINE_F2_CODE | (c << 16) | (c << 8) | c;

    x = 0;
    setLineF2(line);
    *(u32 *)&line->r0 = bright; /* r, g, b and code in one store */
    line->x0 = x;
    line->x1 = leftWidth + 50;
    line->y0 = 209;
    line->y1 = 209;
    addPrimFastWithTempOperand(ot, line, link1);
    line++;
    setLineF2(line);
    *(u32 *)&line->r0 = dim;
    line->x0 = x;
    line->x1 = leftWidth + 50;
    line->y0 = 210;
    line->y1 = 210;
    addPrimFastWithTempOperand(ot, line, link2);
    line++;
    setLineF2(line);
    *(u32 *)&line->r0 = bright;
    line->x0 = rightX;
    line->x1 = 320;
    line->y0 = 209;
    line->y1 = 209;
    addPrimFastWithTempOperand(ot, line, link3);
    line++;
    setLineF2(line);
    *(u32 *)&line->r0 = dim;
    line->x0 = rightX;
    line->x1 = 320;
    line->y0 = 210;
    line->y1 = 210;
    addPrimFastWithTempOperand(ot, line, link4);
    line++;
    return line;
}


/* These scale factors are written as shift/add chains to preserve the
 * retail arithmetic without introducing multiply instructions. */
#define SCALE_BY_50(value) ((((((value) << 1) + (value)) << 3) + (value)) << 1)
#define SCALE_BY_59(value) (((((value) << 4) - (value)) << 2) - (value))
#define SCALE_BY_12(value) ((((value) << 1) + (value)) << 2)


/**
 * @brief Draw the SeeD rank banner and link it into the OT.
 *
 * Draws nothing while @c slide is 0. The rank half (glyph 0xB0, then the rank)
 * slides in from the left and the salary half (the salary, then glyph 0xB1)
 * from the right, along g_animCurveFadeOut. Everything is clipped to a
 * 12-pixel strip at y 196, where the old and new values roll vertically while
 * @c roll is between 0 and 0x1000; func_80031224 then underlines the banner.
 * The grey level is g_hudBrightness, read through RankBannerScratch from the
 * banner base to keep the retail lhu -640($s5).
 *
 * @param ot OT base pointer.
 * @param pkt Current packet buffer pointer.
 * @return Updated packet buffer pointer after rendering.
 */
u8 *func_80031364(void *ot, u8 *pkt) {
    RECT rect;
    u8 *pkt_r;
    RankBanner *p;
    RankBannerScratch *scratch;
    u32 color;
    s32 y;
    s32 curve;
    s32 roll;
    s32 absDir;
    s32 x;
    s32 calc_val;
    s32 rightShift;
    s32 temp;
    s32 yOff;
    s32 pkt_tmp;
    u8 *next;
    u8 *pktAfterLines;
    s32 s0;

    pkt_r = pkt;
    p = &D_80083754;

    if (p->slide <= 0) {
        return pkt_r;
    }

    copyDisplayRect(&rect);

    scratch = &((RankBannerScratch *)p)[-1];
    color = scratch->hudBrightness;
    color >>= 5;
    color &= 0xFF;
    color = (color | (color << 8)) | (color << 16);
    color |= SPRT_CODE;

    curve = 0x1000 - p->slide;
    curve = g_animCurveFadeOut[curve / 64];
    roll = p->roll;
    curve <<= 6;

    if (p->oldRank < p->newRank) {
        absDir = -roll;
        yOff = 12;
    } else {
        absDir = roll;
        yOff = -12;
    }

    calc_val = -SCALE_BY_50(curve);
    y = 0xC4;

    if (calc_val < 0) {
        calc_val += 0xFFF;
    }

    s0 = calc_val >> 12;
    temp = s0 + 0x10;

    pkt_r = func_8002FF34(ot, pkt_r, 0xB0, temp, y, color);
    x = s0 + 0x30;

    if (roll == 0) {
        pkt_r = renderBattleString(ot, pkt_r, p->oldRankText, x, y, color);
    } else if (roll == 0x1000) {
        pkt_r = renderBattleString(ot, pkt_r, p->newRankText, x, y, color);
    } else {
        y = SCALE_BY_12(absDir) / 4096 + 0xC4;
        next = renderBattleString(ot, pkt_r, p->oldRankText, x, y, color);
        pkt_r = renderBattleString(ot, next, p->newRankText, x, y + yOff, color);
    }

    y = 0xC4;
    rightShift = SCALE_BY_59(curve) / 4096;
    x = rightShift + 0xF0;

    pkt_r = func_8002FF34(ot, pkt_r, 0xB1, rightShift + 0x11D, y, color);

    if (roll == 0) {
        pkt_r = renderBattleString(ot, pkt_r, p->oldSalaryText, x, y, color);
    } else if (roll == 0x1000) {
        pkt_r = renderBattleString(ot, pkt_r, p->newSalaryText, x, y, color);
    } else {
        y = SCALE_BY_12(absDir) / 4096 + 0xC4;
        next = renderBattleString(ot, pkt_r, p->oldSalaryText, x, y, color);
        pkt_r = renderBattleString(ot, next, p->newSalaryText, x, y + yOff, color);
    }

    rect.h = 12;
    rect.y += 0xC4;

    SetDrawArea((DR_AREA *)pkt_r, &rect);
    addPrimFastWithTempOperand(ot, pkt_r, pkt_tmp);

    pktAfterLines = func_80031224(ot, (LINE_F2 *)((DR_AREA *)pkt_r + 1), temp,
                            x + p->salaryBlanks * 9);

    return emitDrawEnvPackets(ot, pktAfterLines);
}


/** @brief Start sliding the SeeD rank banner out. */
void hideRankBanner(void) {
    D_80083754.state = 7;
}


/**
 * @brief Show the SeeD rank banner for a rank change.
 *
 * Restarts the banner with @c roll and @c slide at 0 and runs its first step.
 * The ranks are written as two digits, or from 31 up as the D_80052A64 string
 * (a blank and glyph 0xB2, rank A), and the salaries as five digits. Leading
 * zeros become blanks (7), and @c salaryBlanks gets the number of leading
 * blanks the two salaries share.
 *
 * @param oldRank Rank before the change.
 * @param newRank Rank after the change.
 * @param oldSalary Salary before the change.
 * @param newSalary Salary after the change.
 */
void func_800316D4(s32 oldRank, s32 newRank, s32 oldSalary, s32 newSalary)
{
    u8 buf[16];
    RankBanner *d;
    u8 *buf5;
    s32 limit;
    s32 seven;
    s32 i;

    D_80083754.state = 0;
    d = &D_80083754;
    d->roll = 0;
    d->slide = 0;
    d->timer = 0;
    d->oldRank = oldRank;
    d->newRank = newRank;

    buf5 = &buf[5];

    intToDecString(oldSalary, buf, 0x60);
    copyString(d->oldSalaryText, buf5);
    replaceLeadingZeros(d->oldSalaryText, 4, 0x60, 7);

    intToDecString(newSalary, buf, 0x60);
    copyString(d->newSalaryText, buf5);
    replaceLeadingZeros(d->newSalaryText, 4, 0x60, 7);

    if (oldRank < 0x1F)
    {
        intToDecStringShort(oldRank, buf, 0x60);
        copyString(d->oldRankText, &buf[3]);
        replaceLeadingZeros(d->oldRankText, 1, 0x60, 7);
    }
    else
    {
        copyString(d->oldRankText, D_80052A64);
    }

    if (newRank < 0x1F)
    {
        intToDecStringShort(newRank, buf, 0x60);
        copyString(d->newRankText, &buf[3]);
        replaceLeadingZeros(d->newRankText, 1, 0x60, 7);
    }
    else
    {
        copyString(d->newRankText, D_80052A64);
    }

    i = 0;
    limit = 5;
    seven = 7;

    for (; i < 5; i++)
    {
        if (d->newSalaryText[i] != seven && i < limit)
        {
            limit = i;
            break;
        }
    }

    i = 0;
    seven = 7;

    for (; i < 5; i++)
    {
        if (d->oldSalaryText[i] != seven && i < limit)
        {
            limit = i;
            break;
        }
    }

    d->salaryBlanks = limit;
    updateRankBanner();
}


/** @brief Set whether the SeeD salary is enabled. */
void setSalaryEnabled(s32 enabled) {
    D_80083756 = enabled;
}


/**
 * @brief Hide the SeeD rank banner at once.
 *
 * Sets state 9 (hidden) and clears @c roll, @c slide and both ranks.
 */
void resetRankBanner(void) {
    D_80083754.state = 9;
    D_80083754.roll = 0;
    D_80083754.slide = 0;
    D_80083754.oldRank = 0;
    D_80083754.newRank = 0;
}


/* ========================================================================
 * Likely file boundary: the gauges (AnimEntry) start here, after the rank
 * banner's resetRankBanner.
 * ======================================================================== */


/**
 * @brief Perform linear interpolation within a range.
 *
 * Returns 0 if input is below rangeStart, maxOut if at or above rangeEnd,
 * or a proportional value in between.
 *
 * @param rangeStart Minimum input value.
 * @param rangeEnd   Maximum input value.
 * @param input      Current input value to interpolate.
 * @param maxOut     Maximum output value.
 * @return Interpolated value in [0, maxOut].
 */
static s32 lerpRange(s32 rangeStart, s32 rangeEnd, s32 input, s32 maxOut) {
    if (input < rangeStart) {
        return 0;
    }
    if (input >= rangeEnd) {
        return maxOut;
    }
    input -= rangeStart;
    rangeEnd -= rangeStart;
    return input * maxOut / rangeEnd;
}


/**
 * @brief Step animation entries toward their interpolated targets.
 *
 * For each active entry (flags bit 7 set), computes the target via
 * lerpRange and moves the current value toward it by +/-2 per step.
 */
void stepAnimEntries(void) {
    s32 i;
    AnimEntry *entry = D_80083772;
    s32 current;
    s32 target;

    for (i = 0; i < 2; i++, entry++) {
        if (!(entry->flags & ANIM_ENTRY_ACTIVE)) {
            continue;
        }

        current = entry->current;
        target = lerpRange(entry->rangeStart, entry->rangeEnd, entry->inputStart, entry->inputEnd);

        if (current < target) {
            current += 2;
            if (target < current) {
                current = target;
            }
        }
        if (target < current) {
            current -= 2;
            if (current < target) {
                current = target;
            }
        }

        entry->current = current;
    }
}


/**
 * @brief Draw the gauge of one animation entry.
 *
 * Only for an @ref ANIM_ENTRY_ACTIVE entry. The gauge sits at the entry's position:
 * glyphs 0x26 and 0x27 at x + 8 and x + inputEnd + 8, a draw-mode
 * packet that turns the texture window off, then the bar as sprites up to 64
 * pixels wide: its filled part (@c current pixels, 8 high, 3 lines down) over a
 * 16-high background @c inputEnd pixels long. With @ref ANIM_ENTRY_BLINK_LOW set,
 * the filled part blinks white (@ref ANIM_ENTRY_BLINK_ON toggling each call) while
 * the lerped value is at most a quarter of @c inputEnd.
 *
 * @param ot Ordering-table slot the packets are linked into.
 * @param pkt First free packet.
 * @param idx Entry index into D_80083772.
 * @param color Colour word of the glyphs, the background and the bar.
 * @return The packet cursor after emitDrawEnvPackets.
 */
static u8 *func_80031A18(P_TAG *ot, void *pkt, s32 idx, u32 color) {
    AnimEntry *entry;
    DR_TWIN *tw;
    GaugeSprt *p;
    u32 barColor;
    s32 flags;
    s32 originX;
    s32 originY;
    u32 link; /* Regalloc: one link temp per addPrim, each live from entry; this one is spilled. */
    u32 link2;
    u32 link3;
    s32 x;
    s32 y;
    s32 n;
    s32 w;

    p = pkt;
    entry = D_80083772;
    entry = &entry[idx];
    flags = entry->flags;
    originX = entry->x;
    originY = entry->y;
    if (flags & ANIM_ENTRY_ACTIVE) {
        barColor = color;
        if (flags & ANIM_ENTRY_BLINK_LOW) {
            if (lerpRange(entry->rangeStart, entry->rangeEnd, entry->inputStart, entry->inputEnd) <= entry->inputEnd / 4) {
                flags ^= ANIM_ENTRY_BLINK_ON;
                if (flags & ANIM_ENTRY_BLINK_ON) {
                    barColor = color | 0xFFFFFF;
                }
                entry->flags = flags;
            }
        }
        x = originX;
        y = originY;
        p = func_8002FF34(ot, p, 0x26, x + 8, y, color);
        p = func_8002FF34(ot, p, 0x27, x + entry->inputEnd + 8, y, color);
        tw = (DR_TWIN *)p;
        setlen(tw, 2);
        tw->code[0] = 0xE2000000;
        tw->code[1] = 0;
        addPrimFastWithTempOperand(ot, tw, link);
        p = (GaugeSprt *)(tw + 1);

        x += 8;
        y += 3;
        for (n = entry->current; n > 0; n -= 64) {
            w = n;
            if (w > 64) {
                w = 64;
            }
            *(u32 *)&p->r0 = barColor; /* r, g, b and code in one store; the code is set again below */
            setlen(p, 7);
            p->drawMode = _get_mode(1, 0, getTPage(0, 0, GLYPH_TPAGE_X, GLYPH_TPAGE_Y));
            p->texWindow[0] = 0xE20103FF; /* an 8x8 tile at (0, 16) */
            p->texWindow[1] = 0;
            p->code = 0x64;
            setXY0(p, x, y);
            setWH(p, w, 8);
            p->uvClut = getClut(256, 229) << 16;
            addPrimFastWithTempOperand(ot, p, link2);
            p++;
            x += 64;
        }

        x = originX + 8;
        y = originY;
        for (n = entry->inputEnd; n > 0; n -= 64) {
            w = n;
            if (w > 64) {
                w = 64;
            }
            *(u32 *)&p->r0 = color;
            setlen(p, 7);
            p->drawMode = _get_mode(1, 0, getTPage(0, 0, GLYPH_TPAGE_X, GLYPH_TPAGE_Y));
            p->texWindow[0] = 0xE20F5FDF; /* an 8x16 tile at (184, 240) */
            p->texWindow[1] = 0;
            p->code = 0x64;
            setXY0(p, x, y);
            setWH(p, w, 16);
            p->uvClut = getClut(256, 226) << 16;
            addPrimFastWithTempOperand(ot, p, link3);
            p++;
            x += 64;
        }
    }
    return emitDrawEnvPackets(ot, (u8 *)p); /* p itself: copying it back into pkt costs an instruction */
}


/**
 * @brief Render both animation overlay channels.
 *
 * Builds a grayscale GPU color from g_hudBrightness, then calls
 * func_80031A18 twice (once for each channel, indices 0 and 1).
 *
 * @param ot  OT base pointer.
 * @param pkt Packet buffer pointer.
 * @return Updated packet pointer.
 */
u8 *renderAnimOverlay(void *ot, u8 *pkt)
{
    u8 *ret = pkt;
    s32 i;
    u32 packed = g_hudBrightness;

    u32 t = packed >> 5;
    u32 hi = t << 16;
    u32 lo = (t << 8) | SPRT_CODE;

    packed = (hi | lo) | t;

    for (i = 0; i < 2; i++) {
        ret = func_80031A18(ot, ret, i, packed);
    }

    return ret;
}


/**
 * @brief Deactivate an animation entry by clearing the active flag.
 * @param idx Entry index into D_80083772.
 */
void clearAnimEntryActive(s32 idx) {
    AnimEntry *entry = D_80083772;
    entry = &entry[idx];
    entry->flags &= ~ANIM_ENTRY_ACTIVE;
}


/**
 * @brief Update an animation entry's input and optionally recompute current.
 *
 * Sets inputStart to the new value and interpolates via lerpRange.
 * With @ref ANIM_ENTRY_SNAP set, stores the interpolated result to current.
 *
 * @param idx   Entry index into D_80083772.
 * @param value New inputStart value.
 */
void updateAnimEntry(s32 idx, s32 value) {
    AnimEntry *entry = D_80083772;
    s32 result;

    entry = &entry[idx];
    result = lerpRange(entry->rangeStart, entry->rangeEnd, value, entry->inputEnd);
    entry->inputStart = value;
    if (entry->flags & ANIM_ENTRY_SNAP) {
        entry->current = result;
    }
}


/**
 * @brief Copy the first 4 bytes of data into an animation entry.
 *
 * @param idx Entry index into D_80083772.
 * @param src Source for the 4-byte copy.
 */
static void copyAnimEntryField(s32 idx, u16 *src) {
    AnimEntry *entry = D_80083772;
    entry = &entry[idx];
    memcpy(&entry->x, src, 4);
}


/**
 * @brief Initialize an animation entry with full parameters.
 *
 * Copies source data, sets the active flag, configures interpolation
 * range and input values, then computes the initial current value.
 *
 * @param idx     Entry index.
 * @param flags   Flags byte (ORed with 0x80 for active).
 * @param src     Source data pointer (first 4 bytes copied).
 * @param start   Range start value.
 * @param end     Range end value.
 * @param inStart Input start value.
 * @param inEnd   Input end value.
 */
static void initAnimEntry(s32 idx, s32 flags, u16 *src, s32 start, s32 end, s32 inStart, s32 inEnd) {
    AnimEntry *entry = D_80083772;
    s32 activeFlags = flags | ANIM_ENTRY_ACTIVE;

    entry = &entry[idx];
    copyAnimEntryField(idx, src);
    entry->flags = activeFlags;
    entry->rangeStart = start;
    entry->rangeEnd = end;
    entry->inputStart = inStart;
    entry->inputEnd = inEnd;
    entry->current = lerpRange(start, end, inStart, inEnd);
}



/**
 * @brief Initialize an animation entry with default inEnd (0x60).
 *
 * Wrapper for initAnimEntry with a fixed inEnd of 0x60.
 */
void setupAnimEntry(s32 idx, s32 flags, u16 *src, s32 start, s32 end, s32 inStart) {
    initAnimEntry(idx, flags & 0xFF, src, start, end, inStart, 0x60);
}


/**
 * @brief Initialize an animation entry with all parameters.
 *
 * Wrapper for initAnimEntry passing all arguments through.
 */
void setupAnimEntryFull(s32 idx, s32 flags, u16 *src, s32 start, s32 end, s32 inStart, s32 inEnd) {
    initAnimEntry(idx, flags & 0xFF, src, start, end, inStart, inEnd);
}


/**
 * @brief Clear all animation entry flags, marking both as inactive.
 */
void clearAnimEntries(void) {
    AnimEntry *entry = D_80083772;
    s32 i;

    for (i = 0; i < 2; i++, entry++) {
        entry->flags = 0;
    }
}



/* ========================================================================
 * Likely file boundary: the HUD display buffers start here, after the
 * gauges' clearAnimEntries.
 * ======================================================================== */


/**
 * @brief Get a pointer to the global buffer D_80085134.
 * @return Address of D_80085134.
 */
u8 *getBattleBuffer1(void) {
    return D_80085134;
}

/**
 * @brief Get a pointer to the global buffer D_80083938.
 * @return Address of D_80083938.
 */
static u8 *getBattleBuffer2(void) {
    return D_80083938;
}

/**
 * @brief Spin until the GPU is idle.
 *
 * Polls IsIdleGPU with a count of 1 until it stops returning -1, i.e. until
 * the GPU is ready for commands.
 */
void waitGpuIdle(void) {
    while (IsIdleGPU(1) == -1) {
    }
}


/**
 * @brief Return the base address of the battle allocation region.
 * @return 0x801F4000 (fixed address in PS1 RAM).
 */
u32 getBattleAllocBase(void) {
    return 0x801F4000;
}


/**
 * @brief Return the size of the battle allocation region.
 * @return 0x4000 (16384 bytes / 16 KB).
 */
s32 getBattleAllocSize(void) {
    return 0x4000;
}


/**
 * @brief Flip the double-buffered HUD ordering table.
 *
 * Selects the inactive buffer from D_80083920, sets it as active,
 * clears its OT, and resets the packet allocation pointer.
 */
void flipBattleOtBuffer(void) {
    HudDisplayBuf *buf;
    HudDisplayBuf *active;

    buf = D_80083920[0];
    if (D_80083918 == buf) {
        buf = D_80083920[1];
    }
    D_80083918 = buf;
    ClearOTag(buf->ot, 2);
    active = D_80083918;
    active->pktAlloc = &active->pktBase;
}


/**
 * @brief Put the HUD draw environment at the head of the HUD ordering table, then
 * splice the table into the GPU's current drawing.
 *
 * BreakDraw stops the GPU's linked-list DMA and returns the address it stopped at,
 * or -1 while it cannot; it is tried up to 500 times. ContinueDraw then inserts the
 * HUD table and continues from that address.
 */
void func_80032010(void) {
    HudDisplayBuf *buf;
    DR_ENV *env;
    u32 *ot;
    u32 *next;
    s32 i;

    buf = D_80083918;
    env = buf->pktAlloc;
    ot = buf->ot;
    SetDrawEnv(env, &buf->draw);
    addPrimFast(ot, env, s2);
    env++;
    D_80083918->pktAlloc = env;
    for (i = 0; i < 500; i++) {
        next = BreakDraw();
        if (next != (u32 *)-1) {
            break;
        }
    }
    if (next != (u32 *)-1) {
        ContinueDraw(ot, next);
    }
}


/**
 * @brief Set up the HUD display buffer pair for 384x224 double buffering at VRAM x=0
 * and x=512, each buffer drawing where the other displays, clearing to black.
 *
 * The screen is moved down 8 lines, and 24 more when GetVideoMode reports PAL.
 */
void func_800320BC(void) {
    s32 i;

    SetDefDispEnv(&D_80083920[0]->disp, 0, 0, 384, 224);
    SetDefDispEnv(&D_80083920[1]->disp, 512, 0, 384, 224);
    SetDefDrawEnv(&D_80083920[1]->draw, 0, 0, 384, 224);
    SetDefDrawEnv(&D_80083920[0]->draw, 512, 0, 384, 224);
    for (i = 0; i < 2; i++) {
        D_80083920[i]->draw.isbg = 1;
        setRGB0(&D_80083920[i]->draw, 0, 0, 0);
        D_80083920[i]->disp.screen.y += 8;
        D_80083920[i]->disp.screen.h = 224;
        if (GetVideoMode() == MODE_PAL) {
            D_80083920[i]->disp.screen.y += 24;
        }
    }
}


/**
 * @brief Start a HUD frame: flip the HUD ordering table, clear the screen with a
 * black 384x224 tile, then call func_80032010.
 */
void renderBattleFrame(void) {
    HudDisplayBuf *buf;
    TILE *tile;
    u32 *ot;

    flipBattleOtBuffer();
    buf = D_80083918;
    tile = buf->pktAlloc;
    ot = buf->ot; /* read before the tile is filled, the retail load order */
    setTile(tile);
    setRGB0(tile, 0, 0, 0);
    setXY0(tile, 0, 0);
    setWH(tile, 384, 224);
    addPrimFast(ot, tile, s0);
    D_80083918->pktAlloc = tile + 1;
    func_80032010();
}

