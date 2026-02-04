#include "hit_event.h"
#include "player.h"
#include "boss.h"
#include "AnimatorRegistry.h"
#include "BossAnimatorRegistry.h"
#include <cstdio>
#include <Windows.h>

// 简单规则：根据伤害粗略决定受击等级（占位实现）
// 之后你可以改成：每个攻击在帧事件里单独配置 HitLevel。
static HitLevel ChooseHitLevelFromDamage(int dmg)
{
    (void)dmg;
    return HitLevel::Light; // 目前全部当轻受击处理
}

// HitStop：根据受击等级给冻结帧数（60fps 基准）
static int HitStopFramesFromLevel(HitLevel lv)
{
    switch (lv) {
    case HitLevel::Heavy:  return 12;
    case HitLevel::Medium: return 5;
    case HitLevel::Light:
    default: return 6;
    }
}

static void RequestHitStopBoth(int frames)
{
    if (frames <= 0) return;
    AnimatorRegistry_RequestHitStopFrames(frames);
    BossAnimatorRegistry_RequestHitStopFrames(frames);
}

static bool IsSelfHit(void* attackerOwner, void* victimOwner)
{
    // 注意：你用的是“HitboxOwnerToken”和“HurtboxOwnerToken”，所以 self-hit 不能直接 pointer 相等判断
    if (attackerOwner == Player_GetHitboxOwnerToken() && victimOwner == Player_GetHurtboxOwnerToken())
        return true;
    if (attackerOwner == Boss_GetHitboxOwnerToken() && victimOwner == Boss_GetHurtboxOwnerToken())
        return true;
    return false;
}

// 这里先简单写死“谁打谁”，以后可以改成更优雅的表驱动
bool HitEvent_Dispatch(const HitContact& c)
{
    //{
    //    char buf[512];
    //    sprintf_s(buf,
    //        "[HitEvent] attacker=%p victim=%p | PHit=%p PHurt=%p BHit=%p BHurt=%p\n",
    //        c.attackerOwner, c.victimOwner,
    //        Player_GetHitboxOwnerToken(), Player_GetHurtboxOwnerToken(),
    //        Boss_GetHitboxOwnerToken(), Boss_GetHurtboxOwnerToken());
    //    OutputDebugStringA(buf);
    //}

    // 1) 先把“自己打自己”的情况全部过滤掉
    if (!c.attackerOwner || !c.victimOwner) return false;

    // Boss 打到自己、Player 打到自己：直接忽略
    if (IsSelfHit(c.attackerOwner, c.victimOwner))
        return false;

    // 2) Player → Boss 命中（以后做 Boss 受击时可以在这里扩展）
    if (c.attackerOwner == Player_GetHitboxOwnerToken() &&
        c.victimOwner == Boss_GetHurtboxOwnerToken())
    {
        HitParams hp{};
        hp.attackerOwner = c.attackerOwner;
        hp.victimOwner = c.victimOwner;
        hp.damage = c.damage;
        hp.level = ChooseHitLevelFromDamage(c.damage);
        hp.hitPos = c.hitPos;
        hp.level = c.level;     // ★ 来自 SpawnHitBox

        hp.knockbackDistance = c.knockbackDistance;
        hp.attackerPos = c.attackerPos;
        hp.victimPos = c.victimPos;

        //这里才是真正扣血入口
        const bool bossCanTakeDamage = Boss_IsHurtEnabled() && !Boss_IsDead() && (hp.damage > 0);
        Boss_OnIncomingHit(hp);
        if (bossCanTakeDamage) {
            RequestHitStopBoth(HitStopFramesFromLevel(hp.level));
        }

        char buf[256];
        sprintf_s(buf, "[HitEvent] PLAYER hit BOSS! dmg=%d\n", c.damage);
        OutputDebugStringA(buf);

        return true; // 命中成立，消耗 HitBox
    }

    // 3) Boss → Player 命中：这里真正驱动 Player 的受击系统
    if (c.attackerOwner == Boss_GetHitboxOwnerToken() &&
        c.victimOwner == Player_GetHurtboxOwnerToken())
    {
        HitParams hp{};
        hp.attackerOwner = c.attackerOwner;
        hp.victimOwner = c.victimOwner;
        hp.damage = c.damage;
        hp.level = ChooseHitLevelFromDamage(c.damage);
        hp.hitPos = c.hitPos;

        hp.knockbackDistance = c.knockbackDistance;
        hp.attackerPos = c.attackerPos;
        hp.victimPos = c.victimPos;

        PlayerHitResponse r = Player_OnIncomingHit(hp);

        char buf[256];
        switch (r)
        {
        case PlayerHitResponse::Ignored:
            sprintf_s(buf, "[HitEvent] BOSS hit PLAYER -> IGNORED (invincible) dmg=%d\n", c.damage);
            OutputDebugStringA(buf);
            return true; // ★ 仍然消耗 HitBox（按你补充规则）
        case PlayerHitResponse::Parried:
            sprintf_s(buf, "[HitEvent] BOSS hit PLAYER -> PARRIED dmg=%d\n", c.damage);
            OutputDebugStringA(buf);
            return true; // ★ 消耗 HitBox
        case PlayerHitResponse::TookHit:
        default:
            if (hp.damage > 0) {
                RequestHitStopBoth(HitStopFramesFromLevel(hp.level));
            }
            sprintf_s(buf, "[HitEvent] BOSS hit PLAYER! dmg=%d\n", c.damage);
            OutputDebugStringA(buf);
            return true; // ★ 消耗 HitBox
        }
    }

    // 4) 其它组合（预留给将来小怪/友军）
    char buf[256];
    sprintf_s(buf, "[HitEvent] UNKNOWN HIT! attacker=%p victim=%p dmg=%d\n",
        c.attackerOwner, c.victimOwner, c.damage);
    OutputDebugStringA(buf);
    return false;
}
