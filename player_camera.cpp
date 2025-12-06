#include "player.h"
#include "boss.h"
#include "camera.h"
#include "player_camera.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// ------------------ 内部状态 ------------------

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

static inline float Saturate(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline XMFLOAT3 LerpVec3(const XMFLOAT3& a, const XMFLOAT3& b, float t) {
    XMFLOAT3 r;
    r.x = a.x + (b.x - a.x) * t;
    r.y = a.y + (b.y - a.y) * t;
    r.z = a.z + (b.z - a.z) * t;
    return r;
}

// ------------------ 模式/过渡/调参 ------------------

enum class PlayerCameraMode {
    Free,
    LockOn,
};

struct CameraRig {
    XMFLOAT3 eye;
    XMFLOAT3 target;
};

struct CameraTransition {
    bool active = false;
    PlayerCameraMode fromMode = PlayerCameraMode::Free;
    PlayerCameraMode toMode = PlayerCameraMode::Free;

    CameraRig fromRig{};
    CameraRig toRig{};

    float duration = 0.0f;
    float elapsed = 0.0f;
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

static PlayerCameraMode s_mode = PlayerCameraMode::Free;
static CameraTransition s_transition{};
static LockOnTuning     s_lockTune{}; // 用默认值

// ------------------ 小工具 ------------------

static bool Boss_CanBeLockedOn()
{
    // 目前简单认为：Boss 身体 collider 存在就可锁定
    // 将来你可以在这里加“死亡/演出/潜地”等逻辑
    return Boss_GetBodyColliderId() >= 0;
}

// Free 模式下，根据 yaw/pitch/dist 计算“瞬时的”机位和注视点（不做平滑）
static CameraRig ComputeFreeRigImmediate()
{
    // 1) 摄像机朝向向量（眼睛指向目标）
    float cp = std::cos(s_camPitch);
    XMFLOAT3 forward{
        std::sin(s_camYaw) * cp,   // x
        std::sin(s_camPitch),      // y
        std::cos(s_camYaw) * cp    // z
    };

    // 2) 以玩家位置 + lookAtOffset 为中心
    XMFLOAT3 center = Player_GetPosition();
    center.x += s_desc.lookAtOffset.x;
    center.y += s_desc.lookAtOffset.y;
    center.z += s_desc.lookAtOffset.z;

    // 3) 机位 / 注视点
    XMFLOAT3 eye{
        center.x - forward.x * s_camDist,
        center.y - forward.y * s_camDist,
        center.z - forward.z * s_camDist
    };
    XMFLOAT3 target = center;

    return CameraRig{ eye, target };
}

// LockOn 模式下：玩家 & Boss 中点 + 距离相关高度 + 5点钟方向机位
static CameraRig ComputeLockOnRigImmediate()
{
    XMFLOAT3 P = Player_GetPosition();
    XMFLOAT3 B = Boss_GetPosition();

    float dx = B.x - P.x;
    float dz = B.z - P.z;
    float dXZ = std::sqrt(dx * dx + dz * dz);

    XMFLOAT3 C{
        0.5f * (P.x + B.x),
        0.5f * (P.y + B.y),
        0.5f * (P.z + B.z)
    };

    // 根据距离做 0..1 的插值因子
    float t = 0.0f;
    if (s_lockTune.distFar > s_lockTune.distNear) {
        t = Saturate((dXZ - s_lockTune.distNear) / (s_lockTune.distFar - s_lockTune.distNear));
    }

    // 目标点高度：近→高、远→低
    float targetHeight =
        (1.0f - t) * s_lockTune.targetHeightNear +
        t * s_lockTune.targetHeightFar;

    XMFLOAT3 target{
        C.x,
        C.y + targetHeight,
        C.z
    };

    // 中点 -> 玩家 的 XZ 方向
    float vx = P.x - C.x;
    float vz = P.z - C.z;
    float vlen = std::sqrt(vx * vx + vz * vz);
    if (vlen < 1e-4f) {
        // 玩家和 Boss 太近：用当前摄像机 yaw 来做一个兜底方向
        vx = std::sin(s_camYaw);
        vz = std::cos(s_camYaw);
        vlen = std::sqrt(vx * vx + vz * vz);
    }
    vx /= vlen;
    vz /= vlen;

    // 绕 Y 轴旋转 sideOffsetDeg（比如 -30°，落在 5 点钟方向）
    float rad = s_lockTune.sideOffsetDeg * (3.14159265f / 180.0f);
    float cs = std::cos(rad);
    float sn = std::sin(rad);
    float cx = vx * cs - vz * sn;
    float cz = vx * sn + vz * cs;
    XMFLOAT3 camDir{ cx, 0.0f, cz };

    float radius = s_lockTune.baseRadius;

    // 相机高度：近→略低、远→略高
    float camHeight =
        (1.0f - t) * s_lockTune.cameraHeightNear +
        t * s_lockTune.cameraHeightFar;

    XMFLOAT3 eye{
        target.x + camDir.x * radius,
        target.y + camHeight,
        target.z + camDir.z * radius
    };

    return CameraRig{ eye, target };
}

// 根据当前模式更新「给玩家用的移动基向量」
static void UpdateMoveBasis()
{
    if (s_mode == PlayerCameraMode::Free) {
        // Free：以摄像机朝向为基准
        XMFLOAT3 forward{
            s_target.x - s_eye.x,
            s_target.y - s_eye.y,
            s_target.z - s_eye.z
        };
        XMFLOAT3 fwdXZ{ forward.x, 0.0f, forward.z };
        float len = std::sqrt(fwdXZ.x * fwdXZ.x + fwdXZ.z * fwdXZ.z);
        if (len > 1e-4f) {
            fwdXZ.x /= len; fwdXZ.z /= len;
        }
        else {
            fwdXZ = { 0.0f, 0.0f, 1.0f };
        }
        s_moveForwardXZ = fwdXZ;
        s_moveRightXZ = { fwdXZ.z, 0.0f, -fwdXZ.x };
    }
    else {
        // LockOn：以 玩家->Boss 方向为前
        XMFLOAT3 P = Player_GetPosition();
        XMFLOAT3 B = Boss_GetPosition();
        float dx = B.x - P.x;
        float dz = B.z - P.z;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1e-4f) {
            dx /= len; dz /= len;
            s_moveForwardXZ = { dx, 0.0f, dz };
            s_moveRightXZ = { dz, 0.0f, -dx };
        }
        else {
            // 太近时兜底：退回摄像机朝向
            XMFLOAT3 forward{
                s_target.x - s_eye.x,
                s_target.y - s_eye.y,
                s_target.z - s_eye.z
            };
            XMFLOAT3 fwdXZ{ forward.x, 0.0f, forward.z };
            float len2 = std::sqrt(fwdXZ.x * fwdXZ.x + fwdXZ.z * fwdXZ.z);
            if (len2 > 1e-4f) {
                fwdXZ.x /= len2; fwdXZ.z /= len2;
            }
            else {
                fwdXZ = { 0.0f, 0.0f, 1.0f };
            }
            s_moveForwardXZ = fwdXZ;
            s_moveRightXZ = { fwdXZ.z, 0.0f, -fwdXZ.x };
        }
    }
}

// 启动一次模式切换过渡（时间优先 + 速度上限）
static void StartTransition(const CameraRig& from,
    const CameraRig& to,
    PlayerCameraMode fromMode,
    PlayerCameraMode toMode)
{
    s_transition.active = true;
    s_transition.fromMode = fromMode;
    s_transition.toMode = toMode;
    s_transition.fromRig = from;
    s_transition.toRig = to;

    float dx = to.eye.x - from.eye.x;
    float dy = to.eye.y - from.eye.y;
    float dz = to.eye.z - from.eye.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    float T = s_lockTune.transitionBaseTime;
    if (T <= 0.0f) T = 0.001f;

    float avgSpeed = (T > 0.0f) ? (dist / T) : 0.0f;
    if (s_lockTune.transitionMaxSpeed > 0.0f &&
        avgSpeed > s_lockTune.transitionMaxSpeed)
    {
        T = dist / s_lockTune.transitionMaxSpeed;
    }

    s_transition.duration = T;
    s_transition.elapsed = 0.0f;
}

// ------------------ 对外接口 ------------------

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

    s_mode = PlayerCameraMode::Free;
    s_transition.active = false;

    // 初始化 eye/target 用 Free 模式的理想机位
    CameraRig rig = ComputeFreeRigImmediate();
    s_eye = rig.eye;
    s_target = rig.target;

    UpdateMoveBasis();

    // 立刻推一次到底层相机
    XMVECTOR eye = XMLoadFloat3(&s_eye);
    XMVECTOR tgt = XMLoadFloat3(&s_target);
    XMVECTOR f = XMVector3Normalize(tgt - eye);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMFLOAT3 fOut, uOut;
    XMStoreFloat3(&fOut, f);
    XMStoreFloat3(&uOut, up);

    Camera_SetPose(s_eye, fOut, uOut);
}

void PlayerCamera_Update(double dt, const PlayerCameraInput& in)
{
    // 1) 处理锁定 toggle（仅在没有过渡时响应）
    if (in.lockTogglePressed && !s_transition.active) {
        if (s_mode == PlayerCameraMode::Free) {
            // Free -> LockOn
            if (Boss_CanBeLockedOn()) {
                CameraRig from{ s_eye, s_target };
                CameraRig to = ComputeLockOnRigImmediate();
                StartTransition(from, to, PlayerCameraMode::Free, PlayerCameraMode::LockOn);
                s_mode = PlayerCameraMode::LockOn;
            }
            else {
                // 将来可以在这里打 DebugText
            }
        }
        else {
            // LockOn -> Free
            CameraRig from{ s_eye, s_target };
            CameraRig to = ComputeFreeRigImmediate();
            StartTransition(from, to, PlayerCameraMode::LockOn, PlayerCameraMode::Free);
            s_mode = PlayerCameraMode::Free;
        }
    }

    // 2) 自由相机下的鼠标控制，仅 Free 模式且非过渡中才响应
    if (s_mode == PlayerCameraMode::Free && !s_transition.active) {
        const float yawSpeed = 0.003f;
        const float pitchSpeed = 0.003f;
        const float pitchMin = -DirectX::XM_PIDIV2 + 0.1f;
        const float pitchMax = DirectX::XM_PIDIV4;

        s_camYaw += in.deltaX * yawSpeed;
        s_camPitch -= in.deltaY * pitchSpeed;

        // Pitch 上下限
        s_camPitch = std::min(std::max(s_camPitch, pitchMin), pitchMax);

        // 滚轮缩放
        if (in.wheelDelta != 0.0f) {
            float zoomFactor = 1.0f - in.wheelDelta * 0.1f;
            zoomFactor = std::max(0.2f, std::min(2.0f, zoomFactor));
            s_camDist *= zoomFactor;
            s_camDist = std::max(0.5f, std::min(30.0f, s_camDist));
        }
    }
    else {
        // LockOn 模式：当前忽略鼠标旋转/滚轮，后面如果需要可以把滚轮映射到 baseRadius
    }

    // 3) 按当前模式计算「瞬时理想机位」
    CameraRig ideal{};
    if (s_mode == PlayerCameraMode::Free) {
        ideal = ComputeFreeRigImmediate();
    }
    else {
        // 如果锁定中 Boss 突然不可锁（将来有死亡等逻辑），自动切回 Free
        if (!Boss_CanBeLockedOn()) {
            CameraRig from{ s_eye, s_target };
            CameraRig toFree = ComputeFreeRigImmediate();
            StartTransition(from, toFree, s_mode, PlayerCameraMode::Free);
            s_mode = PlayerCameraMode::Free;
            ideal = toFree;
        }
        else {
            ideal = ComputeLockOnRigImmediate();
        }
    }

    // 4) 过渡 or 模式内部平滑
    if (s_transition.active) {
        s_transition.elapsed += static_cast<float>(dt);
        float t = (s_transition.duration > 0.0f)
            ? Saturate(s_transition.elapsed / s_transition.duration)
            : 1.0f;

        s_eye = LerpVec3(s_transition.fromRig.eye, s_transition.toRig.eye, t);
        s_target = LerpVec3(s_transition.fromRig.target, s_transition.toRig.target, t);

        if (t >= 1.0f) {
            s_transition.active = false;
        }
    }
    else {
        // 没在切镜头时，用原来的 ExpLerp 做跟随平滑
        float aF = ExpLerp01(s_desc.followSharpness, static_cast<float>(dt));
        float aL = ExpLerp01(s_desc.lookSharpness, static_cast<float>(dt));

        s_eye.x += (ideal.eye.x - s_eye.x) * aF;
        s_eye.y += (ideal.eye.y - s_eye.y) * aF;
        s_eye.z += (ideal.eye.z - s_eye.z) * aF;

        s_target.x += (ideal.target.x - s_target.x) * aL;
        s_target.y += (ideal.target.y - s_target.y) * aL;
        s_target.z += (ideal.target.z - s_target.z) * aL;
    }

    // 5) 更新移动基向量（Free/LockOn 不同策略）
    UpdateMoveBasis();

    // 6) 写入底层 Camera 模块
    XMVECTOR eye = XMLoadFloat3(&s_eye);
    XMVECTOR tgt = XMLoadFloat3(&s_target);
    XMVECTOR f = XMVector3Normalize(tgt - eye);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMFLOAT3 fOut, uOut;
    XMStoreFloat3(&fOut, f);
    XMStoreFloat3(&uOut, up);

    Camera_SetPose(s_eye, fOut, uOut);
}

void PlayerCamera_GetMoveBasis(XMFLOAT3* outForwardXZ, XMFLOAT3* outRightXZ)
{
    if (outForwardXZ) *outForwardXZ = s_moveForwardXZ;
    if (outRightXZ)   *outRightXZ = s_moveRightXZ;
}

bool PlayerCamera_IsLockOnActive()
{
    return s_mode == PlayerCameraMode::LockOn;
}
