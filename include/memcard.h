#ifndef MEMCARD_H
#define MEMCARD_H

#include "common.h"

extern u16 func_800377B4(s32 len, u8 *buf);
extern void mcZeroFrame(u8 *ptr);
extern s32 func_800379AC(void *arg0);

/* --- Memory card busy flag (D_80085218) --- */
extern void setMcBusy(void);
extern u32  isMcBusy(void);

#endif
