#include "hit_event.h"
#include "player.h"
#include "boss.h"
#include <cstdio>

// 这里先简单写死“谁打谁”，后面你可以改成更优雅的表驱动
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
        // Boss 自己打到自己，直接忽略
        return false;
    }

    // 2) 玩家 HitBox 打到 Boss HurtBox
    if (c.attackerOwner == Player_GetHitboxOwnerToken()
        && c.victimOwner == Boss_GetHurtboxOwnerToken())
    {
        char buf[256];
        sprintf_s(buf, "[HitEvent] PLAYER hit BOSS!\n");
        OutputDebugStringA(buf);
        return true;
    }

    // 3) Boss HitBox 打到 玩家 HurtBox
    if (c.attackerOwner == Boss_GetHitboxOwnerToken()
        && c.victimOwner == Player_GetHurtboxOwnerToken())
    {
        char buf[256];
        sprintf_s(buf, "[HitEvent] BOSS hit PLAYER!\n");
        OutputDebugStringA(buf);
        return true;
    }

    // 4) 其它组合（预留给将来小怪/友军）
    char buf[256];
    sprintf_s(buf, "[HitEvent] UNKNOWN HIT!\n");
    OutputDebugStringA(buf);
    return false;
}
