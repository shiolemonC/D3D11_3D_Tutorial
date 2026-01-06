#pragma once
/*==============================================================================
    ヒットボックス管理 [hitbox_system.h]
    - AnimEvent で Spawn される攻撃判定の生成・更新・寿命管理
==============================================================================*/

#include <vector>
#include <DirectXMath.h>
#include "anim_event.h"      // AnimEvent_SpawnHitBox
#include "collider_system.h" // Hitbox / CollisionWorld

struct ActiveHitbox
{
    int colliderId = -1;                 // CollisionWorld 内 ID
    void* owner = nullptr;               // 创建者（Player* / Enemy* 等）

    DirectX::XMFLOAT3 localOffset;       // 本地偏移
    DirectX::XMFLOAT3 halfSize;          // 半尺寸

    float remainingTime = 0.0f;          // 剩余寿命
    int damage = 0;                      // 伤害数值

    float knockbackDistance = 0.0f; // ★ 新增：击退距离（米）
};

// 简单的全局管理（先KISS）
void HitboxSystem_Clear();
void HitboxSystem_Spawn(const AnimEvent_SpawnHitBox& param, void* owner);
void HitboxSystem_Update(float dt);
