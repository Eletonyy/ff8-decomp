#ifndef PSXSDK_LIBPAD_H
#define PSXSDK_LIBPAD_H

#include "common.h"

void PadStartCom(void);
void PadStopCom(void);
unsigned PadEnableCom(unsigned mode);
int PadGetState(int port);
int PadInfoMode(int port, int term, int offs);
int PadInfoAct(int port, int actno, int term);
int PadInfoComb(int port, int listno, int offs);

s32 func_8003AB64(void);
void func_8003AB84(void);
void func_8003ABA4(void);

#endif
