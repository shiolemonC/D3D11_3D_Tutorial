#pragma once
#include <DirectXMath.h>

struct PlayerCameraDesc {
    DirectX::XMFLOAT3 offsetLocal{ 0, 2.0f, -5.0f }; // 机位相对玩家
    DirectX::XMFLOAT3 lookAtOffset{ 0, 1.5f,  0.0f }; // 看向胸口/头部
    float followSharpness = 8.0f;  // 机位趋近
    float lookSharpness = 12.0f; // 注视点趋近
};

void PlayerCamera_Initialize(const PlayerCameraDesc& d);
void PlayerCamera_Update(double dt);
