#pragma once
/*==============================================================================
    アニメーションイベント（フレームイベント） [anim_event.h]
    - クリップ名ごとに、時間に応じてイベントを発火させる
==============================================================================*/

#include <vector>
#include <string>
#include <DirectXMath.h>

enum class AnimEventType {
    SpawnHitBox,  // 攻击判定生成
    SetHurtBoxEnabled,
};

// SpawnHitBox 的参数
struct AnimEvent_SpawnHitBox {
    DirectX::XMFLOAT3 localOffset;  // 相对「创建者」的本地偏移 (x=右, y=上, z=前)
    DirectX::XMFLOAT3 halfSize;     // AABB 半尺寸
    float durationSec = 0.0f;       // 持续时间（秒）
    int   damage = 0;               // 伤害数值（先简单留个 int）
};

struct AnimEvent_SetHurtBoxEnabled
{
    bool enabled = true;   // true = 开启受击盒，false = 关闭
};

// 通用事件
struct AnimEvent {
    float timeNormalized = 0.0f;    // 0.0～1.0，归一化时间
    AnimEventType type = AnimEventType::SpawnHitBox;
    AnimEvent_SpawnHitBox spawnHitBox; // 目前只有这一种事件
    AnimEvent_SetHurtBoxEnabled setHurtBox;
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
