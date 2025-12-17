#include "hit_event.h"
#include "player.h"
#include "boss.h"
#include <cstdio>
#include <Windows.h>

// 简单规则：根据伤害粗略决定受击等级（占位实现）
// 之后你可以改成：每个攻击在帧事件里单独配置 HitLevel。
static HitLevel ChooseHitLevelFromDamage(int dmg)
{
    (void)dmg;
    return HitLevel::Light; // 目前全部当轻受击处理
}

// 这里先简单写死“谁打谁”，以后可以改成更优雅的表驱动
bool HitEvent_Dispatch(const HitContact& c)
{
    // 1) 先把“自己打自己”的情况全部过滤掉
    if (c.attackerOwner == Player_GetHitboxOwnerToken() &&
        c.victimOwner == Player_GetHurtboxOwnerToken())
    {
        // Player 自己的 HitBox 碰到自己的 HurtBox，直接忽略
        return false;
    }

    if (c.attackerOwner == Boss_GetHitboxOwnerToken() &&
        c.victimOwner == Boss_GetHurtboxOwnerToken())
    {
        // Boss 自己打自己，同样忽略
        return false;
    }

    // 2) Player → Boss 命中（以后做 Boss 受击时可以在这里扩展）
    if (c.attackerOwner == Player_GetHitboxOwnerToken() &&
        c.victimOwner == Boss_GetHurtboxOwnerToken())
    {
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

        // 把受击请求交给 Player 模块，由 Player 缓存并在 Update 中写入 FSM 条件
        Player_RequestHitReaction(hp);

        char buf[256];
        sprintf_s(buf, "[HitEvent] BOSS hit PLAYER! dmg=%d\n", c.damage);
        OutputDebugStringA(buf);
        return true; // 命中成立，消耗 HitBox
    }

    // 4) 其它组合（预留给将来小怪/友军）
    char buf[256];
    sprintf_s(buf, "[HitEvent] UNKNOWN HIT! attacker=%p victim=%p dmg=%d\n",
        c.attackerOwner, c.victimOwner, c.damage);
    OutputDebugStringA(buf);
    return false;
}
