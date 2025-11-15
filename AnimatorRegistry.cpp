// AnimatorRegistry.cpp
#include "AnimatorRegistry.h"
#include "ModelSkinned.h"
#include "shader3d.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cmath>

using namespace DirectX;

// ---------------------------------
// 内部数据
// ---------------------------------
static std::vector<AnimClipDesc> gClips;     // 动作注册表
static int        gCurrent = -1;             // 当前播放索引（-1 = 无）
static XMMATRIX   gBaseWorld = XMMatrixIdentity();

// 记录加载的 mesh+skel 组合（以后可做只换 anim 的优化）
struct MeshSkelKey {
    std::wstring mesh;
    std::wstring skel;
    bool operator==(const MeshSkelKey& o) const { return mesh == o.mesh && skel == o.skel; }
};
static MeshSkelKey gLoadedKey{ L"", L"" };

// RootMotion 累计（由 Update 写入 → 被上层消费）
static XMFLOAT3 gRM_AccumPos = { 0,0,0 };
static float    gRM_AccumYaw = 0.0f;

// 基线 yaw（第一次 Play 的首帧局部 yaw0），用于入场对齐
static bool  gYawBaselineInit = false;
static float gYawBaselineRad = 0.0f;

// 小工具：应用位移到 world（注意：node-fix 在 ModelSkinned_Draw 里做）
static void ApplyWorldWithRootMotion()
{
    XMMATRIX T = XMMatrixTranslation(gRM_AccumPos.x, gRM_AccumPos.y, gRM_AccumPos.z);
    ModelSkinned_SetWorldMatrix(T * gBaseWorld);
}

// ---------------------------------
// 对外实现
// ---------------------------------
bool AnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    gClips.clear();
    gCurrent = -1;
    gBaseWorld = XMMatrixIdentity();
    gLoadedKey = MeshSkelKey{};

    gRM_AccumPos = { 0,0,0 };
    gRM_AccumYaw = 0.0f;
    gYawBaselineInit = false;
    gYawBaselineRad = 0.0f;

    return ModelSkinned_Initialize(dev, ctx);
}

void AnimatorRegistry_Finalize()
{
    gClips.clear();
    gCurrent = -1;
    // 如需 ModelSkinned 侧释放，按你的工程调用相应 finalize
}

void AnimatorRegistry_Clear()
{
    gClips.clear();
    gCurrent = -1;
}

static int FindIndex(const std::wstring& name)
{
    for (int i = 0; i < (int)gClips.size(); ++i)
        if (gClips[i].name == name) return i;
    return -1;
}

bool AnimatorRegistry_Register(const AnimClipDesc& clip)
{
    if (clip.name.empty()) return false;
    if (FindIndex(clip.name) >= 0) return false; // 去重
    gClips.push_back(clip);
    return true;
}

bool AnimatorRegistry_Has(const std::wstring& name)
{
    return FindIndex(name) >= 0;
}

const AnimClipDesc* AnimatorRegistry_Get(const std::wstring& name)
{
    int idx = FindIndex(name);
    return (idx >= 0) ? &gClips[idx] : nullptr;
}

bool AnimatorRegistry_LoadAll()
{
    // 当前实现：真正的加载在 Play 里做；这里返回 true
    return true;
}

bool AnimatorRegistry_Play(const std::wstring& name,
    bool* outChanged,
    bool overrideLoop, bool loopValue,
    bool overrideRate, float rateValue)
{
    if (outChanged) *outChanged = false;

    int idx = FindIndex(name);
    if (idx < 0) return false;

    const AnimClipDesc& clip = gClips[idx];

    // 底层加载描述
    ModelSkinnedDesc d{};
    d.meshPath = clip.meshPath;
    d.skelPath = clip.skelPath;
    d.animPath = clip.animPath;
    d.matPath = clip.matPath;
    d.baseColorTexOverride = clip.baseColorOverride;

    // 目前为稳妥：统一全量加载
    if (!ModelSkinned_Load(d)) return false;

    // ★ 新增：若本动画注册时指定了 motion-root，就在此覆盖
    if (!clip.motionRootNameUTF8.empty()) {
        ModelSkinned_SetMotionRootByName(clip.motionRootNameUTF8.c_str());
#if defined(DEBUG) || defined(_DEBUG)
        {
            char buf[200];
            sprintf_s(buf, "[Anim] Set motion-root to '%s' for clip %ls\n",
                clip.motionRootNameUTF8.c_str(), name.c_str());
            OutputDebugStringA(buf);
        }
#endif
    }

    // —— 入场对齐（基于 motion-root）——
    {
        // 1) 取首帧局部 yaw0：用于 baseline（只取第一次 Play 的序列）
        float yaw0 = 0.0f;
        if (ModelSkinned_DebugGetRootYaw_F0(&yaw0)) {
            if (!gYawBaselineInit) { gYawBaselineInit = true; gYawBaselineRad = yaw0; }

            // 2) 计算“模型空间首帧 yaw”并生成 node-fix：视觉上对齐到 baseline
            float yawModel0 = 0.0f;
            if (ModelSkinned_ComputeRootYaw_ModelSpace_FirstFrame(&yawModel0)) {
                auto wrap = [](float a) { const float PI = 3.14159265358979323846f, T = 2 * PI;
                while (a > PI)a -= T; while (a <= -PI)a += T; return a; };
                const float target = gYawBaselineRad;
                const float nodeFix = wrap(target - yawModel0);
                ModelSkinned_SetNodeYawFix(nodeFix);

#if defined(DEBUG) || defined(_DEBUG)
                char b2[196];
                sprintf_s(b2, "[Anim] Play %ls | yaw0(local)=%.1f°, yaw0(model)=%.1f°, nodeFix=%.1f° (target=%.1f°)\n",
                    name.c_str(),
                    XMConvertToDegrees(yaw0),
                    XMConvertToDegrees(yawModel0),
                    XMConvertToDegrees(nodeFix),
                    XMConvertToDegrees(target));
                OutputDebugStringA(b2);
#endif
            }
        }
    }

#if defined(DEBUG) || defined(_DEBUG)
    {
        float yaw0 = 0.0f;
        ModelSkinned_DebugGetRootYaw_F0(&yaw0);

        char buf[256];
        sprintf_s(buf, "[DEBUG] Clip: %ls, Initial Yaw: %.1f degrees\n",
            name.c_str(), XMConvertToDegrees(yaw0));
        OutputDebugStringA(buf);
    }
#endif

    // 播放参数
    bool  loop = clip.loop;
    float rate = clip.playbackRate;
    if (overrideLoop) loop = loopValue;
    if (overrideRate) rate = rateValue;

    ModelSkinned_SetLoop(loop);
    ModelSkinned_SetPlaybackRate(rate);

    // 切换“当前动画”
    if (gCurrent != idx) {
        gCurrent = idx;
        if (outChanged) *outChanged = true;

        // 清空 RootMotion 累计
        gRM_AccumPos = { 0,0,0 };
        gRM_AccumYaw = 0.0f;
        ApplyWorldWithRootMotion();

        // VelocityDriven 时：清掉根局部 XZ 平移
        ModelSkinned_SetZeroRootTranslationXZ(clip.rmType != RootMotionType::UseAnimDelta);
    }

    // 同步世界矩阵（node-fix 会在 Draw 时组合）
    ModelSkinned_SetWorldMatrix(gBaseWorld);
    return true;
}

void AnimatorRegistry_SetWorld(const XMMATRIX& world)
{
    gBaseWorld = world;
    ApplyWorldWithRootMotion(); // 让位移叠加到新 world 上
}

void AnimatorRegistry_Update(double dtSec)
{
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) {
        // 没有有效动画也要推进底层时间（如静态姿势）
        ModelSkinned_Update(dtSec);
        return;
    }

    const AnimClipDesc& c = gClips[gCurrent];

    switch (c.rmType)
    {
    case RootMotionType::None:
        // 不使用动画位移
        break;

    case RootMotionType::VelocityDriven:
    {
        // 用 BaseWorld 的前向列作为移动方向
        XMFLOAT4X4 m; XMStoreFloat4x4(&m, gBaseWorld);
        XMFLOAT3 fwd = { m._13, m._23, m._33 };
        XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&fwd));
        XMStoreFloat3(&fwd, v);

        gRM_AccumPos.x += fwd.x * c.velocity * (float)dtSec;
        gRM_AccumPos.y += fwd.y * c.velocity * (float)dtSec;
        gRM_AccumPos.z += fwd.z * c.velocity * (float)dtSec;
        ApplyWorldWithRootMotion();
        break;
    }

    case RootMotionType::UseAnimDelta:
    {
        // 以 motion-root 采样局部 ΔT，再映射到世界
        XMFLOAT3 dLocal{};
        if (ModelSkinned_SampleRootDelta_Local((float)dtSec, &dLocal))
        {
            XMFLOAT4X4 m; XMStoreFloat4x4(&m, gBaseWorld);
            XMFLOAT3 right = { m._11, m._21, m._31 };
            XMFLOAT3 up = { m._12, m._22, m._32 };
            XMFLOAT3 fwd = { m._13, m._23, m._33 };

            gRM_AccumPos.x += right.x * dLocal.x + up.x * dLocal.y + fwd.x * dLocal.z;
            gRM_AccumPos.y += right.y * dLocal.x + up.y * dLocal.y + fwd.y * dLocal.z;
            gRM_AccumPos.z += right.z * dLocal.x + up.z * dLocal.y + fwd.z * dLocal.z;
        }

        float dyaw = 0.0f;
        if (ModelSkinned_SampleRootYawDelta((float)dtSec, &dyaw)) {
            gRM_AccumYaw += dyaw; // 如需忽略旋转，只要不加它即可
        }

        ApplyWorldWithRootMotion();
        break;
    }
    }

    // 先采样再推进：保持 [t, t+dt] 采样与推进时序一致
    ModelSkinned_Update(dtSec);
}

void AnimatorRegistry_Draw()
{
    ModelSkinned_Draw(); // 里面会把 node-fix 乘到 World 上
}

// 查询
std::wstring AnimatorRegistry_CurrentName()
{
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) return L"";
    return gClips[gCurrent].name;
}

RootMotionType AnimatorRegistry_CurrentRootMotionType()
{
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) return RootMotionType::None;
    return gClips[gCurrent].rmType;
}

float AnimatorRegistry_CurrentPlaybackRate()
{
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) return 1.0f;
    return gClips[gCurrent].playbackRate;
}

bool AnimatorRegistry_CurrentLoop()
{
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) return true;
    return gClips[gCurrent].loop;
}

// 消费 RootMotion
bool AnimatorRegistry_ConsumeRootMotionDelta(RootMotionDelta* out)
{
    if (!out) return false;
    const float epsPos = 1e-6f;
    const float epsYaw = 1e-6f;

    float len2 = gRM_AccumPos.x * gRM_AccumPos.x
        + gRM_AccumPos.y * gRM_AccumPos.y
        + gRM_AccumPos.z * gRM_AccumPos.z;

    if (len2 < epsPos && std::fabs(gRM_AccumYaw) < epsYaw) return false;

    out->pos = gRM_AccumPos;
    out->yaw = gRM_AccumYaw;

    gRM_AccumPos = { 0,0,0 };
    gRM_AccumYaw = 0.0f;
    return true;
}

// DEBUG（保留在用）
bool AnimatorRegistry_DebugGetCurrentClipName(const wchar_t** outName)
{
    if (!outName) return false;
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) return false;
    *outName = gClips[gCurrent].name.c_str();
    return true;
}

bool AnimatorRegistry_DebugGetRootYaw(float* yaw0, float* yawNow)
{
    if (!yaw0 || !yawNow) return false;
    float a = 0.0f, b = 0.0f;
    bool ok0 = ModelSkinned_DebugGetRootYaw_F0(&a);
    bool ok1 = ModelSkinned_DebugGetRootYaw_Current(&b);
    if (!(ok0 && ok1)) return false;
    *yaw0 = a; *yawNow = b;
    return true;
}

bool AnimatorRegistry_DebugGetCurrentClipLengthSec(float* outSec)
{
    if (!outSec) return false;
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) return false;

    const auto& cur = gClips[gCurrent];
    const uint32_t fc = ModelSkinned_GetFrameCount();
    const float    sr = ModelSkinned_GetSampleRate();
    if (fc == 0 || sr <= 0.0f) return false;

    const float rate = (cur.playbackRate > 0.0f ? cur.playbackRate : 1.0f);
    *outSec = (float(fc) / sr) / rate;
    return true;
}
