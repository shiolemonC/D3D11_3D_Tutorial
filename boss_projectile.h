#pragma once
#include <DirectXMath.h>

enum class BossProjectilePatternId : int
{
    StraightShot = 0,
};

void BossProjectile_Initialize();
void BossProjectile_Finalize();
void BossProjectile_Clear();

void BossProjectile_Fire(
    BossProjectilePatternId patternId,
    const DirectX::XMFLOAT3& spawnPos,
    const DirectX::XMFLOAT3& targetPos,
    void* ownerToken);

void BossProjectile_Update(float dt);
void BossProjectile_Draw();
void BossProjectile_DebugDraw();
