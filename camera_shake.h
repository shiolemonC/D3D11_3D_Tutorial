#pragma once
#include <DirectXMath.h>
#include <cstdint>

enum class CameraShakeMode : int
{
    Horizontal = 0,
    Vertical = 1,
    Both = 2
};

// 清空所有 shake
void CameraShake_Clear();

// 每帧推进（dt=秒）
void CameraShake_Update(float dt);

// 推入一个 shake（magnitude=最大幅度(米)，durationSec=持续时间）
void CameraShake_Push(float magnitude, float durationSec, CameraShakeMode mode, int priority = 0);

// 计算本帧要叠加的相机位移（世界坐标，单位=米）
// 注意：不会改变 eye/target，本函数只是返回 offset
DirectX::XMFLOAT3 CameraShake_GetOffset(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& target);
