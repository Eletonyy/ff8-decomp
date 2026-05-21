#ifndef BTL_ENTITY_H
#define BTL_ENTITY_H

#include "common.h"
#include "battle.h"

extern s32 *getEntityTablePtr(s32 idx);
extern s32  func_8002BF24(s32 ot, s32 head);
extern void dispatchBattleEntity(s32 idx);
extern s32  allocBattleEntitySlot(void);
extern void initAllBattleEntities(void);
extern void setBattleEntityBase(s32 val);
extern s32  getMaxBattleEntities(void);
extern u8   getDigitBaseCode(void);
extern void setDigitBaseCode(u8 val);
extern void setSfxEntryParams(s32 idx, s32 val30, s32 val32);
extern void setSfxEntryTimings(s32 idx, s32 val29, s32 val2A, s32 val2C);
extern void setSfxEntryField2B(s32 idx, s32 val);
extern void setSfxEntryField34(s32 idx, s32 val);
extern void setSfxEntryField38(s32 idx, s32 val);
extern void setSfxEntryVolume(s32 idx, s32 val);

#endif
