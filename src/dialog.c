#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "battle.h"
#include "dialog.h"
#include "numstr.h"
#include "btl_entity.h"
#include "btl_display.h"
#include "btl_anim.h"
#include "btl_anim_packet.h"
#include "ui/icon.h"
#include "snd_sfx.h"
#include "drawbar.h"
#include "menu_tint.h"

/** @brief r, g and b of a colour word shifted right once, without the bits
 * each channel took from the one above. */
#define RGB_HALF_MASK 0x7F7F7F

/** @brief Semi-transparency option bit of a primitive in the colour word. */
#define SPRT_ABE (SPRT_CODE_ABE << SPRT_CODE_SHIFT)

/** @brief Draw-offset command word: the code, then 11-bit x and y fields. */
#define DR_OFFSET_CODE 0xE5000000
#define DR_OFFSET_COORD_MASK 0x7FF
#define DR_OFFSET_Y_SHIFT 11

/** @brief The next-page marker sits this many pixels in from the window's bottom-right corner. */
#define NEXT_PAGE_MARKER_INSET 24

/** @brief Values of @c Dialog.seqState, the text state machine run by updateDialog. */
enum {
    DIALOG_SEQ_START, /**< Clear the button auto-repeat state. */
    DIALOG_SEQ_TICK, /**< Advance the character timer by the text speed. */
    DIALOG_SEQ_PRINT, /**< Wait for the character timer, then print. */
    DIALOG_SEQ_NEXT_CHAR, /**< Decode the next character or command. */
    DIALOG_SEQ_NEWLINE, /**< Scroll first if the window is full. */
    DIALOG_SEQ_SCROLL, /**< Scroll up by one line. */
    DIALOG_SEQ_END, /**< End of the message. */
    DIALOG_SEQ_DONE, /**< Finished; nothing left to run. */
    DIALOG_SEQ_PAGE, /**< Page break: set up the next-page marker. */
    DIALOG_SEQ_PAGE_RELEASE, /**< Wait until only the d-pad is held. */
    DIALOG_SEQ_PAGE_WAIT, /**< Wait for Cross or Square, then start the next page. */
    DIALOG_SEQ_CHOICE_START,
    DIALOG_SEQ_CHOICE_RELEASE, /**< Wait until no button is pressed. */
    DIALOG_SEQ_CHOICE, /**< Move the choice cursor, confirm or cancel. */
    DIALOG_SEQ_WAIT_START,
    DIALOG_SEQ_WAIT, /**< Count down @c Dialog.waitTimer. */
    DIALOG_SEQ_DPAD_RELEASE_START,
    DIALOG_SEQ_DPAD_RELEASE /**< Wait for the d-pad to be released, then offer the choice. */
};

/** @brief Characters of a decoded message that the text state machine acts on. */
enum {
    MSG_END = 0x00,
    MSG_NEW_PAGE = 0x01,
    MSG_NEWLINE = 0x02,
    MSG_NEW_PAGE_MARKED = 0x07 /**< Page break that shows the next-page marker. */
};

/** @brief Command bytes returned in bits 8-15 by nextDialogChar. */
enum {
    MSG_CMD_ICON = 0x05, /**< Argument is an icon, drawn as a multi-cell glyph. */
    MSG_CMD_COLOR = 0x06, /**< Argument is the text colour. */
    MSG_CMD_SPEED = 0x08, /**< Argument sets the text speed (guess from its use). */
    MSG_CMD_WAIT = 0x09 /**< Argument is a delay in frames. */
};

/** @brief Text colour index of white, the colour a message starts in. */
#define DIALOG_COLOR_WHITE 7

/** @brief Command arguments are stored offset by this value (' '). */
#define MSG_ARG_BASE 0x20

/** @brief Bit of @c DialogGlobalState.textBlinkClock that dims blinking text; set for 16 of every 32 frames. */
#define DIALOG_TEXT_BLINK_DIM 0x10

/** @brief @c Dialog.firstChoice / @c lastChoice when the message offers no choice. */
#define DIALOG_NO_CHOICE 0xFF

/** @brief Size of the $gp scratch buffer a message is decoded into. */
#define DIALOG_MSG_BUF_SIZE 128

/** @brief Height of one text line in pixels; a scroll steps @c scrollY once per frame until a line has passed. */
#define DIALOG_LINE_HEIGHT 16

/** @brief The clamp rect of a message window sits this many pixels inside its bound rect. */
#define DIALOG_CLAMP_INSET 6

/** @brief Text starts this many pixels right of and below the window's text origin. */
#define DIALOG_TEXT_MARGIN 2

/** @brief Lines that hold choices are indented by this much, room for the cursor. */
#define DIALOG_CHOICE_INDENT 32

/** @brief Position of the choice cursor within its line. */
#define DIALOG_CURSOR_X 4
#define DIALOG_CURSOR_Y 5

/** @brief Text glyphs are this many pixels square, and a font texture row holds this many. */
#define TEXT_GLYPH_SIZE 12
#define TEXT_GLYPHS_PER_ROW 21

/** @brief CLUT of text colour 0; colour n uses the CLUT n rows below it. */
#define TEXT_CLUT_X 288
#define TEXT_CLUT_Y 224

/** @brief Bit of a text glyph number that selects the font's second texture page. */
#define TEXT_GLYPH_PAGE2 0x400

/** @brief Glyph index within its texture page. */
#define TEXT_GLYPH_INDEX_MASK 0x3FF

/** @brief Draw-mode words for the font's first and second texture page. */
#define TEXT_TPAGE_PAGE1 0xE100041F
#define TEXT_TPAGE_PAGE2 0xE100041D

/**
 * @brief Bit of @c Dialog.ctrl.raw: @c ctrl.bits.nextPageMarker, the window shows its
 * blinking next-page marker.
 */
#define DIALOG_CTRL_NEXT_PAGE_MARKER 0x00800000

/** @brief Position of @c Dialog.ctrl.bits.nextPageMarkerBlink inside @c ctrl.raw. */
#define DIALOG_CTRL_NEXT_PAGE_MARKER_BLINK_SHIFT 16

/**
 * @brief Bit of the 7-bit blink counter @c Dialog.ctrl.bits.nextPageMarkerBlink.
 *
 * Set for 16 of every 32 ticks; the next-page marker is blanked while it is set.
 */
#define DIALOG_NEXT_PAGE_MARKER_BLINK_OFF 0x10

extern u32 g_textBlinkTint; // Same as g_dialogs.state.textBlinkTint
extern u8 D_800834D8[];
static void updateTextBlinkColors(void);
static void applyWindowBrightness(s32 index);
static void drawNextPageMarker(P_TAG *ot, Dialog *entry);
static TSPRT *drawTextIcon(P_TAG *ot, TSPRT *head, s32 idx, s32 x, s32 y);
static inline void updateOpenDialogScale(s32 index);
static void updateDialog(s32 index, u32 input, u32 repeat);
static inline u32 addTextGlyph(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy);
static void drawDialogText(P_TAG *ot, Dialog *entry);
static void drawDialogContents(s32 index, P_TAG *ot);
static s32 autoRepeatPadChannel(EngineState *engine, PadPort *port, DialogSystem *sys, u16 newVal, s32 channel);
static void drawDialog(P_TAG *ot, s32 index);
static inline u32 linkPacket(u32 head, void *p);
static DR_AREA *renderDialogEntity(P_TAG *ot, BattleDisplayEntity *entity, u32 pkt);
static u8 *updateDialogEntity(BattleDisplayEntity *entity, u32 input, u32 repeat);
static void scaleDialogRect(RECT *rect, s32 scale, s32 arg2);
static void initDialog(s32 idx);
static s32 getCharWidth(s32 idx);
static s32 measureText(u8 *s, s32 flag);


/**
 * @brief Refresh the colours that blinking text is tinted with.
 *
 * Text colours 8-15 are the blinking variants of colours 0-7: message text
 * takes its tint from @c state.textBlinkTint instead of @c state.textTint, and menu
 * text from @c g_menuTint[MENU_TINT_BLINK] instead of @c g_menuTint[MENU_TINT_NORMAL]. While
 * @ref DIALOG_TEXT_BLINK_DIM is set in @c state.textBlinkClock each copy is a grey at
 * 75% of its base's red channel (the brightness setters only ever store greys);
 * otherwise it equals the base. The clock steps once per frame, so the text blinks
 * between full and 75% brightness every 16 frames.
 */
static void updateTextBlinkColors(void) {
    u32 textBlinkTint = g_dialogs.state.textTint;
    u8 blinkClock = g_dialogs.state.textBlinkClock;
    u32 menuBlinkTint = g_menuTint[MENU_TINT_NORMAL];

    if (blinkClock & DIALOG_TEXT_BLINK_DIM) {
        textBlinkTint &= 0xFF;
        textBlinkTint = (textBlinkTint * 3) >> 2;
        textBlinkTint = textBlinkTint | ((textBlinkTint << 16) | (textBlinkTint << 8));
        textBlinkTint |= SPRT_CODE;

        menuBlinkTint &= 0xFF;
        menuBlinkTint = (menuBlinkTint * 3) >> 2;
        menuBlinkTint = menuBlinkTint | ((menuBlinkTint << 16) | (menuBlinkTint << 8));
        menuBlinkTint |= SPRT_CODE;
    }

    g_dialogs.state.textBlinkTint = textBlinkTint;
    g_menuTint[MENU_TINT_BLINK] = menuBlinkTint;
}


/**
 * @brief Step the text blink clock by one frame.
 *
 * Counts @c state.textBlinkClock down and refreshes the blink colours
 * (updateTextBlinkColors).
 */
void tickTextBlink(void) {
    g_dialogs.state.textBlinkClock--;
    updateTextBlinkColors();
}


/**
 * @brief Set the text brightness: store a grey in @c state.textTint and refresh the blink copies.
 * @param brightness Brightness, @ref BRIGHTNESS_NORMAL = normal; divided by 32, its
 * low 8 bits become r, g and b (0x80 draws the font unmodulated).
 */
void setTextBrightness(s32 brightness) {
    brightness /= 32;
    brightness &= 0xFF;
    brightness |= (brightness << 16) | (brightness << 8);
    brightness |= SPRT_CODE;
    g_dialogs.state.textTint = brightness;
    updateTextBlinkColors();
}


/**
 * @brief Apply a message window's @c brightness (0x1000 = normal) to its text and next-page marker.
 *
 * Sets the text brightness (setTextBrightness) and the tint of the window's
 * next-page marker (setNextPageMarkerBrightness).
 *
 * @param index Dialog index.
 */
static void applyWindowBrightness(s32 index) {
    Dialog *entry = &g_dialogs.entries[index];
    s32 val = entry->brightness;
    setTextBrightness(val);
    setNextPageMarkerBrightness(val);
}


/**
 * @brief Set the linked battle entity index on a dialog.
 * @param idx Dialog index.
 * @param val Battle entity index.
 */
void setDialogEntityIndex(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->entityIdx = val;
}


/**
 * @brief Swap the state of a dialog, returning the old value.
 * @param idx Dialog index.
 * @param val New state value.
 * @return Previous state value.
 */
s32 swapDialogState(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    s32 old = entry->state;
    entry->state = val;
    return old;
}


/**
 * @brief Get the state of a dialog.
 * @param idx Dialog index.
 * @return State value (0 = inactive, 1 = active).
 */
s32 getDialogState(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];
    return entry->state;
}


/**
 * @brief Set a dialog's text speed and restart its character timer (@c charTimer).
 * @param idx Dialog index.
 * @param val Text speed: added to the timer each frame (0x1000 = a character a frame).
 */
void setDialogTextSpeed(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->textSpeed = val;
    entry->charTimer = 0;
}


/**
 * @brief Set a dialog's open scale.
 * @param idx Dialog index.
 * @param val Open scale: 0 shut, 0x1000 fully open.
 */
void setOpenDialogScale(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->openDialogScale = val;
}


/**
 * @brief Set a dialog's open step, added to its open scale each frame.
 * @param idx Dialog index.
 * @param val Open step; negative closes.
 */
void setOpenDialogStep(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->openDialogStep = val;
}


/**
 * @brief Get a dialog's open scale.
 * @param idx Dialog index.
 * @return The open scale: 0 once shut, 0x1000 once fully open.
 */
s32 getOpenDialogScale(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];
    return entry->openDialogScale;
}


/**
 * @brief Draw the blinking next-page marker of a message window.
 *
 * While the entry's marker bit is set and its blink counter is in the visible
 * half of its cycle, emits icon @ref ICON_NEXT_PAGE_MARKER of @c g_iconTable
 * @ref NEXT_PAGE_MARKER_INSET pixels in from the window's bottom-right
 * corner, tinted with @c g_engine.nextPageMarkerColor. Every cell of the icon becomes one
 * @c TSPRT taken from the display-list packet buffer and linked into @p ot;
 * the advanced packet cursor is stored back afterwards.
 *
 * Per cell: the u/v/CLUT word is the cell's own plus the font CLUT; the
 * texture page is the font page with the cell's blend rate; the colour word
 * gets the cell's semi-transparency bit and is forced to a SPRT code;
 * width/height are copied and the cell's signed offsets are added to the
 * position.
 *
 * @note Same emitter as @ref drawTextIcon plus the colour masking, with the
 * same two load-bearing spellings. The @c (u8) narrowing of the blend
 * rate is a no-op on the value (at most 0x60) but hides its range from
 * the compiler, which otherwise proves @c _get_mode's 0x9FF mask
 * redundant and drops it; the original keeps it. The colour word is
 * built one operation per statement because the original computes the
 * whole chain in the result's own register, which a single expression
 * does not. @c head is handed to @c p and taken back after the loop,
 * the way btl_anim.c threads its packet cursor.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param entry Dialog the marker belongs to.
 */
static void drawNextPageMarker(P_TAG *ot, Dialog *entry) {
    IconTable *table;
    IconCell *cell;
    TSPRT *p;
    s32 head;
    u32 link;
    u32 flags;
    u32 word;
    s32 tpage;
    u32 val;
    u32 color;
    s32 n;
    s32 x;
    s32 y;

    /* On the bitfields gcc folds these two tests into one masked compare, or
     * extracts the marker bit with a shift; only the whole word gives the
     * original and/branch pair. */
    flags = entry->ctrl.raw;
    if (!(flags & DIALOG_CTRL_NEXT_PAGE_MARKER) ||
        ((flags >> DIALOG_CTRL_NEXT_PAGE_MARKER_BLINK_SHIFT) & DIALOG_NEXT_PAGE_MARKER_BLINK_OFF)) {
        return;
    }

    table = &g_iconTable;
    head = getDisplayListHead();
    cell = (IconCell *)table; /* seeding the cursor from its own copy keeps the offset add in the delay slot */
    p = (TSPRT *)head;
    word = table->descriptors[ICON_NEXT_PAGE_MARKER];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);
    x = entry->rect.w - NEXT_PAGE_MARKER_INSET;
    y = entry->rect.h - NEXT_PAGE_MARKER_INSET;
    color = g_engine.nextPageMarkerColor;

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & ICON_UVCLUT_MASK;
        val += getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
        setIconUVClut(p, val);

        val = (word >> ICON_ABR_SHIFT) & ICON_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y);

        val = word >> ICON_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
        val &= SPRT_RGB_MASK;
        val |= SPRT_CODE;
        setIconRGBC(p, val);

        word = cell->metrics;
        val = word & ICON_WH_MASK;
        setIconWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    head = (s32)p;
    storeGpuPacket(head);
}


/**
 * @brief Draw a message window's choice cursor, next-page marker and text.
 *
 * Once the whole message has been typed (@c typingDone), icon
 * @ref ICON_CHOICE_CURSOR goes in front of the selected line @c choiceCursor, grey
 * at the window's @c brightness (0x1000 = full), followed by a
 * draw-area packet for the window entity's clipped bound rect. Then the next-page
 * marker (drawNextPageMarker), the text (drawDialogText) and a draw-mode packet that
 * resets the texture page. A window without a message draws nothing.
 *
 * @param index Dialog index.
 * @param ot Ordering table the packets are linked into.
 */
static void drawDialogContents(s32 index, P_TAG *ot) {
    Dialog *entry = &g_dialogs.entries[index];
    IconTable *table;
    DR_AREA *area;
    DR_TPAGE *tpage;
    BattleDisplayEntity *ent;
    u32 link;
    u32 link2;
    s32 brightness;
    s32 colour;
    s32 y;
    s32 val;
    s32 line;

    if (entry->dataPtr == NULL) {
        return;
    }
    table = &g_iconTable;
    brightness = entry->brightness;
    if (entry->firstChoice != DIALOG_NO_CHOICE) {
        if (entry->typingDone == 1) {
            /* val holds the entity index here and the mode word below: two
             * variables swap the registers of the closing packet. */
            val = entry->entityIdx;
            ent = getBattleEntity(val);
            line = entry->choiceCursor;
            y = line * DIALOG_LINE_HEIGHT + DIALOG_CURSOR_Y;
            area = (DR_AREA *)getDisplayListHead();
            if (table != NULL) { /* the original tests the fixed table address */
                colour = brightness / 32;
                colour = SPRT_CODE | (colour << 16) | (colour << 8) | colour;
                area = drawIcon(ot, area, ICON_CHOICE_CURSOR, DIALOG_CURSOR_X, y, colour);
            }
            SetDrawArea(area, &ent->clipBound.rect);
            addPrimFastWithTempOperand(ot, area, link);
            storeGpuPacket((u32)(area + 1));
        }
    }
    drawNextPageMarker(ot, entry);
    drawDialogText(ot, entry);
    tpage = (DR_TPAGE *)getDisplayListHead();
    setlen(tpage, 1);
    val = _get_mode(1, 0, 0);
    tpage->code[0] = val;
    addPrimFastWithTempOperand(ot, tpage, link2);
    storeGpuPacket((u32)(tpage + 1));
}


/**
 * @brief Scale a dialog rectangle about its center by a fixed-point factor.
 *
 * @p scale is a signed Q12 fixed-point multiplier where @c 0x1000 (4096 == ONE)
 * is 1.0; at exactly 1.0 the rectangle is left unchanged. Otherwise each axis is
 * recentered: the origin moves to @c center @c - @c (dimension @c * @c scale) @c /
 * @c 8192 (unscale the Q12 product, then halve) and the dimension becomes
 * @c (dimension @c * @c scale) @c / @c 4096.
 *
 * Written in the in-place register-reuse style of the sibling RECT helpers
 * (cf. @ref func_8002B080, and @c func_800A3398 which scales a RECT the same
 * way): the packed @c s16 pairs are read once as words and each value is
 * progressively transformed in its own variable, which keeps gcc 2.7.2 from
 * spilling copies. The @c do/while(0) wraps the arithmetic as a single basic
 * block so the compiler emits both extractions before the multiplies rather
 * than interleaving them.
 *
 * @param rect Rectangle to scale in place.
 * @param scale Q12 fixed-point scale factor (@c 0x1000 == 1.0).
 * @param arg2 Unused by this routine (the caller passes @c openDialogStep).
 */
static void scaleDialogRect(RECT *rect, s32 scale, s32 arg2) {
    s32 x, y, w, h, prodW, prodH;

    if (scale == 0x1000) {
        return;
    }

    x = *(s32 *)&rect->x;
    y = x >> 16;
    x = x << 16;
    x = x >> 16;
    w = *(s32 *)&rect->w;
    h = w >> 16;
    w = w << 16;
    do {
        w = w >> 16;
        x = x + ((u32)w / 2);
        y = y + ((u32)h / 2);
        prodW = w * scale;
        prodH = h * scale;
        w = (u32)prodW / 4096;
        rect->x = x - ((u32)prodW / 8192);
        rect->w = w;
        h = (u32)prodH / 4096;
        rect->y = y - ((u32)prodH / 8192);
        rect->h = h;
    } while (0);
}


/**
 * @brief Give a dialog the input focus (@c state.focusedDialog): only the focused
 * dialog reads the pad, to turn pages and pick a choice.
 * @param idx Dialog index, or -1 for none.
 */
void setFocusedDialog(s32 idx) {
    g_dialogs.state.focusedDialog = idx;
}


/**
 * @brief Get the dialog with the input focus (@c state.focusedDialog).
 * @return Dialog index, or -1 for none.
 */
s32 getFocusedDialog(void) {
    return g_dialogs.state.focusedDialog;
}


/**
 * @brief Get the choice the player confirmed in a dialog.
 *
 * Returns -1 until the player has confirmed (@c choiceMade), then the selected
 * line (@c choiceCursor) minus the first choice line (@c firstChoice).
 *
 * @param idx Dialog index.
 * @return The chosen answer (0 = the first choice), or -1.
 */
s32 getDialogChoice(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];
    Dialog *e2;
    s32 always;
    e2 = entry;
    always = 1; /* Regalloc */
    if (always) {
        if (entry->choiceMade == 0) {
            return -1;
        }
        return e2->choiceCursor - e2->firstChoice;
    }
}


/**
 * @brief Keyboard-style auto-repeat for one pad channel's edge bits.
 *
 * Latches @p newVal into @c sys->state.repeatLatched[channel] and, using the per-channel
 * mask, decides whether the (masked) edge bits should fire this frame. If the new
 * and previous masked bits overlap (a held cue) it ticks the per-channel countdown
 * @c sys->state.repeatCounters[channel]: while it is running the event is suppressed
 * (returns 0), and when it expires the countdown reloads to the repeat interval and
 * fires. When the bits do not overlap the countdown resets to the restart delay and
 * fires. Mirrors @c func_800A29D4 (the Triple Triad edge auto-repeat).
 *
 * @param engine Engine state; @c repeatDelays packs the two delays.
 * @param port Pad port; @c unk10[channel] is the channel's button mask.
 * @param sys Dialog system (@c &engine->dialogs); holds the stored bits and counters.
 * @param newVal Raw new edge bitmask for this frame.
 * @param channel Pad channel index, 0..3.
 * @return The masked edge bits that should fire this frame, or 0 while suppressed.
 */
static s32 autoRepeatPadChannel(EngineState *engine, PadPort *port, DialogSystem *sys, u16 newVal, s32 channel) {
    s32 counter;
    s32 restartDelay;
    s32 repeatInterval;
    u16 mask;
    u16 prevMasked;

    prevMasked = sys->state.repeatLatched[channel];
    sys->state.repeatLatched[channel] = newVal;
    restartDelay = engine->repeatDelays.hword;
    counter = sys->state.repeatCounters[channel];
    mask = port->unk10[channel];

    repeatInterval = restartDelay >> 8;
    restartDelay &= 0xFF;
    newVal &= mask;
    prevMasked &= mask;
    if (newVal & prevMasked) {
        if (newVal != prevMasked) {
            counter = restartDelay;
        }
        counter--;
        if (counter < 0) {
            counter = repeatInterval;
        } else {
            newVal = 0;
        }
    } else {
        counter = restartDelay;
    }
    sys->state.repeatCounters[channel] = counter;
    return newVal;
}


/**
 * @brief Auto-repeat the four channels of this frame's pad bits.
 *
 * Runs autoRepeatPadChannel on @p input for channels 0-3 against the port linked
 * to port 0, and ORs the bits that fire.
 *
 * @param input Pad bits of this frame.
 * @return The bits that fire this frame.
 */
s32 autoRepeatPad(s32 input) {
    DialogSystem *sys = &g_engine.dialogs;
    EngineState *engine = &g_engine;
    u16 bits = input;
    PadPort *port = &engine->ports[engine->ports[0].linkedIdx];
    u16 result;

    result = 0;
    result |= autoRepeatPadChannel(engine, port, sys, bits, 0);
    result |= autoRepeatPadChannel(engine, port, sys, bits, 1);
    result |= autoRepeatPadChannel(engine, port, sys, bits, 2);
    result |= autoRepeatPadChannel(engine, port, sys, bits, 3);
    return result;
}


/**
 * @brief Step a dialog's open/close animation and size its entity to it.
 *
 * Adds @c openDialogStep to @c openDialogScale, clamped to 0..ONE (1.0).
 * While the dialog is open, its entity is enabled and its bound rect is the
 * dialog rect scaled by @c openDialogScale, with the clamp rect
 * @ref DIALOG_CLAMP_INSET pixels inside that. At 0 the entity is disabled.
 *
 * @param index Dialog index.
 */
static inline void updateOpenDialogScale(s32 index) {
    Dialog *entry = &g_dialogs.entries[index];
    s32 scale;
    s32 entityId;

    scale = entry->openDialogScale;
    scale += entry->openDialogStep;
    scale = CLAMP(scale, 0, ONE);
    entityId = entry->entityIdx;
    entry->openDialogScale = scale;

    if (scale != 0) {
        RECT rect;

        setBattleEntityField35(entityId, 1);
        rect = entry->rect;
        scaleDialogRect(&rect, scale, entry->openDialogStep);
        setBattleEntityBoundRect(entityId, &rect);
        rect.x += DIALOG_CLAMP_INSET;
        rect.y += DIALOG_CLAMP_INSET;
        rect.w -= DIALOG_CLAMP_INSET * 2;
        rect.h -= DIALOG_CLAMP_INSET * 2;
        setBattleEntityRectClamp(entityId, &rect);
    } else {
        setBattleEntityField35(entityId, 0);
        entry->state = 0;
    }
}

/**
 * @brief Per-frame update of one message window: open/close step, then run its text.
 *
 * After the open/close step (updateOpenDialogScale), a fully open window with a message runs
 * the @c DIALOG_SEQ_* state machine in @c seqState. The message is decoded into
 * a @ref DIALOG_MSG_BUF_SIZE-byte buffer taken from the $gp area: characters appear at the text
 * speed (@c textSpeed added to the timer @c charTimer each frame), full
 * windows scroll, page breaks wait for Cross or Square, and at the end a
 * message with choices lets the player move the cursor @c choiceCursor between
 * @c firstChoice and @c lastChoice. Cross or Square confirms (@c choiceMade = 1);
 * Triangle jumps to the cancel choice in @c ctrl.bits.cancelChoice, if one is set.
 *
 * Only the focused window (@c state.focusedDialog) reads the buttons.
 *
 * @param index Dialog index.
 * @param input Pressed buttons: bits 0-15 drive paging, bits 16-31 the choice
 * (the split is a guess from the tests made on each half).
 * @param repeat Auto-repeating buttons; Up and Down move the choice cursor.
 */
static void updateDialog(s32 index, u32 input, u32 repeat) {
    Dialog *entry;
    s32 *seqState;
    u8 *msgBuf;
    u32 pressed;
    u32 choicePressed;
    s32 state;
    s32 ch;
    s32 cmd;

    updateOpenDialogScale(index);

    entry = &g_dialogs.entries[index];
    if (g_dialogs.state.focusedDialog == index) {
        pressed = input & 0xFFFF;
        choicePressed = input >> 16;
    } else {
        pressed = 0;
        repeat = 0;
        choicePressed = 0;
    }

    if (entry->dataPtr != NULL && entry->openDialogScale >= ONE) {
        GP_ALLOC(msgBuf, DIALOG_MSG_BUF_SIZE);
        seqState = &entry->seqState;
        state = *seqState;
        /* Re-entering the switch is a goto, not a while/for: gcc treats a C
         * loop as one and hoists the constants 1 and 7 into saved registers,
         * which the original does not (93.99% with while (1) and continue). */
    dispatch:
        switch (state) {
        case DIALOG_SEQ_START:
            g_dialogs.state.repeatLatched[0] = 0;
            g_dialogs.state.repeatLatched[1] = 0;
            g_dialogs.state.repeatCounters[0] = 0;
            g_dialogs.state.repeatCounters[1] = 0;
            *seqState = DIALOG_SEQ_TICK;
            /* fallthrough */
        case DIALOG_SEQ_TICK:
            entry->charTimer += entry->textSpeed;
            /* fallthrough */
        case DIALOG_SEQ_PRINT:
            if (entry->textSpeed == 0 || entry->charTimer >= ONE) {
                entry->charTimer -= ONE;
                decodeMessageDirect(entry, msgBuf);
                state = DIALOG_SEQ_NEXT_CHAR;
                goto dispatch;
            }
            break;

        case DIALOG_SEQ_NEXT_CHAR:
            ch = msgBuf[entry->typedChars];
            if (ch == MSG_NEW_PAGE || ch == MSG_NEW_PAGE_MARKED) {
                state = DIALOG_SEQ_PAGE;
                goto dispatch;
            }
            ch = nextDialogChar(entry, msgBuf);
            cmd = ch >> 8;
            ch &= 0xFF;
            if (ch == MSG_NEWLINE) {
                state = DIALOG_SEQ_NEWLINE;
                goto dispatch;
            }
            if (ch == MSG_END) {
                state = DIALOG_SEQ_END;
                goto dispatch;
            }
            if (cmd == MSG_CMD_SPEED) {
                s32 speed = ch - MSG_ARG_BASE;

                if (speed == 0) {
                    speed = ONE;
                } else if (speed == 1) {
                    speed = 0;
                } else {
                    speed = ONE / (ch - (MSG_ARG_BASE + 1));
                }
                entry->textSpeed = speed;
            } else if (cmd == MSG_CMD_WAIT) {
                /* Through an s32 local: assigned straight to the u8 field,
                 * gcc folds the (u8) away and loses the andi. */
                s32 frames = (u8)ch - MSG_ARG_BASE;

                entry->waitTimer = frames;
                state = DIALOG_SEQ_WAIT_START;
                goto dispatch;
            } else if (cmd == MSG_CMD_COLOR) {
                entry->color = ch;
                state = DIALOG_SEQ_NEXT_CHAR;
                goto dispatch;
            } else {
                state = DIALOG_SEQ_PRINT;
                if (entry->textSpeed == 0) {
                    state = DIALOG_SEQ_NEXT_CHAR;
                }
                goto dispatch;
            }
            state = DIALOG_SEQ_NEXT_CHAR;
            goto dispatch;

        case DIALOG_SEQ_NEWLINE:
            state = DIALOG_SEQ_NEXT_CHAR;
            if (entry->typingRow >= entry->visibleRows) {
                entry->charTimer += ONE;
                entry->typingRow--;
                *seqState = DIALOG_SEQ_SCROLL;
                state = DIALOG_SEQ_SCROLL;
            }
            goto dispatch;

        case DIALOG_SEQ_SCROLL:
            entry->scrollY++;
            if (entry->scrollY % DIALOG_LINE_HEIGHT == 0) {
                *seqState = DIALOG_SEQ_TICK;
            }
            break;

        case DIALOG_SEQ_PAGE:
            if (ch == MSG_NEW_PAGE_MARKED) {
                entry->ctrl.bits.nextPageMarker = 1;
                entry->ctrl.bits.nextPageMarkerBlink = 0;
            } else {
                entry->ctrl.bits.nextPageMarker = 0;
            }
            *seqState = DIALOG_SEQ_PAGE_RELEASE;
            break;

        case DIALOG_SEQ_PAGE_RELEASE:
            entry->ctrl.bits.nextPageMarkerBlink++;
            pressed &= ~(PADLup | PADLright | PADLdown | PADLleft);
            if (pressed == 0) {
                *seqState = DIALOG_SEQ_PAGE_WAIT;
            }
            break;

        case DIALOG_SEQ_PAGE_WAIT:
            entry->ctrl.bits.nextPageMarkerBlink++;
            if (pressed & (PADRdown | PADRleft)) {
                entry->ctrl.bits.nextPageMarker = 0;
                entry->pageColor = entry->color;
                decodeMessageDirect(entry, msgBuf);
                nextDialogChar(entry, msgBuf);
                decodeMessageDirect(entry, msgBuf);
                *seqState = DIALOG_SEQ_TICK;
            }
            break;

        case DIALOG_SEQ_END:
            entry->typingDone = 1;
            state = DIALOG_SEQ_CHOICE_START;
            if (entry->firstChoice == DIALOG_NO_CHOICE) {
                *seqState = DIALOG_SEQ_DONE;
                state = DIALOG_SEQ_DONE;
            }
            goto dispatch;

        case DIALOG_SEQ_CHOICE_START:
            *seqState = DIALOG_SEQ_CHOICE_RELEASE;
            state = DIALOG_SEQ_CHOICE_RELEASE;
            goto dispatch;

        case DIALOG_SEQ_CHOICE_RELEASE:
            if (pressed == 0) {
                *seqState = DIALOG_SEQ_CHOICE;
            }
            break;

        case DIALOG_SEQ_WAIT_START:
            *seqState = DIALOG_SEQ_WAIT;
            /* fallthrough */
        case DIALOG_SEQ_WAIT:
            if (entry->waitTimer == 0) {
                *seqState = DIALOG_SEQ_TICK;
                state = DIALOG_SEQ_PRINT;
                goto dispatch;
            }
            entry->waitTimer--;
            break;

        case DIALOG_SEQ_CHOICE:
            if (choicePressed & (PADRdown | PADRleft)) {
                entry->choiceMade = 1;
                sendSpuCommand(2);
                *seqState = DIALOG_SEQ_DONE;
                state = DIALOG_SEQ_DONE;
                goto dispatch;
            }
            if (choicePressed & PADRup) {
                s8 cancel = entry->ctrl.bits.cancelChoice;
                s32 cancelChoice;

                if (cancel >= 0) {
                    cancelChoice = cancel;
                    sendSpuCommand(3);
                }
                /* With no cancel choice this clamps whatever cancelChoice
                 * held; the original reads the register uninitialised too. */
                entry->choiceCursor = CLAMP(cancelChoice, entry->firstChoice, entry->lastChoice);
            } else if (repeat & PADLup) {
                s32 cursor;
                s32 prev;

                cursor = entry->choiceCursor;
                prev = cursor;
                cursor--;
                if (cursor < entry->firstChoice) {
                    cursor = entry->lastChoice;
                }
                if (prev != cursor) {
                    sendSpuCommand(1);
                }
                entry->choiceCursor = cursor;
            } else if (repeat & PADLdown) {
                s32 cursor;
                s32 prev;

                cursor = entry->choiceCursor;
                prev = cursor;
                cursor++;
                if (entry->lastChoice < cursor) {
                    cursor = entry->firstChoice;
                }
                if (prev != cursor) {
                    sendSpuCommand(1);
                }
                entry->choiceCursor = cursor;
            }
            break;

        case DIALOG_SEQ_DPAD_RELEASE_START:
            *seqState = DIALOG_SEQ_DPAD_RELEASE;
            /* fallthrough */
        case DIALOG_SEQ_DPAD_RELEASE:
            if (!(pressed & (PADLup | PADLright | PADLdown | PADLleft))) {
                *seqState = DIALOG_SEQ_CHOICE_START;
            }
            break;
        }
        GP_FREE(DIALOG_MSG_BUF_SIZE);
    }
}


/**
 * @brief Start a message on a dialog.
 *
 * Clears fields, stores the data pointer, starts both text colours at white,
 * calls resetDialogTyping, and marks the message as offering no choice.
 *
 * @param index Dialog index.
 * @param data The message (or NULL).
 */
void setDialogMessage(s32 index, u8 *data) {
    Dialog *entry = &g_dialogs.entries[index];

    entry->typingDone = 0;
    entry->dataPtr = data;
    entry->linePtr = data;
    entry->scrollY = 0;
    entry->seqState = DIALOG_SEQ_START;
    entry->pageColor = DIALOG_COLOR_WHITE;
    entry->color = entry->pageColor;

    resetDialogTyping(entry);

    entry->firstChoice = DIALOG_NO_CHOICE;
    entry->lastChoice = DIALOG_NO_CHOICE;
    entry->choiceMade = 0;
}


/**
 * @brief Skips past a given number of null-terminated strings, then calls setDialogMessage.
 *
 * Advances @p a1 past @p a2 null-terminated strings by scanning bytes until
 * null is found for each string. After skipping, calls setDialogMessage with
 * the original @p a0 and @p a1.
 *
 * @param index Dialog index.
 * @param data Pointer to the start of the string data.
 * @param count Number of strings to skip.
 */
void setDialogMessageAfterStrings(s32 index, u8 *data, s32 count) {
    while (count > 0) {
        while (*data++ != 0) {
        }
        count--;
    }
    setDialogMessage(index, data);
}


/**
 * @brief Start a message with choices on a dialog.
 *
 * Lines @p min to @p max of the message are the choices; the cursor starts on
 * @p val, clamped to them, and Triangle jumps to the choice @p arg5.
 *
 * @param arg0 Dialog index.
 * @param data The message.
 * @param min First choice line.
 * @param max Last choice line.
 * @param val Line the cursor starts on.
 * @param arg5 Cancel choice (@c ctrl.bits.cancelChoice).
 */
void setDialogChoiceMessage(s32 arg0, u8 *data, s32 min, s32 max, s32 val, s32 arg5) {
    val = CLAMP(val, min, max);

    setDialogMessage(arg0, data);
    setDialogChoices(arg0, min, max, arg5);
    setDialogChoiceCursor(arg0, val);
}


/**
 * @brief Start a message with choices on a dialog, after skipping @p count
 * null-terminated strings of @p str (see setDialogChoiceMessage).
 *
 * @param arg0 Dialog index.
 * @param str The strings; the message is the one after the first @p count.
 * @param count Strings to skip.
 * @param min First choice line.
 * @param max Last choice line.
 * @param val Line the cursor starts on.
 * @param arg6 Cancel choice (@c ctrl.bits.cancelChoice).
 */
void setDialogChoiceMessageAfterStrings(s32 arg0, u8 *str, s32 count, s32 min, s32 max, s32 val, s32 arg6) {
    s32 clamped;
    while (count > 0) {
        while (*str++) { }
        count--;
    }

    clamped = val;
    clamped = CLAMP(clamped, min, max);

    setDialogMessage(arg0, str);
    setDialogChoices(arg0, min, max, arg6);
    setDialogChoiceCursor(arg0, clamped);
}


/**
 * @brief Draw an active message window.
 *
 * If the entry is active (state != 0), switches GP to scratchpad,
 * runs the entry's @c drawCallback if set, applies the window's brightness
 * to its text and marker (applyWindowBrightness), and draws the window's cursor,
 * marker and text via drawDialogContents. Restores GP before returning.
 *
 * @param ot Ordering table, passed to the hook and drawDialogContents.
 * @param index Dialog index.
 */
static void drawDialog(P_TAG *ot, s32 index) {
    Dialog *entry = &g_dialogs.entries[index];

    if (entry->state != 0) {
        s32 savedGp;
        s16 saved;
        s32 ret;

        GP_SAVE_SCRATCH(savedGp);
        /* Narrowing the saved GP to s16 here (it never leaves a register, so no
         * truncation happens) forces the same GP-save register routing the
         * original compiler emitted. */
        saved = savedGp;
        if (entry->drawCallback != NULL) {
            entry->drawCallback(entry, ot);
        }
        applyWindowBrightness(index);
        drawDialogContents(index, ot);
        GP_RESTORE_RET(saved, ret);
    }
}


/** @brief Prepend packet @p p to an OT chain; returns the new chain head. */
static inline u32 linkPacket(u32 head, void *p) {
    u32 tag;

    addOtTagFast(p, head, tag);
    return tag;
}


/**
 * @brief Render callback of a message window's battle entity.
 *
 * Draws the window contents (drawDialog) when the clamp rect has room for
 * a glyph, then links a draw area for the entity's clamp rect and a draw
 * offset at the window's inner corner (@ref DIALOG_CLAMP_INSET in from @c rect).
 * Links prepend to the OT, so the GPU runs these before the contents.
 * When the linked entity has both @ref BATTLE_ENTITY_BOX and
 * @ref BATTLE_ENTITY_DIALOG, the window also gets its @c cornerIcon (if
 * non-zero) at the window origin and two bars, grey at the entity's @c brightness
 * (the second one halved and semi-transparent for a
 * @ref BATTLE_ENTITY_SEMI_TRANS entity), then a draw area for the bound rect
 * and a draw offset back at the clip origin.
 *
 * @param ot Ordering table.
 * @param entity The window's battle entity.
 * @param pkt Packet cursor, stored back before drawing.
 * @return The packet cursor after the last packet.
 */
static DR_AREA *renderDialogEntity(P_TAG *ot, BattleDisplayEntity *entity, u32 pkt) {
    Dialog *entry;
    RECT *winRect;
    BattleDisplayEntity *ent;
    DR_AREA *p;
    DR_OFFSET *offset;
    RECT r;
    u32 head;
    u32 colour;
    u32 code;
    s32 index;
    s32 n;
    s32 y;

    index = entity->subFields[0];
    storeGpuPacket(pkt);
    if (entity->clipClamp.rect.w >= TEXT_GLYPH_SIZE && entity->clipClamp.rect.h >= TEXT_GLYPH_SIZE) {
        drawDialog(ot, index);
    }
    p = (DR_AREA *)getDisplayListHead();
    index = entity->subFields[0];
    entry = &g_dialogs.entries[index];
    winRect = &entry->rect;
    getAddrNewFast(ot, head);

    SetDrawArea(p, &entity->clipClamp.rect);
    head = linkPacket(head, p);
    p++;

    offset = (DR_OFFSET *)p;
    copyDisplayRect(&r);
    r.x += entry->rect.x;
    r.y += entry->rect.y;
    r.x += DIALOG_CLAMP_INSET;
    r.y += DIALOG_CLAMP_INSET;
    SetDrawOffset(offset, &r);
    head = linkPacket(head, offset);
    ent = getBattleEntity(entry->entityIdx);
    p = (DR_AREA *)(offset + 1);

    if ((ent->entityType & BATTLE_ENTITY_BOX) && (ent->entityType & BATTLE_ENTITY_DIALOG)) {
        colour = entity->brightness;
        colour >>= 5;
        code = SPRT_CODE;
        colour |= (colour << 16) | ((colour << 8) | code);
        setAddrFast(ot, head);
        n = entry->ctrl.bits.cornerIcon;
        if (n != 0) {
            p = drawIcon(ot, p, n, entry->rect.x, entry->rect.y, colour);
        }
        p = func_8002B898(ot, p, &entity->boundRect, colour);
        if (ent->entityType & BATTLE_ENTITY_SEMI_TRANS) {
            colour >>= 1;
            colour &= RGB_HALF_MASK;
            colour |= code;
            colour |= SPRT_ABE;
        }
        p = drawWindowBackground(ot, p, winRect, colour);
        getAddrNewFast(ot, head);

        SetDrawArea(p, &entity->clipBound.rect);
        head = linkPacket(head, p);
        p++;

        offset = (DR_OFFSET *)p;
        copyDisplayRect(&r);
        setlen(offset, 2);
        /* y is read before code[1] is cleared and x after: gcc keeps both
         * reads on their side of that store. */
        y = r.y & DR_OFFSET_COORD_MASK;
        offset->code[1] = 0;
        offset->code[0] = DR_OFFSET_CODE | (y << DR_OFFSET_Y_SHIFT) | (r.x & DR_OFFSET_COORD_MASK);
        head = linkPacket(head, offset);
        p = (DR_AREA *)(offset + 1);
    }
    setAddrFast(ot, head);
    return p;
}


/**
 * @brief Update callback of a message window's battle entity.
 *
 * With $gp on the scratchpad, runs the window's @c updateCallback, if any,
 * and its text (updateDialog) while the window is active.
 *
 * @param entity The window's battle entity.
 * @param input Pressed buttons.
 * @param repeat Auto-repeating buttons.
 * @return The scratchpad $gp.
 */
static u8 *updateDialogEntity(BattleDisplayEntity *entity, u32 input, u32 repeat) {
    u8 *tempGp;
    u8 *savedGp;
    u8 *ret;
    s32 index;

    GP_SAVE_SCRATCH(tempGp);
    index = entity->subFields[0];
    savedGp = tempGp;
    if (getDialogState(index) != 0) {
        Dialog *entry = &g_dialogs.entries[index];

        if (entry->updateCallback != NULL) {
            entry->updateCallback(entry, input, repeat);
        }
        updateDialog(index, input, repeat);
    }
    GP_RESTORE_RET(savedGp, ret);
    return ret;
}


/**
 * @brief Open a dialog: activate it and set its open step and mode.
 * @param idx Dialog index.
 * @param step Open step: 0x200 opens over 8 frames, 0x1000 at once.
 * @param mode Mode byte (@c ctrl.bits.mode).
 */
void openDialog(s32 idx, s32 step, s32 mode) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->state = 1;
    entry->openDialogStep = step;
    entry->ctrl.bits.mode = mode;
}


/** @brief Open a dialog with its animation: step 0x200, fully open after 8 frames. */
void openDialogAnimated(s32 idx) {
    openDialog(idx, 0x200, 0);
}


/** @brief Open a dialog at once: step 0x1000. */
void openDialogInstant(s32 idx) {
    openDialog(idx, 0x1000, 0);
}


/**
 * @brief Close a dialog by giving it a negative open step.
 * @param idx Dialog index.
 * @param step Open step: -0x200 closes over 8 frames, -0x1000 at once.
 */
void closeDialog(s32 idx, s32 step) {
    setOpenDialogStep(idx, step);
}


/** @brief Close a dialog with its animation: step -0x200, shut after 8 frames. */
void closeDialogAnimated(s32 idx) {
    closeDialog(idx, -0x200);
}


/** @brief Close a dialog at once: step -0x1000. */
void closeDialogInstant(s32 idx) {
    closeDialog(idx, -0x1000);
}


/**
 * @brief Set the anim speed of a dialog's linked entity: @p val + 3, clamped to [3, 11].
 *
 * Adds 3 to @p val, clamps to [3, 11], then calls setBattleEntityAnimSpeed with
 * the entry's entityIdx and the clamped value.
 *
 * @param idx Dialog index.
 * @param val Anim speed before the +3.
 */
void setDialogAnimSpeed(s32 idx, s32 val) {
    Dialog *entry;
    s32 clamped;
    val += 3;
    if (val >= 3) {
        clamped = 11;
        if (val < 12) {
            clamped = val;
        }
    } else {
        clamped = 3;
    }
    entry = &g_dialogs.entries[idx];
    setBattleEntityAnimSpeed(entry->entityIdx, clamped);
}


/**
 * @brief Get whether a dialog has typed its whole message.
 * @param idx Dialog index.
 * @return @c typingDone: 1 once the end of the message has been typed.
 */
s32 getDialogTypingDone(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];
    return entry->typingDone;
}


/**
 * @brief Set entity type flags on the battle entity linked to a dialog.
 *
 * Reads the entity index from the dialog, then sets entity type
 * (and derived draw mode) on that battle entity, adding @ref BATTLE_ENTITY_DIALOG.
 *
 * @param idx Dialog index.
 * @param val Entity type bits (BATTLE_ENTITY_*).
 */
void setDialogEntityType(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    setBattleEntityType(entry->entityIdx, val | BATTLE_ENTITY_DIALOG);
}


/**
 * @brief Read the entity type of the battle entity linked to a dialog.
 * @param idx Dialog index.
 * @return The entity type byte of the linked battle entity.
 */
s32 readDialogEntityType(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];
    return getBattleEntityType(entry->entityIdx);
}


/**
 * @brief Set the icon drawn at a dialog's top-left corner (@c cornerIcon).
 * @param idx Dialog index.
 * @param val Icon number, 0 for none.
 */
void setDialogCornerIcon(s32 idx, s32 val) {
    Dialog *entry = &g_dialogs.entries[idx];
    entry->ctrl.bits.cornerIcon = val;
}


/**
 * @brief Initialize a dialog to default values.
 *
 * Zeros out charTimer, choiceMade, cornerIcon, then configures defaults:
 * text speed = 0x1000, state = 0, anim speed = 3, open scale = 0, open step = 0,
 * entity flags = 6|8, display rect = (64,64,128,128), brightness = 0x1000.
 *
 * @param idx Dialog index.
 */
void initDialogSlot(s32 idx) {
    u8 *nullData = 0;
    Dialog *entry = &g_dialogs.entries[idx];
    entry->charTimer = 0;
    entry->choiceMade = 0;
    entry->ctrl.bits.cornerIcon = 0;
    setDialogMessage(idx, nullData);
    setDialogTextSpeed(idx, 0x1000);
    swapDialogState(idx, 0);
    setDialogAnimSpeed(idx, 3);
    setOpenDialogScale(idx, 0);
    setOpenDialogStep(idx, 0);
    setDialogEntityType(idx, 6);
    {
        RECT buf;
        buf.x = 0x40;
        buf.y = 0x40;
        buf.w = 0x80;
        buf.h = 0x80;
        setDialogRect(idx, &buf);
    }
    setDialogBrightness(idx, 0x1000);
}


/**
 * @brief Initialize a dialog slot: set active flag, callbacks, entity index, and clear fields.
 *
 * Activates the battle entity, assigns its render (renderDialogEntity) and update
 * (updateDialogEntity) callbacks, links the entity index, configures the sub-field, calls
 * initDialogSlot for default values, then clears sequence state and status bits.
 *
 * @param idx Dialog index.
 */
static void initDialog(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];

    setBattleEntityActive(idx, 1);
    entry->entityIdx = idx;
    setBattleEntityField04(idx, (s32)renderDialogEntity);
    setBattleEntityField00(idx, (s32)updateDialogEntity);
    setBattleEntityField35(idx, 0);
    setBattleEntitySubField(idx, 0, idx);
    initDialogSlot(idx);

    entry->seqState = DIALOG_SEQ_START;
    entry->textX = 0;
    entry->textY = 0;
    entry->ctrl.bits.nextPageMarker = 0;

    setDialogDrawCallback(idx, NULL);
    setDialogUpdateCallback(idx, NULL);
}


/**
 * @brief Copy a dialog's source rectangle to destination.
 * @param idx Dialog index.
 * @param dst Destination RECT.
 */
void getDialogRect(s32 idx, RECT *dst) {
    Dialog *entry = &g_dialogs.entries[idx];
    *dst = entry->rect;
}


/**
 * @brief Configure a dialog's rectangle and propagate it to its battle entity.
 *
 * Insets the source rect by @ref DIALOG_CLAMP_INSET on each side, sets visibleRows to
 * the number of text lines that fit (inset height / @ref DIALOG_LINE_HEIGHT,
 * minimum 1), copies the
 * original source rect into the entry, and forwards the original rect (offset by
 * the entry's stored offsets) to the entity's bound rect. The inset rect is then
 * passed to the entity's clamp rect.
 *
 * @param index Dialog index.
 * @param srcRect Source rectangle.
 */
void setDialogRect(s32 index, RECT *srcRect) {
    Dialog *entry = &g_dialogs.entries[index];
    RECT rect;
    s32 entityId;
    Dialog *ep = entry;

    entityId = ep->entityIdx;

    rect.x = srcRect->x;
    rect.y = srcRect->y;
    rect.w = srcRect->w;
    rect.h = srcRect->h;
    rect.x += DIALOG_CLAMP_INSET;
    rect.y += DIALOG_CLAMP_INSET;
    rect.w -= DIALOG_CLAMP_INSET * 2;
    rect.h -= DIALOG_CLAMP_INSET * 2;

    ep->visibleRows = rect.h / DIALOG_LINE_HEIGHT;
    if (ep->visibleRows == 0) {
        ep->visibleRows = 1;
    }

    ep->rect = *srcRect;
    rect = *srcRect;

    scaleDialogRect(&rect, ep->openDialogScale, ep->openDialogStep);
    setBattleEntityBoundRect(entityId, &rect);

    rect.x += DIALOG_CLAMP_INSET;
    rect.y += DIALOG_CLAMP_INSET;
    rect.w -= DIALOG_CLAMP_INSET * 2;
    rect.h -= DIALOG_CLAMP_INSET * 2;
    setBattleEntityRectClamp(entityId, &rect);
}


/**
 * @brief Store a numeric value into one of the 8 message-value slots.
 *
 * The index is clamped to [0, 7]. Used by message formatting to pass
 * numeric arguments into decodeMessage.
 *
 * @param index Slot index (clamped to 0..7).
 * @param value Value to store.
 */
void setMessageValue(s32 index, s32 value) {
    DialogSystem *data = &g_dialogs;
    s32 clamped = CLAMP(index, 0, 7);
    data->msgValues[clamped] = value;
}


/**
 * @brief Reset all dialogs and clear global dialog state.
 *
 * Marks the global state as inactive, reinitializes all 8 dialog slots,
 * clears the text blink clock and the pad auto-repeat state of channels 0 and 1,
 * then calls func_8002C130 to finalize.
 */
void resetAllDialogs(void) {
    DialogSystem *sys = &g_dialogs;
    s32 i;
    sys->state.focusedDialog = -1;
    for (i = 0; i < 8; i++) {
        initDialog(i);
    }
    /* Through the global, not sys: the original stores it with its own %hi/%lo pair. */
    g_dialogs.state.textBlinkClock = 0;
    sys->state.repeatLatched[0] = 0;
    sys->state.repeatLatched[1] = 0;
    sys->state.repeatCounters[0] = 0;
    sys->state.repeatCounters[1] = 0;
    func_8002C130();
}


/**
 * @brief Look up linked entity's animation speed and dispatch.
 *
 * Reads the entity index from the dialog, gets its animation speed,
 * then passes the result to getEntityTablePtr.
 *
 * @param idx Dialog index.
 */
void dispatchDialogAnimSpeed(s32 idx) {
    Dialog *entry = &g_dialogs.entries[idx];
    s32 val = getBattleEntityAnimSpeed(entry->entityIdx);
    getEntityTablePtr(val);
}


/**
 * @brief Draw one icon of @c g_iconTable as a run of sprites.
 *
 * Emits every cell of icon @p idx of @c g_iconTable as one
 * @ref TSPRT, tinted with @c state.textTint and linked into @p ot. Per cell:
 * the u/v/CLUT word is the cell's own plus the icons' CLUT; the texture page is
 * the icons' page with the cell's blend rate; the colour word gets the cell's
 * semi-transparency bit; width/height are copied and the cell's signed offsets
 * are added to (@p x, @p y).
 *
 * @note @c head is handed to @c p and taken back for the return, the way
 * @ref drawNextPageMarker threads its packet cursor; returning @c p directly
 * moves the cursor copy in the prologue. The @c (u8) narrowing of the
 * blend rate keeps @c _get_mode's mask, see @ref drawNextPageMarker.
 *
 * @param ot Ordering-table slot the sprites are linked into.
 * @param head First free packet.
 * @param idx Icon index into @c g_iconTable.
 * @param x Left edge of the glyph.
 * @param y Top edge of the glyph.
 * @return The first free packet after the ones written.
 */
static TSPRT *drawTextIcon(P_TAG *ot, TSPRT *head, s32 idx, s32 x, s32 y) {
    IconTable *table;
    IconCell *cell;
    TSPRT *p;
    u32 link;
    u32 word;
    s32 tpage;
    u32 val;
    u32 color;
    s32 n;

    p = head;
    table = &g_iconTable;
    cell = (IconCell *)table; /* seeded from its own copy, see drawNextPageMarker */
    word = table->descriptors[idx];
    n = word >> 16;
    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);
    color = g_dialogs.state.textTint;

    for (; n > 0; p++, cell++, n--) {
        word = cell->texInfo;
        val = word & ICON_UVCLUT_MASK;
        val += getClut(ICON_CLUT_X, ICON_CLUT_Y) << 16;
        setIconUVClut(p, val);

        val = (word >> ICON_ABR_SHIFT) & ICON_ABR_MASK;
        val = (u8)getTPage(0, val, 0, 0);
        tpage = val;
        tpage |= getTPage(0, 0, ICON_TPAGE_X, ICON_TPAGE_Y);

        val = word >> ICON_ABE_SHIFT;
        val &= SPRT_CODE_ABE;
        val <<= SPRT_CODE_SHIFT;
        val |= color;
        setTSprt(p, 1, 0, tpage);
        setIconRGBC(p, val);

        word = cell->metrics;
        val = word & ICON_WH_MASK;
        setIconWH(p, val);
        val = (s8)(word >> 24); /* signed Y offset, byte 3 */
        word <<= 16;
        word = (s8)(word >> 24); /* signed X offset, byte 1 */
        setXY0(p, x + word, y + val);

        addPrimFastWithTempOperand(ot, p, link);
    }
    head = p;
    return head;
}


/**
 * @brief Compute the bounding width of a multi-cell glyph.
 *
 * Walks glyph @p idx's cells and tracks the unsigned running maximum of each
 * cell's right edge (width + signed X offset) and bottom edge (height + signed
 * Y offset), returning the peak width (low byte).
 *
 * @note The peak height is computed but unused by the return value — the caller
 * presumably only needs the advance width. Purpose inferred from the
 * neighbouring glyph-metric routines.
 *
 * @param idx Icon index into @c g_iconTable.
 * @return Bounding width of the icon, masked to 8 bits.
 */
s32 getIconWidth(s32 idx) {
    IconCell *cell = (IconCell *)&g_iconTable;
    u32 word = g_iconTable.descriptors[idx];
    s32 cellCount = word >> 16;
    s32 maxWidth = 0;
    s32 maxHeight = 0;

    word &= 0xFFFF;
    cell = (IconCell *)((u8 *)cell + word);
    while (cellCount != 0) {
        s32 width, height;
        word = cell->metrics;
        width = word & 0xFF;
        width += (s8)(word >> 8);
        height = (word >> 16) & 0xFF;
        height += (s8)(word >> 24);
        if ((u32)maxWidth < (u32)width) {
            maxWidth = width;
        }
        if ((u32)maxHeight < (u32)height) {
            do { maxHeight = height; } while (0);
        }
        cellCount--;
        cell++;
    }
    return maxWidth & 0xFF;
}


/** @brief Extracts a 4-bit nibble from packed byte array D_800834D8.
 * Even indices return the low nibble; odd indices return the high nibble.
 * @note inline: drawDialogText has it expanded in place.
 * @param idx Nibble index.
 * @return The 4-bit value (0-15).
 */
inline s32 getNibbleValue(s32 idx) {
    u8 *base = D_800834D8;
    u32 val = base[idx >> 1];
    if (idx & 1) {
        val >>= 4;
    }
    return val & 0xF;
}


/**
 * @brief Get the width of character code @p idx from the glyph width table.
 *
 * Folds the character code @p idx into a glyph index depending on which of
 * three ranges it falls in (< 0x100, < 0x1C00, or higher — the last range is
 * offset and tagged with bit 0x400), then reads the packed nibble table
 * @c D_800834D8 exactly like @ref getNibbleValue: even indices take the low
 * nibble, odd indices the high nibble.
 *
 * @param idx Character code.
 * @return The 4-bit table value (0-15).
 */
static s32 getCharWidth(s32 idx) {
    u8 *base;
    u32 val;

    if (idx < 0x100) {
        idx -= 0x20;
    } else if (idx < 0x1C00) {
        idx -= 0x1820;
    } else {
        idx -= 0x1C20;
        idx |= 0x400;
    }

    base = D_800834D8;
    val = base[idx >> 1];
    if (idx & 1) {
        val >>= 4;
    }
    return val & 0xF;
}


/**
 * @brief Measure a battle-message string's packed {width, height}.
 *
 * Scans the string @p s, interpreting control codes (1/2/7 = line breaks,
 * 5 = special glyph resolved via @c func_8002C734 / @c getIconWidth,
 * 0x18-0x1F = multi-byte glyphs) and accumulating each line's pixel width from
 * the packed nibble-width table @ref D_800834D8. Tracks the widest line and the
 * line count; codes below 0x10 (other than 1/2/5/7) consume a following
 * argument byte. When @p flag is 0 the scan stops at the first line break.
 *
 * @param s Null-terminated message string.
 * @param flag If nonzero, measure every line; if zero, stop at the first break.
 * @return Packed dimensions: width in the low 16 bits, @c (lines*16 - 4) in the
 * high 16 bits.
 */
static s32 measureText(u8 *s, s32 flag) {
    s32 width = 0;
    s32 maxWidth = 0;
    s32 lines = 1;
    s32 maxLines = 1;
    s32 height;

    for (;;) {
        s32 c = *s++;
        if (c == 0) {
            if (maxWidth < width) {
                maxWidth = width;
            }
            if (maxLines < lines) {
                maxLines = lines;
            }
            height = maxLines * 16;
            break;
        }
        if ((u32) (c - 1) < 2 || c == 7) {
            /* control codes 1, 2, 7: line break */
            lines++;
            if (c == 1) {
                lines = 1;
            }
            if (c == 7) {
                lines = 1;
            }
            if (maxLines < lines) {
                maxLines = lines;
            }
            if (maxWidth < width) {
                maxWidth = width;
            }
            width = 0;
            if (flag == 0) {
                height = maxLines * 16;
                break;
            }
        } else if (c == 5) {
            c = *s++;
            width += getIconWidth(func_8002C734(c));
            width++;
        } else if (c < 0x10) {
            s++;
        } else if (c >= 0x18) {
            u8 packed;
            s32 nib;
            if (c >= 0x20) {
                c = c - 0x20;
            } else if (c < 0x1C) {
                c *= 0xE0;
                c += *s++;
                c -= 0x1520;
            } else {
                c *= 0xE0;
                c += *s++;
                c -= 0x18A0;
                c |= 0x400;
            }
            packed = D_800834D8[c >> 1];
            if (c & 1) {
                packed >>= 4;
            }
            nib = packed & 0xF;
            width += nib;
        }
    }

    return maxWidth | (((maxLines * 16) - 4) << 16);
}


/**
 * @brief Measure a message: the widest of its decoded lines and its height.
 *
 * Decodes the message one line at a time (nextMessageLine steps to the next,
 * NULL after the last) into a buffer taken from a $gp area set up on the
 * display-list head, and keeps the widest line. The height comes from
 * measuring the raw message as a whole.
 *
 * @param str Message in the game's text encoding.
 * @return Width in the low 16 bits, height in the high 16 bits (see
 * measureText).
 */
s32 measureMessage(u8 *str) {
    u8 *tempGp;
    u8 *savedGp;
    s32 head;
    u8 *buf;
    u8 *ret;
    s32 height;
    s32 maxWidth;
    s32 width;

    height = measureText(str, 1) >> 16;
    height <<= 16;
    head = getDisplayListHead();
    GP_SAVE_SET(tempGp, head);
    savedGp = tempGp;
    GP_ALLOC(buf, DIALOG_MSG_BUF_SIZE);
    maxWidth = 0;
    do {
        decodeMessage(str, buf, -1);
        width = measureText(buf, 0) & 0xFFFF;
        if (maxWidth < width) {
            maxWidth = width;
        }
        str = nextMessageLine(str);
    } while (str != NULL);
    GP_FREE(DIALOG_MSG_BUF_SIZE);
    GP_RESTORE_RET(savedGp, ret);
    return maxWidth | height;
}


/**
 * @brief Measure a text: the width of its widest line and its height.
 * @param str Text in the game's encoding.
 * @return Width in the low 16 bits, height in the high 16 bits (see measureText).
 */
s32 getTextSize(u8 *str) {
    return measureText(str, 1);
}


/**
 * @brief Same as getTextSize.
 * @param str Text in the game's encoding.
 * @return Width in the low 16 bits, height in the high 16 bits.
 */
s32 getTextSizeB(u8 *str) {
    return measureText(str, 1);
}


/**
 * @brief Get the width of a text's widest line.
 * @param str Text in the game's encoding.
 * @return Width in pixels.
 */
s32 getTextWidth(u8 *str) {
    return measureText(str, 1) & 0xFFFF;
}


/**
 * @brief Get the width of a text's first line.
 * @param str Text in the game's encoding.
 * @return Width in pixels.
 */
s32 getFirstLineWidth(u8 *str) {
    return measureText(str, 0) & 0xFFFF;
}


/**
 * @brief Set the menu brightness: store a grey in @c g_menuTint[MENU_TINT_NORMAL], the tint of
 * everything the menus draw, and refresh the blink copies.
 *
 * The raw value is kept in @c g_menuBrightness, where menus read it back to restore it.
 *
 * @param brightness Brightness, @ref BRIGHTNESS_NORMAL = normal; shifted down by 5,
 * its low 8 bits become r, g and b (0x80 draws graphics unmodulated).
 */
void setMenuBrightness(s32 brightness) {
    g_menuBrightness = brightness;
    {
        s32 val = (u32)brightness >> 5;
        brightness = val & 0xFF;
    }
    brightness |= (brightness << 16) | (brightness << 8);
    brightness |= SPRT_CODE;
    g_menuTint[MENU_TINT_NORMAL] = brightness;
    updateTextBlinkColors();
}


/**
 * @brief Fill one 12x12 text-font sprite in the menu colours and prepend it to the OT chain.
 *
 * Colours 0-7 use @c g_menuTint[MENU_TINT_NORMAL], 8-15 @c g_menuTint[MENU_TINT_BLINK];
 * the low 3 bits of the colour pick the CLUT row. Declared inline: drawDecodedText
 * has it expanded in place.
 *
 * @param head Current OT chain head (tag image of the previous packet).
 * @param p Sprite to fill.
 * @param glyph Text glyph number; @ref TEXT_GLYPH_PAGE2 selects the second texture page.
 * @param colour Text colour index (0-15).
 * @param xy Packed position, x in the low half and y in the high half.
 * @return The new chain head (@p p's tag image).
 */
inline u32 emitTextGlyph(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy) {
    u32 tag;
    u32 hi;
    u32 tpage;

    setlen(p, 5);
    addOtTagFast(p, head, tag);
    head = tag;
    hi = colour >> 3;
    colour &= 7;
    p->clut = (colour << 6) + getClut(TEXT_CLUT_X, TEXT_CLUT_Y);
    if (hi != 0) {
        colour = g_menuTint[MENU_TINT_BLINK];
    } else {
        colour = g_menuTint[MENU_TINT_NORMAL];
    }
    if (glyph & TEXT_GLYPH_PAGE2) {
        glyph &= TEXT_GLYPH_INDEX_MASK;
        tpage = TEXT_TPAGE_PAGE2;
    } else {
        tpage = TEXT_TPAGE_PAGE1;
    }
    setIconWH(p, (TEXT_GLYPH_SIZE << 16) | TEXT_GLYPH_SIZE);
    setIconRGBC(p, colour);
    *(u32 *)&p->x0 = xy;
    p->drawMode = tpage;
    *(u16 *)&p->u0 = (glyph % TEXT_GLYPHS_PER_ROW | (glyph / TEXT_GLYPHS_PER_ROW) << 8) * TEXT_GLYPH_SIZE;
    return head;
}


/**
 * @brief Draw a text string with the 12x12 font into the packets at @p p.
 *
 * Emits one sprite per glyph with emitTextGlyph, starting at (@p x, @p y).
 * Newlines return to @p x and move down a line; any other code below 0x19 ends
 * the string. Nothing is drawn when @p y is off screen, and the string stops at
 * the right screen edge. Unlike drawMessageText the text is already decoded.
 *
 * @param ot OT slot the sprites are linked into.
 * @param p First free packet.
 * @param x Left edge in pixels.
 * @param y Top edge in pixels.
 * @param str Decoded text, or NULL to draw nothing.
 * @param colour Text colour index (0-15).
 * @return The first packet after the ones drawn.
 */
TSPRT *drawDecodedText(P_TAG *ot, TSPRT *p, s32 x, s32 y, u8 *str, s32 colour) {
    u32 head;
    s32 startX;
    s32 c;

    if (str == NULL) {
        return p;
    }
    getAddrNewFast(ot, head);
    startX = x;
    if (y >= 0x101) { /* below the screen */
        return p;
    }
    if (y < -8) { /* above the screen */
        return p;
    }
    for (;;) {
        c = *str++;
        if (c == MSG_NEWLINE) {
            x = startX;
            y += DIALOG_LINE_HEIGHT;
            continue;
        }
        if (c < 0x19) {
            break;
        }
        if (x >= 0x181) { /* past the right screen edge */
            break;
        }
        if (c >= 0x20) { /* one-byte glyph */
            c -= 0x20;
        } else if (c < 0x1C) { /* two-byte glyph, first page */
            c *= 0xE0;
            c += *str++;
            c -= 0x1520;
        } else { /* two-byte glyph, second page */
            c *= 0xE0;
            c += *str++;
            c -= 0x18A0;
            c |= TEXT_GLYPH_PAGE2;
        }
        head = emitTextGlyph(head, p, c, colour, (y << 16) | (x & 0xFFFF));
        p++;
        x += getNibbleValue(c);
    }
    setAddrFast(ot, head);
    return p;
}


/**
 * @brief Fill one 12x12 text-font sprite and prepend it to the OT chain.
 *
 * The text renderers' counterpart of emitTextGlyph: colours 0-7 use @c state.textTint,
 * 8-15 (the blinking ones) @c g_textBlinkTint, and the low 3 bits of the colour
 * pick the CLUT row.
 *
 * @param head Current OT chain head (tag image of the previous packet).
 * @param p Sprite to fill.
 * @param glyph Text glyph number; @ref TEXT_GLYPH_PAGE2 selects the second texture page.
 * @param colour Text colour index (0-15).
 * @param xy Packed position, x in the low half and y in the high half.
 * @return The new chain head (@p p's tag image).
 */
static inline u32 addTextGlyph(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy) {
    u32 tag;
    u32 hi;
    u32 tpage;

    setlen(p, 5);
    addOtTagFast(p, head, tag);
    head = tag; /* taken here rather than at the return, as the original's copy order needs */
    hi = colour >> 3;
    colour &= 7;
    p->clut = (colour << 6) + getClut(TEXT_CLUT_X, TEXT_CLUT_Y);
    if (hi != 0) {
        colour = g_textBlinkTint;
    } else {
        colour = g_dialogs.state.textTint;
    }
    if (glyph & TEXT_GLYPH_PAGE2) {
        glyph &= TEXT_GLYPH_INDEX_MASK;
        tpage = TEXT_TPAGE_PAGE2;
    } else {
        tpage = TEXT_TPAGE_PAGE1;
    }
    setIconWH(p, (TEXT_GLYPH_SIZE << 16) | TEXT_GLYPH_SIZE);
    setIconRGBC(p, colour);
    *(u32 *)&p->x0 = xy;
    p->drawMode = tpage;
    *(u16 *)&p->u0 = (glyph % TEXT_GLYPHS_PER_ROW | (glyph / TEXT_GLYPHS_PER_ROW) << 8) * TEXT_GLYPH_SIZE;
    return head;
}


/**
 * @brief Draw a text string at (@p x, @p y) with the message-window font.
 *
 * Decodes @p str one line at a time into a scratch buffer and emits a sprite
 * per glyph, starting in white. While it runs, $gp points at the scratchpad
 * (0x1F800300), which holds the decode buffer. Newlines return to @p x and move
 * down a line; icons (code 0x05) are drawn with drawTextIcon, colour codes
 * (0x06) switch the text colour, and a page break or the end of the text stops.
 *
 * @param ot OT slot the text is linked into.
 * @param x Left edge in pixels.
 * @param y Top edge in pixels.
 * @param str Encoded text, or NULL to draw nothing.
 * @return The scratchpad pointer $gp held while the text was drawn.
 */
u8 *drawMessageText(P_TAG *ot, s32 x, s32 y, u8 *str) {
    u8 *buf;
    s32 startX;
    u8 *savedGp;
    u8 *tempGp;
    u8 *ret;
    TSPRT *p;
    u32 head;
    u32 colour;
    u8 *s;
    s32 c;

    startX = x;
    GP_SAVE_SCRATCH(tempGp);
    savedGp = tempGp;
    colour = DIALOG_COLOR_WHITE;
    GP_ALLOC(buf, DIALOG_MSG_BUF_SIZE);
    p = (TSPRT *)getDisplayListHead();
    getAddrNewFast(ot, head);

    /* str is tested here and at the top of the loop, as in drawDialogText. */
    if (str != NULL) {
        for (;;) {
            if (str == NULL) {
                goto end;
            }
            decodeMessage(str, buf, -1);
            g_messageColor = colour;
            str = nextMessageLine(str);

            s = buf;
            for (;;) {
                c = *s++;
                if (c >= 0x19) {
                    if (c >= 0x20) { /* one-byte glyph */
                        c -= 0x20;
                    } else if (c < 0x1C) { /* two-byte glyph, first page */
                        c *= 0xE0;
                        c += *s++;
                        c -= 0x1520;
                    } else { /* two-byte glyph, second page */
                        c *= 0xE0;
                        c += *s++;
                        c -= 0x18A0;
                        c |= TEXT_GLYPH_PAGE2;
                    }
                    head = addTextGlyph(head, p, c, colour, (y << 16) | (x & 0xFFFF));
                    p++;
                    x += getNibbleValue(c);
                    continue;
                }
                if (c == MSG_NEWLINE) {
                    x = startX;
                    y += DIALOG_LINE_HEIGHT;
                    break;
                }
                if (c == MSG_NEW_PAGE || c == MSG_NEW_PAGE_MARKED || c == MSG_END) {
                    goto end;
                }
                if (c >= 0x10) { /* codes 0x10-0x18 take no argument */
                    continue;
                }
                if (c == MSG_CMD_ICON) {
                    setAddrFast(ot, head);
                    c = *s++;
                    c = func_8002C734(c);
                    p = drawTextIcon(ot, p, c, x, y);
                    x += getIconWidth(c);
                    x++;
                    getAddrNewFast(ot, head);
                    continue;
                }
                if (c == MSG_CMD_COLOR) {
                    colour = *s++;
                    colour &= 0xF;
                    continue;
                }
                s++; /* skip the argument of any other command */
            }
        }
    }
end:
    setAddrFast(ot, head);
    storeGpuPacket((u32)p);
    GP_FREE(DIALOG_MSG_BUF_SIZE);
    GP_RESTORE_RET(savedGp, ret);
    return ret;
}


/**
 * @brief Draw the text of a message window.
 *
 * Decodes the message one line at a time into a scratch buffer taken from the
 * $gp area and emits a sprite per glyph, starting at the window's text origin
 * (@c textX, @c textY) scrolled up by @c scrollY. Lines scrolled out above
 * the window are skipped, and lines @c firstChoice to @c lastChoice (the choices) are
 * indented for the cursor. Lines before @c typingLine are drawn whole; the line
 * being typed shows its first @c typedChars bytes. Icons (code 0x05) are
 * drawn as multi-cell glyphs with drawTextIcon, colour codes (0x06) switch the
 * text colour, and a page break or the end of the message stops the text.
 *
 * @param ot OT slot the text is linked into.
 * @param entry Message window.
 */
static void drawDialogText(P_TAG *ot, Dialog *entry) {
    u8 *buf;
    s32 first;
    s32 last;
    u8 *str;
    TSPRT *p;
    s32 line;
    u32 head;
    s32 y;
    s32 x;
    u32 colour;
    u8 *s;
    s32 c;

    GP_ALLOC(buf, DIALOG_MSG_BUF_SIZE);
    first = entry->firstChoice;
    str = entry->dataPtr;
    last = entry->lastChoice;
    p = (TSPRT *)getDisplayListHead();
    line = 0;
    getAddrNewFast(ot, head);
    /* The scroll offset is read signed (lh) here; updateDialog counts it unsigned. */
    y = entry->textY + (DIALOG_TEXT_MARGIN - (s16)entry->scrollY);
    x = entry->textX + DIALOG_TEXT_MARGIN;
    colour = entry->pageColor;
    if (line >= first && line <= last) {
        x = entry->textX + DIALOG_TEXT_MARGIN + DIALOG_CHOICE_INDENT;
    }
    g_messageColor = entry->pageColor;

    while (y < -DIALOG_LINE_HEIGHT) {
        if (str == NULL) {
            break;
        }
        str = nextMessageLine(str);
        y += DIALOG_LINE_HEIGHT;
        colour = g_messageColor & 0xF;
        line++;
    }

    /* str is tested both here and at the top of the loop, as in the original.
     * The loop is left through goto: with break, gcc rotates the test to the
     * bottom, which the original does not do (98.48%). */
    if (str != NULL) {
        for (;;) {
            if (str == NULL) {
                goto end;
            }
            /* c doubles as the length limit, as the original's register does. */
            c = -1;
            if (line >= entry->typingLine) {
                c = entry->typedChars;
            }
            decodeMessage(str, buf, c);
            g_messageColor = colour;
            str = nextMessageLine(str);
            line++;

            s = buf;
            for (;;) {
                c = *s++;
                if (c >= 0x19) {
                    if (c >= 0x20) { /* one-byte glyph */
                        c -= 0x20;
                    } else if (c < 0x1C) { /* two-byte glyph, first page */
                        /* The second byte is read before the multiply: that is
                         * what schedules its load ahead of it, as in the original. */
                        s32 lo = *s++;

                        c *= 0xE0;
                        c += lo;
                        c -= 0x1520;
                    } else { /* two-byte glyph, second page */
                        s32 lo = *s++;

                        c *= 0xE0;
                        c += lo;
                        c -= 0x18A0;
                        c |= TEXT_GLYPH_PAGE2;
                    }
                    head = addTextGlyph(head, p, c, colour, (y << 16) | (x & 0xFFFF));
                    p++;
                    x += getNibbleValue(c);
                    continue;
                }
                if (c == MSG_NEWLINE) {
                    x = entry->textX + DIALOG_TEXT_MARGIN;
                    if (line >= first && line <= last) {
                        x = entry->textX + DIALOG_TEXT_MARGIN + DIALOG_CHOICE_INDENT;
                    }
                    y += DIALOG_LINE_HEIGHT;
                    break;
                }
                if (c == MSG_NEW_PAGE || c == MSG_NEW_PAGE_MARKED || c == MSG_END) {
                    goto end;
                }
                if (c >= 0x10) { /* codes 0x10-0x18 take no argument */
                    continue;
                }
                if (c == MSG_CMD_ICON) {
                    setAddrFast(ot, head);
                    c = *s++;
                    c = func_8002C734(c);
                    p = drawTextIcon(ot, p, c, x, y);
                    x += getIconWidth(c);
                    x++;
                    getAddrNewFast(ot, head);
                    continue;
                }
                if (c == MSG_CMD_COLOR) {
                    colour = *s++;
                    colour &= 0xF;
                    continue;
                }
                s++; /* skip the argument of any other command */
            }
        }
    }
end:
    setAddrFast(ot, head);
    storeGpuPacket((u32)p);
    GP_FREE(DIALOG_MSG_BUF_SIZE);
}


