#include "player_state.h"   // FSM
#include <DirectXMath.h>
#include <cmath>
#include "player.h"
using namespace DirectX;

// ------------------ 内部状态 ------------------
static XMFLOAT3 s_pos{ 0,0,0 };
static float    s_yaw = 0.0f; // 绕Y，弧度（玩家逻辑朝向）
static float    s_speed = 2.5f;
static float    s_turnK = 10.0f;
static float    s_scale = 1.0f;

static inline float AngleDelta(float a, float b) {
    float d = fmodf(b - a + XM_PI, XM_2PI) - XM_PI;
    return (d < -XM_PI) ? d + XM_2PI : d;
}
static inline float ExpLerp01(float k, float dt) {
    return 1.0f - expf(-k * dt);
}

// ------------------ 初始化 ------------------
void Player_Initialize(const PlayerDesc& d)
{
    s_pos = d.spawnPos;
    s_speed = d.moveSpeed;
    s_turnK = d.turnSharpness;
    s_scale = d.scale;
}

// ------------------ 内部：基于输入的运动（类魂/怪猎） ------------------
// 这里的 in 已经包含「摄像机方向」，所以 WASD 会按摄像机坐标系解释
static void Player_Kinematic_Update(double dt,
    const PlayerUpdateInput& in,
    bool locomotionActive)
{
    // 1) 构建世界系下的移动向量（XZ 平面）
    //    moveDir = moveX * camRight + moveZ * camForward
    XMFLOAT3 f = in.camForwardXZ;
    XMFLOAT3 r = in.camRightXZ;

    XMFLOAT2 v2{
        in.moveX * r.x + in.moveZ * f.x, // 世界 X
        in.moveX * r.z + in.moveZ * f.z  // 世界 Z
    };

    float len = std::sqrt(v2.x * v2.x + v2.y * v2.y);
    if (len > 1e-5f) {
        v2.x /= len;
        v2.y /= len;
    }

    if (locomotionActive && len > 1e-4f) {
        // 2) 计算目标朝向（世界系），并用指数趋近平滑转身
        float targetYaw = std::atan2(v2.x, v2.y); // x=左右，z=前后

        float a = ExpLerp01(s_turnK, static_cast<float>(dt));
        s_yaw += AngleDelta(s_yaw, targetYaw) * a;

        // 3) 沿着当前移动方向前进（速度为常数 s_speed）
        s_pos.x += v2.x * s_speed * static_cast<float>(dt);
        s_pos.z += v2.y * s_speed * static_cast<float>(dt);
    }

    // 4) 把玩家「当前真值」同步到动画系统的 BaseWorld
    XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);

    // ⚠ 这里暂时仍保留 +XM_PI，用来对齐当前 FBX 的朝向。
    //   将来如果你把资源和 nodeFix 调通，可以把 +XM_PI 删掉，让 s_yaw 真正代表模型的面对方向。
    XMMATRIX R = XMMatrixRotationY(s_yaw + XM_PI);
    XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
    XMMATRIX W = S * R * T;

    AnimatorRegistry_SetWorld(W);
}

// ------------------ 内部：应用 RootMotion Δ ------------------
static void Player_ApplyRootMotionDelta(const RootMotionDelta& rm)
{
    s_pos.x += rm.pos.x;
    // s_pos.y += rm.pos.y; // 一般不启用Y，防止动画导致穿地
    s_pos.z += rm.pos.z;

    s_yaw += rm.yaw; // 目前 rm.yaw 在调用前可以为 0，将来需要时可以启用
}

// ------------------ 对外：一帧更新 ------------------
void Player_Update(double dt, const PlayerUpdateInput& in)
{
    // 1) 把输入写入状态机条件
    PlayerSM_SetMoveInput(in.moveX, in.moveZ);
    if (in.attack) {
        PlayerSM_FireTrigger("Attack");
    }

    // 2) 跑 FSM，决定当前播放的状态/动画
    PlayerSMOutput smOut = PlayerSM_Update(dt);

    if (smOut.changed) {
        // 播放新动画
        AnimatorRegistry_Play(smOut.clip, nullptr);

        // 如果该状态没定 length_sec，就用真实动画长度回写
        float clipSec = 0.0f;
        if (AnimatorRegistry_DebugGetCurrentClipLengthSec(&clipSec)) {
            PlayerSM_OverrideCurrentStateLength(clipSec);
        }
    }

    // 3) 根据 FSM 的 locomotionActive 决定是否允许 WASD 驱动位移
    Player_Kinematic_Update(dt, in, smOut.locomotionActive);

    // 4) 动画时间推进 + RootMotion 累积
    AnimatorRegistry_Update(dt);

    // 5) 若当前状态允许使用 RootMotion，就消费动画 Δ 并同步回玩家
    if (smOut.useRootMotion) {
        RootMotionDelta rm{};
        if (AnimatorRegistry_ConsumeRootMotionDelta(&rm)) {
            rm.pos.y = 0.0f; // 一般只保留XZ
            // rm.yaw 可以按照需要启用
            rm.yaw = 0.0f;
            Player_ApplyRootMotionDelta(rm);
        }
    }
}

// ------------------ 查询接口 ------------------
XMMATRIX Player_GetWorld()
{
    XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);
    XMMATRIX R = XMMatrixRotationY(s_yaw);
    XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
    return S * R * T;
}
const XMFLOAT3& Player_GetPosition() { return s_pos; }
float Player_GetYaw() { return s_yaw; }
XMFLOAT3 Player_GetForward()
{
    XMVECTOR f = XMVector3Normalize(XMVectorSet(std::sinf(s_yaw), 0, std::cosf(s_yaw), 0));
    XMFLOAT3 out; XMStoreFloat3(&out, f); return out;
}
