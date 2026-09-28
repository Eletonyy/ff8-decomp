#ifndef BATTLE_ANIM_H
#define BATTLE_ANIM_H

#include "common.h"
#include "psxsdk/libgpu.h"

/* Battle animation state shared across the battle, field, menu, and Triple Triad
 * code. g_battleAnims is a main-RAM global (0x80082DD0); the Triple Triad minigame
 * reuses the battle animation entity system to drive the on-board card animations,
 * so its overlay accesses these same structures. Kept here (rather than battle.h)
 * so non-battle translation units can use them without depending on battle.h. */

typedef struct {
    u8 field00;
    u8 field01;
    u16 field02;
    u8 params[4]; /**< 0x04-0x07: Indexed by param in func_80027FDC. */
    u16 field08;
    u16 field0A;
    u16 field0C;
    u16 field0E;
    u16 field10;
    u16 field12;
} AnimFrame; /* 0x14 = 20 bytes */

typedef struct {
    u8 field00;
    u8 field01;   /**< field06 masked by opacity, shifted right by field08 (func_80027220). */
    u8 field02;   /**< field07 masked by opacity, shifted right by field09 (func_80027220). */
    u8 pad03[3];
    u8 field06;
    u8 field07;
    u8 field08;   /**< Shift amount applied to field06 in func_80027220. */
    u8 field09;   /**< Shift amount applied to field07 in func_80027220. */
    u8 field0A;
    u8 field0B;
    u8 field0C;
    u8 field0D;
    u8 field0E;
    u8 field0F;
    s16 unk10[4];
    u8 frameCounter;
    s8 field19;   /**< Read back as a signed byte in func_80027220. */
    s8 field1A;
    u8 opacity;
    AnimFrame frames[8];
    u8 padBC[6];
    u8 linkedIdx;
    u8 fieldC3;
} BattleAnimEntity;

#define OT_SIZE 18

/** @brief Display list double-buffer entry (stride 0x58 = 88 bytes). */
typedef struct {
    u32 pktAlloc;
    u32 pktLimit;
    u32 ot[OT_SIZE];
    u32 pad50;
    u32 pktBase;
} DisplayListBuf;

/**
 * @brief Bit of @c Dialog.ctrl.raw: the window shows its blinking corner marker.
 *
 * It is bit 7 of @c ctrl.fields.markerBlink; the blink bit below is spelled on
 * the shifted byte instead, because the two tests only compile to the original
 * instruction pair when written that way.
 */
#define DIALOG_CTRL_MARKER 0x00800000

/** @brief Position of @c Dialog.ctrl.fields.markerBlink inside @c ctrl.raw. */
#define DIALOG_CTRL_MARKER_BLINK_SHIFT 16

/**
 * @brief Bit of the 7-bit blink counter in @c Dialog.ctrl.fields.markerBlink.
 *
 * Set for 16 of every 32 ticks; the corner marker is blanked while it is set.
 */
#define DIALOG_MARKER_BLINK_OFF 0x10

struct Dialog;

/** @brief Per-frame hook of a dialog window, run with the frame's pad input. */
typedef void (*DialogCallback)(struct Dialog *entry, u32 input, u32 repeat);

/** @brief Draw hook of a dialog window, run before the window is drawn. */
typedef void (*DialogDrawCallback)(struct Dialog *entry, P_TAG *ot);

typedef struct Dialog {
    RECT rect;
    u8 *dataPtr;
    u8 *linePtr;          /**< Start of the line being typed. */
    s16 textSpeed;        /**< Added to the character timer each frame; a character prints when it reaches ONE (0 = no wait). */
    u16 field12;
    union {
        u32 raw;
        struct {
            s16 field14;
            u8 state;
            u8 field17;
        } fields;
        struct {
            u32 field14 : 16;
            u32 state : 8;
            u32 color : 4;      /**< Current text colour, set by the message colour command. */
            u32 pageColor : 4;  /**< Colour the next page starts in. */
        } bits;
    } flags;
    u8 entityIdx;
    s8 field19;
    s16 brightness;  /**< Window brightness, 0x1000 = full (grey 0x80: drawn unmodulated). */
    s16 openDialogScale; /**< Box scale of the open/close animation: 0 shut, 0x1000 fully open. */
    s16 openDialogStep;  /**< Added to openDialogScale each frame: 0x200 opens over 8 frames, 0x1000 at once, negative closes. */
    u8 field20;
    u8 field21;
    u8 typingLine;        /**< Line being typed; the lines before it are drawn whole. */
    u8 field23;
    s32 seqState;
    u8 field28;
    u8 field29;
    u8 field2A;
    u8 field2B;
    union {
        u32 raw;
        struct {
            u8 field2C;
            u8 mode;
            u8 markerBlink; /**< Bits 0-6: blink counter; bit 7: @ref DIALOG_CTRL_MARKER. */
            u8 field2F;
        } fields;
        /** The marker byte as bitfields: writing @c blink or @c marker is a
         *  read-modify-write of the whole word, as in the original. */
        struct {
            u32 field2C : 8;
            u32 mode : 8;
            u32 blink : 7;
            u32 marker : 1;
            u32 field2F : 8;
        } bits;
    } ctrl;
    u16 field30;
    u8 field32;
    u8 waitTimer; /**< Frames left of a message {Wait} command. */
    DialogDrawCallback drawCallback;
    DialogCallback updateCallback;
} Dialog;

typedef struct {
    u8 pad0[3];
    u8 textBlinkClock;  /**< Counts down once per frame; bit 0x10 marks the dim half of the text blink. */
    u32 textTint;       /**< Grey tint of message and string text (0x80 = unmodulated). */
    u32 textBlinkTint;  /**< Tint of text colours 8-15: @c textTint on the full half of the blink,
                             a grey at 75% of its red channel on the dim half. */
    s8 activeFlag;
    u8 padD[7];
    s8 repeatCounters[4];  /**< Per-channel pad auto-repeat countdown (func_8002CECC). */
    u16 repeatLatched[4];  /**< Per-channel pad bits latched on the previous frame (func_8002CECC). */
} DialogGlobalState;       /* 0x20 */

/** @brief Complete dialog system: 8 entry slots + global state + message display values. */
typedef struct {
    Dialog entries[8];       /* 8 × 60 = 480 bytes */
    DialogGlobalState state;      /* global dialog state (0x20 bytes) */
    u32 msgValues[8];          /* numeric values formatted by decodeMessage */
} DialogSystem;

/** @brief Complete battle animation state (entities + global coords). */
typedef struct {
    /* 0x000 */ BattleAnimEntity entities[2]; /**< Two animation entities. */
    /* 0x188 */ u8 cdBufA[0x24];             /**< CD audio buffer A. */
    /* 0x1AC */ u8 cdBufB[0x24];             /**< CD audio buffer B. */
    /* 0x1D0 */ s16 globalCoords[2][2];      /**< Per-slot coords [slot][axis]. */
    /* 0x1D8 */ u16 clipLeft;               /**< Clip region left edge. */
    /* 0x1DA */ u16 clipTop;                /**< Clip region top edge. */
    /* 0x1DC */ u16 clipRight;              /**< Clip region right edge. */
    /* 0x1DE */ u16 clipBottom;             /**< Clip region bottom edge. */
    /* 0x1E0 */ U16Split repeatDelays;       /**< Pad auto-repeat timing (func_8002CECC): restart delay in @c b.lo, repeat interval in @c b.hi. */
    /* 0x1E2 */ u8 pad1E2[0x3E];             /**< Unknown. */
    /* 0x220 */ DialogSystem dialogs;               /**< Message windows; also addressed directly as @c g_dialogs. */
    /* 0x440 */ u8 pad440[0x200];            /**< Unknown. */
    /* 0x640 */ DisplayListBuf bufs[2];         /**< Double-buffered GPU display lists (2 × 0x58). */
    /* 0x6F0 */ DisplayListBuf *active;      /**< Pointer to active display list buffer. */
    /* 0x6F4 */ s32 halfSize;                /**< Half of total VRAM size. */
    /* 0x6F8 */ u8 pad6F8[4];                /**< Unknown. */
    /* 0x6FC */ s32 field6FC;                /**< Cleared during GPU init. */
    /* 0x700 */ u8 pad700[3];                /**< Unknown. */
    /* 0x703 */ u8 field703;                 /**< Saved/cleared across transition. */
    /* 0x704 */ u8 pad704[0x270];            /**< Unknown. */
    /* 0x974 */ s32 palette[3];              /**< RGB888 palette (0x40BBGGRR). */
    /* 0x980 */ u8 pad980[0x30];             /**< Unknown. */
    /* 0x9B0 */ u8 field9B0;                 /**< Saved/cleared across transition. */
    /* 0x9B1 */ u8 pad9B1[0xF];              /**< Unknown. */
    /* 0x9C0 */ u8 field9C0;                 /**< Saved/cleared across transition. */
    /* 0x9C1 */ u8 pad9C1[1];                /**< Unknown. */
    /* 0x9C2 */ s16 field9C2;               /**< Set to 0x4611 during GPU init. */
    /* 0x9C4 */ s16 cdStreamCounter;         /**< CD stream counter. */
    /* 0x9C6 */ u8 pad9C6[2];                /**< Unknown. */
    /* 0x9C8 */ s32 field9C8;                /**< Cleared during GPU init. */
    /* 0x9CC */ s32 field9CC;                /**< Cleared during GPU init. */
} BattleAnimState;

extern BattleAnimState g_battleAnims;

#endif /* BATTLE_ANIM_H */
