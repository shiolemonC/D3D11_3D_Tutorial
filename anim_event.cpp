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

        // ① 开始就关掉 HurtBox（比如：起手无敌）
        //{
        //    AnimEvent e{};
        //    e.timeNormalized = 0.0f;
        //    e.type = AnimEventType::SetHurtBoxEnabled;
        //    e.setHurtBox.enabled = false;    // 关闭受击框 → 无敌
        //    atk.events.push_back(e);
        //}

        // ② 0.30 出刀：生成 HitBox
        {
            AnimEvent e{};
            e.timeNormalized = 0.30f;               // 攻击动画的 30% 处出刀
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.0f, 1.0f };
            e.spawnHitBox.halfSize = { 0.5f, 0.5f, 0.5f };
            e.spawnHitBox.durationSec = 3.0f / 60.0f;
            e.spawnHitBox.damage = 10;

            atk.events.push_back(e);
        }


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
        e.spawnHitBox.knockbackDistance = 5.0f; // 0=无击退，小=0.2，大=1.0

        bossAtk.events.push_back(e);
        AnimEventRegistry_Register(bossAtk);
    }

    // 翻滚无敌事件
    {
        AnimEventTrack roll{};
        roll.clipName = L"Roll";

        // 进入翻滚立刻无敌
        {
            AnimEvent e{};
            e.timeNormalized = 0.0f;
            e.type = AnimEventType::SetHurtBoxEnabled;
            e.setHurtBox.enabled = false;
            roll.events.push_back(e);
        }

        // 无敌结束（建议别写 1.0，写 0.85/0.9 更稳）
        {
            AnimEvent e{};
            e.timeNormalized = 0.85f;
            e.type = AnimEventType::SetHurtBoxEnabled;
            e.setHurtBox.enabled = true;
            roll.events.push_back(e);
        }

        AnimEventRegistry_Register(roll);
    }

    // ★ Parry：成功格挡窗口事件
    {
        AnimEventTrack parry{};
        parry.clipName = L"Parry";

        // ① 窗口开启：比如 0.18 ~ 0.35（你之后可以根据动画帧微调）
        {
            AnimEvent e{};
            e.timeNormalized = 0.05f;
            e.type = AnimEventType::SetParryWindowEnabled;
            e.setParryWindow.enabled = true;
            parry.events.push_back(e);
        }

        // ② 窗口关闭
        {
            AnimEvent e{};
            e.timeNormalized = 0.85f;
            e.type = AnimEventType::SetParryWindowEnabled;
            e.setParryWindow.enabled = false;
            parry.events.push_back(e);
        }

        AnimEventRegistry_Register(parry);
    }
}
