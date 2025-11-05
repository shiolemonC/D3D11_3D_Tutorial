#pragma once
#include <DirectXMath.h>
#include "AnimatorRegistry.h"

enum class PlayerState { Idle, Move };

struct PlayerDesc {
    DirectX::XMFLOAT3 spawnPos{ 0,0,0 };
    float moveSpeed = 2.5f; // m/s
    float turnSharpness = 10.0f; // 趋近系数（越大转向越快）
    float scale = 1.0f;  // 角色缩放
};

struct PlayerInput {
    float moveX = 0.0f; // -1..1 (A/D)
    float moveZ = 0.0f; // -1..1 (W/S，正向前)
};

void         Player_Initialize(const PlayerDesc& d);
PlayerState  Player_Update(double dt, const PlayerInput& in, PlayerState smState);
void         Player_Kinematic_Update(double dt, const PlayerInput& in, bool locomotionActive);
void         Player_ApplyRootMotionDelta(const RootMotionDelta& rm);

DirectX::XMMATRIX         Player_GetWorld();
const DirectX::XMFLOAT3& Player_GetPosition();
float                     Player_GetYaw();
DirectX::XMFLOAT3         Player_GetForward();
