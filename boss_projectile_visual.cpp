#include "boss_projectile_visual.h"

#include "billboard.h"
#include "direct3d.h"
#include "sampler.h"
#include "shader3d.h"
#include "texture.h"
#include "camera.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <d3d11.h>

using namespace DirectX;

struct ProjectileSphereVertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
    XMFLOAT2 texcoord;
};

struct TextureCacheEntry
{
    std::wstring path;
    int texId = -1;
};

static ID3D11Buffer* s_sphereVertexBuffer = nullptr;
static ID3D11Buffer* s_sphereIndexBuffer = nullptr;
static int s_sphereIndexCount = 0;
static int s_whiteTextureId = -1;
static int s_defaultBillboardTextureId = -1;
static std::vector<TextureCacheEntry> s_textureCache;

static int LoadCachedTexture(const wchar_t* path)
{
    if (!path || !path[0]) return -1;

    for (const auto& entry : s_textureCache)
    {
        if (entry.path == path)
        {
            return entry.texId;
        }
    }

    TextureCacheEntry entry{};
    entry.path = path;
    entry.texId = Texture_Load(path);
    s_textureCache.push_back(entry);
    return entry.texId;
}

static int ResolveVisualTexture(const BossProjectileVisualDesc& visual)
{
    if (visual.textureId >= 0) return visual.textureId;

    const int cached = LoadCachedTexture(visual.texturePath);
    if (cached >= 0) return cached;

    return s_defaultBillboardTextureId;
}

static void ApplyBlend(VfxBlend blend)
{
    switch (blend)
    {
    case VfxBlend::Add:
        Direct3D_SetAlphaBlendAdd();
        break;
    case VfxBlend::Alpha:
    default:
        Direct3D_SetAlphaBlendTransparent();
        break;
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

static void DrawSphereVisual(const XMFLOAT3& position, const BossProjectileVisualDesc& visual)
{
    if (!s_sphereVertexBuffer || !s_sphereIndexBuffer || s_sphereIndexCount <= 0)
    {
        return;
    }

    ID3D11DeviceContext* context = Direct3D_GetContext();
    if (!context) return;

    Shader3d_Begin();
    Shader3d_SetColor(visual.color);

    if (s_whiteTextureId >= 0)
    {
        Texture_SetTexture(s_whiteTextureId);
    }

    UINT stride = sizeof(ProjectileSphereVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &s_sphereVertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(s_sphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const float r = visual.visualRadius;
    const XMMATRIX world =
        XMMatrixScaling(r, r, r) *
        XMMatrixTranslation(position.x, position.y, position.z);
    Shader3d_SetWorldMatrix(world);

    context->DrawIndexed(s_sphereIndexCount, 0, 0);
}

static float VelocityRotationInView(const XMFLOAT3& velocity)
{
    XMFLOAT4X4 viewF = Camera_GetMatrix();
    XMMATRIX view = XMLoadFloat4x4(&viewF);

    XMVECTOR vW = XMLoadFloat3(&velocity);
    XMVECTOR vV = XMVector3TransformNormal(vW, view);
    const float vx = XMVectorGetX(vV);
    const float vy = XMVectorGetY(vV);

    if (vx * vx + vy * vy <= 1e-6f)
    {
        return 0.0f;
    }

    return std::atan2(vy, vx);
}

static bool ProjectedMotionOnScreen(
    const XMFLOAT3& position,
    const XMFLOAT3& velocity,
    XMFLOAT2& outDelta)
{
    constexpr float kSampleSec = 1.0f / 30.0f;

    XMFLOAT2 uv0{};
    XMFLOAT2 uv1{};

    XMFLOAT3 next{
        position.x + velocity.x * kSampleSec,
        position.y + velocity.y * kSampleSec,
        position.z + velocity.z * kSampleSec
    };

    if (!Camera_ProjectWorldToScreenUV(position, uv0)) return false;
    if (!Camera_ProjectWorldToScreenUV(next, uv1)) return false;

    outDelta.x = uv1.x - uv0.x;
    outDelta.y = -(uv1.y - uv0.y); // UV Y is downward, billboard local Y is upward.

    return (outDelta.x * outDelta.x + outDelta.y * outDelta.y) > 1e-8f;
}

static float VelocitySpeedInViewXY(const XMFLOAT3& velocity)
{
    XMFLOAT4X4 viewF = Camera_GetMatrix();
    XMMATRIX view = XMLoadFloat4x4(&viewF);

    XMVECTOR vW = XMLoadFloat3(&velocity);
    XMVECTOR vV = XMVector3TransformNormal(vW, view);
    const float vx = XMVectorGetX(vV);
    const float vy = XMVectorGetY(vV);
    return std::sqrt(vx * vx + vy * vy);
}

static void DrawVelocityBillboardVisual(
    const XMFLOAT3& position,
    const XMFLOAT3& velocity,
    const BossProjectileVisualDesc& visual)
{
    const int texId = ResolveVisualTexture(visual);
    if (texId < 0) return;

    float length = visual.baseLength;
    float width = visual.baseWidth;

    if (visual.streakMul > 0.0f)
    {
        float stretch = 1.0f + visual.streakMul * VelocitySpeedInViewXY(velocity);
        const float streakMax = std::max(visual.streakMax, 1.0f);
        stretch = std::min(stretch, streakMax);
        length *= stretch;
    }

    XMFLOAT2 screenDelta{};
    float theta = VelocityRotationInView(velocity);
    if (ProjectedMotionOnScreen(position, velocity, screenDelta))
    {
        theta = std::atan2(screenDelta.y, screenDelta.x);
    }

    // The current projectile texture is vertical, so align its local +Y axis to motion.
    theta += visual.rotationBias - XM_PIDIV2;

    Direct3D_SetDepthTestNoWrite();
    Sampler_SetFillterLinear();
    ApplyBlend(visual.blend);

    Billboard_DrawExRot(
        texId,
        position,
        { width, length },
        visual.color,
        theta,
        visual.uvScale,
        visual.uvOffset);

    Direct3D_SetDepthEnable(true);
    Direct3D_SetAlphaBlendTransparent();
}

bool BossProjectileVisual_Initialize()
{
    CreateProjectileSphereMesh();
    s_whiteTextureId = Texture_Load(L"resources/white.png");
    s_defaultBillboardTextureId = Texture_Load(L"resources/fx/particle_spark_1.png");
    return true;
}

void BossProjectileVisual_Finalize()
{
    SAFE_RELEASE(s_sphereVertexBuffer);
    SAFE_RELEASE(s_sphereIndexBuffer);
    s_sphereIndexCount = 0;
    s_whiteTextureId = -1;
    s_defaultBillboardTextureId = -1;
    s_textureCache.clear();
}

void BossProjectileVisual_Draw(
    const XMFLOAT3& position,
    const XMFLOAT3& velocity,
    const BossProjectileVisualDesc& visual)
{
    switch (visual.type)
    {
    case BossProjectileVisualType::Sphere:
        DrawSphereVisual(position, visual);
        break;
    case BossProjectileVisualType::VelocityBillboard:
        DrawVelocityBillboardVisual(position, velocity, visual);
        break;
    case BossProjectileVisualType::ParticleTrail:
    case BossProjectileVisualType::Model:
    default:
        DrawSphereVisual(position, visual);
        break;
    }
}
