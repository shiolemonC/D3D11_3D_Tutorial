#include "particle_system.h"

#include <cstdlib>
#include <cmath>
#include <algorithm>

#include "billboard.h"
#include "direct3d.h"
#include "sampler.h"
#include "camera.h"

using namespace DirectX;

static constexpr int MAX_PARTICLES = 2048;
static constexpr float EPS = 1e-5f;

struct Particle
{
    bool alive = false;

    XMFLOAT3 pos{};
    XMFLOAT3 vel{};

    float age = 0.0f;
    float life = 0.0f;

    float size0 = 0.1f;
    float size1 = 0.1f;

    XMFLOAT4 c0{ 1,1,1,1 };
    XMFLOAT4 c1{ 1,1,1,0 };

    float gravityScale = 0.0f;
    float drag = 0.0f;

    int texId = -1;
    VfxBlend blend = VfxBlend::Alpha;

    XMFLOAT2 uvScale{ 1,1 };
    XMFLOAT2 uvOffset{ 0,0 };

    float rotRad = 0.0f;      // 缓存角，速度很小时避免抖
    float streakMul = 0.0f;   // 拉伸强度（0=不开）
    float streakMax = 1.0f;   // 最大拉伸倍率（>=1）
    float rotationBias = 0.0f;// 贴图默认方向修正（可选）
    bool  alignToVelocity = true;

};

static Particle g_particles[MAX_PARTICLES];

static inline float rand01()
{
    return (float)std::rand() / (float)RAND_MAX;
}

static inline float randRange(float a, float b)
{
    return a + (b - a) * rand01();
}

static inline int randRangeI(int a, int b)
{
    if (b < a) { int tmp = a; a = b; b = tmp; }
    int span = (b - a) + 1;
    return a + (std::rand() % span);
}

static inline XMFLOAT3 normalizeSafe(const XMFLOAT3& v, const XMFLOAT3& fallback)
{
    float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
    if (len2 < EPS) return fallback;
    float inv = 1.0f / std::sqrt(len2);
    return { v.x * inv, v.y * inv, v.z * inv };
}

static inline XMFLOAT3 cross(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float dot(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static XMFLOAT3 randomOnSphere()
{
    // Uniform direction on sphere
    float z = randRange(-1.0f, 1.0f);
    float a = randRange(0.0f, XM_2PI);
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return { r * std::cos(a), r * std::sin(a), z };
}

static XMFLOAT3 randomInCone(const XMFLOAT3& axisN, float angleRad)
{
    // Sample uniformly over a cone cap.
    // cosTheta in [cos(angle), 1]
    float u = rand01();
    float v = rand01();

    float cosMin = std::cos(angleRad);
    float cosTheta = cosMin + (1.0f - cosMin) * u;
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = XM_2PI * v;

    // local (cone axis = +Z)
    float lx = std::cos(phi) * sinTheta;
    float ly = std::sin(phi) * sinTheta;
    float lz = cosTheta;

    // Build orthonormal basis around axisN
    XMFLOAT3 up = (std::fabs(axisN.y) < 0.99f) ? XMFLOAT3{ 0,1,0 } : XMFLOAT3{ 1,0,0 };
    XMFLOAT3 tangent = normalizeSafe(cross(up, axisN), { 1,0,0 });
    XMFLOAT3 bitan = cross(axisN, tangent);

    // world = tangent*lx + bitan*ly + axis*lz
    return {
        tangent.x * lx + bitan.x * ly + axisN.x * lz,
        tangent.y * lx + bitan.y * ly + axisN.y * lz,
        tangent.z * lx + bitan.z * ly + axisN.z * lz
    };
}

static int allocParticle()
{
    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        if (!g_particles[i].alive)
            return i;
    }
    return -1;
}

static inline XMFLOAT4 lerp4(const XMFLOAT4& a, const XMFLOAT4& b, float t)
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

bool ParticleSystem_Initialize()
{
    for (int i = 0; i < MAX_PARTICLES; ++i) g_particles[i] = Particle{};
    return true;
}

void ParticleSystem_Finalize()
{
    // no GPU resource in this module (Billboard/Texture are handled elsewhere)
}

void ParticleSystem_Spawn(VfxId id, const XMFLOAT3& position, const XMFLOAT3& direction)
{

    const VfxPreset& preset = VfxConfig_Get(id);

    if (preset.texId < 0)
        return;

    XMFLOAT3 axis = normalizeSafe(direction, { 0,1,0 });

    int count = randRangeI(preset.burstCount.minV, preset.burstCount.maxV);
    for (int n = 0; n < count; ++n)
    {
        int idx = allocParticle();
        if (idx < 0) break;

        Particle& p = g_particles[idx];
        p.alive = true;

        // small random emitter offset (helps look less "point-like")
        XMFLOAT3 emitterJitter = randomOnSphere();
        float jitterR = randRange(0.0f, 0.04f);
        p.pos = {
            position.x + emitterJitter.x * jitterR,
            position.y + emitterJitter.y * jitterR,
            position.z + emitterJitter.z * jitterR
        };

        XMFLOAT3 dirN{};
        if (preset.useCone)
        {
            float angleRad = XMConvertToRadians(preset.coneAngleDeg);
            dirN = randomInCone(axis, angleRad);
        }
        else
        {
            dirN = randomOnSphere();
        }

        float spd = randRange(preset.speed.minV, preset.speed.maxV);
        p.vel = { dirN.x * spd, dirN.y * spd, dirN.z * spd };

        p.age = 0.0f;
        p.life = randRange(preset.lifetime.minV, preset.lifetime.maxV);

        p.size0 = randRange(preset.sizeStart.minV, preset.sizeStart.maxV);
        p.size1 = randRange(preset.sizeEnd.minV, preset.sizeEnd.maxV);

        p.c0 = preset.colorStart;
        p.c1 = preset.colorEnd;

        p.gravityScale = preset.gravityScale;
        p.drag = preset.drag;

        p.texId = preset.texId;
        p.blend = preset.blend;

        p.uvScale = preset.uvScale;
        p.uvOffset = preset.uvOffset;

        p.alignToVelocity = preset.alignToVelocity; // 火花开，血默认关
        p.rotationBias = preset.rotationBias;                                // 需要时再调 ±90°/180°
        p.streakMul = preset.streakMul; // 火花拉伸强度
        p.streakMax = preset.streakMax;                                   // 最长 4 倍
        p.rotRad = preset.rotRad;
    }
}

void ParticleSystem_Update(double dt)
{
    float fdt = (float)dt;

    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        Particle& p = g_particles[i];
        if (!p.alive) continue;

        p.age += fdt;
        if (p.age >= p.life || p.life <= EPS)
        {
            p.alive = false;
            continue;
        }

        // gravity (Y up)
        if (p.gravityScale != 0.0f)
        {
            p.vel.y += (-9.8f * p.gravityScale) * fdt;
        }

        // simple drag
        if (p.drag > 0.0f)
        {
            float damp = 1.0f / (1.0f + p.drag * fdt);
            p.vel.x *= damp;
            p.vel.y *= damp;
            p.vel.z *= damp;
        }

        p.pos.x += p.vel.x * fdt;
        p.pos.y += p.vel.y * fdt;
        p.pos.z += p.vel.z * fdt;
    }
}

static void drawBlend(VfxBlend blend)
{
    // Camera_GetMatrix() 在你工程里是 View（世界->相机）
    XMFLOAT4X4 viewF = Camera_GetMatrix();
    XMMATRIX view = XMLoadFloat4x4(&viewF);

    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        Particle& p = g_particles[i];   // ★ 注意：改成非 const，方便缓存 rotRad
        if (!p.alive) continue;
        if (p.blend != blend) continue;

        float t = (p.life > EPS) ? (p.age / p.life) : 1.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        float baseSize = p.size0 + (p.size1 - p.size0) * t;
        XMFLOAT4 color = lerp4(p.c0, p.c1, t);

        // ---- 屏幕对齐角度（Option A）----
        float theta = p.rotRad;

        // 世界速度 -> 视空间速度
        XMVECTOR vW = XMLoadFloat3(&p.vel);
        XMVECTOR vV = XMVector3TransformNormal(vW, view);
        float vx = XMVectorGetX(vV);
        float vy = XMVectorGetY(vV);
        float vxy2 = vx * vx + vy * vy;

        if (p.alignToVelocity && vxy2 > 1e-6f)
        {
            // 以“屏幕向上(+Y)”为 0°，向右(+X)为 +90°
            theta = std::atan2(vy, vx) + p.rotationBias;
            p.rotRad = theta; // 缓存，避免速度很小时抖动
        }

        // ---- Streak：沿速度方向拉长 ----
        // 只用屏幕平面速度(vx,vy)来决定拉伸（朝向/远离相机时 streak 会自然变短）
        float speedXY = (vxy2 > 1e-6f) ? std::sqrt(vxy2) : 0.0f;

        float width = baseSize;
        float length = baseSize;

        if (p.streakMul > 0.0f)
        {
            float stretch = 1.0f + p.streakMul * speedXY;

            // 可选：随时间衰减，让拖尾主要集中在初期（更像火花）
            // stretch *= (1.0f - 0.7f * t);

            if (p.streakMax < 1.0f) p.streakMax = 1.0f;
            if (stretch > p.streakMax) stretch = p.streakMax;

            length = baseSize * stretch;
        }

        // 注意：我们让贴图“竖直方向(+Y)”对齐速度方向，所以把“长边”放到 scale.y
        Billboard_DrawExRot(
            p.texId,
            p.pos,
            //{ width, length },
            { length, width },
            color,
            theta,
            p.uvScale,
            p.uvOffset
        );
    }
}

void ParticleSystem_DrawWorld()
{

    // Particles are transparent most of the time:
    // - depth test ON (so they don't draw through walls)
    // - depth write OFF (so they don't "punch holes" in later transparent draws)
    Direct3D_SetDepthTestNoWrite();
    Sampler_SetFillterLinear();

    Direct3D_SetAlphaBlendAdd();
    drawBlend(VfxBlend::Add);

    Direct3D_SetAlphaBlendTransparent();
    drawBlend(VfxBlend::Alpha);

    // Restore default states (your pipeline often assumes depth write enabled after 3D meshes)
    Direct3D_SetDepthEnable(true);
    Direct3D_SetAlphaBlendTransparent();
}
