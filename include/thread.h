#ifndef THREAD_H
#define THREAD_H

#include "common.h"

/* Raw controller-input helpers (thread.c). */

/* Public prototypes */
extern void func_800275D4(void);                /**< Refresh the raw controller buffers. */
/* Which axis func_80027DB4 reads. These are mutually exclusive selectors, not
   flags -- the value is an index, never OR-ed or masked. X/Y are as documented
   below; the other pair is named from the order callers sample them in, not from
   a decoded implementation. */
typedef enum {
    PAD_AXIS_X2 = 0,
    PAD_AXIS_Y2 = 1,
    PAD_AXIS_X  = 2,
    PAD_AXIS_Y  = 3
} PadAxis;

extern s32  func_80027DB4(s32 pad, PadAxis axis, s32 c); /**< Read one analog axis. */

extern s32  func_80027CF8(s32 a, s32 b, s32 c); /**< Fold a recentred analog stick into d-pad bits. */

/* getAnimFrameParam (thread.c) returns u16, but consumers like be_object4.c's readPads use the
   result as s32 with no widening mask — an inconsistent caller view that can't share a decl here,
   so those callers keep their own `extern s32 getAnimFrameParam(...)`. */

extern void func_80026D8C(void); /* per-frame battle VSync handler (RENDER_BATTLE) */
extern void func_80027448(void);
#endif /* THREAD_H */
