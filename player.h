#pragma once
#include <DirectXMath.h>
#include "AnimatorRegistry.h"
#include "hit_event.h"


enum class PlayerState { Idle, Move }; // 目前没怎么用，先保留

// ★ 受击处理结果：用于 HitEvent_Dispatch 分支日志/流程控制
enum class PlayerHitResponse : uint8_t
{
    Ignored = 0,   // 无敌等原因忽略（但我们仍可选择消耗 hitbox）
    Parried = 1,   // 成功格挡
    TookHit = 2,   // 正常受击（进入 hit）
};

struct PlayerDesc {
    DirectX::XMFLOAT3 spawnPos{ 0,0,0 };
    float moveSpeed = 2.5f;        // m/s
    float turnSharpness = 10.0f;   // 趋近系数（越大转向越快）
    float scale = 1.0f;            // 角色缩放
};

// 新：一帧内玩家需要的全部输入（对标 魂 / 怪猎）
struct PlayerUpdateInput {
    float moveX = 0.0f; // -1..1 (A/D)
    float moveZ = 0.0f; // -1..1 (W/S, 正向前)

    bool attack = false; // 本帧是否触发攻击（鼠标左键刚按下）
    bool roll = false; // ★ 本帧是否触发翻滚（Shift 等）
    bool parry = false; // ★ 本帧是否触发格挡/弹反（鼠标右键刚按下）


    // 摄像机在 XZ 平面上的前/右向量（由 PlayerCamera 提供）
    DirectX::XMFLOAT3 camForwardXZ{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 camRightXZ{ 1.0f, 0.0f, 0.0f };
};

void Player_Initialize(const PlayerDesc& d);

// 新：一帧内把状态机 + 动画 + 位移 + RootMotion 全部跑完
void Player_Update(double dt, const PlayerUpdateInput& in);

// 查询接口（Camera/调试用）
DirectX::XMMATRIX         Player_GetWorld();
const DirectX::XMFLOAT3& Player_GetPosition();
float                     Player_GetYaw();
DirectX::XMFLOAT3         Player_GetForward();

int Player_GetBodyColliderId();

void* Player_GetHitboxOwnerToken();
void* Player_GetHurtboxOwnerToken();

// ------------------ HP ------------------
void Player_SetMaxHP(int maxHp, bool fullHeal = true);
int  Player_GetMaxHP();
int  Player_GetHP();
bool Player_IsDead();

void Player_OnDeathRequested();
void Player_ApplyDamage(int damage); // damage<=0 自动忽略

using PlayerOnDeathFn = void(*)();
void Player_SetOnDeath(PlayerOnDeathFn fn);
// death hook（先留空实现，之后你再接死亡逻辑）


// HurtBox 开关（无敌帧用）
void Player_SetHurtEnabled(bool enabled);
bool Player_IsHurtEnabled();

// Debug / 系统查询
int  Player_GetHurtColliderId();

// 受击请求（由 HitEvent_Dispatch 调用）
void Player_RequestHitReaction(const HitParams& hit);

// ★ 新增：统一入口（Boss->Player 命中时调用）
PlayerHitResponse Player_OnIncomingHit(const HitParams& hit);

//（保留原接口也可以，内部会转调）
// 受击请求（由 HitEvent_Dispatch 调用）
void Player_RequestHitReaction(const HitParams& hit);

// ★ 成功格挡窗口（Parry Window）
void Player_SetParryWindowEnabled(bool enabled);
bool Player_IsParryWindowEnabled();