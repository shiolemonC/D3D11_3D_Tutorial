
#include "player.h"
#include "camera.h"
#include "player_camera.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

static XMFLOAT3        s_eye{ 0,0,0 };
static XMFLOAT3        s_target{ 0,0,0 };
static PlayerCameraDesc s_desc{};

// 轨道相机参数（独立于玩家 yaw）
static float s_camYaw = 0.0f; // 绕世界Y轴
static float s_camPitch = 0.0f; // 绕X轴（俯仰）
static float s_camDist = 5.0f; // 半径

// 提供给玩家移动用的基向量（XZ 平面）
static XMFLOAT3 s_moveForwardXZ{ 0.0f, 0.0f, 1.0f };
static XMFLOAT3 s_moveRightXZ{ 1.0f, 0.0f, 0.0f };

static inline float ExpLerp01(float k, float dt) {
    return 1.0f - std::exp(-k * dt);
}

// 根据当前 yaw/pitch/dist 更新机位和注视点
static void UpdateCameraPose(double dt)
{
    // 1) 计算摄像机的「朝向向量」（从眼睛指向目标）
    float cp = std::cos(s_camPitch);
    XMFLOAT3 forward{
        std::sin(s_camYaw) * cp,   // x
        std::sin(s_camPitch),      // y
        std::cos(s_camYaw) * cp    // z
    };

    // XZ 平面上的移动基向量
    XMFLOAT3 fwdXZ{ forward.x, 0.0f, forward.z };
    float len = std::sqrt(fwdXZ.x * fwdXZ.x + fwdXZ.z * fwdXZ.z);
    if (len > 1e-4f) {
        fwdXZ.x /= len; fwdXZ.z /= len;
    }
    else {
        fwdXZ = { 0.0f, 0.0f, 1.0f };
    }
    s_moveForwardXZ = fwdXZ;
    s_moveRightXZ = { fwdXZ.z, 0.0f, -fwdXZ.x }; // up(0,1,0) × forwardXZ

    // 2) 以玩家位置+lookAtOffset 为中心
    XMFLOAT3 center = Player_GetPosition();
    center.x += s_desc.lookAtOffset.x;
    center.y += s_desc.lookAtOffset.y;
    center.z += s_desc.lookAtOffset.z;

    // 3) 目标机位 / 目标注视点
    XMFLOAT3 eyeTarget{
        center.x - forward.x * s_camDist,
        center.y - forward.y * s_camDist,
        center.z - forward.z * s_camDist
    };
    XMFLOAT3 tgtTarget = center;

    // 4) 平滑跟随
    float aF = ExpLerp01(s_desc.followSharpness, static_cast<float>(dt));
    float aL = ExpLerp01(s_desc.lookSharpness, static_cast<float>(dt));

    s_eye.x += (eyeTarget.x - s_eye.x) * aF;
    s_eye.y += (eyeTarget.y - s_eye.y) * aF;
    s_eye.z += (eyeTarget.z - s_eye.z) * aF;

    s_target.x += (tgtTarget.x - s_target.x) * aL;
    s_target.y += (tgtTarget.y - s_target.y) * aL;
    s_target.z += (tgtTarget.z - s_target.z) * aL;

    // 5) 写入底层 Camera 模块
    XMVECTOR eye = XMLoadFloat3(&s_eye);
    XMVECTOR tgt = XMLoadFloat3(&s_target);
    XMVECTOR f = XMVector3Normalize(tgt - eye);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMFLOAT3 fOut, uOut;
    XMStoreFloat3(&fOut, f);
    XMStoreFloat3(&uOut, up);

    Camera_SetPose(s_eye, fOut, uOut);
}

void PlayerCamera_Initialize(const PlayerCameraDesc& d)
{
    s_desc = d;

    // 用 offsetLocal 推算初始 yaw/pitch/dist
    XMFLOAT3 off = d.offsetLocal;
    s_camDist = std::max(0.1f, std::sqrt(off.x * off.x + off.y * off.y + off.z * off.z));

    // 眼到目标的向量是 -offsetLocal
    XMFLOAT3 dir{ -off.x, -off.y, -off.z };
    float horiz = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    if (horiz > 1e-4f) {
        s_camYaw = std::atan2(dir.x, dir.z);
        s_camPitch = std::atan2(dir.y, horiz);
    }
    else {
        s_camYaw = 0.0f;
        s_camPitch = 0.0f;
    }

    // 初始化 eye/target
    s_eye = { 0,0,0 };
    s_target = { 0,0,0 };
    UpdateCameraPose(0.0);
}

void PlayerCamera_Update(double dt, const PlayerCameraInput& in)
{
    // 鼠标控制 yaw/pitch（怪猎式）
    const float yawSpeed = 0.003f; // 每像素对应的弧度速度，你可以之后自己调感觉
    const float pitchSpeed = 0.003f;
    const float pitchMin = -DirectX::XM_PIDIV2 + 0.1f;
    const float pitchMax = DirectX::XM_PIDIV4;

    s_camYaw += in.deltaX * yawSpeed;
    s_camPitch -= in.deltaY * pitchSpeed;

    // Pitch 上下限
    s_camPitch = std::min(std::max(s_camPitch, pitchMin), pitchMax);

    // 滚轮缩放（简单版，可根据你 Mouse_State 的 wheel 字段调整）
    if (in.wheelDelta != 0.0f) {
        float zoomFactor = 1.0f - in.wheelDelta * 0.1f;
        zoomFactor = std::max(0.2f, std::min(2.0f, zoomFactor));
        s_camDist *= zoomFactor;
        s_camDist = std::max(0.5f, std::min(30.0f, s_camDist));
    }

    UpdateCameraPose(dt);
}

void PlayerCamera_GetMoveBasis(XMFLOAT3* outForwardXZ, XMFLOAT3* outRightXZ)
{
    if (outForwardXZ) *outForwardXZ = s_moveForwardXZ;
    if (outRightXZ)   *outRightXZ = s_moveRightXZ;
}
