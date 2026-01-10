#include "hitbox_system.h"
#include "player.h"          // 先只支持 Player，之后可以扩展到 Enemy
#include "boss.h"
#include "hit_event.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

using namespace DirectX;

static std::vector<ActiveHitbox> g_hitboxes;

static inline DirectX::XMFLOAT3 AABBCenter(const BOXAABB& a)
{
    return {
        0.5f * (a.min.x + a.max.x),
        0.5f * (a.min.y + a.max.y),
        0.5f * (a.min.z + a.max.z)
    };
}

// 通过 ColliderWorld 内的 colliderId 找回我们自己的 ActiveHitbox
static ActiveHitbox* FindHitboxByCollider(const ColliderBase* col)
{
    if (!col) return nullptr;

    CollisionWorld& world = GetCollisionWorld();

    for (auto& hb : g_hitboxes)
    {
        if (hb.colliderId < 0) continue;

        ColliderBase* myCol = world.GetCollider(hb.colliderId);
        if (myCol == col)
        {
            return &hb;
        }
    }
    return nullptr;
}

// 获取“owner”的世界位置和方向（先只支持 Player）
static void QueryOwnerTransform(void* owner,
    XMFLOAT3& outPos,
    XMFLOAT3& outForward,
    XMFLOAT3& outRight,
    XMFLOAT3& outUp)
{
    using namespace DirectX;

    outUp = XMFLOAT3{ 0.0f, 1.0f, 0.0f };

    if (owner == Player_GetHitboxOwnerToken()) {
        outPos = Player_GetPosition();
        outForward = Player_GetForward();
    }
    else if (owner == Boss_GetHitboxOwnerToken()) {
        outPos = Boss_GetPosition();
        outForward = Boss_GetForward();
    }
    else {
        // fallback：万一没匹配上，就随便给一个默认
        outPos = XMFLOAT3{ 0,0,0 };
        outForward = XMFLOAT3{ 0,0,1 };
    }

    XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&outForward));
    XMVECTOR u = XMVector3Normalize(XMLoadFloat3(&outUp));
    XMVECTOR r = XMVector3Normalize(XMVector3Cross(u, f));

    XMStoreFloat3(&outRight, r);
}

static void UpdateHitboxAABB(CollisionWorld& world, ActiveHitbox& hb)
{
    ColliderBase* col = world.GetCollider(hb.colliderId);
    if (!col) return;
    if (col->shape.type != ColliderShapeType::AABB) return;

    XMFLOAT3 pos, fwd, right, up;
    QueryOwnerTransform(hb.owner, pos, fwd, right, up);

    // worldCenter = pos + R*off.x + U*off.y + F*off.z
    XMVECTOR P = XMLoadFloat3(&pos);
    XMVECTOR F = XMLoadFloat3(&fwd);
    XMVECTOR R = XMLoadFloat3(&right);
    XMVECTOR U = XMLoadFloat3(&up);

    XMVECTOR offset =
        R * hb.localOffset.x +
        U * hb.localOffset.y +
        F * hb.localOffset.z;

    XMFLOAT3 center{};
    XMStoreFloat3(&center, P + offset);

    const XMFLOAT3& h = hb.halfSize;

    col->shape.aabb.min = { center.x - h.x, center.y - h.y, center.z - h.z };
    col->shape.aabb.max = { center.x + h.x, center.y + h.y, center.z + h.z };
}

//// 小工具：从 ColliderCategory 得到 bit
//static unsigned int CategoryBit(ColliderCategory cat)
//{
//    return 1u << static_cast<unsigned int>(cat);
//}

void HitboxSystem_Clear()
{
    // 先把所有 collider 从 CollisionWorld 注销
    auto& world = GetCollisionWorld();
    for (auto& hb : g_hitboxes) {
        if (hb.colliderId >= 0) {
            world.UnregisterCollider(hb.colliderId);
        }
    }
    g_hitboxes.clear();
}

void HitboxSystem_Spawn(const AnimEvent_SpawnHitBox& param, void* owner)
{
    auto& world = GetCollisionWorld();

    // 1) 创建 collider
    auto col = std::make_unique<Hitbox>();
    col->category = ColliderCategory::Hitbox;
    col->collideMask = CategoryBit(ColliderCategory::Hurtbox); // 只和 Hurtbox 碰
    col->userPtr = owner;
    col->shape.type = ColliderShapeType::AABB;

    // 先用一个临时位置，真正的位置会在下一帧 Update 里刷新
    col->shape.aabb.min = { 0,0,0 };
    col->shape.aabb.max = { 0,0,0 };

    int id = world.RegisterCollider(std::move(col));

    // 2) 记录 ActiveHitbox
    ActiveHitbox hb{};
    hb.colliderId = id;
    hb.owner = owner;
    hb.localOffset = param.localOffset;
    hb.halfSize = param.halfSize;
    hb.remainingTime = param.durationSec;
    hb.damage = param.damage;

    hb.hitLevel = param.hitLevel; // ★

    hb.knockbackDistance = param.knockbackDistance; // ★ 新增

    g_hitboxes.push_back(hb);

    // ★ 关键：生成当帧就把 AABB 刷到正确位置，否则会有 1 帧看不见/打不到
    UpdateHitboxAABB(world, g_hitboxes.back());
}


void HitboxSystem_Update(float dt)
{

     auto& world = GetCollisionWorld();

    // ------------------------------------------------------------
    // (A) 更新寿命 & 更新每个 hitbox collider 的 AABB（保留你原本逻辑）
    //     这里不展开：你原来怎么从 owner/localOffset 算 AABB，就继续用。
    // ------------------------------------------------------------
    for (auto& hb : g_hitboxes)
    {
        hb.remainingTime -= dt;

        if (hb.remainingTime <= 0.0f)
        {
            // 先不在这里 Unregister，交给最后统一清理
            continue;
        }

        // TODO: 这里调用你原来的“更新 hitbox AABB”的逻辑
        UpdateHitboxAABB(world, hb);
    }

    // ------------------------------------------------------------
    // (B) Step 一次，收集本帧所有碰撞对
    // ------------------------------------------------------------
    std::vector<CollisionPair> pairs;
    world.Step(pairs);

    // hitbox collider* -> g_hitboxes index
    std::unordered_map<const ColliderBase*, size_t> hitColToIndex;
    hitColToIndex.reserve(g_hitboxes.size());
    for (size_t i = 0; i < g_hitboxes.size(); ++i)
    {
        const ActiveHitbox& hb = g_hitboxes[i];
        if (hb.colliderId < 0) continue;
        if (hb.remainingTime <= 0.0f) continue;

        const ColliderBase* col = world.GetCollider(hb.colliderId);
        if (col) hitColToIndex[col] = i;
    }

    // 同帧去重 key: (hitboxColliderId, victimOwnerToken)
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(pairs.size() * 2 + 8);

    auto MakeKey = [](int hitboxColliderId, void* victimOwner) -> std::uint64_t
    {
        std::uint64_t a = static_cast<std::uint64_t>(static_cast<std::uint32_t>(hitboxColliderId));
        std::uint64_t v = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(victimOwner) >> 4);
        return (v << 32) ^ a;
    };

    std::vector<int> toUnregister;
    toUnregister.reserve(16);

    // ------------------------------------------------------------
    // (C) 处理命中：consumed -> 立刻标记死亡 + 收集注销列表
    // ------------------------------------------------------------
    for (const CollisionPair& cp : pairs)
    {
        const ColliderBase* colA = cp.a;
        const ColliderBase* colB = cp.b;
        if (!colA || !colB) continue;

        const ColliderBase* hitCol = nullptr;
        const ColliderBase* hurtCol = nullptr;

        if (colA->category == ColliderCategory::Hitbox &&
            colB->category == ColliderCategory::Hurtbox)
        {
            hitCol = colA;
            hurtCol = colB;
        }
        else if (colB->category == ColliderCategory::Hitbox &&
                 colA->category == ColliderCategory::Hurtbox)
        {
            hitCol = colB;
            hurtCol = colA;
        }
        else
        {
            continue;
        }

        auto it = hitColToIndex.find(hitCol);
        if (it == hitColToIndex.end()) continue;

        ActiveHitbox& hb = g_hitboxes[it->second];
        if (hb.colliderId < 0 || hb.remainingTime <= 0.0f) continue;

        // 同帧去重
        const std::uint64_t key = MakeKey(hb.colliderId, hurtCol->userPtr);
        if (seen.find(key) != seen.end()) continue;
        seen.insert(key);

        // 计算 attacker/victim/hit 位置：用 AABB center
        if (hitCol->shape.type != ColliderShapeType::AABB ||
            hurtCol->shape.type != ColliderShapeType::AABB)
        {
            continue;
        }

        const DirectX::XMFLOAT3 atkPos  = AABBCenter(hitCol->shape.aabb);
        const DirectX::XMFLOAT3 vicPos  = AABBCenter(hurtCol->shape.aabb);
        const DirectX::XMFLOAT3 hitPos  = vicPos;

        HitContact c{};
        c.attackerOwner = hb.owner;
        c.victimOwner   = hurtCol->userPtr;
        c.damage        = hb.damage;
        c.knockbackDistance = hb.knockbackDistance;
        c.hitPos        = hitPos;
        c.attackerPos   = atkPos;
        c.victimPos     = vicPos;
        c.level         = hb.hitLevel;


        const bool consumed = HitEvent_Dispatch(c);
        if (consumed)
        {
            toUnregister.push_back(hb.colliderId);

            hb.colliderId = -1;      // ★ 本帧后续 pair 直接跳过
            hb.remainingTime = 0.0f; // ★ 标记死亡
        }
    }

    // ------------------------------------------------------------
    // (D) 统一注销 collider
    // ------------------------------------------------------------
    for (int id : toUnregister)
    {
        world.UnregisterCollider(id);
    }

    // ------------------------------------------------------------
    // (E) 清理过期/死亡 hitbox（如果还有没注销的，这里兜底注销）
    // ------------------------------------------------------------
    g_hitboxes.erase(
        std::remove_if(g_hitboxes.begin(), g_hitboxes.end(),
            [&](const ActiveHitbox& hb)
            {
                if (hb.colliderId < 0) return true;
                if (hb.remainingTime <= 0.0f)
                {
                    world.UnregisterCollider(hb.colliderId);
                    return true;
                }
                return false;
            }),
        g_hitboxes.end());
}
