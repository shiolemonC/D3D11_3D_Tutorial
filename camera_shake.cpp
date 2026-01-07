#include "camera_shake.h"
#include <vector>
#include <algorithm>
#include <cmath>

using namespace DirectX;

struct ShakeInstance
{
    float magnitude = 0.0f;
    float durationSec = 0.0f;
    float elapsedSec = 0.0f;
    CameraShakeMode mode = CameraShakeMode::Both;
    int priority = 0;
    std::uint32_t seed = 0;
};

static std::vector<ShakeInstance> g_shakes;
static std::uint32_t g_seedCounter = 0xA341316C;

// 你可以调：抖动采样频率（越高越细腻，但也更“嗡嗡”）
static constexpr float kShakeFreqHz = 25.0f;

// 叠加后 clamp：允许一定叠加，但不让镜头飞走
static constexpr float kHardClampMax = 0.75f; // 米，硬上限（兜底）

static inline float Saturate(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float SmoothStep01(float t)
{
    t = Saturate(t);
    return t * t * (3.0f - 2.0f * t);
}

static inline float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline std::uint32_t HashU32(std::uint32_t x)
{
    // 轻量 hash，稳定可复现
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

static inline float HashSigned01(std::uint32_t seed, std::uint32_t salt)
{
    std::uint32_t h = HashU32(seed ^ (salt * 0x9E3779B9u));
    // 取 24bit 转 [0,1)
    float v = (h & 0x00FFFFFFu) / float(0x01000000u);
    return v * 2.0f - 1.0f; // [-1,1)
}

void CameraShake_Clear()
{
    g_shakes.clear();
}

void CameraShake_Update(float dt)
{
    if (g_shakes.empty()) return;

    for (auto& s : g_shakes)
    {
        s.elapsedSec += dt;
    }

    g_shakes.erase(
        std::remove_if(g_shakes.begin(), g_shakes.end(),
            [](const ShakeInstance& s)
            {
                return (s.durationSec <= 0.0f) || (s.elapsedSec >= s.durationSec);
            }),
        g_shakes.end());
}

void CameraShake_Push(float magnitude, float durationSec, CameraShakeMode mode, int priority)
{
    if (magnitude <= 0.0f) return;
    if (durationSec <= 0.0f) return;

    ShakeInstance s{};
    s.magnitude = magnitude;
    s.durationSec = durationSec;
    s.elapsedSec = 0.0f;
    s.mode = mode;
    s.priority = priority;

    // seed：用计数器滚动即可
    g_seedCounter = HashU32(g_seedCounter + 0x9E3779B9u);
    s.seed = g_seedCounter;

    // 简单上限：最多保留 16 个（太多也没意义）
    constexpr int kMaxShakes = 16;
    if ((int)g_shakes.size() >= kMaxShakes)
    {
        // 移除“优先级最低且最弱”的一个
        int bestIdx = -1;
        for (int i = 0; i < (int)g_shakes.size(); ++i)
        {
            if (bestIdx < 0) { bestIdx = i; continue; }
            const auto& a = g_shakes[i];
            const auto& b = g_shakes[bestIdx];

            if (a.priority < b.priority) bestIdx = i;
            else if (a.priority == b.priority && a.magnitude < b.magnitude) bestIdx = i;
        }
        if (bestIdx >= 0) g_shakes.erase(g_shakes.begin() + bestIdx);
    }

    g_shakes.push_back(s);
}

DirectX::XMFLOAT3 CameraShake_GetOffset(const XMFLOAT3& eye, const XMFLOAT3& target)
{
    if (g_shakes.empty()) return XMFLOAT3{ 0,0,0 };

    // 以“当前视线”建立相机局部轴：right / upOrtho
    XMFLOAT3 fwd{ target.x - eye.x, target.y - eye.y, target.z - eye.z };
    float fl = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (fl < 1e-5f) return XMFLOAT3{ 0,0,0 };
    fwd.x /= fl; fwd.y /= fl; fwd.z /= fl;

    XMFLOAT3 worldUp{ 0.0f, 1.0f, 0.0f };

    XMVECTOR F = XMLoadFloat3(&fwd);
    XMVECTOR U = XMLoadFloat3(&worldUp);
    XMVECTOR R = XMVector3Cross(U, F);
    float rl = XMVectorGetX(XMVector3Length(R));
    if (rl < 1e-5f) {
        // 退化兜底
        R = XMVectorSet(1, 0, 0, 0);
    }
    else {
        R = XMVector3Normalize(R);
    }

    XMVECTOR UpOrtho = XMVector3Normalize(XMVector3Cross(F, R)); // 正交 up

    XMFLOAT3 right{}, up{};
    XMStoreFloat3(&right, R);
    XMStoreFloat3(&up, UpOrtho);

    // 叠加所有 shake（在相机平面上：x=right, y=up）
    float sumX = 0.0f;
    float sumY = 0.0f;

    float maxCurrentMag = 0.0f; // 用于动态 clamp（允许一定叠加）
    for (const auto& s : g_shakes)
    {
        const float dur = s.durationSec;
        if (dur <= 0.0f) continue;

        const float t01 = Saturate(s.elapsedSec / dur);
        float env = 1.0f - t01;            // 线性衰减
        env = SmoothStep01(env);           // 平滑一点
        const float curMag = s.magnitude * env;
        if (curMag <= 0.0f) continue;

        maxCurrentMag = std::max(maxCurrentMag, curMag);

        // 用 elapsed * freq 取样：n 与 n+1 做插值（平滑噪声）
        const float phase = s.elapsedSec * kShakeFreqHz;
        const std::uint32_t n = (std::uint32_t)std::floor(phase);
        const float frac = phase - (float)n;
        const float tt = SmoothStep01(frac);

        float ax = HashSigned01(s.seed, n * 2u + 0u);
        float ay = HashSigned01(s.seed, n * 2u + 1u);
        float bx = HashSigned01(s.seed, (n + 1u) * 2u + 0u);
        float by = HashSigned01(s.seed, (n + 1u) * 2u + 1u);

        float ox = Lerp(ax, bx, tt) * curMag;
        float oy = Lerp(ay, by, tt) * curMag;

        switch (s.mode)
        {
        case CameraShakeMode::Horizontal: oy = 0.0f; break;
        case CameraShakeMode::Vertical:   ox = 0.0f; break;
        case CameraShakeMode::Both:       break;
        default: break;
        }

        sumX += ox;
        sumY += oy;
    }

    // 动态 clamp：允许一定叠加，但不无限大
    float clampDist = maxCurrentMag * 1.5f;          // 允许 1.5 倍“主 shake”叠加
    clampDist = std::max(clampDist, 0.02f);          // 太小会抖不出来
    clampDist = std::min(clampDist, kHardClampMax);  // 硬上限兜底

    // 将 (sumX,sumY) 转到世界位移
    XMFLOAT3 offset{
        right.x * sumX + up.x * sumY,
        right.y * sumX + up.y * sumY,
        right.z * sumX + up.z * sumY
    };

    float ol = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (ol > clampDist && ol > 1e-6f)
    {
        float k = clampDist / ol;
        offset.x *= k;
        offset.y *= k;
        offset.z *= k;
    }

    return offset;
}
