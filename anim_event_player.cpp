#include "anim_event_player.h"
#include "hitbox_system.h"
// ★ 新增：
#include "player.h"
#include "boss.h"
#include "player_camera.h"
#include "camera_shake.h"
#include "postfx.h"

#include <DirectXMath.h>

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

    case AnimEventType::CameraLockOnPreset:
        // 只要当前相机是 LockOn，PlayerCamera 内部会执行演出；Free 模式会自动忽略
        PlayerCamera_PushLockOnPreset(
            ev.cameraLockOnPreset.presetId,
            ev.cameraLockOnPreset.blendInSec,
            ev.cameraLockOnPreset.holdSec,
            ev.cameraLockOnPreset.blendOutSec,
            ev.cameraLockOnPreset.priority
        );
        break;

    case AnimEventType::CameraShake: // ★ 新增
        // 目前只有玩家相机会抖（单相机项目）
        if (owner == Player_GetHitboxOwnerToken())
        {
            PlayerCamera_PushShake(
                ev.cameraShake.magnitude,
                ev.cameraShake.durationSec,
                ev.cameraShake.mode,
                0.0f,
                ev.cameraShake.priority
            );
        }
        break;
    case AnimEventType::PostFxRadialBlurStart:
        using namespace DirectX;

        // 1) 取 Boss 运行态基准
        XMFLOAT3 bossPos = Boss_GetPosition();
        XMFLOAT3 fwd = Boss_GetForward();              // (sinYaw,0,cosYaw)
        XMFLOAT3 up = XMFLOAT3(0, 1, 0);

        // 2) 由 up 和 forward 算 right（注意：XMVector3Cross 是数学右手叉乘，up×fwd 在你这套约定下会得到正确的 right）
        XMVECTOR vUp = XMLoadFloat3(&up);
        XMVECTOR vFwd = XMLoadFloat3(&fwd);
        XMVECTOR vRight = XMVector3Normalize(XMVector3Cross(vUp, vFwd));
        XMFLOAT3 right{};
        XMStoreFloat3(&right, vRight);

        // 3) 事件里存的是“Boss局部偏移”
        XMFLOAT3 off = ev.postFxRadialBlurStart.centerWorld; // 当作 offsetLocal

        // 4) 局部偏移 -> 世界坐标
        XMFLOAT3 centerWorld;
        centerWorld.x = bossPos.x + right.x * off.x + up.x * off.y + fwd.x * off.z;
        centerWorld.y = bossPos.y + right.y * off.x + up.y * off.y + fwd.y * off.z;
        centerWorld.z = bossPos.z + right.z * off.x + up.z * off.y + fwd.z * off.z;

        PostFx_StartRadialBlurWorld(
            centerWorld,
            ev.postFxRadialBlurStart.durationSec,
            ev.postFxRadialBlurStart.strength,
            ev.postFxRadialBlurStart.radius,
            ev.postFxRadialBlurStart.sampleCount
        );
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
