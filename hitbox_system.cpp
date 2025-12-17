#include "hitbox_system.h"
#include "player.h"          // 先只支持 Player，之后可以扩展到 Enemy
#include "boss.h"
#include "hit_event.h"
#include <algorithm>

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
    auto& world = GetCollisionWorld();

    for (size_t i = 0; i < g_hitboxes.size(); )
    {
        ActiveHitbox& hb = g_hitboxes[i];
        hb.remainingTime -= dt;

        // 更新 AABB 位置（跟随 owner）
        ColliderBase* col = world.GetCollider(hb.colliderId);
        if (col && col->shape.type == ColliderShapeType::AABB)
        {
            XMFLOAT3 pos, fwd, right, up;
            QueryOwnerTransform(hb.owner, pos, fwd, right, up);

            // worldPos = pos + localOffset.x * right + localOffset.y * up + localOffset.z * fwd
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
            col->shape.aabb.min = {
                center.x - h.x,
                center.y - h.y,
                center.z - h.z
            };
            col->shape.aabb.max = {
                center.x + h.x,
                center.y + h.y,
                center.z + h.z
            };
        }

        // 寿命结束 → 删除 collider + 从列表移除
        if (hb.remainingTime <= 0.0f || !col)
        {
            if (hb.colliderId >= 0) {
                world.UnregisterCollider(hb.colliderId);
            }
            g_hitboxes.erase(g_hitboxes.begin() + i);
        }
        else {
            ++i;
        }

        // ---- 4) 处理 Hitbox ↔ Hurtbox 命中 → 转成 HitContact → 交给 HitEvent_Dispatch ----
        {
            std::vector<CollisionPair> pairs;
            world.Step(pairs);   // 这里返回的是 ColliderBase* 的组合

            for (const CollisionPair& cp : pairs)
            {
                ColliderBase* colA = cp.a;
                ColliderBase* colB = cp.b;
                if (!colA || !colB) continue;

                const ColliderBase* hitCol = nullptr;
                const ColliderBase* hurtCol = nullptr;

                // 判定哪一边是 Hitbox、哪一边是 Hurtbox
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
                    continue; // 其他组合一律忽略
                }

                // 通过 collider 指针找到我们自己的 ActiveHitbox
                ActiveHitbox* hb = FindHitboxByCollider(hitCol);
                if (!hb) continue;

                // ★ 新增：已经“死亡/被消耗”的 HitBox 不再触发
                if (hb->remainingTime <= 0.0f)
                {
                    continue;
                }

                // 计算一个大致的命中位置：
// 这里简单用 HurtBox AABB 的中心点（以后你可以改成更精确的 HitBox 面/点）
                DirectX::XMFLOAT3 hitPos{};
                if (hurtCol->shape.type == ColliderShapeType::AABB)
                {
                    const auto& aabb = hurtCol->shape.aabb;
                    hitPos.x = 0.5f * (aabb.min.x + aabb.max.x);
                    hitPos.y = 0.5f * (aabb.min.y + aabb.max.y);
                    hitPos.z = 0.5f * (aabb.min.z + aabb.max.z);
                }

                // 构造 HitContact
                HitContact contact{};
                contact.attackerOwner = hb->owner;        // 或 hb->owner，看你 struct 的命名
                contact.victimOwner = hurtCol->userPtr;      // HurtBox 上事先填了 Player/Boss 的 hurt-token
                contact.damage = hb->damage;       // 目前先只带伤害

                // 让 HitEvent 告诉我们，这次算不算“真正 hit”
                const bool consumed = HitEvent_Dispatch(contact);

                if (consumed)
                {
                    // 只有真正命中（比如 Boss→Player / Player→Boss）才消耗掉 hitbox
                    hb->remainingTime = 0.0f;
                }
            }
        }
    }
}
