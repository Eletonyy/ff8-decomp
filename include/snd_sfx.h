#ifndef SND_SFX_H
#define SND_SFX_H

#include "common.h"

extern void sendSpuCommand(s32 idx);
extern void playSoundEffect(s32 idx);
extern void enableSoundReverb(s32 mask);

#endif
