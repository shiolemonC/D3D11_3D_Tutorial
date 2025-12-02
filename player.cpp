#include "player_state.h"   // FSM
#include <DirectXMath.h>
#include <cmath>
#include "player.h"
#include "collider_system.h"   // collider
#include <memory>              // ★ std::make_unique 用
#include "anim_event_player.h" // ★ 新增：帧事件播放器
#include "boss.h"
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

// ★ Player 的受击框（HurtBox）信息
static int      s_hurtColliderId = -1;
static XMFLOAT3 s_hurtHalfSize{ 0.5f, 1.2f, 0.5f };  // 先和 body 差不多，将来可单独调
static bool     s_hurtEnabled = true;              // 是否可被打（无敌帧时会关）

// 已有：body 的 ignore flag（如果你前面加过）
static bool     s_ignoreBodyBlock = false;

static inline float AngleDelta(float a, float b) {
    float d = fmodf(b - a + XM_PI, XM_2PI) - XM_PI;
    return (d < -XM_PI) ? d + XM_2PI : d;
}
static inline float ExpLerp01(float k, float dt) {
    return 1.0f - expf(-k * dt);
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
    col->userPtr = nullptr;                   // TODO: 将来可以塞 Player* 或部位信息

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


// ------------------ 初始化 ------------------
void Player_Initialize(const PlayerDesc& d)
{
    s_pos = d.spawnPos;
    s_speed = d.moveSpeed;
    s_turnK = d.turnSharpness;
    s_scale = d.scale;

    Player_CreateBodyCollider();
    Player_CreateHurtCollider();   

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
        // 2) 计算目标朝向（世界系），并用指数趋近平滑转身
        float targetYaw = std::atan2(v2.x, v2.y); // x=左右，z=前后

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
    XMMATRIX R = XMMatrixRotationY(s_yaw + XM_PI);
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
}

// ------------------ 对外：一帧更新 ------------------
void Player_Update(double dt, const PlayerUpdateInput& in)
{
    // 1) 把输入写入状态机条件
    PlayerSM_SetMoveInput(in.moveX, in.moveZ);
    if (in.attack) {
        PlayerSM_FireTrigger("Attack");
    }

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
    Player_Kinematic_Update(dt, in, smOut.locomotionActive);

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
            rm.pos.y = 0.0f; // 一般只保留XZ
            // rm.yaw 可以按照需要启用
            rm.yaw = 0.0f;
            Player_ApplyRootMotionDelta(rm);
        }
    }

    // 6) Player vs Boss 身体 AABB 碰撞解決（事后 MTV 推开）
   //    注意：确保 Boss_Update 已在本帧调用过，这样 Boss 的 AABB 是最新的。
    Player_ResolveBodyCollisionWithBoss();
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
    return &s_playerHitboxOwnerTag;;
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
