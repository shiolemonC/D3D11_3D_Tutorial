#include "player.h"
using namespace DirectX;

static XMFLOAT3 s_pos{ 0,0,0 };
static float    s_yaw = 0.0f; // 绕Y，弧度
static float    s_speed = 2.5f;
static float    s_turnK = 10.0f;
static float    s_scale = 1.0f;

static inline float AngleDelta(float a, float b) {
    float d = fmodf(b - a + XM_PI, XM_2PI) - XM_PI;
    return (d < -XM_PI) ? d + XM_2PI : d;
}
static inline float ExpLerp01(float k, float dt) {
    return 1.0f - expf(-k * (float)dt);
}

void Player_Initialize(const PlayerDesc& d) {
    s_pos = d.spawnPos;
    s_speed = d.moveSpeed;
    s_turnK = d.turnSharpness;
    s_scale = d.scale;
}

PlayerState Player_Update(double dt, const PlayerInput& in, PlayerState smState)
{
    // 1) 运动（仅在 Move 状态且有输入时）
    XMFLOAT2 v2{ in.moveX, in.moveZ };
    float len = sqrtf(v2.x * v2.x + v2.y * v2.y);
    if (len > 1e-5f) { v2.x /= len; v2.y /= len; }

    if (smState == PlayerState::Move && len > 1e-4f) {
        float targetYaw = atan2f(v2.x, v2.y); // x→左右，z→前后（右手系）
        float a = ExpLerp01(s_turnK, (float)dt);
        s_yaw += AngleDelta(s_yaw, targetYaw) * a;

        s_pos.x += v2.x * s_speed * (float)dt;
        s_pos.z += v2.y * s_speed * (float)dt;
        // Y 由地形/重力系统管理，这里不改
    }

    // 2) 拼世界矩阵并喂给动画系统
    XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);
    XMMATRIX R = XMMatrixRotationY(s_yaw);
    XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
    XMMATRIX W = S * R * T;
    AnimatorRegistry_SetWorld(W);

    return smState;
}

void Player_Kinematic_Update(double dt, const PlayerInput& in, bool locomotionActive)
{
    XMFLOAT2 v2{ in.moveX, in.moveZ };
    float len = sqrtf(v2.x * v2.x + v2.y * v2.y);
    if (len > 1e-5f) { v2.x /= len; v2.y /= len; }
        if (locomotionActive && len > 1e-4f) {
            float targetYaw = atan2f(v2.x, v2.y);
            float a = ExpLerp01(s_turnK, (float)dt);
            s_yaw += AngleDelta(s_yaw, targetYaw) * a;

            s_pos.x += v2.x * s_speed * (float)dt;
            s_pos.z += v2.y * s_speed * (float)dt;
        }

        XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);
        XMMATRIX R = XMMatrixRotationY(s_yaw);
        XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
        XMMATRIX W = S * R * T;
        AnimatorRegistry_SetWorld(W);
}

void Player_ApplyRootMotionDelta(const RootMotionDelta& rm)
{
    // 常见做法：只取 XZ；若有需要可保留 Y（突进起伏）
    s_pos.x += rm.pos.x;
    // s_pos.y += rm.pos.y; // 基本先不启用
    s_pos.z += rm.pos.z;
    s_yaw += rm.yaw;     // 目前 0，将来如需可启用
}

XMMATRIX Player_GetWorld() {
    XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);
    XMMATRIX R = XMMatrixRotationY(s_yaw);
    XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
    return S * R * T;
}
const XMFLOAT3& Player_GetPosition() { return s_pos; }
float Player_GetYaw() { return s_yaw; }
XMFLOAT3 Player_GetForward() {
    XMVECTOR f = XMVector3Normalize(XMVectorSet(sinf(s_yaw), 0, cosf(s_yaw), 0));
    XMFLOAT3 out; XMStoreFloat3(&out, f); return out;
}