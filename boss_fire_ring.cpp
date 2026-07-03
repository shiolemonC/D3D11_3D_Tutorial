#include "boss_fire_ring.h"

#include <algorithm>
#include <cmath>
#include <Windows.h>

#include "boss.h"
#include "hit_event.h"
#include "particle_system.h"
#include "player.h"

using namespace DirectX;

namespace
{
    enum class FireRingPhase
    {
        Inactive,
        Telegraph,
        Highlight,
        Explosion,
    };

    struct FireRingState
    {
        FireRingPhase phase = FireRingPhase::Inactive;
        XMFLOAT3 center{ 0, 0, 0 };
        void* ownerToken = nullptr;
        float phaseTime = 0.0f;
        float emitTimer = 0.0f;
        float radius = 0.0f;
        float angleOffset = 0.0f;
        bool damageApplied = false;
    };

    FireRingState g_ring{};

    constexpr float kStartRadius = 0.8f;
    constexpr float kMaxRadius = 6.0f;
    constexpr float kTelegraphDuration = 1.55f;
    constexpr float kHighlightDuration = 0.24f;
    constexpr float kExplosionDuration = 0.38f;
    constexpr float kTelegraphEmitInterval = 0.065f;
    constexpr float kHighlightEmitInterval = 0.040f;
    constexpr float kExplosionEmitInterval = 0.055f;
    constexpr float kParticleSpacing = 0.48f;
    constexpr float kPlayerRadiusMargin = 0.5f;
    constexpr int kExplosionDamage = 35;
    constexpr float kExplosionKnockback = 2.4f;

    XMFLOAT3 NormalizeOrUp(const XMFLOAT3& v)
    {
        const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq <= 1e-6f) return { 0.0f, 1.0f, 0.0f };

        const float invLen = 1.0f / std::sqrt(lenSq);
        return { v.x * invLen, v.y * invLen, v.z * invLen };
    }

    void EmitRing(VfxId id, float radius, float radialDirection, float upwardDirection)
    {
        const float circumference = XM_2PI * std::max(radius, 0.1f);
        const int count = std::clamp(
            static_cast<int>(std::ceil(circumference / kParticleSpacing)),
            16,
            96);

        for (int i = 0; i < count; ++i)
        {
            const float angle = g_ring.angleOffset + XM_2PI * static_cast<float>(i) / count;
            const float cs = std::cos(angle);
            const float sn = std::sin(angle);

            XMFLOAT3 position{
                g_ring.center.x + cs * radius,
                g_ring.center.y,
                g_ring.center.z + sn * radius
            };
            XMFLOAT3 direction = NormalizeOrUp({
                cs * radialDirection,
                upwardDirection,
                sn * radialDirection
            });

            ParticleSystem_Spawn(id, position, direction);
        }

        g_ring.angleOffset += 0.17f;
        if (g_ring.angleOffset >= XM_2PI) g_ring.angleOffset -= XM_2PI;
    }

    void ApplyExplosionDamage()
    {
        if (g_ring.damageApplied) return;
        g_ring.damageApplied = true;

        const XMFLOAT3 playerPos = Player_GetPosition();
        const float dx = playerPos.x - g_ring.center.x;
        const float dz = playerPos.z - g_ring.center.z;
        const float damageRadius = kMaxRadius + kPlayerRadiusMargin;
        if (dx * dx + dz * dz > damageRadius * damageRadius) return;

        HitContact contact{};
        contact.attackerOwner = g_ring.ownerToken
            ? g_ring.ownerToken
            : Boss_GetHitboxOwnerToken();
        contact.victimOwner = Player_GetHurtboxOwnerToken();
        contact.damage = kExplosionDamage;
        contact.level = HitLevel::Heavy;
        contact.sourceType = HitSourceType::AreaSpell;
        contact.hitPos = { playerPos.x, playerPos.y + 1.0f, playerPos.z };
        contact.knockbackDistance = kExplosionKnockback;
        contact.attackerPos = g_ring.center;
        contact.victimPos = playerPos;
        HitEvent_Dispatch(contact);
    }

    void BeginExplosion()
    {
        g_ring.phase = FireRingPhase::Explosion;
        g_ring.phaseTime = 0.0f;
        g_ring.emitTimer = 0.0f;
        g_ring.radius = kMaxRadius;

        ApplyExplosionDamage();

        EmitRing(VfxId::FireRingRedBurst, kMaxRadius * 0.35f, 1.0f, 0.55f);
        EmitRing(VfxId::FireRingRedBurst, kMaxRadius * 0.68f, 1.0f, 0.55f);
        EmitRing(VfxId::FireRingRedBurst, kMaxRadius, 1.0f, 0.55f);
    }
}

bool BossFireRing_Initialize()
{
    g_ring = FireRingState{};
    return true;
}

void BossFireRing_Finalize()
{
    g_ring = FireRingState{};
}

void BossFireRing_Start(const XMFLOAT3& center, void* ownerToken)
{
    g_ring = FireRingState{};
    g_ring.phase = FireRingPhase::Telegraph;
    g_ring.center = center;
    g_ring.center.y += 0.38f;
    g_ring.ownerToken = ownerToken;
    g_ring.radius = kStartRadius;

    EmitRing(VfxId::FireRingGreen, g_ring.radius, 0.30f, 1.0f);
    OutputDebugStringA("[BossFireRing] Start\n");
}

void BossFireRing_Update(float dt)
{
    if (dt <= 0.0f || g_ring.phase == FireRingPhase::Inactive) return;

    g_ring.phaseTime += dt;
    g_ring.emitTimer += dt;

    switch (g_ring.phase)
    {
    case FireRingPhase::Telegraph:
    {
        const float t = std::clamp(g_ring.phaseTime / kTelegraphDuration, 0.0f, 1.0f);
        const float eased = t * t * (3.0f - 2.0f * t);
        g_ring.radius = kStartRadius + (kMaxRadius - kStartRadius) * eased;

        while (g_ring.emitTimer >= kTelegraphEmitInterval)
        {
            g_ring.emitTimer -= kTelegraphEmitInterval;
            EmitRing(VfxId::FireRingGreen, g_ring.radius, 0.30f, 1.0f);
        }

        if (g_ring.phaseTime >= kTelegraphDuration)
        {
            g_ring.phase = FireRingPhase::Highlight;
            g_ring.phaseTime = 0.0f;
            g_ring.emitTimer = 0.0f;
            g_ring.radius = kMaxRadius;
            EmitRing(VfxId::FireRingGreenHighlight, kMaxRadius, 0.25f, 1.0f);
        }
        break;
    }

    case FireRingPhase::Highlight:
        while (g_ring.emitTimer >= kHighlightEmitInterval)
        {
            g_ring.emitTimer -= kHighlightEmitInterval;
            EmitRing(VfxId::FireRingGreenHighlight, kMaxRadius, 0.25f, 1.0f);
        }

        if (g_ring.phaseTime >= kHighlightDuration)
        {
            BeginExplosion();
        }
        break;

    case FireRingPhase::Explosion:
        while (g_ring.emitTimer >= kExplosionEmitInterval)
        {
            g_ring.emitTimer -= kExplosionEmitInterval;
            EmitRing(VfxId::FireRingRedBurst, kMaxRadius, 1.0f, 0.55f);
        }

        if (g_ring.phaseTime >= kExplosionDuration)
        {
            g_ring.phase = FireRingPhase::Inactive;
            OutputDebugStringA("[BossFireRing] Complete\n");
        }
        break;

    case FireRingPhase::Inactive:
    default:
        break;
    }
}

bool BossFireRing_IsActive()
{
    return g_ring.phase != FireRingPhase::Inactive;
}

float BossFireRing_GetCurrentRadius()
{
    return g_ring.radius;
}

