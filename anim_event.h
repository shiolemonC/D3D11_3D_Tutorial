#pragma once
/*==============================================================================
    アニメーションイベント（フレームイベント） [anim_event.h]
    - クリップ名ごとに、時間に応じてイベントを発火させる
==============================================================================*/

#include "camera_shake.h" // ★ 新增，用同一个 enum
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "hit_event.h" // ★ 为了使用 HitLevel

enum class AnimEventType {
    SpawnHitBox,  // 攻击判定生成

    SetHurtBoxEnabled,

    SetParryWindowEnabled,

    CameraLockOnPreset,

    CameraShake, // ★ 新增

    PostFxRadialBlurStart,
};

// SpawnHitBox 的参数
struct AnimEvent_SpawnHitBox {
    DirectX::XMFLOAT3 localOffset;  // 相对「创建者」的本地偏移 (x=右, y=上, z=前)
    DirectX::XMFLOAT3 halfSize;     // AABB 半尺寸
    float durationSec = 0.0f;       // 持续时间（秒）
    int   damage = 0;               // 伤害数值（先简单留个 int）

    // ★ 新增：该攻击的受击等级
    HitLevel hitLevel = HitLevel::Light;

    float knockbackDistance = 0.0f; // ★ 新增：击退距离（米），0=不击退
};

struct AnimEvent_SetHurtBoxEnabled
{
    bool enabled = true;   // true = 开启受击盒，false = 关闭
};

struct AnimEvent_SetParryWindowEnabled
{
    bool enabled = false;  // true=开启成功格挡窗口，false=关闭
};

struct AnimEvent_CameraLockOnPreset
{
    int   presetId = 0;          // 0=Hit, 1=Block_Attack ...
    float blendInSec = 0.03f;    // 进场时间（秒）
    float holdSec = 0.06f;    // 保持时间（秒）
    float blendOutSec = 0.20f;    // 退场时间（秒）
    int   priority = 200;      // 优先级（大覆盖小）
};

struct AnimEvent_CameraShake
{
    float magnitude = 0.0f;    // 米
    float durationSec = 0.0f;  // 秒
    CameraShakeMode mode = CameraShakeMode::Both;
    int priority = 0;
};

struct AnimEventPostFxRadialBlurStart
{
    DirectX::XMFLOAT3 centerWorld;
    float durationSec;
    float strength;
    float radius;
    int   sampleCount;
    float _pad[3];
};

// 通用事件
struct AnimEvent 
{
    float timeNormalized = 0.0f;    // 0.0～1.0，归一化时间
    AnimEventType type = AnimEventType::SpawnHitBox;
    AnimEvent_SpawnHitBox spawnHitBox; // 目前只有这一种事件
    AnimEvent_SetHurtBoxEnabled setHurtBox;
    AnimEvent_SetParryWindowEnabled setParryWindow; // ★ 新增

    AnimEvent_CameraLockOnPreset cameraLockOnPreset; // ★ 新增

    AnimEvent_CameraShake cameraShake;

    AnimEventPostFxRadialBlurStart postFxRadialBlurStart;
};

// 某个剪辑的事件轨道
struct AnimEventTrack {
    std::wstring clipName;          // 例如 L"Attack"
    std::vector<AnimEvent> events;  // 按 timeNormalized 升序
};

// 注册表 API
void AnimEventRegistry_Clear();
void AnimEventRegistry_Register(const AnimEventTrack& track);
const AnimEventTrack* AnimEventRegistry_Find(const std::wstring& clipName);

// 用于一次性填表（类似 AnimatorRegister）
void AnimEventRegister();  // 在 Game_Initialize 里调用
