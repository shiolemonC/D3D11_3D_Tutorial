#include "AnimatorRegistry.h"
#include "ModelSkinned.h"   // 直接复用你现成的 ModelSkinned_* API
#include <Windows.h>
#include <cassert>

using namespace DirectX;

// 内部状态
struct RegistryState {
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;

    std::vector<AnimClipDesc> list;             // 顺序保存
    std::unordered_map<std::wstring, size_t> map; // name -> index

    // 当前播放的“名”（通过 Play 设置）
    std::wstring currentName;
} gAR;

static void LogW(const wchar_t* s) { OutputDebugStringW(s); }
static void LogA(const char* s) { OutputDebugStringA(s); }

// -----------------------------------------------------------

bool AnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    gAR = {};
    gAR.dev = dev;
    gAR.ctx = ctx;
    // ModelSkinned 仍旧是单实例黑盒，我们只初始化一次
    return ModelSkinned_Initialize(dev, ctx);
}

void AnimatorRegistry_Finalize()
{
    // 先卸载 ModelSkinned（释放 VB/IB/…）
    ModelSkinned_Finalize();

    gAR.list.clear();
    gAR.map.clear();
    gAR.currentName.clear();
    gAR.dev = nullptr;
    gAR.ctx = nullptr;
}

void AnimatorRegistry_Clear()
{
    gAR.list.clear();
    gAR.map.clear();
    gAR.currentName.clear();
}

bool AnimatorRegistry_Register(const AnimClipDesc& clip)
{
    if (clip.name.empty()) return false;
    if (gAR.map.count(clip.name)) {
        // 重复名直接拒绝，避免混淆
        return false;
    }
    size_t idx = gAR.list.size();
    gAR.list.push_back(clip);
    gAR.map.emplace(clip.name, idx);
    return true;
}

bool AnimatorRegistry_Has(const std::wstring& name)
{
    return gAR.map.count(name) != 0;
}

const AnimClipDesc* AnimatorRegistry_Get(const std::wstring& name)
{
    auto it = gAR.map.find(name);
    if (it == gAR.map.end()) return nullptr;
    return &gAR.list[it->second];
}

bool AnimatorRegistry_LoadAll()
{
    // 这里“最小可用版”就不做提前 IO 了（因为 ModelSkinned_Load 已经做了）
    // 但可以做一点简单校验，防止路径串错。
    // 如果你想严格，可以对每个 clip 检查文件存在性。
    // 现在返回 true，表示注册表可用。
    return true;
}

// -----------------------------------------------------------
// 播放：把注册信息套到 ModelSkinned_* 黑盒上
// -----------------------------------------------------------
bool AnimatorRegistry_Play(const std::wstring& name,
    bool* outChanged,
    bool overrideLoop,
    bool loopValue,
    bool overrideRate,
    float rateValue)
{
    if (outChanged) *outChanged = false;

    auto* clip = AnimatorRegistry_Get(name);
    if (!clip) return false;

    // 如果“当前名”相同且不需要强制重载，可直接返回
    if (gAR.currentName == name && !overrideLoop && !overrideRate) {
        if (outChanged) *outChanged = false;
        return true;
    }

    // 组装你已有的 ModelSkinnedDesc
    ModelSkinnedDesc d;
    d.meshPath = clip->meshPath;
    d.skelPath = clip->skelPath;
    d.animPath = clip->animPath;
    d.matPath = clip->matPath;
    d.baseColorTexOverride = clip->baseColorOverride;

    if (!ModelSkinned_Load(d)) {
        // 加载失败
        return false;
    }

    // Loop 与 Rate：优先使用外部覆盖，否则用注册表默认值
    const bool loop = overrideLoop ? loopValue : clip->loop;
    const float rate = overrideRate ? rateValue : clip->playbackRate;

    ModelSkinned_SetLoop(loop);
    ModelSkinned_SetPlaybackRate(rate);

    // 更新当前名
    gAR.currentName = name;

    if (outChanged) *outChanged = true;
    return true;
}

// 透传给 ModelSkinned
void AnimatorRegistry_SetWorld(const XMMATRIX& world)
{
    ModelSkinned_SetWorldMatrix(world);
}

void AnimatorRegistry_Update(double dtSec)
{
    ModelSkinned_Update(dtSec);
}

void AnimatorRegistry_Draw()
{
    ModelSkinned_Draw();
}

// 查询当前播放属性（没播放则给默认）
std::wstring AnimatorRegistry_CurrentName()
{
    return gAR.currentName;
}

RootMotionType AnimatorRegistry_CurrentRootMotionType()
{
    auto* clip = AnimatorRegistry_Get(gAR.currentName);
    return clip ? clip->rmType : RootMotionType::None;
}

float AnimatorRegistry_CurrentPlaybackRate()
{
    auto* clip = AnimatorRegistry_Get(gAR.currentName);
    return clip ? clip->playbackRate : 1.0f;
}

bool AnimatorRegistry_CurrentLoop()
{
    auto* clip = AnimatorRegistry_Get(gAR.currentName);
    return clip ? clip->loop : true;
}
