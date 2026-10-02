#ifndef UI_DIALOG_H
#define UI_DIALOG_H

#include "common.h"
#include "psxsdk/libgpu.h"

/** @brief Brightness 1.0 on the 0x1000 scale: graphics are drawn at their own colour. */
#define BRIGHTNESS_NORMAL 0x1000

/** @brief Where the font's glyph sheet is loaded in VRAM: the texture page of TEXT_TPAGE_PAGE1. */
#define TEXT_FONT_X 960
#define TEXT_FONT_Y 256

/** @brief Text glyphs are this many pixels square, and a font texture row holds this many. */
#define TEXT_GLYPH_SIZE 12
#define TEXT_GLYPHS_PER_ROW 21

/** @brief CLUT of text colour 0; colour n uses the CLUT n rows below it. */
#define TEXT_CLUT_X 288
#define TEXT_CLUT_Y 224

/** @brief Text colours: the font CLUT rows that are loaded. */
#define TEXT_CLUT_ROWS 16

/** @brief Size of the $gp scratch buffer a message is decoded into. */
#define DIALOG_MSG_BUF_SIZE 128

/** @brief Height of one text line in pixels; a scroll steps @c scrollY once per frame until a line has passed. */
#define DIALOG_LINE_HEIGHT 16

struct Dialog;

/** @brief Per-frame hook of a dialog window, run with the frame's pad input. */
typedef void (*DialogCallback)(struct Dialog *entry, u32 input, u32 repeat);

/** @brief Draw hook of a dialog window, run before the window is drawn. */
typedef void (*DialogDrawCallback)(struct Dialog *entry, P_TAG *ot);

/** @brief One message window: its box, message, typing progress and choices. */
typedef struct Dialog {
    RECT rect;
    u8 *dataPtr;
    u8 *linePtr; /**< Start of the line being typed. */
    s16 textSpeed; /**< Added to charTimer each frame; a character prints when it reaches ONE (0 = no wait). */
    u16 scrollY; /**< Pixels the text is scrolled up by; a scroll adds 1 a frame until a line has passed. */
    s16 charTimer; /**< A character is typed each time it reaches ONE. */
    u8 state;
    u32 color : 4; /**< Current text colour, set by the message colour command. */
    u32 pageColor : 4; /**< Colour the next page starts in. */
    u8 entityIdx;
    s8 choiceMade; /**< 1 once the player has confirmed a choice. */
    s16 brightness; /**< Window brightness, 0x1000 = full (grey 0x80: drawn unmodulated). */
    s16 openDialogScale; /**< Box scale of the open/close animation: 0 shut, 0x1000 fully open. */
    s16 openDialogStep; /**< Added to openDialogScale each frame: 0x200 opens over 8 frames, 0x1000 at once, negative closes. */
    u8 typedChars; /**< Bytes of the line being typed shown so far (two-byte glyphs and commands count 2). */
    u8 typingRow; /**< Row of the box being typed on; a newline on the last row scrolls the text up instead. */
    u8 typingLine; /**< Line being typed; the lines before it are drawn whole. */
    u8 visibleRows; /**< Lines the box holds, set with its rect. */
    s32 seqState;
    u8 typingDone; /**< 1 once the end of the message has been typed. */
    u8 firstChoice; /**< First choice line; 0xFF (DIALOG_NO_CHOICE) when the message offers none. */
    u8 lastChoice; /**< Last choice line; the cursor stays between the two. */
    u8 choiceCursor; /**< Choice line the cursor is on. */
    /** The control word: its fields, or the whole word for the next-page marker test. */
    union {
        u32 raw;
        struct {
            u32 cancelChoice : 8; /**< Line Triangle moves the cursor to; none when negative as an s8. */
            u32 mode : 8;
            u32 nextPageMarkerBlink : 7; /**< Blink counter of the next-page marker. */
            u32 nextPageMarker : 1; /**< The window shows its blinking next-page marker. */
            u32 cornerIcon : 8; /**< Icon drawn at the window's top-left corner, 0 for none. */
        } bits;
    } ctrl;
    u16 textX; /**< Text origin x in pixels, inside the window. */
    u8 textY; /**< Text origin y in pixels. */
    u8 waitTimer; /**< Frames left of a message {Wait} command. */
    DialogDrawCallback drawCallback;
    DialogCallback updateCallback;
} Dialog;

typedef struct {
    u8 pad0[3];
    u8 textBlinkClock; /**< Counts down once per frame; bit 0x10 marks the dim half of the text blink. */
    u32 textTint; /**< Grey tint of message and string text (0x80 = unmodulated). */
    u32 textBlinkTint; /**< Tint of text colours 8-15: @c textTint on the full half of the blink,
                             a grey at 75% of its red channel on the dim half. */
    s8 focusedDialog; /**< Dialog with the input focus, -1 for none: only it reads the pad (paging, choices). */
    u8 padD[7];
    s8 repeatCounters[4]; /**< Per-channel pad auto-repeat countdown (autoRepeatPadChannel). */
    u16 repeatLatched[4]; /**< Per-channel pad bits latched on the previous frame (autoRepeatPadChannel). */
} DialogGlobalState; /* 0x20 */

/** @brief Complete dialog system: 8 entry slots + global state + message display values. */
typedef struct {
    Dialog entries[8]; /* 8 × 60 = 480 bytes */
    DialogGlobalState state; /* global dialog state (0x20 bytes) */
    u32 msgValues[8]; /* numeric values formatted by decodeMessage */
} DialogSystem;

/** @brief The message windows: @c g_engine.dialogs under a symbol of its own. */
extern DialogSystem g_dialogs;

/** @brief The font's width table: one nibble per character, the even one in the low nibble. */
extern u8 D_800834D8[0x1C4];

extern void setDialogTextOrigin(s32 idx, s32 x, s32 y);
extern void setDialogChoices(s32 idx, s32 first, s32 last, s32 cancel);
extern void setDialogChoiceCursor(s32 idx, s32 val);
extern void setDialogDrawCallback(s32 idx, DialogDrawCallback val);
extern void setDialogUpdateCallback(s32 idx, DialogCallback val);
extern s32 setDialogBrightness(s32 idx, s32 val);
extern void updateTextBlinkColors(void);
extern void tickTextBlink(void);
extern void setTextBrightness(s32 brightness);
extern void setDialogEntityIndex(s32 idx, s32 val);
extern s32 swapDialogState(s32 idx, s32 val);
extern s32 getDialogState(s32 idx);
extern void setDialogTextSpeed(s32 idx, s32 val);
extern void setOpenDialogScale(s32 idx, s32 val);
extern void setOpenDialogStep(s32 idx, s32 val);
extern s32 getOpenDialogScale(s32 idx);
extern void setFocusedDialog(s32 idx);
extern s32 getFocusedDialog(void);
extern s32 getDialogChoice(s32 idx);
extern s32 autoRepeatPad(s32 input);
extern void setDialogMessage(s32 index, u8 *data);
extern void setDialogMessageAfterStrings(s32 index, u8 *data, s32 count);
extern void setDialogChoiceMessage(s32 arg0, u8 *data, s32 min, s32 max, s32 val, s32 arg5);
extern void setDialogChoiceMessageAfterStrings(s32 arg0, u8 *str, s32 count, s32 min, s32 max, s32 val, s32 arg6);
extern void openDialog(s32 idx, s32 step, s32 mode);
extern void openDialogAnimated(s32 idx);
extern void openDialogInstant(s32 idx);
extern void closeDialog(s32 idx, s32 step);
extern void closeDialogAnimated(s32 idx);
extern void closeDialogInstant(s32 idx);
extern void setDialogAnimSpeed(s32 idx, s32 val);
extern s32 getDialogTypingDone(s32 idx);
extern void setDialogEntityType(s32 idx, s32 val);
extern s32 readDialogEntityType(s32 idx);
extern void setDialogCornerIcon(s32 idx, s32 val);
extern void initDialogSlot(s32 idx);
extern void getDialogRect(s32 idx, RECT *dst);
extern void setDialogRect(s32 index, RECT *srcRect);
extern void setMessageValue(s32 index, s32 value);
extern void resetAllDialogs(void);
extern void dispatchDialogAnimSpeed(s32 idx);

#endif
