// boss.cpp
#include "boss.h"
#include "boss_ai.h"
#include "BossAnimatorRegistry.h"
#include "AnimatorRegistry.h"   // 为了 RootMotionDelta 等
#include "collider_system.h"   // ★ 碰撞中台
#include "anim_event_player.h"   // ★ 新增帧事件
#include <cmath>
#include "hit_event.h"
#include "sprite_effect.h"
#include "player.h"
#include "camera.h"
#include "scene.h"
#include "particle_system.h"
#include <cstdlib>
#include "player_camera.h"
#include "input_gamepad_xinput.h"
#include "boss_projectile.h"

using namespace DirectX;

// ---- 内部状态 ----
static XMFLOAT3 s_bossPos{ 0,0,0 };
static float    s_bossYaw = 0.0f;
static float    s_bossScale = 1.0f;

static char s_bossHitboxOwnerTag;
static char s_bossHurtboxOwnerTag;

static AnimEventPlayer s_bossEventPlayer;   // ★ Boss 的帧事件播放器

// ---- Boss 身体 AABB collider ----
static int      s_bossBodyColliderId = -1;               // CollisionWorld 内 ID
static XMFLOAT3 s_bossBodyHalfSize{ 1.6f, 1.5f, 1.6f };  // 比玩家略大一点，可再调

// ★ Boss 的 HurtBox
static int      s_bossHurtColliderId = -1;
static XMFLOAT3 s_bossHurtHalfSize{ 1.8f, 1.8f, 1.8f }; // 先和 body 一致
static bool     s_bossHurtEnabled = true;

// ------------ HP -----------
static int s_hpMax = 300;
static int s_hp = 300;
static BossOnDeathFn s_onDeath = nullptr;
static bool s_dead = false;

// ------------ start roar -----------
static bool  s_introRoarTriggered = false;  // 开场Roar只触发一次
static float s_introRoarTimer = 0.0f;   // ★Idle内计时
static float s_introRoarDelay = 1.5f;   // ★延迟秒数：默认1.5秒（你也可改成1~2随机）

// ----------- state machine --------------
enum class BossState {
    Idle,
    Roar,
    Chase,
    Attack,
    Combo,
    CastSpell,
    Hit,    
    Dead,   
};

static BossState s_state = BossState::Idle;
static double    s_timeInState = 0.0;

// 攻击冷却
static double s_attackCooldown = 0.0;
static double s_comboCooldown = 0.0;
static double s_spellCooldown = 0.0;

// 常量参数（之后你可以抽到配置里）
static const float kBossMoveSpeed = 3.0f;  // 追击速度
static const float kBossChaseRange = 20.0f;  // 超过这个距离才开始追（可选视作“感知范围”）
static const float kBossAttackRange = 4.0f;   // 进入这个距离可以攻击

static const float  kBossComboRange = 4.5f;     // 你可以调成 4.0f/5.0f
static const double kAttackCooldownSec = 4.0;   // 攻击冷却时间
static const double kComboCooldownSec = 8.0;     // ★ Combo 自己的 CD（比如 6 秒）
static const double kSpellCooldownSec = 6.0;
static const float  kBossSpellMinRange = 7.0f;
static const float  kBossSpellMaxRange = 22.0f;

static inline DirectX::XMFLOAT3 RotateY(const DirectX::XMFLOAT3& v, float rad)
{
    float c = std::cosf(rad);
    float s = std::sinf(rad);
    return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

static inline float RandomSign45Deg()
{
    return (std::rand() & 1) ? DirectX::XM_PIDIV4 : -DirectX::XM_PIDIV4;
}

static void Boss_OnDeathStub()
{
    OutputDebugStringA("[Boss] DEAD (stub)\n");
}

static BossAIContext Boss_MakeAIContext(BossAIState aiState,
    const BossUpdateContext& ctx,
    bool stateFinished = false)
{
    BossAIContext ai{};
    ai.state = aiState;
    ai.bossPos = s_bossPos;
    ai.playerPos = ctx.playerPos;
    ai.stateFinished = stateFinished;
    ai.introRoarTriggered = s_introRoarTriggered;
    ai.introRoarTimer = s_introRoarTimer;
    ai.introRoarDelay = s_introRoarDelay;
    ai.attackCooldown = s_attackCooldown;
    ai.comboCooldown = s_comboCooldown;
    ai.spellCooldown = s_spellCooldown;
    ai.chaseRange = kBossChaseRange;
    ai.attackRange = kBossAttackRange;
    ai.comboRange = kBossComboRange;
    ai.spellMinRange = kBossSpellMinRange;
    ai.spellMaxRange = kBossSpellMaxRange;
    return ai;
}

// 工具：状态切换统一入口（方便以后加 debug）
static void Boss_ChangeState(BossState next, const wchar_t* clipName, bool useCrossFade = true)
{
    s_state = next;
    s_timeInState = 0.0;

    if (clipName && clipName[0]) {
        if (useCrossFade) {
            // 0.2 秒 ease_in_out 过渡
            BossAnimatorRegistry_CrossFade(clipName, 0.5f, "ease_in_out");
        }
        else {
            BossAnimatorRegistry_Play(clipName, nullptr);
        }

        // ★ 不论是 CrossFade 还是 Play，只要指定了 clip，就要通知帧事件播放器
        // 如果 AnimEventPlayer::OnClipChanged 接受 std::wstring&：
        s_bossEventPlayer.OnClipChanged(std::wstring{ clipName });
        // 如果你把 OnClipChanged 改成 const wchar_t* 版本，就可以直接：
        // s_bossEventPlayer.OnClipChanged(clipName);
    }

    if (next == BossState::Idle && !s_introRoarTriggered)
    {
        s_introRoarTimer = 0.0f; // ★进入Idle开始计时
    }

}

static void Boss_ApplyAICommand(BossAICommand cmd)
{
    switch (cmd)
    {
    case BossAICommand::Idle:
        if (s_state != BossState::Idle)
        {
            Boss_ChangeState(BossState::Idle, L"Boss_Idle");
        }
        break;

    case BossAICommand::Chase:
        if (s_state != BossState::Chase)
        {
            Boss_ChangeState(BossState::Chase, L"Boss_Chase");
        }
        break;

    case BossAICommand::Attack:
        Boss_ChangeState(BossState::Attack, L"Boss_Attack");
        break;

    case BossAICommand::Combo:
        Boss_ChangeState(BossState::Combo, L"Boss_Combo");
        break;

    case BossAICommand::CastSpell:
        Boss_ChangeState(BossState::CastSpell, L"Boss_CastSpell");
        break;

    case BossAICommand::Roar:
        s_introRoarTriggered = true;
        Boss_ChangeState(BossState::Roar, L"Boss_Roar", true);

        if (!PlayerCamera_IsLockOnActive())
        {
            PlayerCamera_EnsureLockOn();
        }
        break;

    case BossAICommand::None:
    default:
        break;
    }
}

// Boss の「体」用 AABB コライダーを作成
static void Boss_CreateBodyCollider()
{
    // 先把旧的删掉（热重载之类情况下安全一点）
    if (s_bossBodyColliderId >= 0) {
        GetCollisionWorld().UnregisterCollider(s_bossBodyColliderId);
        s_bossBodyColliderId = -1;
    }

    auto col = std::make_unique<ColliderBase>();
    col->category = ColliderCategory::CharacterBody; // 先和玩家用同一类
    col->collideMask = 0;                               // 暂时只用于 debug 可视化
    col->userPtr = nullptr;                         // 将来也可以放 Boss* 指针

    col->shape.type = ColliderShapeType::AABB;

    const XMFLOAT3& c = s_bossPos;
    const XMFLOAT3& half = s_bossBodyHalfSize;

    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y, c.z + half.z };

    s_bossBodyColliderId = GetCollisionWorld().RegisterCollider(std::move(col));
}

// Boss の AABB を現在位置に合わせて更新
static void Boss_UpdateBodyCollider()
{
    if (s_bossBodyColliderId < 0) return;

    ColliderBase* col = GetCollisionWorld().GetCollider(s_bossBodyColliderId);
    if (!col) return;

    const XMFLOAT3& c = s_bossPos;
    const XMFLOAT3& half = s_bossBodyHalfSize;

    col->shape.type = ColliderShapeType::AABB;
    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y, c.z + half.z };
}

//hurtBox
static void Boss_CreateHurtCollider()
{
    if (s_bossHurtColliderId >= 0) {
        GetCollisionWorld().UnregisterCollider(s_bossHurtColliderId);
        s_bossHurtColliderId = -1;
    }

    auto col = std::make_unique<ColliderBase>();
    col->category = ColliderCategory::Hurtbox;
    col->collideMask = CategoryBit(ColliderCategory::Hitbox);
    col->active = s_bossHurtEnabled;
    col->userPtr = Boss_GetHurtboxOwnerToken(); // TODO: 将来可以放 Boss* 或部位信息

    col->shape.type = ColliderShapeType::AABB;

    const XMFLOAT3& c = s_bossPos;
    const XMFLOAT3& half = s_bossHurtHalfSize;

    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y, c.z + half.z };

    s_bossHurtColliderId = GetCollisionWorld().RegisterCollider(std::move(col));
}

static void Boss_UpdateHurtCollider()
{
    if (s_bossHurtColliderId < 0) return;

    ColliderBase* col = GetCollisionWorld().GetCollider(s_bossHurtColliderId);
    if (!col) return;

    const XMFLOAT3& c = s_bossPos;
    const XMFLOAT3& half = s_bossHurtHalfSize;

    col->shape.type = ColliderShapeType::AABB;
    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y, c.z + half.z };

    col->active = s_bossHurtEnabled;

    // TODO: 将来可以根据 Boss 状态（举盾/趴下/蓄力）改变 HurtBox 尺寸
}

void Boss_Initialize(const BossDesc& d)
{
    s_bossPos = d.spawnPos;
    s_bossScale = d.scale;
    s_bossYaw = 0.0f;

    s_state = BossState::Idle;
    s_timeInState = 0.0;
    s_attackCooldown = 0.0;
    s_comboCooldown = 0.0;
    s_spellCooldown = 0.0;
    s_dead = false;

    // 初始播放 Idle
    BossAnimatorRegistry_Play(L"Boss_Idle", nullptr);

    Boss_CreateBodyCollider();   // ★ 创建 Boss 身体 AABB
    Boss_CreateHurtCollider();   // ★ 新增

    Boss_SetMaxHP(300, true);

    s_introRoarTriggered = false;
    s_introRoarTimer = 0.0f;
    s_introRoarDelay = 3.5f; // 或随机

    // ★ 帧事件播放器绑定到 Boss owner token
    s_bossEventPlayer.Initialize(Boss_GetHitboxOwnerToken());
}

DirectX::XMFLOAT3 Boss_GetPosition() { return s_bossPos; }

static inline float Boss_GetNodeYawFix()
{
    return BossModelSkinned_GetNodeYawFix(); // 或者 BossAnimatorRegistry_GetNodeYawFix()
}

static inline float Boss_GetVisualYaw()
{
    return s_bossYaw + Boss_GetNodeYawFix();
}


DirectX::XMFLOAT3 Boss_GetForward()
{
    //float sy = std::sinf(s_bossYaw);
    //float cy = std::cosf(s_bossYaw);
    //return DirectX::XMFLOAT3{ sy, 0.0f, cy }; // 和 Player_GetForward 同一约定
    const float yaw = Boss_GetVisualYaw();
    return { std::sinf(yaw), 0.0f, std::cosf(yaw) };
}
float Boss_GetYaw() { return s_bossYaw; }

DirectX::XMMATRIX Boss_GetWorld()
{
    XMMATRIX S = XMMatrixScaling(s_bossScale, s_bossScale, s_bossScale);
    XMMATRIX R = XMMatrixRotationY(s_bossYaw + XM_PI);
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

    // ★ Combo 冷却
    if (s_comboCooldown > 0.0) {
        s_comboCooldown -= dt;
        if (s_comboCooldown < 0.0) s_comboCooldown = 0.0;
    }

    if (s_spellCooldown > 0.0) {
        s_spellCooldown -= dt;
        if (s_spellCooldown < 0.0) s_spellCooldown = 0.0;
    }

    switch (s_state)
    {
    case BossState::Idle:
    {
        // ★开场缓冲：Idle 停 1~2 秒，再进入 Roar（只触发一次）
        if (!s_introRoarTriggered)
        {
            s_introRoarTimer += (float)dt; // dt如果是秒就直接加；如果你dt是毫秒就改成 dt*0.001f
        }

        Boss_ApplyAICommand(BossAI_Decide(
            Boss_MakeAIContext(BossAIState::Idle, ctx)));
        break;
    }

    case BossState::Roar:
    {
        float norm = 0.0f;
        if (BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm))
        {
            if (norm >= 1.0f)
            {
                Boss_ChangeState(BossState::Idle, L"Boss_Idle");
            }
        }
        break;
    }

    case BossState::Chase:
    {
        Boss_ApplyAICommand(BossAI_Decide(
            Boss_MakeAIContext(BossAIState::Chase, ctx)));
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

    case BossState::Combo:
    {
        float norm = 0.0f;
        if (BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm)) {
            if (norm >= 1.0f) {
                s_comboCooldown = kComboCooldownSec;

                s_attackCooldown = std::max(s_attackCooldown, 1.5); // 或直接 = kAttackCooldownSec;

                Boss_ChangeState(BossState::Idle, L"Boss_Idle");
            }
        }
        break;
    }

    case BossState::CastSpell:
    {
        float norm = 0.0f;
        if (BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm)) {
            if (norm >= 1.0f) {
                s_spellCooldown = kSpellCooldownSec;
                Boss_ChangeState(BossState::Idle, L"Boss_Idle");
            }
        }
        break;
    }

    case BossState::Hit:
    {
        float norm = 0.0f;
        BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm);
        if (norm >= 1.0f)
        {
            Boss_ApplyAICommand(BossAI_Decide(
                Boss_MakeAIContext(BossAIState::Hit, ctx, true)));
        }
        break;
    }

    case BossState::Dead:
        float norm = 0.0f;
        if (BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm)) {
            if (norm >= 1.0f) {
                // 一次攻击结束
                Scene_Change(SCENE_RESULT);
            }
        }
        break;
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

            const float yawToPlayer = std::atan2(dx, dz);
            s_bossYaw = yawToPlayer - Boss_GetNodeYawFix();
            //s_bossYaw = std::atan2(dx, dz);
        }
    }
    else// if (s_state == BossState::Idle || s_state == BossState::Attack || s_state == BossState::Hit) {
    {
        // Idle / Attack 时也可以让 Boss 面向玩家（看你喜好）
        float dx = ctx.playerPos.x - s_bossPos.x;
        float dz = ctx.playerPos.z - s_bossPos.z;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1e-3f) {
            const float yawToPlayer = std::atan2(dx, dz);
            s_bossYaw = yawToPlayer - Boss_GetNodeYawFix();
            //s_bossYaw = std::atan2(dx, dz);
        }
    }

    // ---- 3) 把 Boss 当前世界矩阵传给动画层 ----
    XMMATRIX W = Boss_GetWorld();
    BossAnimatorRegistry_SetWorld(W);

    // 3.5) Boss 身体 AABB 同步到当前世界位置
    Boss_UpdateBodyCollider();

    Boss_UpdateHurtCollider();

    // ---- 4) 推进 Boss 动画（含 CrossFade / RootMotion 累计）----
    BossAnimatorRegistry_Update(dt);


    // ---- 4.5) 把 RootMotion 同步到逻辑坐标（rm.pos 已是世界系）----
    RootMotionDelta rm{};
    if (BossAnimatorRegistry_ConsumeRootMotionDelta(&rm))
    {
        s_bossPos.x -= rm.pos.x;
        //s_bossPos.y += rm.pos.y;   // 不需要Y就删掉
        s_bossPos.z -= rm.pos.z;
    }

    // 现在再更新一次世界矩阵/碰撞盒（让 collider 跟上）
    BossAnimatorRegistry_SetWorld(Boss_GetWorld());
    Boss_UpdateBodyCollider();
    Boss_UpdateHurtCollider();
    // 第一版不使用 Boss RootMotion（rmType 全部设为 None）
    // 如果以后要做冲刺，可以在 Attack 状态里 Consume RootMotion

        // ---- 帧事件（Boss 出招帧）----
    float norm = 0.0f;
    if (BossAnimatorRegistry_DebugGetCurrentNormalizedTime(&norm)) {
        s_bossEventPlayer.Update(norm);
    }
}



int Boss_GetBodyColliderId()
{
    return s_bossBodyColliderId;
}

void* Boss_GetHitboxOwnerToken()
{
    return &s_bossHitboxOwnerTag;
}

void* Boss_GetHurtboxOwnerToken()
{
    return &s_bossHurtboxOwnerTag;
}

void Boss_SetHurtEnabled(bool enabled)
{
    s_bossHurtEnabled = enabled;
    Boss_UpdateHurtCollider();
}

bool Boss_IsHurtEnabled()
{
    return s_bossHurtEnabled;
}

int Boss_GetHurtColliderId()
{
    return s_bossHurtColliderId;
}

void Boss_SetMaxHP(int maxHp, bool fullHeal)
{
    s_hpMax = (maxHp > 1) ? maxHp : 1;
    if (fullHeal) s_hp = s_hpMax;
    if (s_hp > s_hpMax) s_hp = s_hpMax;
}

int Boss_GetMaxHP() { return s_hpMax; }
int Boss_GetHP() { return s_hp; }
bool Boss_IsDead() { return s_dead; }

void Boss_SetOnDeath(BossOnDeathFn fn)
{
    s_onDeath = fn;
}

void Boss_ApplyDamage(int damage)
{
    if (damage <= 0) return;
    s_hp -= damage;
    if (s_hp < 0) s_hp = 0;

    char buf[256];
    sprintf_s(buf, "[Boss] ApplyDamage dmg=%d => hp=%d/%d\n", damage, s_hp, s_hpMax);
    OutputDebugStringA(buf);

    if (s_hp <= 0 && !s_dead)
    {
        s_dead = true;

        // 死亡瞬间：关闭 HurtBox，切到死亡动画
        s_bossHurtEnabled = false;
        Boss_UpdateHurtCollider();

        Boss_ChangeState(BossState::Dead, L"Boss_Die", false);

        Boss_OnDeathStub(); // 你原本预留的死亡接口
    }
}

void Boss_OnIncomingHit(const HitParams& hit)
{
    if (s_dead || s_state == BossState::Dead) return;
    if (!s_bossHurtEnabled) return;

    Boss_ApplyDamage(hit.damage);

    XMFLOAT3 p = hit.hitPos;
    p.y += 1.0f; // 让特效离地一点

    // 让特效朝玩家方向“突出”一点，避免被 Boss 身体挡住
    {
        XMFLOAT3 camPos = Camera_GetPosition();
        XMFLOAT3 playerPos = Player_GetPosition(); // 或者用 bossPos

        // 相机朝向：这里用“相机指向玩家”的近似 forward（你也可以从 camera 模块直接拿 forward）
        XMVECTOR vCam = XMLoadFloat3(&camPos);
        XMVECTOR vTarget = XMLoadFloat3(&playerPos);
        XMVECTOR camForward = XMVector3Normalize(vTarget - vCam);

        float push = 1.1f;
        XMVECTOR vP = XMLoadFloat3(&p);
        vP += camForward * push;
        XMStoreFloat3(&p, vP);
    }
    // ★ Blood：方向=受击者(Boss)前方 + 随机左右45°
    XMFLOAT3 dir = RotateY(Boss_GetForward(), RandomSign45Deg());
    ParticleSystem_Spawn(VfxId::BloodSlash, p, dir);

    //SpriteEffect_SpawnHit(p, { 2.2f, 2.2f });

    // Boss_ApplyDamage 里已经处理了死亡瞬间切 Dead
    if (s_dead) return;

    // ★ 只有 Heavy 才进入受击
    if (hit.level == HitLevel::Heavy)
    {
        input::xinput::PlayImpulse(8.3f, 8.5f, 0.2f);
        // Hit 过程中再次 Heavy：强制重开受击动画
        if (s_state == BossState::Hit)
        {
            s_timeInState = 0.0;
            BossAnimatorRegistry_ForceRestartCurrent();                 // ★关键：清零 normalized 时钟 + Seek(0)
            s_bossEventPlayer.OnClipChanged(std::wstring{ L"Boss_Hit" }); // ★关键：让帧事件能再触发一遍
            return;
        }

        // 非 Hit 状态：正常进入 Hit（你原本的逻辑）
        Boss_ChangeState(BossState::Hit, L"Boss_Hit", true);
        return;
    }
}
