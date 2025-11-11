// AnimatorRegistry.cpp  —— 与你给的 AnimatorRegistry.h 完全匹配
#include "AnimatorRegistry.h"
#include "ModelSkinned.h"
#include "shader3d.h"

#include <vector>
#include <string>
#include <filesystem>

using namespace DirectX;
namespace fs = std::filesystem;

// -----------------------------
// 内部数据
// -----------------------------
static std::vector<AnimClipDesc> gClips;   // 注册表
static int   gCurrent = -1;                // 当前播放索引（-1 表示无）
static XMMATRIX gBaseWorld = XMMatrixIdentity();

// 记录当前已加载的 mesh+skel 组合（方便将来仅换 anim；目前为稳妥总是全量加载）
struct MeshSkelKey {
    std::wstring mesh;
    std::wstring skel;
    bool operator==(const MeshSkelKey& o) const { return mesh == o.mesh && skel == o.skel; }
};
static MeshSkelKey gLoadedKey{ L"", L"" };

// 辅助：查找 clip
static int FindIndex(const std::wstring& name)
{
    for (int i = 0; i < (int)gClips.size(); ++i)
        if (gClips[i].name == name) return i;
    return -1;
}

//算位移用
static DirectX::XMFLOAT3 gRM_AccumPos = { 0,0,0 };
static float gRM_AccumYaw = 0.0f;  // 动画产生的 Δyaw（单位：弧度）

//在 Play 时计算并设置 yawFix
static bool  gYawBaselineInit = false;
static float gYawBaselineRad = 0.0f; // 第一次 Play 的剪辑首帧 rootYaw0 作为基准

static void ApplyWorldWithRootMotion()
{
    using namespace DirectX;
    XMMATRIX T = XMMatrixTranslation(gRM_AccumPos.x, gRM_AccumPos.y, gRM_AccumPos.z);
    ModelSkinned_SetWorldMatrix(T * gBaseWorld);
}

// -----------------------------
// 对外实现
// -----------------------------
bool AnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    gClips.clear();
    gCurrent = -1;
    gBaseWorld = XMMatrixIdentity();
    gLoadedKey = MeshSkelKey{};

    gRM_AccumPos = { 0,0,0 }; //重置位移

    // 初始化底层蒙皮播放器
    return ModelSkinned_Initialize(dev, ctx);
}

void AnimatorRegistry_Finalize()
{
    gClips.clear();
    gCurrent = -1;
    // ModelSkinned 里有自己的 Finalize/资源释放（如果你需要的话在别处调用）
}

void AnimatorRegistry_Clear()
{
    gClips.clear();
    gCurrent = -1;
}

bool AnimatorRegistry_Register(const AnimClipDesc& clip)
{
    if (clip.name.empty()) return false;
    if (FindIndex(clip.name) >= 0) return false; // 不允许重名
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
    // 当前版本：不做磁盘 IO；真正加载在 Play 时进行
    return true;
}

bool AnimatorRegistry_Play(const std::wstring& name,
    bool* outChanged,
    bool overrideLoop,
    bool loopValue,
    bool overrideRate,
    float rateValue)
{
    if (outChanged) *outChanged = false;

    int idx = FindIndex(name);
    if (idx < 0) return false;

    const AnimClipDesc& clip = gClips[idx];

    // 组装底层加载描述
    ModelSkinnedDesc d;
    d.meshPath = clip.meshPath;
    d.skelPath = clip.skelPath;
    d.animPath = clip.animPath;
    d.matPath = clip.matPath;
    d.baseColorTexOverride = clip.baseColorOverride;

    // 为了稳定，当前一律全量加载（如果你已经实现了 ModelSkinned_LoadAnimOnly，这里可以做“同 mesh+skel 只换 anim”的优化）
    bool ok = ModelSkinned_Load(d);
    if (!ok) return false;

    // ★ 读取该剪辑首帧 rootYaw0，设定“对齐目标 = baseline”，并重置轨迹
    {
        float yaw0 = 0.0f;
        if (ModelSkinned_DebugGetRootYaw_F0(&yaw0)) {
            if (!gYawBaselineInit) { gYawBaselineInit = true; gYawBaselineRad = yaw0; }

            // 骨架“局部层”的入场基准
            ModelSkinned_SetRootYawAlignTarget(gYawBaselineRad);
            ModelSkinned_ResetRootYawTrack(yaw0);

            // ---- 节点/世界层 的入场对齐（关键！）----
            const float target = gYawBaselineRad; // 你的基准（通常取 Idle 的 baseline）
            float yawModel0 = 0.0f;
            if (ModelSkinned_ComputeRootYaw_ModelSpace_FirstFrame(&yawModel0)) {
                auto wrap = [](float a) { const float PI = 3.14159265358979f, T = 2 * PI;
                while (a > PI)a -= T; while (a <= -PI)a += T; return a; };
                const float nodeFix = wrap(target - yawModel0);
                ModelSkinned_SetNodeYawFix(nodeFix);

                // 日志（度）
                char b2[200];
                sprintf_s(b2,
                    "[Anim] Play %ls | yaw0(local)=%.1f°, yaw0(model)=%.1f°, nodeFix=%.1f° (target=%.1f°)\n",
                    name.c_str(),
                    DirectX::XMConvertToDegrees(yaw0),
                    DirectX::XMConvertToDegrees(yawModel0),
                    DirectX::XMConvertToDegrees(nodeFix),
                    DirectX::XMConvertToDegrees(target));
                OutputDebugStringA(b2);
            }

            // （可选）原有日志
            char buf[160];
            sprintf_s(buf, "[Anim] Play %ls | yaw0=%.1f°, target=%.1f°(baseline)\n",
                name.c_str(),
                DirectX::XMConvertToDegrees(yaw0),
                DirectX::XMConvertToDegrees(gYawBaselineRad));
            OutputDebugStringA(buf);
        }
    }

    // 应用播放参数（可被临时覆盖）
    bool loop = clip.loop;
    float rate = clip.playbackRate;
    if (overrideLoop) loop = loopValue;
    if (overrideRate) rate = rateValue;

    ModelSkinned_SetLoop(loop);
    ModelSkinned_SetPlaybackRate(rate);

    // 切换“当前动画”
    if (gCurrent != idx) {
        gCurrent = idx;
        if (outChanged) *outChanged = true;

        //清空位移
        gRM_AccumPos = { 0,0,0 };
        ApplyWorldWithRootMotion();
        ModelSkinned_SetZeroRootTranslationXZ(clip.rmType == RootMotionType::VelocityDriven);
    }

    // 同步世界矩阵
    ModelSkinned_SetWorldMatrix(gBaseWorld);

    return true;
}

void AnimatorRegistry_SetWorld(const DirectX::XMMATRIX& world)
{
    gBaseWorld = world;
    ApplyWorldWithRootMotion();
    //ModelSkinned_SetWorldMatrix(gBaseWorld);
}

void AnimatorRegistry_Update(double dtSec)
{
    if (gCurrent < 0 || gCurrent >= (int)gClips.size()) {
        ModelSkinned_Update(dtSec);
        return;
    }

    const AnimClipDesc& c = gClips[gCurrent];

    using namespace DirectX;

    switch (c.rmType)
    {
    case RootMotionType::None:
        // 不改位移
        break;

    case RootMotionType::VelocityDriven:
    {
        // 用 BaseWorld 的“前向列”当方向（和你之前一致：第三列是前）
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
            gRM_AccumYaw += dyaw; // 想丢弃旋转就不累加
        }

        ApplyWorldWithRootMotion();
        break;
    }
}

    // 最后再推进骨骼播放器（让骨架时间与采样区间对齐）
    ModelSkinned_Update(dtSec);
}

void AnimatorRegistry_Draw()
{
    ModelSkinned_Draw();
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

bool AnimatorRegistry_ConsumeRootMotionDelta(RootMotionDelta* out)
{
    if (!out) return false;
    const float eps = 1e-6f;
    float len2 = gRM_AccumPos.x * gRM_AccumPos.x
        + gRM_AccumPos.y * gRM_AccumPos.y
        + gRM_AccumPos.z * gRM_AccumPos.z;
    if (len2 < eps) return false;

    out->pos = gRM_AccumPos;
    out->yaw = gRM_AccumYaw;
    gRM_AccumPos = { 0,0,0 };
    gRM_AccumYaw = 0.0f;
    // 不要在这里重新 SetWorld；同帧 draw 用的还是刚刚那帧的世界矩阵
    return true;
}


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

    const auto& cur = gClips[gCurrent];           // 含 playbackRate
    const uint32_t fc = ModelSkinned_GetFrameCount();
    const float    sr = ModelSkinned_GetSampleRate();
    if (fc == 0 || sr <= 0.0f) return false;

    const float rate = (cur.playbackRate > 0.0f ? cur.playbackRate : 1.0f);
    *outSec = (float(fc) / sr) / rate;            // 时长 = 帧数/采样率/播放速率
    return true;
}
