#include "collider_system.h"
#include <algorithm>
#include <cmath>
#include "direct3d.h"
#include "cube.h"

using namespace DirectX;

//------------------------------------------------------------------------------
// 内部ヘルパー
//------------------------------------------------------------------------------
namespace
{
    // ビットマスク用：カテゴリ → 1 << index
    inline unsigned int CategoryBit(ColliderCategory cat)
    {
        return 1u << static_cast<unsigned int>(cat);
    }

    // A と B が判定対象かどうか
    bool ShouldCheckPair(const ColliderBase& a, const ColliderBase& b)
    {
        unsigned int bitA = CategoryBit(a.category);
        unsigned int bitB = CategoryBit(b.category);
        return (a.collideMask & bitB) && (b.collideMask & bitA);
    }

    // Sphere-Sphere の重なり
    bool Overlap_Sphere_Sphere(const ColliderSphere& a, const ColliderSphere& b)
    {
        XMVECTOR pa = XMLoadFloat3(&a.center);
        XMVECTOR pb = XMLoadFloat3(&b.center);
        XMVECTOR d = XMVectorSubtract(pa, pb);
        float distSq = XMVectorGetX(XMVector3LengthSq(d));
        float r = a.radius + b.radius;
        return distSq <= (r * r);
    }

    // Sphere-AABB の重なり
    bool Overlap_Sphere_AABB(const ColliderSphere& s, const BOXAABB& box)
    {
        // box の最近傍点を求めて、その点と球中心の距離を見る
        float cx = std::max(box.min.x, std::min(s.center.x, box.max.x));
        float cy = std::max(box.min.y, std::min(s.center.y, box.max.y));
        float cz = std::max(box.min.z, std::min(s.center.z, box.max.z));

        float dx = s.center.x - cx;
        float dy = s.center.y - cy;
        float dz = s.center.z - cz;

        float distSq = dx * dx + dy * dy + dz * dz;
        return distSq <= (s.radius * s.radius);
    }

    // 汎用 Overlap
    bool Overlap_Generic(const ColliderBase& a, const ColliderBase& b)
    {
        const ColliderShape& sa = a.shape;
        const ColliderShape& sb = b.shape;

        if (sa.type == ColliderShapeType::Sphere && sb.type == ColliderShapeType::Sphere)
        {
            return Overlap_Sphere_Sphere(sa.sphere, sb.sphere);
        }
        else if (sa.type == ColliderShapeType::AABB && sb.type == ColliderShapeType::AABB)
        {
            // 3D AABB 同士は既存の関数を流用
            return Collision_OverlapAABB(sa.aabb, sb.aabb);
        }
        else if (sa.type == ColliderShapeType::Sphere && sb.type == ColliderShapeType::AABB)
        {
            return Overlap_Sphere_AABB(sa.sphere, sb.aabb);
        }
        else if (sa.type == ColliderShapeType::AABB && sb.type == ColliderShapeType::Sphere)
        {
            return Overlap_Sphere_AABB(sb.sphere, sa.aabb);
        }

        // 未対応の組み合わせ
        return false;
    }

    // Debug 用カラー
    XMFLOAT4 DebugColorForCategory(ColliderCategory cat)
    {
        switch (cat)
        {
        case ColliderCategory::WorldStatic:   return XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
        case ColliderCategory::CharacterBody: return XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f); // シアン
        case ColliderCategory::Hitbox:        return XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f); // 赤
        case ColliderCategory::Hurtbox:       return XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f); // 緑
        default:                              return XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }
}

//------------------------------------------------------------------------------
// CollisionWorld 実装
//------------------------------------------------------------------------------

int CollisionWorld::RegisterCollider(std::unique_ptr<ColliderBase> collider)
{
    if (!collider) return -1;

    // 既存の空きを探す（あれば再利用）
    for (size_t i = 0; i < m_colliders.size(); ++i)
    {
        if (!m_colliders[i])
        {
            collider->id = static_cast<int>(i);
            m_colliders[i] = std::move(collider);
            return static_cast<int>(i);
        }
    }

    // なければ末尾に追加
    int newId = static_cast<int>(m_colliders.size());
    collider->id = newId;
    m_colliders.push_back(std::move(collider));
    return newId;
}

void CollisionWorld::UnregisterCollider(int id)
{
    if (id < 0) return;
    size_t idx = static_cast<size_t>(id);
    if (idx >= m_colliders.size()) return;
    m_colliders[idx].reset(); // unique_ptr を解放
}

ColliderBase* CollisionWorld::GetCollider(int id)
{
    if (id < 0) return nullptr;
    size_t idx = static_cast<size_t>(id);
    if (idx >= m_colliders.size()) return nullptr;
    return m_colliders[idx].get();
}

const ColliderBase* CollisionWorld::GetCollider(int id) const
{
    if (id < 0) return nullptr;
    size_t idx = static_cast<size_t>(id);
    if (idx >= m_colliders.size()) return nullptr;
    return m_colliders[idx].get();
}

void CollisionWorld::Step(std::vector<CollisionPair>& outPairs)
{
    outPairs.clear();

    const size_t N = m_colliders.size();
    for (size_t i = 0; i < N; ++i)
    {
        ColliderBase* A = m_colliders[i].get();
        if (!A || !A->active) continue;

        for (size_t j = i + 1; j < N; ++j)
        {
            ColliderBase* B = m_colliders[j].get();
            if (!B || !B->active) continue;

            if (!ShouldCheckPair(*A, *B)) continue;

            if (Overlap_Generic(*A, *B))
            {
                outPairs.push_back({ A, B });
            }
        }
    }
}

void CollisionWorld::DebugDraw() const
{
    const size_t N = m_colliders.size();
    for (size_t i = 0; i < N; ++i)
    {
        const ColliderBase* col = m_colliders[i].get();
        if (!col || !col->active) continue;

        XMFLOAT4 color = DebugColorForCategory(col->category);
        const ColliderShape& s = col->shape;

        // ★ 世界 → デバッグ2D座標のスケール
        constexpr float DBG_SCALE = 30.0f; // 1m -> 30px 程度のイメージ

        if (s.type == ColliderShapeType::Sphere)
        {
            float cx = s.sphere.center.x;
            float cz = s.sphere.center.z;

            Circle c{};
            c.center.x = cx * DBG_SCALE;
            c.center.y = cz * DBG_SCALE;
            c.radius = s.sphere.radius * DBG_SCALE;

            Collision_DebugDraw(c, color);
        }
        else if (s.type == ColliderShapeType::AABB)
        {
            float cx = 0.5f * (s.aabb.min.x + s.aabb.max.x);
            float cz = 0.5f * (s.aabb.min.z + s.aabb.max.z);
            float hw = 0.5f * (s.aabb.max.x - s.aabb.min.x);
            float hh = 0.5f * (s.aabb.max.z - s.aabb.min.z);

            Box b{};
            float screenCX = Direct3D_GetBackBufferWidth() * 0.5f; // 或者 Direct3D_GetBackBufferWidth() * 0.5f;
            float screenCY = Direct3D_GetBackBufferHeight() * 0.5f;
            b.center.x = cx * DBG_SCALE + screenCX;
            b.center.y = cz * DBG_SCALE + screenCY;
            b.half_width = hw * DBG_SCALE;
            b.half_height = hh * DBG_SCALE;

            Collision_DebugDraw(b, color);
        }
    }
}

void CollisionWorld::DebugDraw3D() const
{
    const size_t N = m_colliders.size();
    for (size_t i = 0; i < N; ++i)
    {
        const ColliderBase* col = m_colliders[i].get();
        if (!col || !col->active) continue;

        const ColliderShape& s = col->shape;
        if (s.type != ColliderShapeType::AABB) {
            // 现在先只画 AABB，Sphere 以后可以改成用缩放过的球体 mesh
            continue;
        }

        const BOXAABB& box = s.aabb;

        // 1) AABB -> center + size （世界空间）
        XMFLOAT3 center{
            0.5f * (box.min.x + box.max.x),
            0.5f * (box.min.y + box.max.y),
            0.5f * (box.min.z + box.max.z),
        };
        XMFLOAT3 size{
            box.max.x - box.min.x,
            box.max.y - box.min.y,
            box.max.z - box.min.z,
        };

        // 2) 用缩放 + 平移构建世界矩阵：1x1x1 cube -> AABB 大小
        XMMATRIX S = XMMatrixScaling(size.x, size.y, size.z);
        XMMATRIX T = XMMatrixTranslation(center.x, center.y, center.z);
        XMMATRIX W = S * T;

        // 3) 根据 Category 决定颜色
        XMFLOAT4 color = DebugColorForCategory(col->category);

        // 4) 画 wireframe cube
        Cube_DrawWireframe(W, color);
    }
}

void CollisionWorld::Clear()
{
    m_colliders.clear();
}

// グローバルインスタンス
static CollisionWorld g_CollisionWorld;

CollisionWorld& GetCollisionWorld()
{
    return g_CollisionWorld;
}
