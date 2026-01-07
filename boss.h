// boss.h
#pragma once
#include "hit_event.h" 
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
void* Boss_GetHurtboxOwnerToken();

void Boss_SetHurtEnabled(bool enabled);
bool Boss_IsHurtEnabled();

int  Boss_GetHurtColliderId();

// ------------- HP and Damage ---------------
void Boss_SetMaxHP(int maxHp, bool fullHeal = true);
int  Boss_GetMaxHP();
int  Boss_GetHP();
bool Boss_IsDead();

void Boss_ApplyDamage(int damage);

using BossOnDeathFn = void(*)();
void Boss_SetOnDeath(BossOnDeathFn fn);

// 玩家->Boss 命中统一入口
void Boss_OnIncomingHit(const HitParams& hit);