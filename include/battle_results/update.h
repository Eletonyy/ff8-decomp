#ifndef BATTLE_RESULTS_UPDATE_H
#define BATTLE_RESULTS_UPDATE_H

#include "common.h"

void formatRewardName(s32 id, u8 *buf);
void formatMessage(u8 *msg, u8 *dst, s32 reward, s32 arg3, s32 ability, s32 number, s32 magic);
u8 *getRewardDesc(s32 id);
void updateResults(void);

#endif /* BATTLE_RESULTS_UPDATE_H */
