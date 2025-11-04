#include "player_camera.h"
#include "player.h"
#include "camera.h"
using namespace DirectX;

static XMFLOAT3 s_eye{ 0,0,0 }, s_target{ 0,0,0 };
static PlayerCameraDesc s_desc{};

static inline float ExpLerp01(float k, float dt) { return 1.0f - expf(-k * dt); }

void PlayerCamera_Initialize(const PlayerCameraDesc& d)
{
    s_desc = d;
    // 初值：直接放到玩家后上方
    const auto& p = Player_GetPosition();
    float yaw = Player_GetYaw();
    float sy = sinf(yaw), cy = cosf(yaw);
    XMFLOAT3 offW{ s_desc.offsetLocal.x * cy + s_desc.offsetLocal.z * sy,
                   s_desc.offsetLocal.y,
                  -s_desc.offsetLocal.x * sy + s_desc.offsetLocal.z * cy };
    s_eye = { p.x + offW.x, p.y + offW.y, p.z + offW.z };
    s_target = { p.x + s_desc.lookAtOffset.x, p.y + s_desc.lookAtOffset.y, p.z + s_desc.lookAtOffset.z };

    Camera_EnableExternalControl(true);
}

void PlayerCamera_Update(double dt)
{
    const auto& p = Player_GetPosition();
    float yaw = Player_GetYaw();
    float sy = sinf(yaw), cy = cosf(yaw);

    XMFLOAT3 offW{ s_desc.offsetLocal.x * cy + s_desc.offsetLocal.z * sy,
                   s_desc.offsetLocal.y,
                  -s_desc.offsetLocal.x * sy + s_desc.offsetLocal.z * cy };
    XMFLOAT3 eyeTarget{ p.x + offW.x, p.y + offW.y, p.z + offW.z };
    XMFLOAT3 tgtTarget{ p.x + s_desc.lookAtOffset.x, p.y + s_desc.lookAtOffset.y, p.z + s_desc.lookAtOffset.z };

    float aF = ExpLerp01(s_desc.followSharpness, (float)dt);
    float aL = ExpLerp01(s_desc.lookSharpness, (float)dt);

    s_eye.x += (eyeTarget.x - s_eye.x) * aF;
    s_eye.y += (eyeTarget.y - s_eye.y) * aF;
    s_eye.z += (eyeTarget.z - s_eye.z) * aF;

    s_target.x += (tgtTarget.x - s_target.x) * aL;
    s_target.y += (tgtTarget.y - s_target.y) * aL;
    s_target.z += (tgtTarget.z - s_target.z) * aL;

    XMVECTOR eye = XMLoadFloat3(&s_eye);
    XMVECTOR tgt = XMLoadFloat3(&s_target);
    XMVECTOR front = XMVector3Normalize(tgt - eye);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMFLOAT3 f, u; XMStoreFloat3(&f, front); XMStoreFloat3(&u, up);
    Camera_SetPose(s_eye, f, u);
}
