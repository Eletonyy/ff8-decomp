#ifndef BTL_DISPLAY_H
#define BTL_DISPLAY_H

#include "common.h"
#include "battle.h"

/** @brief @c BattleDisplayEntity.entityType bits, as setBattleEntityType stores them. */
#define BATTLE_ENTITY_SEMI_TRANS 0x01 /**< The box is drawn semi-transparent. */
#define BATTLE_ENTITY_BOX 0x02 /**< The entity has a box: frame and background. */
#define BATTLE_ENTITY_DIALOG 0x08 /**< A dialog window: its render callback draws the box, not the generic renderer. */

extern void setBattleEntityType(s32 idx, s32 val);
extern void setBattleEntityField00(s32 idx, s32 val);
extern void setBattleEntityField04(s32 idx, s32 val);
extern void setBattleEntityField36(s32 idx, s32 val);
extern u32  getBattleEntityField36(s32 idx);
extern void setBattleEntityField35(s32 idx, s32 val);
extern u32  getBattleEntityField35(s32 idx);
extern void setBattleEntityActive(s32 idx, s32 value);
extern s32  GetActiveFlag(s32 idx);
extern void setBattleEntityBrightness(s32 idx, s32 val);
extern s32 getBattleEntityBrightness(s32 idx);
extern void initBattleEntity(s32 idx);
extern s32  clipBlitRects(BlitParams *arg);

#endif
