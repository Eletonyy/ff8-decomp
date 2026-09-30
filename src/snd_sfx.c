#include "common.h"
#include "snd_sfx.h"

/* --- Externs (sorted by address) --- */

extern u8 g_battleSfxTable[]; /* 0x80052A34 — SPU command table */

/* --- Private functions --- */

static void disableSoundReverb(s32 mask);

/**
 * @brief Send an SPU command from the battle SFX table.
 *
 * Looks up a command byte from g_battleSfxTable and sends it to the
 * SPU via sndKeyOn.
 *
 * @param idx Index into g_battleSfxTable.
 */
void sendSpuCommand(s32 idx) {
    sndKeyOn(g_battleSfxTable[idx]);
}


/**
 * @brief Play a sound effect from the battle SFX table.
 *
 * Looks up a sound ID from g_battleSfxTable and plays it via sndPlaySfx
 * with default volume (0x80) and pan (0x7F).
 *
 * @param idx Index into the g_battleSfxTable sound table.
 */
void playSoundEffect(s32 idx) {
    sndPlaySfx(g_battleSfxTable[idx], 0, 0x80, 0x7F);
}


/**
 * @brief Configure sound reverb channels based on a bitmask.
 *
 * Reads hardware state via func_80047384, optionally pauses/resumes audio
 * hardware. Mutes master volume, then enables reverb on channels indicated
 * by bits 0-2 of @p mask. If @p mask is 7, enables reverb on channel 0 (all).
 *
 * @param mask Bitmask of reverb channels to enable (bits 0, 1, 2).
 */
void enableSoundReverb(s32 mask) {
    s32 hwState = func_80047384();

    if (!(hwState & 4)) {
        func_800472E4();
    }
    sndSetMasterVolume(0);
    if (mask == 7) {
        sndEnableReverb(0);
    } else {
        if (mask & 1) {
            sndEnableReverb(1);
        }
        if (mask & 2) {
            sndEnableReverb(2);
        }
        if (mask & 4) {
            sndEnableReverb(3);
        }
    }
    if (!(hwState & 4)) {
        func_800472F4();
    }
}


/**
 * @brief Disable sound reverb channels based on a bitmask and restore volume.
 *
 * Reads hardware state via func_80047384, optionally pauses/resumes audio
 * hardware. Disables reverb on channels indicated by bits 0-2 of @p mask
 * (all of them via channel 0 when it is 7), then restores master volume to 0x7F.
 *
 * @param mask Bitmask of reverb channels to disable (bits 0, 1, 2).
 */
static void disableSoundReverb(s32 mask) {
    s32 hwState = func_80047384();

    if (!(hwState & 4)) {
        func_800472E4();
    }
    if (mask == 7) {
        sndDisableReverb(0);
    } else {
        if (mask & 1) {
            sndDisableReverb(1);
        }
        if (mask & 2) {
            sndDisableReverb(2);
        }
        if (mask & 4) {
            sndDisableReverb(3);
        }
    }
    sndSetMasterVolume(0x7F);
    if (!(hwState & 4)) {
        func_800472F4();
    }
}
