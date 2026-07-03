#pragma once
#include <DirectXMath.h>

enum class BossAIState
{
    Idle,
    Chase,
    Hit,
};

enum class BossAICommand
{
    None,
    Idle,
    Roar,
    Chase,
    Attack,
    Combo,
    CastSpell,
    CastFireRing,
};

struct BossAIContext
{
    BossAIState state = BossAIState::Idle;

    DirectX::XMFLOAT3 bossPos{ 0,0,0 };
    DirectX::XMFLOAT3 playerPos{ 0,0,0 };

    bool stateFinished = false;

    bool introRoarTriggered = false;
    float introRoarTimer = 0.0f;
    float introRoarDelay = 0.0f;

    double attackCooldown = 0.0;
    double comboCooldown = 0.0;
    double spellCooldown = 0.0;
    double fireRingCooldown = 0.0;

    float chaseRange = 0.0f;
    float attackRange = 0.0f;
    float comboRange = 0.0f;
    float spellMinRange = 0.0f;
    float spellMaxRange = 0.0f;
    float fireRingRange = 0.0f;
};

BossAICommand BossAI_Decide(const BossAIContext& ctx);
