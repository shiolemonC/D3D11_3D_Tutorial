#include "boss_ai.h"

static float LengthSqXZ(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    return dx * dx + dz * dz;
}

BossAICommand BossAI_Decide(const BossAIContext& ctx)
{
    const float distSq = LengthSqXZ(ctx.bossPos, ctx.playerPos);
    const float chaseRangeSq = ctx.chaseRange * ctx.chaseRange;
    const float attackRangeSq = ctx.attackRange * ctx.attackRange;
    const float comboRangeSq = ctx.comboRange * ctx.comboRange;
    const float spellMinRangeSq = ctx.spellMinRange * ctx.spellMinRange;
    const float spellMaxRangeSq = ctx.spellMaxRange * ctx.spellMaxRange;
    const float fireRingRangeSq = ctx.fireRingRange * ctx.fireRingRange;
    const bool canCastSpell =
        ctx.spellCooldown <= 0.0 &&
        distSq >= spellMinRangeSq &&
        distSq <= spellMaxRangeSq;
    const bool canCastFireRing =
        ctx.fireRingCooldown <= 0.0 &&
        distSq <= fireRingRangeSq;

    switch (ctx.state)
    {
    case BossAIState::Idle:
        if (!ctx.introRoarTriggered)
        {
            if (ctx.introRoarTimer >= ctx.introRoarDelay)
            {
                return BossAICommand::Roar;
            }
            return BossAICommand::None;
        }

        if (canCastFireRing)
        {
            return BossAICommand::CastFireRing;
        }
        if (distSq <= comboRangeSq && ctx.comboCooldown <= 0.0)
        {
            return BossAICommand::Combo;
        }
        if (distSq <= attackRangeSq && ctx.attackCooldown <= 0.0)
        {
            return BossAICommand::Attack;
        }
        if (canCastSpell)
        {
            return BossAICommand::CastSpell;
        }
        if (distSq > attackRangeSq && distSq <= chaseRangeSq)
        {
            return BossAICommand::Chase;
        }
        return BossAICommand::None;

    case BossAIState::Chase:
        if (canCastFireRing)
        {
            return BossAICommand::CastFireRing;
        }
        if (distSq <= comboRangeSq && ctx.comboCooldown <= 0.0)
        {
            return BossAICommand::Combo;
        }
        if (distSq <= attackRangeSq && ctx.attackCooldown <= 0.0)
        {
            return BossAICommand::Attack;
        }
        if (canCastSpell)
        {
            return BossAICommand::CastSpell;
        }
        if (distSq > chaseRangeSq)
        {
            return BossAICommand::Idle;
        }
        return BossAICommand::None;

    case BossAIState::Hit:
        if (!ctx.stateFinished)
        {
            return BossAICommand::None;
        }
        if (distSq > chaseRangeSq)
        {
            return BossAICommand::Idle;
        }
        if (distSq > attackRangeSq)
        {
            return BossAICommand::Chase;
        }
        if (canCastFireRing)
        {
            return BossAICommand::CastFireRing;
        }
        if (ctx.attackCooldown <= 0.0)
        {
            return BossAICommand::Attack;
        }
        if (canCastSpell)
        {
            return BossAICommand::CastSpell;
        }
        return BossAICommand::Idle;

    default:
        break;
    }

    return BossAICommand::None;
}
