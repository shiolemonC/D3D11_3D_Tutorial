#pragma once

#include <DirectXMath.h>

bool BossFireRing_Initialize();
void BossFireRing_Finalize();

void BossFireRing_Start(const DirectX::XMFLOAT3& center, void* ownerToken);
void BossFireRing_Update(float dt);

bool BossFireRing_IsActive();
float BossFireRing_GetCurrentRadius();

