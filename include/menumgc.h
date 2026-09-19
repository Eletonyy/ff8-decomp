#ifndef MENUMGC_H
#define MENUMGC_H

#include "common.h"

/* ======================================================================== */
/* Public typedefs/structs                                                  */
/* ======================================================================== */

typedef struct { u8 unk00[0x14]; } Unk14;
typedef struct { u8 unk00[0x40]; } Unk40;

/* ======================================================================== */
/* Data owned by this unit                                                  */
/* ======================================================================== */

extern u8 D_801EC814[];
extern Unk14 D_801ECF60[];
extern Unk40 D_801ECF90[];
extern s16 D_801ED010[];

#endif /* MENUMGC_H */
