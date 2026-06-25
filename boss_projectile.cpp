#include "boss_projectile.h"

#include "boss.h"
#include "collider_system.h"
#include "cube.h"
#include "hit_event.h"
#include "player.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <Windows.h>

using namespace DirectX;

struct BossProjectilePattern
{
    float speed = 10.0f;
    float radius = 0.35f;
    float lifeSec = 3.0f;
    int damage = 10;
    HitLevel hitLevel = HitLevel::Light;
    float knockbackDistance = 0.0f;
    bool destroyOnHit = true;
};

struct BossProjectile
{
    BossProjectilePatternId patternId = BossProjectilePatternId::StraightShot;
    XMFLOAT3 position{ 0,0,0 };
    XMFLOAT3 velocity{ 0,0,0 };
    float radius = 0.35f;
    float lifeRemain = 0.0f;
    int damage = 0;
    HitLevel hitLevel = HitLevel::Light;
    float knockbackDistance = 0.0f;
    bool destroyOnHit = true;
    void* ownerToken = nullptr;
};

static std::vector<BossProjectile> s_projectiles;

static const BossProjectilePattern& GetPattern(BossProjectilePatternId id)
{
    static const BossProjectilePattern kStraightShot{
        12.0f,
        0.45f,
        4.0f,
        18,
        HitLevel::Medium,
        1.2f,
        true
    };

    switch (id)
    {
    case BossProjectilePatternId::StraightShot:
    default:
        return kStraightShot;
    }
}

static XMFLOAT3 NormalizeOrFallback(const XMFLOAT3& v, const XMFLOAT3& fallback)
{
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq <= 1e-6f)
    {
        return fallback;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    return { v.x * invLen, v.y * invLen, v.z * invLen };
}

static XMFLOAT3 AABBCenter(const BOXAABB& aabb)
{
    return {
        0.5f * (aabb.min.x + aabb.max.x),
        0.5f * (aabb.min.y + aabb.max.y),
        0.5f * (aabb.min.z + aabb.max.z)
    };
}

static bool SphereOverlapsAABB(const XMFLOAT3& center, float radius, const BOXAABB& aabb)
{
    const float cx = std::max(aabb.min.x, std::min(center.x, aabb.max.x));
    const float cy = std::max(aabb.min.y, std::min(center.y, aabb.max.y));
    const float cz = std::max(aabb.min.z, std::min(center.z, aabb.max.z));

    const float dx = center.x - cx;
    const float dy = center.y - cy;
    const float dz = center.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= radius * radius;
}

static bool ProjectileHitsPlayer(const BossProjectile& p)
{
    const int hurtId = Player_GetHurtColliderId();
    if (hurtId < 0) return false;

    const ColliderBase* hurt = GetCollisionWorld().GetCollider(hurtId);
    if (!hurt || !hurt->active) return false;

    if (hurt->shape.type == ColliderShapeType::AABB)
    {
        return SphereOverlapsAABB(p.position, p.radius, hurt->shape.aabb);
    }

    if (hurt->shape.type == ColliderShapeType::Sphere)
    {
        const XMFLOAT3 c = hurt->shape.sphere.center;
        const float r = hurt->shape.sphere.radius + p.radius;
        const float dx = p.position.x - c.x;
        const float dy = p.position.y - c.y;
        const float dz = p.position.z - c.z;
        return (dx * dx + dy * dy + dz * dz) <= r * r;
    }

    return false;
}

static void DispatchPlayerHit(BossProjectile& p)
{
    const ColliderBase* hurt = GetCollisionWorld().GetCollider(Player_GetHurtColliderId());
    if (!hurt) return;

    XMFLOAT3 victimPos = Player_GetPosition();
    if (hurt->shape.type == ColliderShapeType::AABB)
    {
        victimPos = AABBCenter(hurt->shape.aabb);
    }
    else if (hurt->shape.type == ColliderShapeType::Sphere)
    {
        victimPos = hurt->shape.sphere.center;
    }

    HitContact c{};
    c.attackerOwner = p.ownerToken ? p.ownerToken : Boss_GetHitboxOwnerToken();
    c.victimOwner = Player_GetHurtboxOwnerToken();
    c.damage = p.damage;
    c.level = p.hitLevel;
    c.hitPos = p.position;
    c.knockbackDistance = p.knockbackDistance;
    c.attackerPos = p.position;
    c.victimPos = victimPos;

    const bool consumed = HitEvent_Dispatch(c);
    if (consumed && p.destroyOnHit)
    {
        p.lifeRemain = 0.0f;
    }
}

void BossProjectile_Initialize()
{
    s_projectiles.clear();
}

void BossProjectile_Finalize()
{
    s_projectiles.clear();
}

void BossProjectile_Clear()
{
    s_projectiles.clear();
}

void BossProjectile_Fire(
    BossProjectilePatternId patternId,
    const XMFLOAT3& spawnPos,
    const XMFLOAT3& targetPos,
    void* ownerToken)
{
    const BossProjectilePattern& pattern = GetPattern(patternId);

    XMFLOAT3 dir{
        targetPos.x - spawnPos.x,
        targetPos.y - spawnPos.y,
        targetPos.z - spawnPos.z
    };
    dir = NormalizeOrFallback(dir, Boss_GetForward());

    BossProjectile p{};
    p.patternId = patternId;
    p.position = spawnPos;
    p.velocity = {
        dir.x * pattern.speed,
        dir.y * pattern.speed,
        dir.z * pattern.speed
    };
    p.radius = pattern.radius;
    p.lifeRemain = pattern.lifeSec;
    p.damage = pattern.damage;
    p.hitLevel = pattern.hitLevel;
    p.knockbackDistance = pattern.knockbackDistance;
    p.destroyOnHit = pattern.destroyOnHit;
    p.ownerToken = ownerToken;

    s_projectiles.push_back(p);
    OutputDebugStringA("[BossProjectile] Fire StraightShot\n");
}

void BossProjectile_Update(float dt)
{
    if (dt <= 0.0f) return;

    for (auto& p : s_projectiles)
    {
        if (p.lifeRemain <= 0.0f) continue;

        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        p.lifeRemain -= dt;

        if (p.lifeRemain > 0.0f && ProjectileHitsPlayer(p))
        {
            DispatchPlayerHit(p);
        }
    }

    s_projectiles.erase(
        std::remove_if(s_projectiles.begin(), s_projectiles.end(),
            [](const BossProjectile& p)
            {
                return p.lifeRemain <= 0.0f;
            }),
        s_projectiles.end());
}

void BossProjectile_Draw()
{
    for (const auto& p : s_projectiles)
    {
        const float d = p.radius * 2.0f;
        const XMMATRIX world =
            XMMatrixScaling(d, d, d) *
            XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
        Cube_Draw(world);
    }
}

void BossProjectile_DebugDraw()
{
    for (const auto& p : s_projectiles)
    {
        const float d = p.radius * 2.0f;
        const XMMATRIX world =
            XMMatrixScaling(d, d, d) *
            XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
        Cube_DrawWireframe(world, { 1.0f, 0.35f, 0.05f, 1.0f });
    }
}
