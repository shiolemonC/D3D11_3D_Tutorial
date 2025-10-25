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
        // 重点：在推进骨骼时间之前，采样 [t, t+dt] 的“根关节局部位移”
        XMFLOAT3 dLocal{};
        if (ModelSkinned_SampleRootDelta_Local((float)dtSec, &dLocal))
        {
            // 把“局部 ΔT”变到世界：只用 BaseWorld 的三列作为基向量
            XMFLOAT4X4 m; XMStoreFloat4x4(&m, gBaseWorld);
            XMFLOAT3 right = { m._11, m._21, m._31 };
            XMFLOAT3 up = { m._12, m._22, m._32 };
            XMFLOAT3 fwd = { m._13, m._23, m._33 };

            // 如只想在 XZ 平面移动，可把 up 项去掉
            gRM_AccumPos.x += right.x * dLocal.x + up.x * dLocal.y + fwd.x * dLocal.z;
            gRM_AccumPos.y += right.y * dLocal.x + up.y * dLocal.y + fwd.y * dLocal.z;
            gRM_AccumPos.z += right.z * dLocal.x + up.z * dLocal.y + fwd.z * dLocal.z;

            ApplyWorldWithRootMotion();
        }
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
