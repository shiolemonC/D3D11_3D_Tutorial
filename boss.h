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
float             Boss_GetYaw();
DirectX::XMMATRIX Boss_GetWorld();
