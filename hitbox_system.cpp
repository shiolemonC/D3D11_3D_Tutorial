#include "hitbox_system.h"
#include "player.h"          // 先只支持 Player，之后可以扩展到 Enemy
#include "boss.h"
#include "hit_event.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

using namespace DirectX;

static std::vector<ActiveHitbox> g_hitboxes;

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

    g_hitboxes.push_back(hb);
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

void HitboxSystem_Update(float dt)
{
//    auto& world = GetCollisionWorld();
//
//    for (size_t i = 0; i < g_hitboxes.size(); )
//    {
//        ActiveHitbox& hb = g_hitboxes[i];
//        hb.remainingTime -= dt;
//
//        // 更新 AABB 位置（跟随 owner）
//        ColliderBase* col = world.GetCollider(hb.colliderId);
//        if (col && col->shape.type == ColliderShapeType::AABB)
//        {
//            XMFLOAT3 pos, fwd, right, up;
//            QueryOwnerTransform(hb.owner, pos, fwd, right, up);
//
//            // worldPos = pos + localOffset.x * right + localOffset.y * up + localOffset.z * fwd
//            XMVECTOR P = XMLoadFloat3(&pos);
//            XMVECTOR F = XMLoadFloat3(&fwd);
//            XMVECTOR R = XMLoadFloat3(&right);
//            XMVECTOR U = XMLoadFloat3(&up);
//
//            XMVECTOR offset =
//                R * hb.localOffset.x +
//                U * hb.localOffset.y +
//                F * hb.localOffset.z;
//
//            XMVECTOR centerV = P + offset;
//            XMFLOAT3 center;
//            XMStoreFloat3(&center, centerV);
//
//            const XMFLOAT3& h = hb.halfSize;
//            col->shape.aabb.min = {
//                center.x - h.x,
//                center.y - h.y,
//                center.z - h.z
//            };
//            col->shape.aabb.max = {
//                center.x + h.x,
//                center.y + h.y,
//                center.z + h.z
//            };
//        }
//
//        // 寿命结束 → 删除 collider + 从列表移除
//        if (hb.remainingTime <= 0.0f || !col)
//        {
//            if (hb.colliderId >= 0) {
//                world.UnregisterCollider(hb.colliderId);
//            }
//            g_hitboxes.erase(g_hitboxes.begin() + i);
//        }
//        else {
//            ++i;
//        }
//
//        // ---- 4) 处理 Hitbox ↔ Hurtbox 命中 → 转成 HitContact → 交给 HitEvent_Dispatch ----
//        {
//            std::vector<CollisionPair> pairs;
//            world.Step(pairs);   // 这里返回的是 ColliderBase* 的组合
//
//            for (const CollisionPair& cp : pairs)
//            {
//                ColliderBase* colA = cp.a;
//                ColliderBase* colB = cp.b;
//                if (!colA || !colB) continue;
//
//                const ColliderBase* hitCol = nullptr;
//                const ColliderBase* hurtCol = nullptr;
//
//                // 判定哪一边是 Hitbox、哪一边是 Hurtbox
//                if (colA->category == ColliderCategory::Hitbox &&
//                    colB->category == ColliderCategory::Hurtbox)
//                {
//                    hitCol = colA;
//                    hurtCol = colB;
//                }
//                else if (colB->category == ColliderCategory::Hitbox &&
//                    colA->category == ColliderCategory::Hurtbox)
//                {
//                    hitCol = colB;
//                    hurtCol = colA;
//                }
//                else
//                {
//                    continue; // 其他组合一律忽略
//                }
//
//                // 通过 collider 指针找到我们自己的 ActiveHitbox
//                ActiveHitbox* hb = FindHitboxByCollider(hitCol);
//                if (!hb) continue;
//
//                // ★ 新增：已经“死亡/被消耗”的 HitBox 不再触发
//                if (hb->remainingTime <= 0.0f)
//                {
//                    continue;
//                }
//
//                // 计算一个大致的命中位置：
//// 这里简单用 HurtBox AABB 的中心点（以后你可以改成更精确的 HitBox 面/点）
//                DirectX::XMFLOAT3 hitPos{};
//                if (hurtCol->shape.type == ColliderShapeType::AABB)
//                {
//                    const auto& aabb = hurtCol->shape.aabb;
//                    hitPos.x = 0.5f * (aabb.min.x + aabb.max.x);
//                    hitPos.y = 0.5f * (aabb.min.y + aabb.max.y);
//                    hitPos.z = 0.5f * (aabb.min.z + aabb.max.z);
//                }
//
//                // 构造 HitContact
//                HitContact contact{};
//                contact.attackerOwner = hb->owner;        // 或 hb->owner，看你 struct 的命名
//                contact.victimOwner = hurtCol->userPtr;      // HurtBox 上事先填了 Player/Boss 的 hurt-token
//                contact.damage = hb->damage;       // 目前先只带伤害
//
//                // 让 HitEvent 告诉我们，这次算不算“真正 hit”
//                const bool consumed = HitEvent_Dispatch(contact);
//
//                if (consumed)
//                {
//                    // 只有真正命中（比如 Boss→Player / Player→Boss）才消耗掉 hitbox
//                    hb->remainingTime = 0.0f;
//                }
//            }
//        }
//    }
    auto& world = GetCollisionWorld();

    // -----------------------------
    // 1) Tick + 跟随 owner 更新 AABB
    //    同时构建 “ColliderBase* -> ActiveHitbox*” 的快速映射
    // -----------------------------
    std::unordered_map<const ColliderBase*, ActiveHitbox*> hitMap;
    hitMap.reserve(g_hitboxes.size() * 2);

    for (auto& hb : g_hitboxes)
    {
        // 先扣寿命
        hb.remainingTime -= dt;

        // 已到期：同帧立刻注销 collider（避免本帧还参与 Step）
        if (hb.remainingTime <= 0.0f)
        {
            if (hb.colliderId >= 0)
            {
                world.UnregisterCollider(hb.colliderId);
                hb.colliderId = -1;
            }
            continue;
        }

        // 无 collider 的也跳过（后面 cleanup 会移除）
        if (hb.colliderId < 0)
            continue;

        ColliderBase* col = world.GetCollider(hb.colliderId);
        if (!col)
        {
            // world 里已经没了，标记死亡（cleanup 会移除）
            hb.remainingTime = 0.0f;
            hb.colliderId = -1;
            continue;
        }

        // 记录映射（用于 pairs 快速找到 hb）
        hitMap[col] = &hb;

        // 更新 AABB（跟随 owner）
        if (col->shape.type == ColliderShapeType::AABB)
        {
            XMFLOAT3 pos, fwd, right, up;
            QueryOwnerTransform(hb.owner, pos, fwd, right, up);

            XMVECTOR P = XMLoadFloat3(&pos);
            XMVECTOR F = XMLoadFloat3(&fwd);
            XMVECTOR R = XMLoadFloat3(&right);
            XMVECTOR U = XMLoadFloat3(&up);

            XMVECTOR offset =
                R * hb.localOffset.x +
                U * hb.localOffset.y +
                F * hb.localOffset.z;

            XMVECTOR centerV = P + offset;
            XMFLOAT3 center;
            XMStoreFloat3(&center, centerV);

            const XMFLOAT3& h = hb.halfSize;
            col->shape.aabb.min = { center.x - h.x, center.y - h.y, center.z - h.z };
            col->shape.aabb.max = { center.x + h.x, center.y + h.y, center.z + h.z };
        }
    }

    // -----------------------------
    // 2) Step 一次 + 同帧去重 + consumed 同帧立刻注销
    //    说明：为了避免 pairs 中的指针在遍历中变成悬空指针，
    //          这里对 consumed 的“逻辑失效”是立即的，
    //          但真正 Unregister 放到遍历 pairs 之后立即执行（仍然是同帧）。
    // -----------------------------
    std::vector<CollisionPair> pairs;
    world.Step(pairs);

    std::unordered_set<const ColliderBase*> consumedHitCols;
    consumedHitCols.reserve(16);

    std::vector<int> toUnregister;
    toUnregister.reserve(16);

    for (const CollisionPair& cp : pairs)
    {
        const ColliderBase* colA = cp.a;
        const ColliderBase* colB = cp.b;
        if (!colA || !colB) continue;

        // 同帧去重：这个 hitbox 已经 consumed 的话，本帧后续 pairs 全部忽略（且不需要再 deref）
        if (consumedHitCols.find(colA) != consumedHitCols.end() ||
            consumedHitCols.find(colB) != consumedHitCols.end())
        {
            continue;
        }

        const ColliderBase* hitCol = nullptr;
        const ColliderBase* hurtCol = nullptr;

        // 只处理 Hitbox ↔ Hurtbox
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

        // 通过 hitMap 快速定位 ActiveHitbox
        auto it = hitMap.find(hitCol);
        if (it == hitMap.end())
            continue;

        ActiveHitbox* hb = it->second;
        if (!hb)
            continue;

        // 已死亡 / 已消耗：跳过（这是第二层保险）
        if (hb->remainingTime <= 0.0f)
            continue;

        // 构造 HitContact
        HitContact contact{};
        contact.attackerOwner = hb->owner;
        contact.victimOwner = hurtCol->userPtr;
        contact.damage = hb->damage;

        // 询问业务层：是否算“有效命中”并消耗 Hitbox
        const bool consumed = HitEvent_Dispatch(contact);

        if (consumed)
        {
            // ★ 同帧立刻失效（去重核心）
            hb->remainingTime = 0.0f;
            consumedHitCols.insert(hitCol);

            // ★ 同帧立刻注销：为了避免 pairs 中后续出现悬空指针，
            //   这里先记录 id，遍历 pairs 结束后立刻 Unregister（仍为同帧）
            if (hb->colliderId >= 0)
            {
                toUnregister.push_back(hb->colliderId);
                hb->colliderId = -1; // 防止后续逻辑再使用
            }
        }
    }

    // pairs 遍历结束后：同帧立刻注销 collider
    for (int id : toUnregister)
    {
        world.UnregisterCollider(id);
    }

    // -----------------------------
    // 3) cleanup：移除死亡 hitbox（不会再 Step 多次）
    // -----------------------------
    for (size_t i = 0; i < g_hitboxes.size(); )
    {
        ActiveHitbox& hb = g_hitboxes[i];

        // colliderId < 0 基本意味着已注销/异常丢失；remainingTime<=0 代表死亡
        const bool dead = (hb.remainingTime <= 0.0f) || (hb.colliderId < 0);

        if (dead)
        {
            // 如果还有残留 collider（理论上不该），兜底注销
            if (hb.colliderId >= 0)
            {
                world.UnregisterCollider(hb.colliderId);
            }
            g_hitboxes.erase(g_hitboxes.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}
