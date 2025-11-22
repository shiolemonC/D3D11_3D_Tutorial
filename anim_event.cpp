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
}
