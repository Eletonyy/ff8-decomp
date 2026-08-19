#ifndef CDREAD_H
#define CDREAD_H

#include "common.h"

/** @brief Saved state shared by the LZSS decompression entry points. */
typedef struct {
    /* 0x00 */ u8 *src;
    /* 0x04 */ u8 pad04[0x24];
    /* 0x28 */ s32 outputSize;
} LzssState; /* 0x2C */

extern LzssState D_80039418;

// CD read/seek state-machine handlers (cdread.c), invoked directly and through
// the D_800562D8 dispatch table before their definitions appear.

void cdPollReadState(void);
void cdReadSectors(void);
void cdHandleReadSync(void);
void cdPollSeekState(void);
void func_80039140(void);
void func_80039218(void);

#endif /* CDREAD_H */
