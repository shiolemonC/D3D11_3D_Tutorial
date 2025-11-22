#include "anim_event_player.h"
#include "hitbox_system.h"

void AnimEventPlayer::Update(float currentTimeNorm)
{
    if (!m_track) {
        m_prevTimeNorm = currentTimeNorm;
        return;
    }

    // 简单处理：假设攻击动作不循环，
    // norm 从 0.0 单调增加到 1.0
    for (const AnimEvent& ev : m_track->events)
    {
        if (m_prevTimeNorm < ev.timeNormalized &&
            ev.timeNormalized <= currentTimeNorm)
        {
            // 触发事件
            switch (ev.type)
            {
            case AnimEventType::SpawnHitBox:
                HitboxSystem_Spawn(ev.spawnHitBox, m_owner);
                break;
            default:
                break;
            }
        }
    }

    m_prevTimeNorm = currentTimeNorm;
}
