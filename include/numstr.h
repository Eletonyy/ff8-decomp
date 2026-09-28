#ifndef NUMSTR_H
#define NUMSTR_H

#include "common.h"

/* Number / message string formatting helpers (numstr.c). */

struct Dialog;

/** @brief How numbers are printed: the digit glyphs and the thousands separator. */
typedef struct {
    u8 digits[0x10]; /* glyph codes of the digits 0-F; [0] is the decimal digit base */
    u8 separator; /* thousands separator character */
} MsgFormatConfig;

extern MsgFormatConfig g_numberFormat;

/** @brief Text colour the message decoder is in; its colour code (0x06) sets it. */
extern u8 g_messageColor;

/* Public prototypes */
extern void intToDecString(u32 value, u8 *buf, s32 digitBase);
extern void intToDecStringShort(u32 value, u8 *buf, s32 digitBase);
extern void replaceLeadingZeros(u8 *buf, s32 count, s32 digitBase, s32 replacement);
extern void lookupHexChar(s32 idx, u8 *dst);
extern void byteToHexString(s32 byte, u8 *buf);
extern void decodeMessage(u8 *input, u8 *output, s32 maxLen);
extern void decodeDialogLine(struct Dialog *dialog, u8 *output);
extern void advanceAndDecodeMessage(struct Dialog *dialog, u8 *output);
extern void decodeMessageDirect(struct Dialog *dialog, u8 *output);
/** Returns the next character of @p dialog's line in bits 0-7 and its command byte in bits 8-15. */
extern s32 nextDialogChar(struct Dialog *dialog, u8 *output);

u8 *nextMessageLine(u8 *src);

#endif /* NUMSTR_H */
