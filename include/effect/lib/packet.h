#ifndef EFFECT_LIB_PACKET_H
#define EFFECT_LIB_PACKET_H

#include "effect/lib/particle.h"

/**
 * @file packet.h
 * @brief Build a mesh packet for a particle and link its copies into the OT.
 */

/** @brief Build one mesh packet for @p particle and link every copy into the OT. */
void effectBuildMeshPacket(EffectParticle *particle, EffectPoseStep *step);

#endif /* EFFECT_LIB_PACKET_H */
