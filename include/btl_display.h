#ifndef BTL_DISPLAY_H
#define BTL_DISPLAY_H

#include "common.h"
#include "battle.h"

extern void setBattleEntityType(s32 idx, s32 val);
extern void setBattleEntityField00(s32 idx, s32 val);
extern void setBattleEntityField04(s32 idx, s32 val);
extern void setBattleEntityField36(s32 idx, s32 val);
extern u32  getBattleEntityField36(s32 idx);
extern void setBattleEntityField35(s32 idx, s32 val);
extern u32  getBattleEntityField35(s32 idx);
extern void setBattleEntityActive(s32 idx, s32 value);
extern s32  GetActiveFlag(s32 idx);
extern void setBattleEntityScale(s32 idx, s32 val);
extern s32  getBattleEntityScale(s32 idx);
extern void initBattleEntity(s32 idx);
extern s32  clipBlitRects(BlitParams *arg);

#endif
