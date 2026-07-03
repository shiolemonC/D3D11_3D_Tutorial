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

    // （可选）快速抓 dt 单位问题：如果 dt > 1，几乎肯定你传的是毫秒
    // if (dt > 1.0f) { OutputDebugStringA("[HitboxSystem] dt looks like ms!\n"); }

    // ------------------------------------------------------------
    // (A) 先更新 AABB：只要“帧开始时还活着”，就允许参与本帧命中
    // ------------------------------------------------------------
    for (auto& hb : g_hitboxes)
    {
        if (hb.colliderId < 0) continue;
        if (hb.remainingTime <= 0.0f) continue;

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
        if (hb.remainingTime <= 0.0f) continue; // 以“帧开始时是否存活”为准

        const ColliderBase* col = world.GetCollider(hb.colliderId);
        if (col) hitColToIndex[col] = i;
    }

    std::unordered_set<std::uint64_t> seen;
    seen.reserve(pairs.size() * 2 + 8);

    auto MakeKey = [](int hitboxColliderId, void* victimOwner) -> std::uint64_t
        {
            uint64_t a = (uint64_t)(uint32_t)hitboxColliderId;
            uint64_t v = (uint64_t)(uintptr_t)victimOwner;

            // 把 64-bit 指针折叠一下，避免只看高位或低位
            uint64_t h = v ^ (v >> 32);

            return (h << 32) ^ a;
        };

    std::vector<int> toUnregister;
    toUnregister.reserve(32);

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

        if (colA->category == ColliderCategory::Hitbox && colB->category == ColliderCategory::Hurtbox) {
            hitCol = colA; hurtCol = colB;
        }
        else if (colB->category == ColliderCategory::Hitbox && colA->category == ColliderCategory::Hurtbox) {
            hitCol = colB; hurtCol = colA;
        }
        else continue;

        auto it = hitColToIndex.find(hitCol);
        if (it == hitColToIndex.end()) continue;

        ActiveHitbox& hb = g_hitboxes[it->second];
        if (hb.colliderId < 0) continue;
        if (hb.remainingTime <= 0.0f) continue; // 帧开始时活着才允许命中

        const std::uint64_t key = MakeKey(hb.colliderId, hurtCol->userPtr);
        if (seen.find(key) != seen.end()) continue;
        seen.insert(key);

        if (hitCol->shape.type != ColliderShapeType::AABB || hurtCol->shape.type != ColliderShapeType::AABB)
            continue;

        const DirectX::XMFLOAT3 atkPos = AABBCenter(hitCol->shape.aabb);
        const DirectX::XMFLOAT3 vicPos = AABBCenter(hurtCol->shape.aabb);

        HitContact c{};
        c.attackerOwner = hb.owner;
        c.victimOwner = hurtCol->userPtr;
        c.damage = hb.damage;
        c.sourceType = HitSourceType::Melee;
        c.knockbackDistance = hb.knockbackDistance;
        c.hitPos = vicPos;
        c.attackerPos = atkPos;
        c.victimPos = vicPos;
        c.level = hb.hitLevel;

        const bool consumed = HitEvent_Dispatch(c);
        if (consumed)
        {
            toUnregister.push_back(hb.colliderId);
            hb.colliderId = -1;
            hb.remainingTime = 0.0f;
        }
    }

    // ------------------------------------------------------------
    // (D) 最后再扣寿命：并把“自然到期”的 hitbox 注销
    // ------------------------------------------------------------
    for (auto& hb : g_hitboxes)
    {
        if (hb.colliderId < 0) continue;

        hb.remainingTime -= dt;
        if (hb.remainingTime <= 0.0f)
        {
            toUnregister.push_back(hb.colliderId);
            hb.colliderId = -1;
        }
    }

    // ------------------------------------------------------------
    // (E) 统一注销 collider（去重可选，但一般不会很多）
    // ------------------------------------------------------------
    for (int id : toUnregister)
        world.UnregisterCollider(id);

    // ------------------------------------------------------------
    // (F) 清理死亡 hitbox
    // ------------------------------------------------------------
    g_hitboxes.erase(
        std::remove_if(g_hitboxes.begin(), g_hitboxes.end(),
            [&](const ActiveHitbox& hb)
            {
                return (hb.colliderId < 0) || (hb.remainingTime <= 0.0f);
            }),
        g_hitboxes.end());
}
