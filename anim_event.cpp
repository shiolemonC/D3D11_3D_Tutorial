#include "anim_event.h"
#include <algorithm>

using namespace DirectX;

// 静态注册表
static std::vector<AnimEventTrack> g_tracks;

void AnimEventRegistry_Clear()
{
    g_tracks.clear();
}

void AnimEventRegistry_Register(const AnimEventTrack& trackIn)
{
    // 先 copy 一份，保证 events 按时间排序
    AnimEventTrack t = trackIn;
    std::sort(t.events.begin(), t.events.end(),
        [](const AnimEvent& a, const AnimEvent& b) {
            return a.timeNormalized < b.timeNormalized;
        });

    // 如果已存在同名 clip，覆盖
    for (auto& existing : g_tracks) {
        if (existing.clipName == t.clipName) {
            existing = std::move(t);
            return;
        }
    }

    g_tracks.push_back(std::move(t));
}

const AnimEventTrack* AnimEventRegistry_Find(const std::wstring& clipName)
{
    for (auto& t : g_tracks) {
        if (t.clipName == clipName) {
            return &t;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
// 实际配置：针对 "Attack" 动画加一个 SpawnHitBox 事件
//------------------------------------------------------------------------------
void AnimEventRegister()
{
    AnimEventRegistry_Clear();

    // 攻击动作事件
    {
        AnimEventTrack atk{};
        atk.clipName = L"Attack";

        AnimEvent e{};
        e.timeNormalized = 0.30f;               // 攻击动画的 30% 处出刀（你可之后调）
        e.type = AnimEventType::SpawnHitBox;
        e.spawnHitBox.localOffset = { 0.0f, 1.0f, 1.0f };  // 身体前方 1m，高度 1m
        e.spawnHitBox.halfSize = { 0.5f, 0.5f, 0.5f };  // 1x1x1 的方块
        e.spawnHitBox.durationSec = 3.0f / 60.0f;          // 假设 60fps => 持续 3 帧
        e.spawnHitBox.damage = 10;

        atk.events.push_back(e);
        AnimEventRegistry_Register(atk);
    }

    // ★ 在这里加 Boss 攻击事件
    {
        AnimEventTrack bossAtk{};
        bossAtk.clipName = L"Boss_Attack";   // 必须和 BossAnimatorRegistry 的 clip 名字一致

        AnimEvent e{};
        e.timeNormalized = 0.35f;            // 比如 35% 处出刀（之后可以慢慢调）
        e.type = AnimEventType::SpawnHitBox;
        e.spawnHitBox.localOffset = { 0.0f, 1.2f, 2.0f }; // Boss 身前更远一点
        e.spawnHitBox.halfSize = { 0.8f, 0.8f, 0.8f }; // HitBox 稍大一些
        e.spawnHitBox.durationSec = 60.0f / 60.0f;        
        e.spawnHitBox.damage = 20;                   // 比玩家伤害高

        bossAtk.events.push_back(e);
        AnimEventRegistry_Register(bossAtk);
    }
}
