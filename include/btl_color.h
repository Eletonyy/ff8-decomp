#ifndef BTL_COLOR_H
#define BTL_COLOR_H

#include "common.h"
#include "battle.h"

extern void clearEntityColor(SfxEntry *entry);
extern void buildGrayscaleGpuColor(s32 intensity);
extern void buildRgbGpuColor(s32 r, s32 g, s32 b);
extern void setDefaultGpuColor(void);
extern void btlColorStub0234(void);
extern void setCameraVibrateIntensity(s32 val);
extern void setCameraVibrateState(u32 enable);
extern void setCameraShakeParams(s32 intensity, s32 direction);
extern void updateCameraVibrate(void);
extern void resetBattleCameraState(void);
extern BattleCmdEntry *getBattleCmdTable(void);
extern BattleCmdEntry *findBestBattleCmd(s32 threshold);
extern s32  isAnyBattleCmdActive(void);
extern s32  checkBattleCmdSource(s32 cmd);
extern void deactivateBattleCmd(s32 id);
extern s32  loadBattleCmd(u8 *data, s32 idx, s32 priority);
extern void advanceBattleTimer(s32 delta);
extern void initBattleCmdEntries(void);
extern void sendSpuCommand(s32 idx);
extern void playSoundEffect(s32 idx);
extern void enableSoundReverb(s32 mask);
extern void disableSoundReverb(s32 mask);
extern u16  remapControllerInput(u16 bitmask);
extern s32  remapButtonIndex(s32 index);
extern s32  reverseButtonRemap(s32 index);
extern void btlColorStub1044(void);
extern void updatePaletteTransition(void);
extern s32  renderBattleString(s32 ot, s32 pkt, u8 *str, s32 y, s32 width, s32 color);
extern void setTransitionPhase7(void);
extern void setTransitionFlag(s32 val);
extern void initBattleTransition(void);
extern s32  lerpRange(s32 rangeStart, s32 rangeEnd, s32 input, s32 maxOut);
extern void stepAnimEntries(void);
extern void clearAnimEntryActive(s32 idx);
extern void updateAnimEntry(s32 idx, s32 value);
extern void copyAnimEntryField(s32 idx, u8 *src);
extern void initAnimEntry(s32 idx, s32 flags, s32 src, s32 start, s32 end, s32 inStart, s32 inEnd);
extern void setupAnimEntry(s32 idx, s32 flags, s32 src, s32 start, s32 end, s32 inStart);
extern void setupAnimEntryFull(s32 idx, s32 flags, s32 src, s32 start, s32 end, s32 inStart, s32 inEnd);
extern void clearAnimEntries(void);
extern u8  *getBattleBuffer1(void);
extern u8  *getBattleBuffer2(void);
extern void waitBattleVSync(void);
extern u32  getBattleAllocBase(void);
extern s32  getBattleAllocSize(void);
extern void flipBattleOtBuffer(void);

#endif
