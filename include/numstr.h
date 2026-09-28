#ifndef NUMSTR_H
#define NUMSTR_H

#include "common.h"

/* Number / message string formatting helpers (numstr.c). */

/**
 * @brief Message cursor handed to the decode/advance entry points.
 */
typedef struct {
    s32 unk0;
    s32 unk4;
    u8 *streamPtr;              /* start of the message stream */
    u8 *storedPtr;              /* start of the segment decoded last */
    u8 pad10[0x12];
    u8 skipCount;               /* segments to skip before decoding */
    u8 pad23;
} MsgState;

/** @brief Text colour the message decoder is in; its colour code (0x06) sets it. */
extern u8 D_8008386C;

/* Public prototypes */
extern void intToDecString(u32 value, u8 *buf, s32 digitBase);
extern void intToDecStringShort(u32 value, u8 *buf, s32 digitBase);
extern void replaceLeadingZeros(u8 *buf, s32 count, s32 digitBase, s32 replacement);
extern void lookupHexChar(s32 idx, u8 *dst);
extern void byteToHexString(s32 byte, u8 *buf);
extern void decodeMessage(u8 *input, u8 *output, s32 maxLen);
extern void func_8002FD28(MsgState *msg, u8 *output);
extern void advanceAndDecodeMessage(MsgState *msg, u8 *output);
extern void decodeMessageDirect(MsgState *msg, u8 *output);
/** Returns the next character of @p msg in bits 0-7 and its command byte in bits 8-15. */
extern s32 func_8002FE0C(MsgState *msg, u8 *output);

u8 *func_8002F548(u8 *src);

#endif /* NUMSTR_H */
