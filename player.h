#pragma once
#include <DirectXMath.h>
#include "AnimatorRegistry.h"

enum class PlayerState { Idle, Move }; // 目前没怎么用，先保留

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
