#include "boss_projectile.h"

#include "boss.h"
#include "collider_system.h"
#include "cube.h"
#include "direct3d.h"
#include "hit_event.h"
#include "player.h"
#include "shader3d.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <Windows.h>

using namespace DirectX;

enum class BossProjectileVisualType
{
    Sphere,
    Particle,
    SpriteEffect,
    Billboard,
    Model,
};

struct BossProjectileLogicDesc
{
    float speed = 10.0f;
    float collisionRadius = 0.35f;
    float lifeSec = 3.0f;
    int damage = 10;
    HitLevel hitLevel = HitLevel::Light;
    float knockbackDistance = 0.0f;
    bool destroyOnHit = true;
};

struct BossProjectileVisualDesc
{
    BossProjectileVisualType type = BossProjectileVisualType::Sphere;
    float visualRadius = 0.35f;
    XMFLOAT4 color{ 0.55f, 0.85f, 1.0f, 1.0f };
    int resourceId = -1;
};

struct BossProjectilePattern
{
    BossProjectileLogicDesc logic;
    BossProjectileVisualDesc visual;
};

struct BossProjectile
{
    BossProjectilePatternId patternId = BossProjectilePatternId::StraightShot;
    XMFLOAT3 position{ 0,0,0 };
    XMFLOAT3 velocity{ 0,0,0 };
    float collisionRadius = 0.35f;
    float lifeRemain = 0.0f;
    int damage = 0;
    HitLevel hitLevel = HitLevel::Light;
    float knockbackDistance = 0.0f;
    bool destroyOnHit = true;
    void* ownerToken = nullptr;
    BossProjectileVisualDesc visual;
};

static std::vector<BossProjectile> s_projectiles;

struct ProjectileSphereVertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
    XMFLOAT2 texcoord;
};

static ID3D11Buffer* s_sphereVertexBuffer = nullptr;
static ID3D11Buffer* s_sphereIndexBuffer = nullptr;
static int s_sphereIndexCount = 0;
static int s_whiteTextureId = -1;

static const BossProjectilePattern& GetPattern(BossProjectilePatternId id)
{
    static const BossProjectilePattern kStraightShot
    {
        {
            12.0f,            // speed
            0.45f,            // collisionRadius
            4.0f,             // lifeSec
            18,               // damage
            HitLevel::Medium, // hitLevel
            1.2f,             // knockbackDistance
            true              // destroyOnHit
        },
        {
            BossProjectileVisualType::Sphere,
            0.45f,                            // visualRadius
            { 0.55f, 0.85f, 1.0f, 1.0f },     // light blue
            -1
        }
    };

    switch (id)
    {
    case BossProjectilePatternId::StraightShot:
    default:
        return kStraightShot;
    }
}

static void CreateProjectileSphereMesh()
{
    if (s_sphereVertexBuffer && s_sphereIndexBuffer) return;

    ID3D11Device* device = Direct3D_GetDevice();
    if (!device) return;

    constexpr int kSlices = 24;
    constexpr int kStacks = 12;

    std::vector<ProjectileSphereVertex> vertices;
    std::vector<std::uint16_t> indices;
    vertices.reserve((kSlices + 1) * (kStacks + 1));
    indices.reserve(kSlices * kStacks * 6);

    for (int stack = 0; stack <= kStacks; ++stack)
    {
        const float v = static_cast<float>(stack) / static_cast<float>(kStacks);
        const float phi = XM_PI * v;
        const float y = std::cos(phi);
        const float r = std::sin(phi);

        for (int slice = 0; slice <= kSlices; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(kSlices);
            const float theta = XM_2PI * u;
            const float x = r * std::cos(theta);
            const float z = r * std::sin(theta);

            ProjectileSphereVertex vertex{};
            vertex.position = { x, y, z };
            vertex.normal = { x, y, z };
            vertex.color = { 1, 1, 1, 1 };
            vertex.texcoord = { u, v };
            vertices.push_back(vertex);
        }
    }

    for (int stack = 0; stack < kStacks; ++stack)
    {
        for (int slice = 0; slice < kSlices; ++slice)
        {
            const int row0 = stack * (kSlices + 1);
            const int row1 = (stack + 1) * (kSlices + 1);

            const std::uint16_t i0 = static_cast<std::uint16_t>(row0 + slice);
            const std::uint16_t i1 = static_cast<std::uint16_t>(row1 + slice);
            const std::uint16_t i2 = static_cast<std::uint16_t>(row0 + slice + 1);
            const std::uint16_t i3 = static_cast<std::uint16_t>(row1 + slice + 1);

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = static_cast<UINT>(sizeof(ProjectileSphereVertex) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertices.data();
    device->CreateBuffer(&bd, &sd, &s_sphereVertexBuffer);

    bd.ByteWidth = static_cast<UINT>(sizeof(std::uint16_t) * indices.size());
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem = indices.data();
    device->CreateBuffer(&bd, &sd, &s_sphereIndexBuffer);

    s_sphereIndexCount = static_cast<int>(indices.size());
}

static void DrawProjectileSphere(const BossProjectile& p)
{
    if (!s_sphereVertexBuffer || !s_sphereIndexBuffer || s_sphereIndexCount <= 0)
    {
        return;
    }

    ID3D11DeviceContext* context = Direct3D_GetContext();
    if (!context) return;

    Shader3d_Begin();
    Shader3d_SetColor(p.visual.color);

    if (s_whiteTextureId >= 0)
    {
        Texture_SetTexture(s_whiteTextureId);
    }

    UINT stride = sizeof(ProjectileSphereVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &s_sphereVertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(s_sphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const float r = p.visual.visualRadius;
    const XMMATRIX world =
        XMMatrixScaling(r, r, r) *
        XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
    Shader3d_SetWorldMatrix(world);

    context->DrawIndexed(s_sphereIndexCount, 0, 0);
}

static XMFLOAT3 NormalizeOrFallback(const XMFLOAT3& v, const XMFLOAT3& fallback)
{
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq <= 1e-6f)
    {
        return fallback;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    return { v.x * invLen, v.y * invLen, v.z * invLen };
}

static XMFLOAT3 AABBCenter(const BOXAABB& aabb)
{
    return {
        0.5f * (aabb.min.x + aabb.max.x),
        0.5f * (aabb.min.y + aabb.max.y),
        0.5f * (aabb.min.z + aabb.max.z)
    };
}

static bool SphereOverlapsAABB(const XMFLOAT3& center, float radius, const BOXAABB& aabb)
{
    const float cx = std::max(aabb.min.x, std::min(center.x, aabb.max.x));
    const float cy = std::max(aabb.min.y, std::min(center.y, aabb.max.y));
    const float cz = std::max(aabb.min.z, std::min(center.z, aabb.max.z));

    const float dx = center.x - cx;
    const float dy = center.y - cy;
    const float dz = center.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= radius * radius;
}

static bool ProjectileHitsPlayer(const BossProjectile& p)
{
    const int hurtId = Player_GetHurtColliderId();
    if (hurtId < 0) return false;

    const ColliderBase* hurt = GetCollisionWorld().GetCollider(hurtId);
    if (!hurt || !hurt->active) return false;

    if (hurt->shape.type == ColliderShapeType::AABB)
    {
        return SphereOverlapsAABB(p.position, p.collisionRadius, hurt->shape.aabb);
    }

    if (hurt->shape.type == ColliderShapeType::Sphere)
    {
        const XMFLOAT3 c = hurt->shape.sphere.center;
        const float r = hurt->shape.sphere.radius + p.collisionRadius;
        const float dx = p.position.x - c.x;
        const float dy = p.position.y - c.y;
        const float dz = p.position.z - c.z;
        return (dx * dx + dy * dy + dz * dz) <= r * r;
    }

    return false;
}

static void DispatchPlayerHit(BossProjectile& p)
{
    const ColliderBase* hurt = GetCollisionWorld().GetCollider(Player_GetHurtColliderId());
    if (!hurt) return;

    XMFLOAT3 victimPos = Player_GetPosition();
    if (hurt->shape.type == ColliderShapeType::AABB)
    {
        victimPos = AABBCenter(hurt->shape.aabb);
    }
    else if (hurt->shape.type == ColliderShapeType::Sphere)
    {
        victimPos = hurt->shape.sphere.center;
    }

    HitContact c{};
    c.attackerOwner = p.ownerToken ? p.ownerToken : Boss_GetHitboxOwnerToken();
    c.victimOwner = Player_GetHurtboxOwnerToken();
    c.damage = p.damage;
    c.level = p.hitLevel;
    c.hitPos = p.position;
    c.knockbackDistance = p.knockbackDistance;
    c.attackerPos = p.position;
    c.victimPos = victimPos;

    const bool consumed = HitEvent_Dispatch(c);
    if (consumed && p.destroyOnHit)
    {
        p.lifeRemain = 0.0f;
    }
}

void BossProjectile_Initialize()
{
    s_projectiles.clear();
    CreateProjectileSphereMesh();
    s_whiteTextureId = Texture_Load(L"resources/white.png");
}

void BossProjectile_Finalize()
{
    s_projectiles.clear();
    SAFE_RELEASE(s_sphereVertexBuffer);
    SAFE_RELEASE(s_sphereIndexBuffer);
    s_sphereIndexCount = 0;
    s_whiteTextureId = -1;
}

void BossProjectile_Clear()
{
    s_projectiles.clear();
}

void BossProjectile_Fire(
    BossProjectilePatternId patternId,
    const XMFLOAT3& spawnPos,
    const XMFLOAT3& targetPos,
    void* ownerToken)
{
    const BossProjectilePattern& pattern = GetPattern(patternId);

    XMFLOAT3 dir{
        targetPos.x - spawnPos.x,
        targetPos.y - spawnPos.y,
        targetPos.z - spawnPos.z
    };
    dir = NormalizeOrFallback(dir, Boss_GetForward());

    BossProjectile p{};
    p.patternId = patternId;
    p.position = spawnPos;
    p.velocity = {
        dir.x * pattern.logic.speed,
        dir.y * pattern.logic.speed,
        dir.z * pattern.logic.speed
    };
    p.collisionRadius = pattern.logic.collisionRadius;
    p.lifeRemain = pattern.logic.lifeSec;
    p.damage = pattern.logic.damage;
    p.hitLevel = pattern.logic.hitLevel;
    p.knockbackDistance = pattern.logic.knockbackDistance;
    p.destroyOnHit = pattern.logic.destroyOnHit;
    p.ownerToken = ownerToken;
    p.visual = pattern.visual;

    s_projectiles.push_back(p);
    OutputDebugStringA("[BossProjectile] Fire StraightShot\n");
}

void BossProjectile_Update(float dt)
{
    if (dt <= 0.0f) return;

    for (auto& p : s_projectiles)
    {
        if (p.lifeRemain <= 0.0f) continue;

        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        p.lifeRemain -= dt;

        if (p.lifeRemain > 0.0f && ProjectileHitsPlayer(p))
        {
            DispatchPlayerHit(p);
        }
    }

    s_projectiles.erase(
        std::remove_if(s_projectiles.begin(), s_projectiles.end(),
            [](const BossProjectile& p)
            {
                return p.lifeRemain <= 0.0f;
            }),
        s_projectiles.end());
}

void BossProjectile_Draw()
{
    for (const auto& p : s_projectiles)
    {
        switch (p.visual.type)
        {
        case BossProjectileVisualType::Sphere:
            DrawProjectileSphere(p);
            break;
        default:
            DrawProjectileSphere(p);
            break;
        }
    }
}

void BossProjectile_DebugDraw()
{
    for (const auto& p : s_projectiles)
    {
        const float d = p.collisionRadius * 2.0f;
        const XMMATRIX world =
            XMMatrixScaling(d, d, d) *
            XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
        Cube_DrawWireframe(world, { 1.0f, 0.35f, 0.05f, 1.0f });
    }
}
