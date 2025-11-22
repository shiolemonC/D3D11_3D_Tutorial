// boss.cpp
#include "boss.h"
#include "BossAnimatorRegistry.h"
#include "AnimatorRegistry.h"   // 为了 RootMotionDelta 等
#include <cmath>

using namespace DirectX;

// ---- 内部状态 ----
static XMFLOAT3 s_bossPos{ 0,0,0 };
static float    s_bossYaw = 0.0f;
static float    s_bossScale = 1.0f;

enum class BossState {
    Idle,
    Chase,
    Attack,
};

static BossState s_state = BossState::Idle;
static double    s_timeInState = 0.0;

// 攻击冷却
static double s_attackCooldown = 0.0;

// 常量参数（之后你可以抽到配置里）
static const float kBossMoveSpeed = 3.0f;  // 追击速度
static const float kBossChaseRange = 20.0f;  // 超过这个距离才开始追（可选视作“感知范围”）
static const float kBossAttackRange = 4.0f;   // 进入这个距离可以攻击
static const double kAttackCooldownSec = 2.0;   // 攻击冷却时间

// 工具函数：向量长度²
static float LengthSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
{
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    return dx * dx + dz * dz;
}

// 工具：状态切换统一入口（方便以后加 debug）
static void Boss_ChangeState(BossState next, const wchar_t* clipName, bool useCrossFade = true)
{
    s_state = next;
    s_timeInState = 0.0;

    if (clipName && clipName[0]) {
        if (useCrossFade) {
            // 0.2 秒 ease_in_out 过渡
            BossAnimatorRegistry_CrossFade(clipName, 0.2f, "ease_in_out");
        }
        else {
            BossAnimatorRegistry_Play(clipName, nullptr);
        }
    }
}

void Boss_Initialize(const BossDesc& d)
{
    s_bossPos = d.spawnPos;
    s_bossScale = d.scale;
    s_bossYaw = 0.0f;

    s_state = BossState::Idle;
    s_timeInState = 0.0;
    s_attackCooldown = 0.0;

    // 初始播放 Idle
    BossAnimatorRegistry_Play(L"Boss_Idle", nullptr);
}

DirectX::XMFLOAT3 Boss_GetPosition() { return s_bossPos; }
float Boss_GetYaw() { return s_bossYaw; }

DirectX::XMMATRIX Boss_GetWorld()
{
    XMMATRIX S = XMMatrixScaling(s_bossScale, s_bossScale, s_bossScale);
    XMMATRIX R = XMMatrixRotationY(s_bossYaw);
    XMMATRIX T = XMMatrixTranslation(s_bossPos.x, s_bossPos.y, s_bossPos.z);
    return S * R * T;
}

void Boss_Update(double dt, const BossUpdateContext& ctx)
{
    s_timeInState += dt;

    // 先更新冷却
    if (s_attackCooldown > 0.0) {
        s_attackCooldown -= dt;
        if (s_attackCooldown < 0.0) s_attackCooldown = 0.0;
    }

    // ---- 1) 根据状态 + 玩家距离决策，下一个状态 ----
    const float distSq = LengthSqXZ(s_bossPos, ctx.playerPos);
    const float chaseRangeSq = kBossChaseRange * kBossChaseRange;
    const float attackRangeSq = kBossAttackRange * kBossAttackRange;

    switch (s_state)
    {
    case BossState::Idle:
    {
        // 玩家远 → 追击
        if (distSq > attackRangeSq && distSq <= chaseRangeSq) {
            Boss_ChangeState(BossState::Chase, L"Boss_Chase");
        }
        // 玩家很近且冷却结束 → 攻击
        else if (distSq <= attackRangeSq && s_attackCooldown <= 0.0) {
            Boss_ChangeState(BossState::Attack, L"Boss_Attack");
        }
        break;
    }

    case BossState::Chase:
    {
        // 距离够近且冷却结束 → 攻击
        if (distSq <= attackRangeSq && s_attackCooldown <= 0.0) {
            Boss_ChangeState(BossState::Attack, L"Boss_Attack");
        }
        // 玩家太远/离开感知范围 → 回 Idle
        else if (distSq > chaseRangeSq) {
            Boss_ChangeState(BossState::Idle, L"Boss_Idle");
        }
        break;
    }

    case BossState::Attack:
    {
        // 攻击状态：等动画播完回 Idle，并开始冷却
        float norm = 0.0f;
        if (BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm)) {
            if (norm >= 1.0f) {
                // 一次攻击结束
                s_attackCooldown = kAttackCooldownSec;
                Boss_ChangeState(BossState::Idle, L"Boss_Idle");
            }
        }
        break;
    }
    }

    // ---- 2) 逻辑位移 & 朝向（Chase 状态） ----
    if (s_state == BossState::Chase) {
        float dx = ctx.playerPos.x - s_bossPos.x;
        float dz = ctx.playerPos.z - s_bossPos.z;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1e-3f) {
            dx /= len; dz /= len;
            s_bossPos.x += dx * kBossMoveSpeed * float(dt);
            s_bossPos.z += dz * kBossMoveSpeed * float(dt);

            s_bossYaw = std::atan2(dx, dz);
        }
    }
    else if (s_state == BossState::Idle || s_state == BossState::Attack) {
        // Idle / Attack 时也可以让 Boss 面向玩家（看你喜好）
        float dx = ctx.playerPos.x - s_bossPos.x;
        float dz = ctx.playerPos.z - s_bossPos.z;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1e-3f) {
            s_bossYaw = std::atan2(dx, dz);
        }
    }

    // ---- 3) 把 Boss 当前世界矩阵传给动画层 ----
    XMMATRIX W = Boss_GetWorld();
    BossAnimatorRegistry_SetWorld(W);

    // ---- 4) 推进 Boss 动画（含 CrossFade / RootMotion 累计）----
    BossAnimatorRegistry_Update(dt);

    // 第一版不使用 Boss RootMotion（rmType 全部设为 None）
    // 如果以后要做冲刺，可以在 Attack 状态里 Consume RootMotion
}
