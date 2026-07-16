#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"

/* --- PS1 kernel structures (PsyQ KERNEL.H, simplified) --- */

typedef struct ToT  { unsigned long *head; long size; } ToT;
typedef struct TCBH { struct TCB *entry;   long flag; } TCBH;
typedef struct TCB  { s32 status; u8 pad04[0xBC]; } TCB;

#define TcbStACTIVE 0x4000
#define KERNEL_TOT  ((ToT *)0x100)

#endif /* KERNEL_H */
