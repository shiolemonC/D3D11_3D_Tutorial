#pragma once
/*==============================================================================
    3D コライダーシステム [collider_system.h]
    - プレイヤー / 敵 / ギミック など共通の当たり判定管理
==============================================================================*/

#include <cstdint>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "collision.h"   // BOXAABB, Circle, Box, Collision_DebugDraw など

//------------------------------------------------------------------------------
// 形状種別
//------------------------------------------------------------------------------
enum class ColliderShapeType : uint8_t
{
    Sphere = 0,
    AABB = 1,
};

// 球
struct ColliderSphere
{
    DirectX::XMFLOAT3 center;
    float             radius;
};

// AABB は既存の BOXAABB をそのまま流用
// struct BOXAABB { XMFLOAT3 min; XMFLOAT3 max; };

// 汎用形状
struct ColliderShape
{
    ColliderShapeType type;

    union
    {
        ColliderSphere sphere;
        BOXAABB        aabb;
    };

    ColliderShape()
        : type(ColliderShapeType::Sphere)
        , sphere{ DirectX::XMFLOAT3(0,0,0), 1.0f }
    {
    }
};

//------------------------------------------------------------------------------
// カテゴリ（何者か）
//------------------------------------------------------------------------------
enum class ColliderCategory : uint8_t
{
    WorldStatic = 0,  // 壁・地形など（ダメージは発生しない）
    CharacterBody = 1,  // プレイヤー・敵の「体」
    Hitbox = 2,  // 攻撃判定
    Hurtbox = 3,  // 被弾判定
};

inline unsigned int CategoryBit(ColliderCategory c) noexcept
{
    return 1ull << (uint32_t)c;
}

//------------------------------------------------------------------------------
// 汎用コライダー
//------------------------------------------------------------------------------
struct ColliderBase
{
    int                 id = -1;      // CollisionWorld 内で一意なID
    bool                active = true;    // 無効にしたい場合は false
    ColliderCategory    category = ColliderCategory::WorldStatic;
    unsigned int        collideMask = 0;     // どのカテゴリと判定を取るか（ビットマスク）
    ColliderShape       shape;               // 形状
    void* userPtr = nullptr; // 所有者（Player*, Enemy* など）

    virtual ~ColliderBase() = default;
};

// 攻撃判定
struct Hitbox : public ColliderBase
{
    int damage = 0;
};

// 被弾判定
struct Hurtbox : public ColliderBase
{
    int hpGroupIndex = 0;  // 部位IDなどに使える
};

//------------------------------------------------------------------------------
// 判定結果ペア
//------------------------------------------------------------------------------
struct CollisionPair
{
    ColliderBase* a = nullptr;
    ColliderBase* b = nullptr;
};

//------------------------------------------------------------------------------
// コリジョンワールド（当たり判定の「中台」）
//------------------------------------------------------------------------------
class CollisionWorld
{
public:
    CollisionWorld() = default;
    ~CollisionWorld() = default;

    // コライダー登録：所有権は CollisionWorld に移る
    int RegisterCollider(std::unique_ptr<ColliderBase> collider);

    // コライダー削除
    void UnregisterCollider(int id);

    // ID からコライダー取得（失敗時 nullptr）
    ColliderBase* GetCollider(int id);
    const ColliderBase* GetCollider(int id) const;

    // 毎フレーム呼び出し：当たり判定を行い、結果ペアを outPairs に積む
    void Step(std::vector<CollisionPair>& outPairs);

    // デバッグ描画（DEBUG ビルドで Game_Draw などから呼ぶ想定）
    void DebugDraw() const;

    // ★ 新增：3D Gizmo 调试（Unity 式 BoxCollider 视图）
    void DebugDraw3D() const;

    // 全コライダーをクリア
    void Clear();

private:
    std::vector<std::unique_ptr<ColliderBase>> m_colliders;
};

// グローバルアクセス用ヘルパー
CollisionWorld& GetCollisionWorld();
