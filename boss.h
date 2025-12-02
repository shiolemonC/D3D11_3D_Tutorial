// boss.h
#pragma once
#include <DirectXMath.h>

struct BossDesc {
    DirectX::XMFLOAT3 spawnPos{ 0,0,0 };
    float             scale = 1.0f;
};

struct BossUpdateContext {
    DirectX::XMFLOAT3 playerPos;  // 玩家位置（世界坐标）
};

void Boss_Initialize(const BossDesc& d);
void Boss_Update(double dt, const BossUpdateContext& ctx);

// 查询（给相机/子弹用）
DirectX::XMFLOAT3 Boss_GetPosition();
DirectX::XMFLOAT3 Boss_GetForward();
float             Boss_GetYaw();
DirectX::XMMATRIX Boss_GetWorld();

int Boss_GetBodyColliderId();

// Hitbox 所属者标记
void* Boss_GetHitboxOwnerToken();

void Boss_SetHurtEnabled(bool enabled);
bool Boss_IsHurtEnabled();

int  Boss_GetHurtColliderId();