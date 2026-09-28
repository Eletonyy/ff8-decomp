#ifndef DIALOG_H
#define DIALOG_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "battle.h"

/** @brief Brightness 1.0 on the 0x1000 scale: graphics are drawn at their own colour. */
#define BRIGHTNESS_NORMAL 0x1000

extern void tickTextBlink(void);
extern void setTextBrightness(s32 brightness);
extern void setDialogEntityIndex(s32 idx, s32 val);
extern s32  swapDialogState(s32 idx, s32 val);
extern s32  getDialogState(s32 idx);
extern void setDialogTextSpeed(s32 idx, s32 val);
extern void setOpenDialogScale(s32 idx, s32 val);
extern void setOpenDialogStep(s32 idx, s32 val);
extern s32  getOpenDialogScale(s32 idx);
extern void setDialogGlobalFlag(s32 val);
extern s32  getDialogGlobalFlag(void);
extern s32  getDialogChoice(s32 idx);
extern s32  func_8002CF54(s32 input);
extern void setDialogMessage(s32 index, u8 *data);
extern void setDialogMessageAfterStrings(s32 index, u8 *data, s32 count);
extern void setDialogChoiceMessage(s32 arg0, u8 *data, s32 min, s32 max, s32 val, s32 arg5);
extern void setDialogChoiceMessageAfterStrings(s32 arg0, u8 *str, s32 count, s32 min, s32 max, s32 val, s32 arg6);
extern void openDialog(s32 idx, s32 step, s32 mode);
extern void openDialogAnimated(s32 idx);
extern void openDialogInstant(s32 idx);
extern void closeDialog(s32 idx, s32 step);
extern void closeDialogAnimated(s32 idx);
extern void closeDialogInstant(s32 idx);
extern void setDialogAnimSpeed(s32 idx, s32 val);
extern s32  getDialogField28(s32 idx);
extern void setDialogEntityType(s32 idx, s32 val);
extern s32  readDialogEntityType(s32 idx);
extern void setDialogField2F(s32 idx, s32 val);
extern void initDialogSlot(s32 idx);
extern void func_8002DF5C(s32 idx);
extern void getDialogRect(s32 idx, RECT *dst);
extern void func_8002E064(s32 index, RECT *srcRect);
extern void func_8002E1B4(s32 index, s32 value);
extern void resetAllDialogs(void);
extern void dispatchDialogAnimSpeed(s32 idx);
extern s32  getNibbleValue(s32 idx);
extern u32  func_8002E810(u32 head, TSPRT *p, s32 glyph, u32 colour, u32 xy);
extern u8  *func_8002EAD0(P_TAG *ot, s32 x, s32 y, u8 *str);
extern s32  getGlyphWidthA(u8 *code);
extern void getGlyphWidthB(u8 *code);
extern u16  getGlyphWidthU16(u8 *code);
extern u16  getGlyphStatusU16(u8 *code);
extern void setMenuBrightness(s32 brightness);

#endif
