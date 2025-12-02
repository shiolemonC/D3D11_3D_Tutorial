#pragma once
/*==============================================================================
    アニメーションイベント再生 [anim_event_player.h]
==============================================================================*/

#include <string>
#include "anim_event.h"

class AnimEventPlayer
{
public:
    AnimEventPlayer() = default;

    void Initialize(void* ownerPtr)
    {
        m_owner = ownerPtr;
        m_currentClip.clear();
        m_track = nullptr;
        m_prevTimeNorm = 0.0f;
    }

    // 切换剪辑时调用
    void OnClipChanged(const std::wstring& newClipName);

    // 每帧调用：currentTimeNorm 是当前剪辑的归一化时间
    void Update(float currentTimeNorm);

private:
    void* m_owner = nullptr;

    std::wstring m_currentClip;
    const AnimEventTrack* m_track = nullptr;
    float m_prevTimeNorm = 0.0f;
};
