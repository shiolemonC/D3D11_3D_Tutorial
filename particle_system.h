#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <DirectXMath.h>
#include "vfx_config.h"

// A tiny CPU particle system for "spark / blood" style VFX.
// Renders with Billboard pipeline.

bool ParticleSystem_Initialize();
void ParticleSystem_Finalize();

void ParticleSystem_Update(double dt);
void ParticleSystem_DrawWorld();

// Spawn one-shot burst using a preset (configured in vfx_config.cpp).
// - position: emitter position in world
// - direction: main direction (used for cone emission); can be {0,0,0}
void ParticleSystem_Spawn(VfxId id,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& direction);

#endif // PARTICLE_SYSTEM_H
