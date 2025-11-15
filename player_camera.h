#pragma once
#include <DirectXMath.h>

struct PlayerCameraDesc {
    DirectX::XMFLOAT3 offsetLocal{ 0, 2.0f, -5.0f };  // 机位相对玩家（初始半径和方向）
    DirectX::XMFLOAT3 lookAtOffset{ 0, 1.5f,  0.0f }; // 看向胸口/头部
    float followSharpness = 8.0f;   // 机位跟随趋近
    float lookSharpness = 12.0f;  // 注视点趋近
};

// 新：一帧内摄像机收到的输入（鼠标/滚轮）
struct PlayerCameraInput {
    float deltaX = 0.0f; // 鼠标 X 方向移动量（像素）
    float deltaY = 0.0f; // 鼠标 Y 方向移动量（像素）
    float wheelDelta = 0.0f; // 鼠标滚轮（可选用）
};

void PlayerCamera_Initialize(const PlayerCameraDesc& d);

// 新签名：摄像机更新需要鼠标输入
void PlayerCamera_Update(double dt, const PlayerCameraInput& in);

// 新：提供「按摄像机方向移动」所需的 XZ 平面基向量
void PlayerCamera_GetMoveBasis(DirectX::XMFLOAT3* outForwardXZ,
    DirectX::XMFLOAT3* outRightXZ);
