#ifndef BTL_SFX_H
#define BTL_SFX_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "battle.h"

extern void func_8002C8A4(void);
extern void decrementSfxCounter(void);
extern void setFlashColor(s32 intensity);
extern void dispatchSfxColorUpdate(s32 idx);
extern void setSfxEntityIndex(s32 idx, s32 val);
extern s32  swapSfxState(s32 idx, s32 val);
extern s32  getSfxState(s32 idx);
extern void setSfxPitch(s32 idx, s32 val);
extern void setSfxField1C(s32 idx, s32 val);
extern void setSfxRateDelta(s32 idx, s32 val);
extern s32  getSfxField1C(s32 idx);
extern void setSfxGlobalFlag(s32 val);
extern s32  getSfxGlobalFlag(void);
extern s32  func_8002CE84(s32 idx);
extern void initSfxPlayback(s32 index, u8 *data);
extern void initSfxPlaybackAfterStrings(s32 index, u8 *data, s32 count);
extern void func_8002D784(s32 arg0, u8 *data, s32 min, s32 max, s32 val, s32 arg5);
extern void func_8002D818(s32 arg0, u8 *str, s32 count, s32 min, s32 max, s32 val, s32 arg6);
extern void configureSfxPlayback(s32 idx, s32 rate, s32 mode);
extern void startSfxSlow(s32 idx);
extern void startSfxNormal(s32 idx);
extern void setSfxFadeRate(s32 idx, s32 val);
extern void fadeOutSfxSlow(s32 idx);
extern void fadeOutSfxFast(s32 idx);
extern void setSfxReverbMode(s32 idx, s32 val);
extern s32  getSfxField28(s32 idx);
extern void setSfxEntityType(s32 idx, s32 val);
extern s32  readSfxEntityType(s32 idx);
extern void setSfxField2F(s32 idx, s32 val);
extern void initSfxSlot(s32 idx);
extern void func_8002DF5C(s32 idx);
extern void getSfxRect(s32 idx, RECT *dst);
extern void func_8002E064(s32 index, RECT *srcRect);
extern void func_8002E1B4(s32 index, s32 value);
extern void resetAllSfx(void);
extern void dispatchSfxAnimSpeed(s32 idx);
extern s32  getNibbleValue(s32 idx);
extern void getGlyphWidthA(s32 code);
extern void getGlyphWidthB(s32 code);
extern u16  getGlyphWidthU16(s32 code);
extern u16  getGlyphStatusU16(s32 code);
extern void setMenuColorIntensity(s32 intensity);

#endif
