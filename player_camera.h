#pragma once
#include "camera_shake.h" // ★ 新增
#include <DirectXMath.h>

struct PlayerCameraDesc {
    DirectX::XMFLOAT3 offsetLocal{ 0, 2.0f, -5.0f };  // 机位相对玩家（初始半径和方向）
    DirectX::XMFLOAT3 lookAtOffset{ 0, 1.5f,  0.0f }; // 看向胸口/头部
    float followSharpness = 8.0f;   // 机位跟随趋近
    float lookSharpness = 12.0f;  // 注视点趋近
};

struct LockOnTuning {
    float distNear = 3.0f;    // 距离近（压迫感区间）
    float distFar = 15.0f;   // 距离远（overview 区间）

    float sideOffsetDeg = -30.0f; // 从“中点->玩家”方向旋转多少度得到相机方向（5点钟）

    float baseRadius = 7.0f;  // 相机与中点的基础半径

    // 目标点高度：近→高、远→低
    float targetHeightNear = 2.0f;
    float targetHeightFar = 1.0f;

    // 相机高度：近→略低、远→略高（和目标点方向相反一点）
    float cameraHeightNear = 1.2f;
    float cameraHeightFar = 2.0f;

    // 模式切换：时间优先 + 速度上限
    float transitionBaseTime = 0.35f; // 想要的切换时间
    float transitionMaxSpeed = 25.0f; // 最大允许速度（m/s）
};

// 一帧内摄像机收到的输入（鼠标/滚轮/锁定）
struct PlayerCameraInput {
    float deltaX = 0.0f;     // 鼠标 X 方向移动量（像素）
    float deltaY = 0.0f;     // 鼠标 Y 方向移动量（像素）
    float wheelDelta = 0.0f; // 鼠标滚轮（可选用）

    // 鼠标中键 toggle：按一下在 Free / LockOn 之间切换（由调用方转换成“一帧一次”）
    bool  lockTogglePressed = false;
};

void PlayerCamera_Initialize(const PlayerCameraDesc& d);

// 摄像机更新需要输入
void PlayerCamera_Update(double dt, const PlayerCameraInput& in);

// 提供「按摄像机/锁定方向移动」用的 XZ 平面基向量
void PlayerCamera_GetMoveBasis(DirectX::XMFLOAT3* outForwardXZ,
    DirectX::XMFLOAT3* outRightXZ);

// 是否处于锁定模式（Player 用来决定朝向）
bool PlayerCamera_IsLockOnActive();

// ★ LockOn 相机参数临时切换（镜头演出）
// - 仅在 LockOn 模式下生效（Free 模式下调用会被忽略）
// - priority 更高的会覆盖更低的
// - presetId 目前约定：0=Hit, 1=Block_Attack（你可以继续扩展）
void PlayerCamera_PushLockOnPreset(int presetId,
    float blendInSec,
    float holdSec,
    float blendOutSec,
    int priority);


void PlayerCamera_PushShake(float magnitude, float durationSec, CameraShakeMode mode,
    float blendInSec = 0.0f, int priority = 0);