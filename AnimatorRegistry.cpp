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

// -----------------------------
// 对外实现
// -----------------------------
bool AnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    gClips.clear();
    gCurrent = -1;
    gBaseWorld = XMMatrixIdentity();
    gLoadedKey = MeshSkelKey{};

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
    }

    // 同步世界矩阵
    ModelSkinned_SetWorldMatrix(gBaseWorld);

    return true;
}

void AnimatorRegistry_SetWorld(const DirectX::XMMATRIX& world)
{
    gBaseWorld = world;
    ModelSkinned_SetWorldMatrix(gBaseWorld);
}

void AnimatorRegistry_Update(double dtSec)
{
    // 这里未来可以根据 RootMotionType 做位移/朝向累计；
    // 目前先只推进底层骨骼播放器
    (void)dtSec;
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
