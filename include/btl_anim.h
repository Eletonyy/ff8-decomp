#ifndef BTL_ANIM_H
#define BTL_ANIM_H

#include "common.h"
#include "psxsdk/libgpu.h"

/* Battle display-list render helpers (btl_anim.c). */

/* Public prototypes */
extern void renderAndUpdateDisplay(s32 frameCount); /**< Advance and render the battle display list. */
extern s32  renderBattleDisplayList(s32 *colorTag); /**< Walk the ordering table and emit its primitives. */
extern u8  *emitDrawEnvPackets(P_TAG *ot, u8 *pkt); /**< Emit SetDrawArea/SetDrawOffset packets, link into OT. */
extern s32  getDisplayListHead(void);              /**< Next free packet in the active display-list buffer. */
extern void setBattleEntityBoundRect(s32 idx, RECT *src); /**< Set an entity's bounding rect from @p src. */
extern void setBattleEntityRectClamp(s32 idx, RECT *src); /**< Set an entity's clamp rect from @p src. */
extern struct BattleDisplayEntity *getBattleEntity(s32 idx); /**< Battle entity @p idx. */
extern void copyDisplayRect(RECT *dst); /**< Copy the active draw environment's clip rect to @p dst. */

extern s32 getAnimGlobalState(void);
extern s32 setAnimGlobalState(s32 value);
extern void setPadMotors(s32 idx, s32 motor1, s32 motor0);
extern void setPadVibration(s32 idx, s32 val);
extern void setPadRepeatMask(s32 unused, s32 channel, s32 mask);

/* Public data */
extern u8 g_animCurveFadeOut[]; /**< Easing curve: 65 entries that rise exponentially to 64. */

#endif /* BTL_ANIM_H */
