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

    {
        AnimEventTrack atk{};
        atk.clipName = L"Attack";

        {
            AnimEvent e{};
            e.timeNormalized = 0.415f;               
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.0f, 1.0f };
            e.spawnHitBox.halfSize = { 0.5f, 0.5f, 0.5f };
            e.spawnHitBox.durationSec = 3.0f / 60.0f;
            e.spawnHitBox.damage = 10;

            atk.events.push_back(e);
        }


        AnimEventRegistry_Register(atk);
    }

    // 攻击动作事件
    {
        AnimEventTrack atk{};
        atk.clipName = L"Attack_2";

        {
            AnimEvent e{};
            e.timeNormalized = 0.468f;               // 攻击动画的 30% 处出刀
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.0f, 1.0f };
            e.spawnHitBox.halfSize = { 0.5f, 0.5f, 0.5f };
            e.spawnHitBox.durationSec = 3.0f / 60.0f;
            e.spawnHitBox.damage = 10;

            atk.events.push_back(e);
        }



        AnimEventRegistry_Register(atk);
    }

    // ★ Block_Attack：进入瞬间触发镜头演出 + 抖动（同一个 track 注册一次）
    {
        AnimEventTrack t{};
        t.clipName = L"Attack_3";

        // 1) LockOn preset
        {
            AnimEvent e{};
            e.timeNormalized = 0.0f;
            e.type = AnimEventType::CameraLockOnPreset;
            e.cameraLockOnPreset.presetId = 1;       // 1 = Block_Attack
            e.cameraLockOnPreset.blendInSec = 0.2f;
            e.cameraLockOnPreset.holdSec = 1.12f;
            e.cameraLockOnPreset.blendOutSec = 0.5f;
            e.cameraLockOnPreset.priority = 200;
            t.events.push_back(e);
        }

        // 2) Camera shake
        {
            AnimEvent e{};
            e.timeNormalized = 0.5f;
            e.type = AnimEventType::CameraShake;
            e.cameraShake.magnitude = 0.58f;
            e.cameraShake.durationSec = 1.12f;
            e.cameraShake.mode = CameraShakeMode::Both;
            e.cameraShake.priority = 100;
            t.events.push_back(e);
        }

        {
            AnimEvent e{};
            e.timeNormalized = 0.55f;               // 攻击动画的 30% 处出刀
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.0f, 1.0f };
            e.spawnHitBox.halfSize = { 0.8f, 0.8f, 0.8f };
            e.spawnHitBox.durationSec = 4.0f / 60.0f;
            e.spawnHitBox.damage = 15;
            e.spawnHitBox.hitLevel = HitLevel::Heavy;
            t.events.push_back(e);
        }
        AnimEventRegistry_Register(t);
    }


    // ★ 在这里加 Boss 攻击事件
    {
        AnimEventTrack bossAtk{};
        bossAtk.clipName = L"Boss_Attack";   // 必须和 BossAnimatorRegistry 的 clip 名字一致

        AnimEvent e{};
        e.timeNormalized = 0.50f;            // 比如 35% 处出刀（之后可以慢慢调）
        e.type = AnimEventType::SpawnHitBox;
        e.spawnHitBox.localOffset = { 0.0f, 1.2f, 2.0f }; // Boss 身前更远一点
        e.spawnHitBox.halfSize = { 2.8f, 1.8f, 1.8f }; // HitBox 稍大一些
        e.spawnHitBox.durationSec = 4.0f / 60.0f;        
        e.spawnHitBox.damage = 20;                   // 比玩家伤害高
        e.spawnHitBox.knockbackDistance = 2.0f; // 0=无击退，小=0.2，大=1.0

        bossAtk.events.push_back(e);
        AnimEventRegistry_Register(bossAtk);
    }

    // ★ 在这里加 Boss 攻击事件
    {
        AnimEventTrack bossRoar{};
        bossRoar.clipName = L"Boss_Roar";   // 必须和 BossAnimatorRegistry 的 clip 名字一致

        {
            AnimEvent e{};
            e.timeNormalized = 0.33f;               
            e.type = AnimEventType::PostFxRadialBlurStart;

            // 注意：这里我们把 centerWorld 当成 “boss局部偏移” 来用（更好调）
            // x=右, y=上, z=前
            e.postFxRadialBlurStart.centerWorld = DirectX::XMFLOAT3(0.0f, 1.8f, 0.6f);

            e.postFxRadialBlurStart.durationSec = 2.45f;
            e.postFxRadialBlurStart.strength = 1.75f;
            e.postFxRadialBlurStart.radius = 0.40f;
            e.postFxRadialBlurStart.sampleCount = 12;

            //bossRoar.events.push_back(e);
        }

        {
            AnimEvent e{};
            e.timeNormalized = 0.34f;
            e.type = AnimEventType::CameraShake;
            e.cameraShake.magnitude = 0.4f;
            e.cameraShake.durationSec = 2.0f;
            e.cameraShake.mode = CameraShakeMode::Both;
            e.cameraShake.priority = 110;
            bossRoar.events.push_back(e);
        }
        AnimEventRegistry_Register(bossRoar);
    }

    // ★ Boss_Combo：更强攻击
    {
        AnimEventTrack bossCombo{};
        bossCombo.clipName = L"Boss_Combo"; // 必须和 BossAnimatorRegistry 的 clip 名一致
        {

            AnimEvent e{};
            e.timeNormalized = 0.31f;           // 你按动画出手帧微调
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.2f, 2.3f };
            e.spawnHitBox.halfSize = { 1.9f, 1.9f, 0.9f };
            e.spawnHitBox.durationSec = 3.0f / 60.0f;
            e.spawnHitBox.damage = 25;      // ★ 更高伤害
            e.spawnHitBox.knockbackDistance = 0.6f; // ★ 更强击退

            e.spawnHitBox.hitLevel = HitLevel::Light;

            bossCombo.events.push_back(e);
        }

        {

            AnimEvent e{};
            e.timeNormalized = 0.50f;           // 你按动画出手帧微调
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.2f, 2.3f };
            e.spawnHitBox.halfSize = { 1.9f, 1.9f, 0.9f };
            e.spawnHitBox.durationSec = 3.0f / 60.0f;
            e.spawnHitBox.damage = 30;      // ★ 更高伤害
            e.spawnHitBox.knockbackDistance = 2.0f; // ★ 更强击退

            e.spawnHitBox.hitLevel = HitLevel::Light;

            bossCombo.events.push_back(e);
        }

        {

            AnimEvent e{};
            e.timeNormalized = 0.73f;           // 你按动画出手帧微调
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.2f, 2.3f };
            e.spawnHitBox.halfSize = { 1.9f, 1.9f, 0.9f };
            e.spawnHitBox.durationSec = 3.0f / 60.0f;
            e.spawnHitBox.damage = 40;      // ★ 更高伤害
            e.spawnHitBox.knockbackDistance = 1.6f; // ★ 更强击退

            e.spawnHitBox.hitLevel = HitLevel::Light;

            bossCombo.events.push_back(e);
        }


        AnimEventRegistry_Register(bossCombo);
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

    // ★ Hit（受击）：进入受击动画瞬间触发镜头演出（time=0）
//    注意：clipName 必须和 AnimatorRegistry 的 clip 名完全一致
    {
        auto AddHitCamEvent = [&](const wchar_t* clip)
            {
                AnimEventTrack tr{};
                tr.clipName = clip;

                AnimEvent e{};
                e.timeNormalized = 0.0f;
                e.type = AnimEventType::CameraLockOnPreset;
                e.cameraLockOnPreset.presetId = 0;       // 0 = Hit
                e.cameraLockOnPreset.blendInSec = 0.03f;
                e.cameraLockOnPreset.holdSec = 0.06f;
                e.cameraLockOnPreset.blendOutSec = 0.20f;
                e.cameraLockOnPreset.priority = 200;

                tr.events.push_back(e);
                AnimEventRegistry_Register(tr);
            };

        AddHitCamEvent(L"Hit_Light");
        AddHitCamEvent(L"Hit_Medium");
        AddHitCamEvent(L"Hit_Heavy");
    }
    // ★ Block：进入瞬间触发镜头演出 + 抖动（同一个 track 注册一次）
    {
        AnimEventTrack t{};
        t.clipName = L"Block";


        // 2) Camera shake
        {
            AnimEvent e{};
            e.timeNormalized = 0.0f;
            e.type = AnimEventType::CameraShake;
            e.cameraShake.magnitude = 0.12f;
            e.cameraShake.durationSec = 0.12f;
            e.cameraShake.mode = CameraShakeMode::Both;
            e.cameraShake.priority = 120;
            t.events.push_back(e);
        }

        {
            AnimEvent e{};
            e.timeNormalized = 0.0f;
            e.type = AnimEventType::CameraLockOnPreset;
            e.cameraLockOnPreset.presetId = 2;       // 2 = Block
            e.cameraLockOnPreset.blendInSec = 0.2f;
            e.cameraLockOnPreset.holdSec = 0.3f;
            e.cameraLockOnPreset.blendOutSec = 0.5f;
            e.cameraLockOnPreset.priority = 150;
            t.events.push_back(e);
        }

        AnimEventRegistry_Register(t);
    }

    // ★ Block_Attack：进入瞬间触发镜头演出 + 抖动（同一个 track 注册一次）
    {
        AnimEventTrack t{};
        t.clipName = L"Block_Attack";

        // 1) LockOn preset
        {
            AnimEvent e{};
            e.timeNormalized = 0.0f;
            e.type = AnimEventType::CameraLockOnPreset;
            e.cameraLockOnPreset.presetId = 1;       // 1 = Block_Attack
            e.cameraLockOnPreset.blendInSec = 0.4f;
            e.cameraLockOnPreset.holdSec = 0.7f;
            e.cameraLockOnPreset.blendOutSec = 0.3f;
            e.cameraLockOnPreset.priority = 200;
            t.events.push_back(e);
        }

        // 2) Camera shake
        {
            AnimEvent e{};
            e.timeNormalized = 0.5f;
            e.type = AnimEventType::CameraShake;
            e.cameraShake.magnitude = 0.38f;
            e.cameraShake.durationSec = 0.6f;
            e.cameraShake.mode = CameraShakeMode::Both;
            e.cameraShake.priority = 100;
            t.events.push_back(e);
        }

        {
            AnimEvent e{};
            e.timeNormalized = 0.55f;               // 攻击动画的 30% 处出刀
            e.type = AnimEventType::SpawnHitBox;
            e.spawnHitBox.localOffset = { 0.0f, 1.0f, 1.0f };
            e.spawnHitBox.halfSize = { 0.8f, 0.8f, 0.8f };
            e.spawnHitBox.durationSec = 4.0f / 60.0f;
            e.spawnHitBox.damage = 15;
            e.spawnHitBox.hitLevel = HitLevel::Heavy;
            t.events.push_back(e);
        }
        AnimEventRegistry_Register(t);
    }
}
