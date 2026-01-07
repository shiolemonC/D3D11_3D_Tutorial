#include "player.h"
#include "boss.h"
#include "camera.h"
#include "camera_shake.h"
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

// ★ LockOn 镜头演出：预设参数（在 default 基础上微调）
static LockOnTuning s_lockTuneHit{};
static LockOnTuning s_lockTuneBlockAttack{};

struct LockOnPresetState {
    bool  active = false;
    int   presetId = 0;
    int   priority = 0;
    float blendInSec = 0.03f;
    float holdSec = 0.06f;
    float blendOutSec = 0.20f;
    float elapsedSec = 0.0f;
};

static LockOnPresetState s_preset{};


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
static CameraRig ComputeLockOnRigImmediate(const LockOnTuning& tune)
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
    if (tune.distFar > tune.distNear) {
        t = Saturate((dXZ - tune.distNear) / (tune.distFar - tune.distNear));
    }

    // 目标点高度：近→高、远→低
    float targetHeight =
        (1.0f - t) * tune.targetHeightNear +
        t * tune.targetHeightFar;

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
    float rad = tune.sideOffsetDeg * (3.14159265f / 180.0f);
    float cs = std::cos(rad);
    float sn = std::sin(rad);
    float cx = vx * cs - vz * sn;
    float cz = vx * sn + vz * cs;
    XMFLOAT3 camDir{ cx, 0.0f, cz };

    float radius = tune.baseRadius;

    // 相机高度：近→略低、远→略高
    float camHeight =
        (1.0f - t) * tune.cameraHeightNear +
        t * tune.cameraHeightFar;

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

    // ★ LockOn 演出预设（你之后可以根据手感继续调这些参数）
    s_lockTuneHit = s_lockTune;
    s_lockTuneHit.baseRadius = std::max(0.1f, s_lockTune.baseRadius * 0.85f);
    s_lockTuneHit.cameraHeightNear = s_lockTune.cameraHeightNear * 0.90f;
    s_lockTuneHit.cameraHeightFar = s_lockTune.cameraHeightFar * 0.90f;

    s_lockTuneBlockAttack = s_lockTune;
    s_lockTuneBlockAttack.baseRadius = std::max(0.1f, s_lockTune.baseRadius * 0.90f);
    s_lockTuneBlockAttack.sideOffsetDeg = s_lockTune.sideOffsetDeg - 10.0f; // 更侧一点（更有“演出感”）
    s_lockTuneBlockAttack.cameraHeightNear = s_lockTune.cameraHeightNear * 0.95f;
    s_lockTuneBlockAttack.cameraHeightFar = s_lockTune.cameraHeightFar * 0.95f;

    s_preset.active = false;

    // 立刻推一次到底层相机
    XMVECTOR eye = XMLoadFloat3(&s_eye);
    XMVECTOR tgt = XMLoadFloat3(&s_target);
    XMVECTOR f = XMVector3Normalize(tgt - eye);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMFLOAT3 fOut, uOut;
    XMStoreFloat3(&fOut, f);
    XMStoreFloat3(&uOut, up);

    Camera_SetPose(s_eye, fOut, uOut);
    CameraShake_Clear();
}

void PlayerCamera_Update(double dt, const PlayerCameraInput& in)
{
    const float dtf = static_cast<float>(dt);

    //--------------------------------------------------------------------------
    // 1) 处理锁定 toggle（仅在没有过渡时响应）
    //--------------------------------------------------------------------------
    if (in.lockTogglePressed && !s_transition.active)
    {
        if (s_mode == PlayerCameraMode::Free)
        {
            // Free -> LockOn
            if (Boss_CanBeLockedOn())
            {
                CameraRig from{ s_eye, s_target };
                CameraRig to = ComputeLockOnRigImmediate(s_lockTune);
                StartTransition(from, to, PlayerCameraMode::Free, PlayerCameraMode::LockOn);
                s_mode = PlayerCameraMode::LockOn;

                // ★ 切模式时清空演出
                s_preset.active = false;
            }
        }
        else
        {
            // LockOn -> Free
            CameraRig from{ s_eye, s_target };
            CameraRig to = ComputeFreeRigImmediate(); // ← 这里替换成你真实的 Free 参数变量
            StartTransition(from, to, PlayerCameraMode::LockOn, PlayerCameraMode::Free);
            s_mode = PlayerCameraMode::Free;

            // ★ 切模式时清空演出
            s_preset.active = false;
        }
    }

    //--------------------------------------------------------------------------
    // 2) 自由相机下的鼠标控制，仅 Free 且非过渡中才响应
    //--------------------------------------------------------------------------
    if (s_mode == PlayerCameraMode::Free && !s_transition.active)
    {
        const float yawSpeed = 0.003f;
        const float pitchSpeed = 0.003f;
        const float pitchMin = -DirectX::XM_PIDIV2 + 0.1f;
        const float pitchMax = DirectX::XM_PIDIV4;

        s_camYaw += in.deltaX * yawSpeed;
        s_camPitch -= in.deltaY * pitchSpeed;

        s_camPitch = std::min(std::max(s_camPitch, pitchMin), pitchMax);

        if (in.wheelDelta != 0.0f)
        {
            float zoomFactor = 1.0f - in.wheelDelta * 0.1f;
            zoomFactor = std::max(0.2f, std::min(2.0f, zoomFactor));
            s_camDist *= zoomFactor;
            s_camDist = std::max(0.5f, std::min(30.0f, s_camDist));
        }
    }
    // else: LockOn 模式目前忽略鼠标输入（你原本的策略）

    //--------------------------------------------------------------------------
    // 3) 按当前模式计算「瞬时理想机位」 +（LockOn 演出混合）
    //--------------------------------------------------------------------------
    float presetWeight = 0.0f;
    CameraRig ideal{};

    if (s_mode == PlayerCameraMode::Free)
    {
        ideal = ComputeFreeRigImmediate(); // ← 替换成你真实的 Free 参数变量
    }
    else // LockOn
    {
        // 如果锁定中 Boss 突然不可锁，自动切回 Free
        if (!Boss_CanBeLockedOn())
        {
            CameraRig from{ s_eye, s_target };
            CameraRig toFree = ComputeFreeRigImmediate(); // ← 替换成你真实的 Free 参数变量
            StartTransition(from, toFree, s_mode, PlayerCameraMode::Free);
            s_mode = PlayerCameraMode::Free;

            // ★ 清空演出
            s_preset.active = false;

            ideal = toFree;
        }
        else
        {
            // base：默认 LockOn rig
            CameraRig base = ComputeLockOnRigImmediate(s_lockTune);
            ideal = base;

            // ★ 演出：只在 LockOn 且非 transition 时生效
            if (!s_transition.active && s_preset.active)
            {
                s_preset.elapsedSec += dtf;

                float t = s_preset.elapsedSec;
                const float inSec = std::max(1e-6f, s_preset.blendInSec);
                const float holdSec = std::max(0.0f, s_preset.holdSec);
                const float outSec = std::max(1e-6f, s_preset.blendOutSec);

                if (t < inSec) {
                    presetWeight = t / inSec;
                }
                else {
                    t -= inSec;
                    if (t < holdSec) {
                        presetWeight = 1.0f;
                    }
                    else {
                        t -= holdSec;
                        if (t < outSec) {
                            presetWeight = 1.0f - (t / outSec);
                        }
                        else {
                            s_preset.active = false;
                            presetWeight = 0.0f;
                        }
                    }
                }

                // 混合 base / fx
                if (presetWeight > 0.0f)
                {
                    const LockOnTuning& fxTune =
                        (s_preset.presetId == 1) ? s_lockTuneBlockAttack : s_lockTuneHit;

                    CameraRig fx = ComputeLockOnRigImmediate(fxTune);

                    ideal.eye = LerpVec3(base.eye, fx.eye, presetWeight);
                    ideal.target = LerpVec3(base.target, fx.target, presetWeight);
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    // 4) 过渡 or 模式内部平滑
    //--------------------------------------------------------------------------
    if (s_transition.active)
    {
        s_transition.elapsed += dtf;
        float t = (s_transition.duration > 0.0f)
            ? Saturate(s_transition.elapsed / s_transition.duration)
            : 1.0f;

        s_eye = LerpVec3(s_transition.fromRig.eye, s_transition.toRig.eye, t);
        s_target = LerpVec3(s_transition.fromRig.target, s_transition.toRig.target, t);

        if (t >= 1.0f) {
            s_transition.active = false;
        }
    }
    else
    {
        float followK = s_desc.followSharpness;
        float lookK = s_desc.lookSharpness;

        // ★ 演出期间加速跟随
        if (presetWeight > 0.0f) {
            followK = std::max(followK, 30.0f);
            lookK = std::max(lookK, 40.0f);
        }

        float aF = ExpLerp01(followK, dtf);
        float aL = ExpLerp01(lookK, dtf);

        s_eye.x += (ideal.eye.x - s_eye.x) * aF;
        s_eye.y += (ideal.eye.y - s_eye.y) * aF;
        s_eye.z += (ideal.eye.z - s_eye.z) * aF;

        s_target.x += (ideal.target.x - s_target.x) * aL;
        s_target.y += (ideal.target.y - s_target.y) * aL;
        s_target.z += (ideal.target.z - s_target.z) * aL;
    }

    //--------------------------------------------------------------------------
    // 5) 更新移动基向量
    //--------------------------------------------------------------------------
    UpdateMoveBasis();

    //----------------------------------------------------------------------
    // 5.5) 相机抖动：叠加层（不写回 s_eye/s_target，防止累积漂移）
//----------------------------------------------------------------------
    CameraShake_Update(dtf);

    XMFLOAT3 eyeOut = s_eye;
    XMFLOAT3 tgtOut = s_target;

    const XMFLOAT3 shakeOff = CameraShake_GetOffset(s_eye, s_target);
    eyeOut.x += shakeOff.x; eyeOut.y += shakeOff.y; eyeOut.z += shakeOff.z;
    tgtOut.x += shakeOff.x; tgtOut.y += shakeOff.y; tgtOut.z += shakeOff.z;


    //--------------------------------------------------------------------------
    // 6) 写入底层 Camera
    //--------------------------------------------------------------------------
    XMVECTOR eye = XMLoadFloat3(&eyeOut);
    XMVECTOR tgt = XMLoadFloat3(&tgtOut);

    XMVECTOR f = XMVector3Normalize(tgt - eye);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMFLOAT3 fOut, uOut;
    XMStoreFloat3(&fOut, f);
    XMStoreFloat3(&uOut, up);

    Camera_SetPose(eyeOut, fOut, uOut);
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

void PlayerCamera_PushLockOnPreset(int presetId, float blendInSec, float holdSec, float blendOutSec, int priority)
{
    // 仅 LockOn 下有效
    if (s_mode != PlayerCameraMode::LockOn) return;
    if (s_transition.active) return;

    // 低优先级不能覆盖高优先级
    if (s_preset.active && priority < s_preset.priority) {
        return;
    }

    s_preset.active = true;
    s_preset.presetId = presetId;
    s_preset.priority = priority;
    s_preset.blendInSec = std::max(0.0f, blendInSec);
    s_preset.holdSec = std::max(0.0f, holdSec);
    s_preset.blendOutSec = std::max(0.0f, blendOutSec);
    s_preset.elapsedSec = 0.0f;
}

void PlayerCamera_PushShake(float magnitude, float durationSec, CameraShakeMode mode, float blendInSec, int priority)
{
    // 任何模式都允许抖动叠加（Free/LockOn/transition 都可共存）
    CameraShake_Push(magnitude, durationSec, mode, priority);
}
