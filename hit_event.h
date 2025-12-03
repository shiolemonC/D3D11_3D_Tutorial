#pragma once
#include <cstdint>

// 一次命中的信息（目前很简陋，后面可以慢慢扩展）
struct HitContact
{
    void* attackerOwner = nullptr;  // 攻击方 ownerToken（玩家 / Boss 等）
    void* victimOwner = nullptr;  // 挨打方 ownerToken
    int   damage = 0;        // 伤害值（先直接用 hitbox 自带的 damage）
};

// 用来把命中分发到各自的业务层
bool HitEvent_Dispatch(const HitContact& c);
