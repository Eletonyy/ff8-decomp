/**
 * @file trail.c
 * @brief Aim the trail at the battle slot.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/pose.h"
#include "effect/lib/particle.h"
#include "effect/lib/trail.h"

/** @brief Aim the trail at the battle slot: its two points and its height. */
void effectTrailAim(void) {
    BattleEffectSlot *slot = &D_800EF2D0[g_effectDrawList->slot];
    SVECTOR pt;

    if (g_effectDrawList->unk024 != 2) {
        func_800B3960(slot, 0xF1, 0, &pt);
    }
    pt.vy = slot->unk024;
    g_effectDrawList->trail[1].vx = pt.vx << 16;
    g_effectDrawList->trail[1].vy = pt.vy << 16;
    g_effectDrawList->trail[1].vz = pt.vz << 16;
    g_effectDrawList->trail[2].vx = g_effectDrawList->trail[1].vx;
    g_effectDrawList->trail[2].vy = 0;
    g_effectDrawList->trail[2].vz = g_effectDrawList->trail[1].vz;
    if (g_effectDrawList->unk024 != 2) {
        func_800B3960(slot, 0xF1, 0, &pt);
    }
    g_effectDrawList->trail[3].vx = pt.vx << 16;
    g_effectDrawList->trail[3].vy = pt.vy << 16;
    g_effectDrawList->trail[3].vz = pt.vz << 16;
    if (g_effectDrawList->unk024 != 2) {
        func_800B3960(slot, 0xF0, 0, &pt);
    }
    g_effectDrawList->trail[4].vx = pt.vx << 16;
    g_effectDrawList->trail[4].vy = pt.vy << 16;
    g_effectDrawList->trail[4].vz = pt.vz << 16;
    g_effectDrawList->trail[5].vx = g_effectDrawList->trail[1].vx;
    g_effectDrawList->trail[5].vy = slot->unk03C << 16;
    g_effectDrawList->trail[5].vz = g_effectDrawList->trail[1].vz;
}
