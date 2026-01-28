#include "player_state.h"   // FSM
#include <DirectXMath.h>
#include <cmath>
#include "player.h"
#include "collider_system.h"   // collider
#include <memory>              // ★ std::make_unique 用
#include "anim_event_player.h" // ★ 新增：帧事件播放器
#include "boss.h"
#include "player_camera.h"
#include "ModelSkinned.h"
#include "sprite_effect.h"
#include "scene.h"
#include "particle_system.h"
#include <cstdlib>
using namespace DirectX;

// ------------------ 内部状态 ------------------
static XMFLOAT3 s_pos{ 0,0,0 };
static float    s_yaw = 0.0f; // 绕Y，弧度（玩家逻辑朝向）
static float    s_speed = 2.5f;
static float    s_turnK = 10.0f;
static float    s_scale = 1.0f;

// ---- Player 用コライダー情報 ----
static int      s_bodyColliderId = -1;               // CollisionWorld 内のID
static XMFLOAT3 s_bodyHalfSize{ 0.4f, 0.9f, 0.4f };  // AABB 半サイズ (x,y,z)

static AnimEventPlayer s_animEventPlayer;  // ★ 每个玩家一份的事件播放器

// HitBox owner 用的小标记（实际内容无所谓，只要地址唯一）
static char s_playerHitboxOwnerTag;
static char s_playerHurtboxOwnerTag;

 // ------------------ 受击缓存 ------------------
static bool      s_hitRequested = false;        // 本帧是否收到新的受击请求
static HitParams s_pendingHit{};                  // HitEvent_Dispatch 填写的受击信息
static HitParams s_lastHit{};                     // 最近一次真正生效的受击（状态机可选使用）
static HitLevel  s_lastHitLevel = HitLevel::Light;

// ★ Player 的受击框（HurtBox）信息
static int      s_hurtColliderId = -1;
static XMFLOAT3 s_hurtHalfSize{ 0.5f, 1.2f, 0.5f };  // 先和 body 差不多，将来可单独调
static bool     s_hurtEnabled = true;              // 是否可被打（无敌帧时会关）

static bool     s_parryWindowEnabled = false;   // ★ 成功格挡窗口开关

// 已有：body 的 ignore flag（如果你前面加过）
static bool     s_ignoreBodyBlock = false;

// ------------------ HP ------------------
static int s_hpMax = 300;
static int s_hp = 300;
static PlayerOnDeathFn s_onDeath = nullptr;

// ------------------ 击退 ------------------
static bool     s_knockActive = false;
static DirectX::XMFLOAT3 s_knockDirXZ{ 0,0,0 };
static float    s_knockSpeed = 0.0f;
static float    s_knockRemainDist = 0.0f;

// 可调参数：减速度（越大越“猛停”）
static float    s_knockDecel = 20.0f;


static inline float AngleDelta(float a, float b) {
    float d = fmodf(b - a + XM_PI, XM_2PI) - XM_PI;
    return (d < -XM_PI) ? d + XM_2PI : d;
}
static inline float ExpLerp01(float k, float dt) {
    return 1.0f - expf(-k * dt);
}

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

// プレイヤーの「体」用 AABB コライダーを作成
static void Player_CreateBodyCollider()
{
    // 既にあれば一旦消して作り直す
    if (s_bodyColliderId >= 0) {
        GetCollisionWorld().UnregisterCollider(s_bodyColliderId);
        s_bodyColliderId = -1;
    }

    auto col = std::make_unique<ColliderBase>();
    col->category = ColliderCategory::CharacterBody;
    col->collideMask = 0;            // ★ まだ何とも当てない。今は debug 可視化専用
    col->userPtr = nullptr;       // 将来 Player* を入れてもOK

    col->shape.type = ColliderShapeType::AABB;

    const XMFLOAT3& c = s_pos;
    const XMFLOAT3& half = s_bodyHalfSize;

    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y,   c.z + half.z };

    s_bodyColliderId = GetCollisionWorld().RegisterCollider(std::move(col));
}

// 位置が変わったときに AABB を更新
static void Player_UpdateBodyCollider()
{
    if (s_bodyColliderId < 0) return;

    ColliderBase* col = GetCollisionWorld().GetCollider(s_bodyColliderId);
    if (!col) return;

    const XMFLOAT3& c = s_pos;
    const XMFLOAT3& half = s_bodyHalfSize;

    col->shape.type = ColliderShapeType::AABB;
    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y,   c.z + half.z };
}

// ★ Player vs Boss の「体」同士のめり込みを最小平行移動ベクトル(MTV)で解消
static void Player_ResolveBodyCollisionWithBoss()
{
    // 将来扩展点1：被技能强制位移时可以暂时跳过 body 阻挡
    //if (s_ignoreBodyBlock) {
    //    return;
    //}

    auto& world = GetCollisionWorld();

    const int playerId = s_bodyColliderId;
    const int bossId = Boss_GetBodyColliderId();
    if (playerId < 0 || bossId < 0) return;

    ColliderBase* pCol = world.GetCollider(playerId);
    ColliderBase* bCol = world.GetCollider(bossId);
    if (!pCol || !bCol) return;
    if (pCol->shape.type != ColliderShapeType::AABB ||
        bCol->shape.type != ColliderShapeType::AABB) {
        return;
    }

    const auto& A = pCol->shape.aabb; // Player
    const auto& B = bCol->shape.aabb; // Boss

    // 3D AABB 交差判定（ここでは Y 方向も見ておく）
    if (A.max.x <= B.min.x || A.min.x >= B.max.x ||
        A.max.y <= B.min.y || A.min.y >= B.max.y ||
        A.max.z <= B.min.z || A.min.z >= B.max.z) {
        // 不交差 → 无需修正
        return;
    }

    // 计算中心与半尺寸
    const float AxCenter = 0.5f * (A.min.x + A.max.x);
    const float AzCenter = 0.5f * (A.min.z + A.max.z);
    const float BxCenter = 0.5f * (B.min.x + B.max.x);
    const float BzCenter = 0.5f * (B.min.z + B.max.z);

    const float AxHalf = 0.5f * (A.max.x - A.min.x);
    const float AzHalf = 0.5f * (A.max.z - A.min.z);
    const float BxHalf = 0.5f * (B.max.x - B.min.x);
    const float BzHalf = 0.5f * (B.max.z - B.min.z);

    const float dx = AxCenter - BxCenter;
    const float dz = AzCenter - BzCenter;

    const float overlapX = (AxHalf + BxHalf) - std::fabs(dx);
    const float overlapZ = (AzHalf + BzHalf) - std::fabs(dz);

    if (overlapX <= 0.0f || overlapZ <= 0.0f) {
        // 数值上应该不会走到这里（前面已经判定交差），保险起见
        return;
    }

    // 选重叠更小的轴作为 MTV 方向（尽量少移动）
    float mtvX = 0.0f;
    float mtvZ = 0.0f;

    if (overlapX < overlapZ) {
        // 沿 X 轴推开
        mtvX = (dx > 0.0f) ? overlapX : -overlapX;
        // mtvZ = 0;
    }
    else {
        // 沿 Z 轴推开
        mtvZ = (dz > 0.0f) ? overlapZ : -overlapZ;
        // mtvX = 0;
    }

    // 将 MTV 应用到玩家位置（Boss 视为重量级障碍物，不动）
    s_pos.x += mtvX;
    s_pos.z += mtvZ;

    // 更新玩家身体 AABB
    Player_UpdateBodyCollider();

    // 将来扩展点2：
    // - 在这里可以根据 MTV 方向/大小计算“被挤压感”的反馈（镜头震动/音效等）
    // - 或者在玩家状态机里标记“贴脸状态”，用于锁定/特殊攻击
}


// ------------------ Player HurtBox：受击框 ------------------

// 创建/注册 Player 的 HurtBox
static void Player_CreateHurtCollider()
{
    // 先清掉旧的
    if (s_hurtColliderId >= 0) {
        GetCollisionWorld().UnregisterCollider(s_hurtColliderId);
        s_hurtColliderId = -1;
    }

    auto col = std::make_unique<ColliderBase>();
    col->category = ColliderCategory::Hurtbox;
    col->active = s_hurtEnabled;   // ★ 简单用 mask 做启用/禁用
    col->userPtr = Player_GetHurtboxOwnerToken();                   // TODO: 将来可以塞 Player* 或部位信息
    col->collideMask = CategoryBit(ColliderCategory::Hitbox);

    col->shape.type = ColliderShapeType::AABB;

    const XMFLOAT3& c = s_pos;
    const XMFLOAT3& half = s_hurtHalfSize;

    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y, c.z + half.z };

    s_hurtColliderId = GetCollisionWorld().RegisterCollider(std::move(col));
}

// 位置/开关改变时更新 HurtBox
static void Player_UpdateHurtCollider()
{
    if (s_hurtColliderId < 0) return;

    ColliderBase* col = GetCollisionWorld().GetCollider(s_hurtColliderId);
    if (!col) return;

    const XMFLOAT3& c = s_pos;
    const XMFLOAT3& half = s_hurtHalfSize;

    col->shape.type = ColliderShapeType::AABB;
    col->shape.aabb.min = { c.x - half.x, c.y,                 c.z - half.z };
    col->shape.aabb.max = { c.x + half.x, c.y + 2.0f * half.y, c.z + half.z };

    // ★ 用 collideMask 控制“是否受击”
    col->active = s_hurtEnabled;

    // TODO: 将来如果做多部位 HurtBox，可以在这里根据状态调整尺寸/位置
}

// ------------------ 击退运动 ------------------

static void Player_StartKnockback(float distance,
    const DirectX::XMFLOAT3& attackerPos,
    const DirectX::XMFLOAT3& victimPos)
{
    if (distance <= 0.0f) return;

    // 覆盖规则：只接受更强的击退
    if (s_knockActive && distance <= s_knockRemainDist) return;

    DirectX::XMFLOAT2 d2{ victimPos.x - attackerPos.x, victimPos.z - attackerPos.z };
    float len = std::sqrt(d2.x * d2.x + d2.y * d2.y);
    if (len < 1e-4f) return;

    d2.x /= len; d2.y /= len;
    s_knockDirXZ = { d2.x, 0.0f, d2.y };
    s_knockRemainDist = distance;

    const float a = std::max(1e-3f, s_knockDecel);
    s_knockSpeed = std::sqrt(2.0f * a * distance);
    s_knockActive = true;
}


static bool Player_UpdateKnockback(double dt)
{
    if (!s_knockActive) return false;

    const float fdt = static_cast<float>(dt);
    if (fdt <= 0.0f) return true;

    if (s_knockRemainDist <= 1e-4f || s_knockSpeed <= 1e-3f)
    {
        s_knockActive = false;
        s_knockSpeed = 0.0f;
        s_knockRemainDist = 0.0f;
        return false;
    }

    const float step = std::min(s_knockRemainDist, s_knockSpeed * fdt);
    s_pos.x += s_knockDirXZ.x * step;
    s_pos.z += s_knockDirXZ.z * step;
    s_knockRemainDist -= step;

    s_knockSpeed = std::max(0.0f, s_knockSpeed - s_knockDecel * fdt);

    Player_UpdateBodyCollider();
    Player_UpdateHurtCollider();

    if (s_knockRemainDist <= 1e-4f || s_knockSpeed <= 1e-3f)
    {
        s_knockActive = false;
        s_knockSpeed = 0.0f;
        s_knockRemainDist = 0.0f;
        return false;
    }

    return true;
}


// ------------------ 初始化 ------------------
void Player_Initialize(const PlayerDesc& d)
{
    s_pos = d.spawnPos;
    s_speed = d.moveSpeed;
    s_turnK = d.turnSharpness;
    s_scale = d.scale;

    Player_CreateBodyCollider();
    Player_CreateHurtCollider();   

    //s_hpMax = 300;   // TODO: 以后你想做配置就从 d 里读
    s_hp = s_hpMax;

    // ★ 帧事件播放器绑定到这个“玩家”
// 目前你没有 Player 实例，就先传 nullptr，将来有 Player* 再改
    s_animEventPlayer.Initialize(Player_GetHitboxOwnerToken());
}

// ------------------ 内部：基于输入的运动（类魂/怪猎） ------------------
// 这里的 in 已经包含「摄像机方向」，所以 WASD 会按摄像机坐标系解释
static void Player_Kinematic_Update(double dt,
    const PlayerUpdateInput& in,
    bool locomotionActive)
{
    // 1) 构建世界系下的移动向量（XZ 平面）
    //    moveDir = moveX * camRight + moveZ * camForward
    XMFLOAT3 f = in.camForwardXZ;
    XMFLOAT3 r = in.camRightXZ;

    XMFLOAT2 v2{
        in.moveX * r.x + in.moveZ * f.x, // 世界 X
        in.moveX * r.z + in.moveZ * f.z  // 世界 Z
    };

    float len = std::sqrt(v2.x * v2.x + v2.y * v2.y);
    if (len > 1e-5f) {
        v2.x /= len;
        v2.y /= len;
    }

    if (locomotionActive && len > 1e-4f) {
        // 2) 计算目标朝向（世界系）
        float targetYaw = std::atan2(v2.x, v2.y); // 默认：面向移动方向

        // 如果处于锁定模式，则改为面向 Boss
        if (PlayerCamera_IsLockOnActive()) {
            DirectX::XMFLOAT3 bossPos = Boss_GetPosition();
            float bdx = bossPos.x - s_pos.x;
            float bdz = bossPos.z - s_pos.z;
            float blen = std::sqrt(bdx * bdx + bdz * bdz);
            if (blen > 1e-4f) {
                targetYaw = std::atan2(bdx, bdz); // 注意和玩家 forward 约定一致：x=左右，z=前后
            }
        }

        // 指数趋近平滑转身
        float a = ExpLerp01(s_turnK, static_cast<float>(dt));
        s_yaw += AngleDelta(s_yaw, targetYaw) * a;

        // 3) 沿着当前移动方向前进（速度为常数 s_speed）
        s_pos.x += v2.x * s_speed * static_cast<float>(dt);
        s_pos.z += v2.y * s_speed * static_cast<float>(dt);
    }

    // 4) 把玩家「当前真值」同步到动画系统的 BaseWorld
    XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);

    // ⚠ 这里暂时仍保留 +XM_PI，用来对齐当前 FBX 的朝向。
    //   将来如果你把资源和 nodeFix 调通，可以把 +XM_PI 删掉，让 s_yaw 真正代表模型的面对方向。
    XMMATRIX R = XMMatrixRotationY(s_yaw +XM_PI);
    XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
    XMMATRIX W = S * R * T;

    AnimatorRegistry_SetWorld(W);
    Player_UpdateBodyCollider();  // ★ 新增
    Player_UpdateHurtCollider();
}

// ------------------ 内部：应用 RootMotion Δ ------------------
static void Player_ApplyRootMotionDelta(const RootMotionDelta& rm)
{
    s_pos.x += rm.pos.x;
    // s_pos.y += rm.pos.y; // 一般不启用Y，防止动画导致穿地
    s_pos.z += rm.pos.z;

    s_yaw += rm.yaw; // 目前 rm.yaw 在调用前可以为 0，将来需要时可以启用

    Player_UpdateBodyCollider();
    Player_UpdateHurtCollider();
}

// ------------------ 对外：一帧更新 ------------------
void Player_Update(double dt, const PlayerUpdateInput& in)
{
    // 0) 处理 Hit 触发（来自 HitEvent_Dispatch）
//    每帧先清掉 hit.trigger，防止旧值残留
    PlayerSM_SetBool("hit.trigger", false);

    if (s_hitRequested)
    {
        s_hitRequested = false;

        s_lastHit = s_pendingHit;
        s_lastHitLevel = s_pendingHit.level;

        // 把一次性的“受击触发”以及等级写入 FSM 条件系统
        PlayerSM_SetBool("hit.trigger", true);

        float levelValue = 0.0f;
        switch (s_lastHitLevel)
        {
        case HitLevel::Light:  levelValue = 0.0f; break;
        case HitLevel::Medium: levelValue = 1.0f; break;
        case HitLevel::Heavy:  levelValue = 2.0f; break;
        default:               levelValue = 0.0f; break;
        }
        PlayerSM_SetFloat("hit.level", levelValue);
    }


    // 1) 把输入写入状态机条件
    PlayerSM_SetMoveInput(in.moveX, in.moveZ);
    if (in.attack) {
        PlayerSM_FireTrigger("Attack");
    }

    if (in.roll) {
        PlayerSM_FireTrigger("Roll");   // ★ 新增：翻滚 Trigger
    }

    if (in.parry) {
        PlayerSM_FireTrigger("Parry"); // ★ 新增：格挡/弹反 Trigger
    }

//#if defined(DEBUG) || defined(_DEBUG)
//    if (in.parry)
//    {
//        XMFLOAT3 p = Player_GetPosition();
//        p.y += 1.0f;
//
//        XMFLOAT3 dir = in.camForwardXZ;
//
//        // 绕Y轴 +90°：朝“右侧”
//        XMFLOAT3 dirRight = { dir.z, 0.0f, -dir.x };
//
//        ParticleSystem_Spawn(VfxId::SparkParry, p, dirRight);
//    }
//    if (in.attack)
//    {
//        XMFLOAT3 p = Player_GetPosition();
//        p.y += 1.0f;
//
//        XMFLOAT3 dir = in.camForwardXZ;
//
//        // 绕Y轴 -90°：朝“左侧”
//        XMFLOAT3 dirLeft = { -dir.z, 0.0f, dir.x };
//
//        ParticleSystem_Spawn(VfxId::BloodSlash, p, dirLeft);
//    }
//#endif

    // 2) 跑 FSM，决定当前播放的状态/动画
    PlayerSMOutput smOut = PlayerSM_Update(dt);

    if (smOut.changed) {
        if (smOut.blendSeconds > 0.0f) {
            AnimatorRegistry_CrossFade(smOut.clip,
                smOut.blendSeconds,
                smOut.blendCurve); // "linear"/"ease_in"...
        }
        else {
            AnimatorRegistry_Play(smOut.clip, nullptr);
        }

        float clipSec = 0.0f;
        if (AnimatorRegistry_DebugGetCurrentClipLengthSec(&clipSec)) {
            PlayerSM_OverrideCurrentStateLength(clipSec);
        }
        // ★ 告诉帧事件播放器：当前播放的动画剪辑变成了 smOut.clip
        s_animEventPlayer.OnClipChanged(smOut.clip);
    }

    // 3) 根据 FSM 的 locomotionActive 决定是否允许 WASD 驱动位移
    const bool knockActive = Player_UpdateKnockback(dt);

    Player_Kinematic_Update(dt, in, smOut.locomotionActive && !knockActive);

    //Player_Kinematic_Update(dt, in, smOut.locomotionActive);

    // 4) 动画时间推进 + RootMotion 累积
    AnimatorRegistry_Update(dt);

    // ★ 4.5) 使用当前动画的归一化时间驱动帧事件系统
    {
        float norm = 0.0f;
        if (AnimatorRegistry_DebugGetCurrentNormalizedTime(&norm)) {
            s_animEventPlayer.Update(norm);
        }
    }

    // 5) 若当前状态允许使用 RootMotion，就消费动画 Δ 并同步回玩家
    if (smOut.useRootMotion) {
        RootMotionDelta rm{};
        if (AnimatorRegistry_ConsumeRootMotionDelta(&rm)) {
            rm.pos.y = 0.0f;
            if (!knockActive) {
                Player_ApplyRootMotionDelta(rm);
            }
            // else: discard
        }
    }

//#if defined(_DEBUG) || defined(DEBUG)
//    // ★ 在这里打同步日志：此时 s_pos 已经是“本帧最终逻辑位置”
//    static int s_sync = 0;
//    if ((s_sync++ % 30) == 0) {
//        DirectX::XMFLOAT3 mw{};
//        ModelSkinned_GetWorldTranslation(&mw); // 下面我会说怎么实现
//
//        char b[256];
//        sprintf_s(b,
//            "[Sync] logicPos=(%.3f,%.3f,%.3f) modelWorld=(%.3f,%.3f,%.3f) diff=(%.3f,%.3f,%.3f)\n",
//            s_pos.x, s_pos.y, s_pos.z,
//            mw.x, mw.y, mw.z,
//            mw.x - s_pos.x, mw.y - s_pos.y, mw.z - s_pos.z);
//        OutputDebugStringA(b);
//    }
//#endif

    // 6) Player vs Boss 身体 AABB 碰撞解決（事后 MTV 推开）
   //    注意：确保 Boss_Update 已在本帧调用过，这样 Boss 的 AABB 是最新的。
    Player_ResolveBodyCollisionWithBoss();

    //{
    //    char buf[256];
    //    sprintf_s(buf, "Player pos=(%.2f,%.2f,%.2f), yaw=%.2f\n",
    //        s_pos.x, s_pos.y, s_pos.z, s_yaw);
    //    OutputDebugStringA(buf);
    //}
}

// ------------------ 查询接口 ------------------
XMMATRIX Player_GetWorld()
{
    XMMATRIX S = XMMatrixScaling(s_scale, s_scale, s_scale);
    XMMATRIX R = XMMatrixRotationY(s_yaw);
    XMMATRIX T = XMMatrixTranslation(s_pos.x, s_pos.y, s_pos.z);
    return S * R * T;
}
const XMFLOAT3& Player_GetPosition() { return s_pos; }
float Player_GetYaw() { return s_yaw; }
XMFLOAT3 Player_GetForward()
{
    XMVECTOR f = XMVector3Normalize(XMVectorSet(std::sinf(s_yaw), 0, std::cosf(s_yaw), 0));
    XMFLOAT3 out; XMStoreFloat3(&out, f); return out;
}

int Player_GetBodyColliderId()
{
    return s_bodyColliderId;
}

void* Player_GetHitboxOwnerToken()
{
    return &s_playerHitboxOwnerTag;
}

void* Player_GetHurtboxOwnerToken()
{
    return &s_playerHurtboxOwnerTag;
}

void Player_SetHurtEnabled(bool enabled)
{
    s_hurtEnabled = enabled;
    // 立刻同步一次，避免等到下一帧
    Player_UpdateHurtCollider();
}

bool Player_IsHurtEnabled()
{
    return s_hurtEnabled;
}

int Player_GetHurtColliderId()
{
    return s_hurtColliderId;
}

// ------------- HP and Damage --------------
void Player_SetMaxHP(int maxHp, bool fullHeal)
{
    s_hpMax = (maxHp > 1) ? maxHp : 1;
    if (fullHeal) s_hp = s_hpMax;
    if (s_hp > s_hpMax) s_hp = s_hpMax;
}

int Player_GetMaxHP() { return s_hpMax; }
int Player_GetHP() { return s_hp; }
bool Player_IsDead() { return s_hp <= 0; }

void Player_SetOnDeath(PlayerOnDeathFn fn)
{
    s_onDeath = fn;
}

void Player_OnDeathRequested()
{
    Scene_Change(SCENE_OVER);
    OutputDebugStringA("[Player] DEAD (stub)\n");
}

void Player_ApplyDamage(int damage)
{
    if (damage <= 0) return;

    const int before = s_hp;
    s_hp = std::max(0, s_hp - damage);

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "[HP][Player] -%d  %d/%d\n", damage, s_hp, s_hpMax);
    OutputDebugStringA(buf);
    // 你也可以同时 printf：
    // std::printf("%s", buf);

    if (before > 0 && s_hp <= 0)
    {
        OutputDebugStringA("[HP][Player] DEAD (hook reserved)\n");
        Player_OnDeathRequested();
    }
}



void Player_SetParryWindowEnabled(bool enabled)
{
    s_parryWindowEnabled = enabled;
}

bool Player_IsParryWindowEnabled()
{
    return s_parryWindowEnabled;
}

static void Player_OnParrySuccess_Log(const HitParams& hit)
{
    char buf[256];
    sprintf_s(buf, "[Parry] SUCCESS! attacker=%p dmg=%d\n",
        hit.attackerOwner, hit.damage);
    OutputDebugStringA(buf);
}

// ★ 统一入口：Boss->Player 命中时只走这里
PlayerHitResponse Player_OnIncomingHit(const HitParams& hit)
{
    // 1) 无敌（Roll）：忽略受击，但根据你最新规则“仍然消耗 hitbox”
    if (!s_hurtEnabled)
    {
        return PlayerHitResponse::Ignored;
    }

    // 2) 成功格挡窗口内：触发成功格挡事件（暂时只打 log），不进入 hit
    if (s_parryWindowEnabled)
    {
        Player_OnParrySuccess_Log(hit);

        if (hit.knockbackDistance > 0.0f) {
            Player_StartKnockback(hit.knockbackDistance * 0.20f, hit.attackerPos, hit.victimPos);
        }

        XMFLOAT3 p = Player_GetPosition();
        p.y += 1.0f;
        ParticleSystem_Spawn(VfxId::SparkParry, p, Player_GetForward());
       // SpriteEffect_SpawnParry(p, { 2.6f, 2.6f });

        // ★ 关键：触发 FSM 的成功格挡分支
        PlayerSM_FireTrigger("ParrySuccess");

        return PlayerHitResponse::Parried;

    }

    // 3) 正常受击：缓存受击信息，等待 Player_Update 写入 FSM 条件
    Player_ApplyDamage(hit.damage);
    s_hitRequested = true;
    s_pendingHit = hit;

    XMFLOAT3 p = hit.hitPos;
    p.y += 1.0f; // 让特效离地一点
    // ★ Blood：方向=受击者(玩家)前方 + 随机左右45°
    XMFLOAT3 dir = RotateY(Player_GetForward(), RandomSign45Deg());
    ParticleSystem_Spawn(VfxId::BloodSlash, p, dir);

    //SpriteEffect_SpawnHit(p, { 1.2f, 1.2f });

    if (hit.knockbackDistance > 0.0f) {
        Player_StartKnockback(hit.knockbackDistance, hit.attackerPos, hit.victimPos);
    }

    return PlayerHitResponse::TookHit;
}


// 兼容旧调用点：内部转调到新入口
void Player_RequestHitReaction(const HitParams& hit)
{
    (void)Player_OnIncomingHit(hit);
}
