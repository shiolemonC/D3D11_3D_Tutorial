#include "hitbox_system.h"
#include "player.h"          // 先只支持 Player，之后可以扩展到 Enemy
#include "boss.h"
#include <algorithm>

using namespace DirectX;

static std::vector<ActiveHitbox> g_hitboxes;

// 小工具：从 ColliderCategory 得到 bit
static unsigned int CategoryBit(ColliderCategory cat)
{
    return 1u << static_cast<unsigned int>(cat);
}

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
    }
}
