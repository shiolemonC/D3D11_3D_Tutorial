#include "anim_event_player.h"
#include "hitbox_system.h"
// ★ 新增：
#include "player.h"
#include "boss.h"

static void ApplyHurtBoxEnabledToOwner(void* owner, bool enabled)
{
    // 你之前给 Player / Boss 做过“owner token”，这里复用那一套
    if (owner == Player_GetHitboxOwnerToken())
    {
        Player_SetHurtEnabled(enabled);
    }
    else if (owner == Boss_GetHitboxOwnerToken())
    {
        Boss_SetHurtEnabled(enabled);
    }
    else
    {
        // 将来如果有别的角色（小怪），可以在这里继续加
    }
}

static void ApplyParryWindowEnabledToOwner(void* owner, bool enabled)
{
    if (owner == Player_GetHitboxOwnerToken())
    {
        Player_SetParryWindowEnabled(enabled);
    }
    // Boss 如果以后也要做格挡，可在这里扩展
}

static void FireAnimEvent(const AnimEvent& ev, void* owner)
{
    switch (ev.type)
    {
    case AnimEventType::SpawnHitBox:
        HitboxSystem_Spawn(ev.spawnHitBox, owner);
        break;

    case AnimEventType::SetHurtBoxEnabled:
        ApplyHurtBoxEnabledToOwner(owner, ev.setHurtBox.enabled);
        break;

    case AnimEventType::SetParryWindowEnabled:
        ApplyParryWindowEnabledToOwner(owner, ev.setParryWindow.enabled);
        break;

    default:
        break;
    }
}

//----------------- OnClipChanged：在这里触发 time≈0 的事件 -----------------

void AnimEventPlayer::OnClipChanged(const std::wstring& newClipName)
{
    // 兜底
    if (m_currentClip == L"Roll") {
        ApplyHurtBoxEnabledToOwner(m_owner, true);
    }

    // ★ 兜底：任何换 clip 的瞬间，先把成功格挡窗口关掉
    if (m_owner == Player_GetHitboxOwnerToken())
    {
        Player_SetParryWindowEnabled(false);
    }


    m_currentClip = newClipName;
    m_track = AnimEventRegistry_Find(newClipName);
    m_prevTimeNorm = 0.0f;

    if (!m_track) {
        return;
    }

    // ★ 方案 A：time=0 的事件在换 clip 的瞬间直接触发
    constexpr float EPS = 1e-4f;

    // 假设 events 已按 timeNormalized 升序排列
    for (const AnimEvent& ev : m_track->events)
    {
        if (ev.timeNormalized <= EPS)
        {
            FireAnimEvent(ev, m_owner);
        }
        else
        {
            // 后面的时间都 >0，不需要在这里处理，交给 Update()
            break;
        }
    }
}

void AnimEventPlayer::Update(float currentTimeNorm)
{
    if (!m_track) {
        m_prevTimeNorm = currentTimeNorm;
        return;
    }

    float prev = m_prevTimeNorm;
    float curr = currentTimeNorm;

    // （可选）如果以后有循环动画，可以在这里处理 curr < prev 的情况：
    // if (curr < prev) {
    //     prev = 0.0f;
    // }

    for (const AnimEvent& ev : m_track->events)
    {
        if (prev < ev.timeNormalized &&
            ev.timeNormalized <= curr)
        {
            FireAnimEvent(ev, m_owner);
        }
    }

    m_prevTimeNorm = currentTimeNorm;
}
