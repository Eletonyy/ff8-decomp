#ifndef INPUT_VIBRATION_H
#define INPUT_VIBRATION_H

#include "common.h"

extern s32 isAnyBattleCmdActive(void);
extern s32 checkBattleCmdSource(s32 cmd);
extern void deactivateBattleCmd(s32 id);
extern s32 loadBattleCmd(u8 *data, s32 idx, s32 priority);
extern void func_80030B2C(void);
extern void advanceBattleTimer(s32 delta);
extern void initBattleCmdEntries(void);

#endif
