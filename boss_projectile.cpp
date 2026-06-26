#include "boss_projectile.h"

#include "boss.h"
#include "boss_projectile_visual.h"
#include "collider_system.h"
#include "cube.h"
#include "hit_event.h"
#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <Windows.h>

using namespace DirectX;

struct BossProjectileLogicDesc
{
    float speed = 10.0f;
    float collisionRadius = 0.35f;
    float lifeSec = 3.0f;
    int damage = 10;
    HitLevel hitLevel = HitLevel::Light;
    float knockbackDistance = 0.0f;
    bool destroyOnHit = true;
};

struct BossProjectilePattern
{
    BossProjectileLogicDesc logic;
    BossProjectileVisualDesc visual;
};

enum class BossProjectileMoveMode
{
    Straight,
    FireworkScatterThenAim,
};

struct BossProjectile
{
    BossProjectilePatternId patternId = BossProjectilePatternId::StraightShot;
    BossProjectileMoveMode moveMode = BossProjectileMoveMode::Straight;
    XMFLOAT3 position{ 0,0,0 };
    XMFLOAT3 velocity{ 0,0,0 };
    float collisionRadius = 0.35f;
    float lifeRemain = 0.0f;
    float age = 0.0f;
    int damage = 0;
    HitLevel hitLevel = HitLevel::Light;
    float knockbackDistance = 0.0f;
    bool destroyOnHit = true;
    void* ownerToken = nullptr;
    BossProjectileVisualDesc visual;

    float phaseTimer = 0.0f;
    float scatterDuration = 0.0f;
    float homingSpeed = 0.0f;
    bool hasLockedTarget = false;
    XMFLOAT3 lockedTarget{ 0,0,0 };
};

static std::vector<BossProjectile> s_projectiles;

static const BossProjectilePattern& GetPattern(BossProjectilePatternId id)
{
    static const BossProjectilePattern kStraightShot = []()
    {
        BossProjectilePattern pattern{};
        pattern.logic.speed = 12.0f;
        pattern.logic.collisionRadius = 0.45f;
        pattern.logic.lifeSec = 4.0f;
        pattern.logic.damage = 18;
        pattern.logic.hitLevel = HitLevel::Medium;
        pattern.logic.knockbackDistance = 1.2f;
        pattern.logic.destroyOnHit = true;

        pattern.visual.type = BossProjectileVisualType::VelocityBillboard;
        pattern.visual.visualRadius = 0.45f;
        pattern.visual.color = { 0.45f, 0.85f, 1.0f, 1.0f };
        pattern.visual.blend = VfxBlend::Add;
        pattern.visual.texturePath = L"resources/fx/particle_spark_1.png";
        pattern.visual.baseWidth = 0.55f;
        pattern.visual.baseLength = 0.95f;
        pattern.visual.streakMul = 0.10f;
        pattern.visual.streakMax = 2.8f;
        pattern.visual.rotationBias = 0.0f;

        return pattern;
    }();

    static const BossProjectilePattern kFireworkHomingBarrage = []()
    {
        BossProjectilePattern pattern{};
        pattern.logic.speed = 5.5f;
        pattern.logic.collisionRadius = 0.38f;
        pattern.logic.lifeSec = 5.0f;
        pattern.logic.damage = 12;
        pattern.logic.hitLevel = HitLevel::Medium;
        pattern.logic.knockbackDistance = 0.8f;
        pattern.logic.destroyOnHit = true;

        pattern.visual.type = BossProjectileVisualType::VelocityBillboard;
        pattern.visual.visualRadius = 0.38f;
        pattern.visual.color = { 0.50f, 0.90f, 1.0f, 1.0f };
        pattern.visual.blend = VfxBlend::Add;
        pattern.visual.texturePath = L"resources/fx/particle_spark_1.png";
        pattern.visual.baseWidth = 0.42f;
        pattern.visual.baseLength = 0.78f;
        pattern.visual.streakMul = 0.10f;
        pattern.visual.streakMax = 2.7f;
        pattern.visual.rotationBias = 0.0f;

        return pattern;
    }();

    switch (id)
    {
    case BossProjectilePatternId::FireworkHomingBarrage:
        return kFireworkHomingBarrage;
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

static float RandomRange(float minValue, float maxValue)
{
    const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minValue + (maxValue - minValue) * t;
}

static XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

static XMFLOAT3 Mul(const XMFLOAT3& v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

static XMFLOAT3 NormalizeXZOrFallback(const XMFLOAT3& v, const XMFLOAT3& fallback)
{
    XMFLOAT3 xz{ v.x, 0.0f, v.z };
    return NormalizeOrFallback(xz, fallback);
}

static bool ProjectileHitsPlayer(const BossProjectile& p)
{
    const int hurtId = Player_GetHurtColliderId();
    if (hurtId < 0) return false;

    const ColliderBase* hurt = GetCollisionWorld().GetCollider(hurtId);
    if (!hurt || !hurt->active) return false;

    if (hurt->shape.type == ColliderShapeType::AABB)
    {
        return SphereOverlapsAABB(p.position, p.collisionRadius, hurt->shape.aabb);
    }

    if (hurt->shape.type == ColliderShapeType::Sphere)
    {
        const XMFLOAT3 c = hurt->shape.sphere.center;
        const float r = hurt->shape.sphere.radius + p.collisionRadius;
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

static void SetupProjectileFromPattern(
    BossProjectile& p,
    BossProjectilePatternId patternId,
    const BossProjectilePattern& pattern,
    const XMFLOAT3& spawnPos,
    void* ownerToken)
{
    p.patternId = patternId;
    p.position = spawnPos;
    p.collisionRadius = pattern.logic.collisionRadius;
    p.lifeRemain = pattern.logic.lifeSec;
    p.age = 0.0f;
    p.damage = pattern.logic.damage;
    p.hitLevel = pattern.logic.hitLevel;
    p.knockbackDistance = pattern.logic.knockbackDistance;
    p.destroyOnHit = pattern.logic.destroyOnHit;
    p.ownerToken = ownerToken;
    p.visual = pattern.visual;
}

static void PushStraightShot(
    BossProjectilePatternId patternId,
    const BossProjectilePattern& pattern,
    const XMFLOAT3& spawnPos,
    const XMFLOAT3& targetPos,
    void* ownerToken)
{
    XMFLOAT3 dir{
        targetPos.x - spawnPos.x,
        targetPos.y - spawnPos.y,
        targetPos.z - spawnPos.z
    };
    dir = NormalizeOrFallback(dir, Boss_GetForward());

    BossProjectile p{};
    SetupProjectileFromPattern(p, patternId, pattern, spawnPos, ownerToken);
    p.moveMode = BossProjectileMoveMode::Straight;
    p.velocity = Mul(dir, pattern.logic.speed);

    s_projectiles.push_back(p);
}

static void PushFireworkHomingBarrage(
    BossProjectilePatternId patternId,
    const BossProjectilePattern& pattern,
    const XMFLOAT3& spawnPos,
    const XMFLOAT3& targetPos,
    void* ownerToken)
{
    constexpr int kCount = 7;
    constexpr float kScatterDuration = 0.75f;
    constexpr float kHomingSpeed = 13.0f;
    constexpr float kSpreadDeg = 120.0f;
    constexpr float kScatterUpSpeed = 7.5f;

    const XMFLOAT3 baseForward = NormalizeXZOrFallback(
        { targetPos.x - spawnPos.x, 0.0f, targetPos.z - spawnPos.z },
        Boss_GetForward());
    const XMFLOAT3 up{ 0.0f, 1.0f, 0.0f };
    const XMFLOAT3 right = NormalizeOrFallback(Cross(up, baseForward), { 1.0f, 0.0f, 0.0f });

    for (int i = 0; i < kCount; ++i)
    {
        const float center = (static_cast<float>(kCount) - 1.0f) * 0.5f;
        const float normalizedIndex = (static_cast<float>(i) - center) / center;
        const float sideAngle = XMConvertToRadians((kSpreadDeg * 0.5f) * normalizedIndex);
        const float c = std::cos(sideAngle);
        const float s = std::sin(sideAngle);

        XMFLOAT3 scatterDir = Add(Mul(baseForward, c), Mul(right, s));
        scatterDir.y = RandomRange(0.75f, 1.10f);
        scatterDir = NormalizeOrFallback(scatterDir, up);

        BossProjectile p{};
        SetupProjectileFromPattern(p, patternId, pattern, spawnPos, ownerToken);
        p.moveMode = BossProjectileMoveMode::FireworkScatterThenAim;
        p.velocity = {
            scatterDir.x * pattern.logic.speed,
            scatterDir.y * kScatterUpSpeed,
            scatterDir.z * pattern.logic.speed
        };
        p.scatterDuration = kScatterDuration + RandomRange(-0.08f, 0.08f);
        p.homingSpeed = kHomingSpeed;

        s_projectiles.push_back(p);
    }
}

static void LockFireworkTarget(BossProjectile& p)
{
    XMFLOAT3 target = Player_GetPosition();
    target.y += 1.0f;
    p.lockedTarget = target;

    XMFLOAT3 dir{
        target.x - p.position.x,
        target.y - p.position.y,
        target.z - p.position.z
    };
    dir = NormalizeOrFallback(dir, { 0.0f, -0.2f, 1.0f });

    p.velocity = Mul(dir, p.homingSpeed);
    p.hasLockedTarget = true;
}

static void UpdateProjectileMovement(BossProjectile& p, float dt)
{
    p.age += dt;

    if (p.moveMode == BossProjectileMoveMode::FireworkScatterThenAim)
    {
        p.phaseTimer += dt;

        if (!p.hasLockedTarget && p.phaseTimer >= p.scatterDuration)
        {
            LockFireworkTarget(p);
        }
    }

    p.position.x += p.velocity.x * dt;
    p.position.y += p.velocity.y * dt;
    p.position.z += p.velocity.z * dt;
}

void BossProjectile_Initialize()
{
    s_projectiles.clear();
    BossProjectileVisual_Initialize();
}

void BossProjectile_Finalize()
{
    s_projectiles.clear();
    BossProjectileVisual_Finalize();
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

    switch (patternId)
    {
    case BossProjectilePatternId::FireworkHomingBarrage:
        PushFireworkHomingBarrage(patternId, pattern, spawnPos, targetPos, ownerToken);
        OutputDebugStringA("[BossProjectile] Fire FireworkHomingBarrage\n");
        break;
    case BossProjectilePatternId::StraightShot:
    default:
        PushStraightShot(patternId, pattern, spawnPos, targetPos, ownerToken);
        OutputDebugStringA("[BossProjectile] Fire StraightShot\n");
        break;
    }
}

void BossProjectile_Update(float dt)
{
    if (dt <= 0.0f) return;

    for (auto& p : s_projectiles)
    {
        if (p.lifeRemain <= 0.0f) continue;

        UpdateProjectileMovement(p, dt);
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
        BossProjectileVisual_Draw(p.position, p.velocity, p.visual);
    }
}

void BossProjectile_DebugDraw()
{
    for (const auto& p : s_projectiles)
    {
        const float d = p.collisionRadius * 2.0f;
        const XMMATRIX world =
            XMMatrixScaling(d, d, d) *
            XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
        Cube_DrawWireframe(world, { 1.0f, 0.35f, 0.05f, 1.0f });
    }
}
