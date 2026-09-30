#ifndef INPUT_BUTTON_REMAP_H
#define INPUT_BUTTON_REMAP_H

#include "common.h"

extern u16 applyButtonRemapTranslation(u16 bitmask);
extern s32 reverseButtonRemap(s32 index);
extern void initButtonRemap(void);

#endif
