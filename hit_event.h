#pragma once
#include <cstdint>
#include <DirectXMath.h>

// ------------------ 命中基础接触信息（由碰撞系统提供） ------------------

// 受击等级：轻 / 中 / 重（以后可以扩展 Launch / Down 等）
enum class HitLevel : uint8_t
{
    Light = 0,
    Medium = 1,
    Heavy = 2,
};

enum class HitSourceType : uint8_t
{
    Melee = 0,
    Projectile = 1,
    AreaSpell = 2,
};


struct HitContact
{
    void* attackerOwner = nullptr;              // 攻击方 ownerToken（玩家 / Boss 等）
    void* victimOwner = nullptr;              // 挨打方 ownerToken
    int   damage = 0;                    // 伤害值（先直接用 hitbox 自带的 damage）

    HitLevel level = HitLevel::Light;
    HitSourceType sourceType = HitSourceType::Melee;
    DirectX::XMFLOAT3 hitPos{ 0.0f,0.0f,0.0f }; // 命中点（世界坐标）

    float knockbackDistance = 0.0f;          // ★ 新增
    DirectX::XMFLOAT3 attackerPos{ 0,0,0 };    // ★ 新增（世界坐标）
    DirectX::XMFLOAT3 victimPos{ 0,0,0 };      // ★ 新增（世界坐标）
};


// 供业务层（Player / Boss 等）消费的受击信息
struct HitParams
{
    // 1) 基础身份信息
    void* attackerOwner = nullptr;
    void* victimOwner = nullptr;

    // 2) 受击类型 + 状态机分支用
    int      damage = 0;
    HitLevel level = HitLevel::Light;
    HitSourceType sourceType = HitSourceType::Melee;

    // 3) 命中位置（世界坐标）—— 用于受击方向、特效等
    DirectX::XMFLOAT3 hitPos{ 0.0f,0.0f,0.0f };

    float knockbackDistance = 0.0f;          // ★ 新增
    DirectX::XMFLOAT3 attackerPos{ 0,0,0 };    // ★ 新增
    DirectX::XMFLOAT3 victimPos{ 0,0,0 };      // ★ 新增
};

// 用来把命中分发到各自的业务层（Player / Boss 等）
bool HitEvent_Dispatch(const HitContact& c);
