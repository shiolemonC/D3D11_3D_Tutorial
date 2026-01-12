#include "sprite_effect.h"

#include <vector>
#include <array>

#include "texture.h"
#include "sprite_anim.h"   // SpriteAnim_RegisterPattern/CreatePlayer/IsStopped/DestroyPlayer
#include "billboard.h"     // BillboardAnim_Draw 在 sprite_anim.h 里声明

using namespace DirectX;

struct EffectAsset
{
    int texId = -1;
    int patternId = -1;
    SpriteSheetDesc desc{};
};

struct EffectInstance
{
    SpriteEffectKind kind{};
    int playId = -1;

    XMFLOAT3 pos{};
    XMFLOAT2 scale{ 1.0f, 1.0f };
    XMFLOAT2 pivot{ 0.0f, 0.0f };
};

static std::array<EffectAsset, 2> gAssets{};
static std::vector<EffectInstance> gInstances;

static bool RegisterOne(SpriteEffectKind kind, const SpriteSheetDesc& d)
{
    const int idx = (int)kind;

    if (!d.path || d.cols <= 0 || d.rows <= 0 || d.secondsPerFrame <= 0.0)
        return false;

    EffectAsset a{};
    a.desc = d;

    a.texId = Texture_Load(d.path);
    if (a.texId < 0) return false;

    XMUINT2 frameSize = d.frameSizePx;
    if (frameSize.x == 0 || frameSize.y == 0)
    {
        // 自动计算每帧像素尺寸（要求图集刚好整除）
        frameSize.x = (unsigned)Texture_Width(a.texId) / (unsigned)d.cols;
        frameSize.y = (unsigned)Texture_Height(a.texId) / (unsigned)d.rows;
    }

    int totalFrames = (d.frameCount > 0) ? d.frameCount : (d.cols * d.rows);

    a.patternId = SpriteAnim_RegisterPattern(
        a.texId,
        totalFrames,
        d.cols,                 // h_pattern_max：每行多少帧
        d.secondsPerFrame,
        frameSize,
        d.startPx,
        d.looped
    );

    if (a.patternId < 0) return false;

    gAssets[idx] = a;
    return true;
}

bool SpriteEffect_Initialize(const SpriteSheetDesc& hit, const SpriteSheetDesc& parry)
{
    // 注意：这里不调用 SpriteAnim_Initialize()，避免把你别处注册的 Pattern 清掉
    // 你要保证 SpriteAnim_Initialize() 在更上层被调用过（项目里应该已经有了）。

    gInstances.clear();
    gAssets = {};

    if (!RegisterOne(SpriteEffectKind::Hit, hit))   return false;
    if (!RegisterOne(SpriteEffectKind::Parry, parry)) return false;

    return true;
}

void SpriteEffect_Finalize()
{
    SpriteEffect_ClearAll();
    // 纹理释放是否需要做，要看你 texture 系统有没有 Unload API；这里保持“只Load不手动Unload”的风格，与你现有代码一致
}

void SpriteEffect_ClearAll()
{
    for (auto& it : gInstances)
    {
        if (it.playId >= 0)
            SpriteAnim_DestroyPlayer(it.playId);
    }
    gInstances.clear();
}

static void Spawn(SpriteEffectKind kind, const XMFLOAT3& pos, const XMFLOAT2& scale, const XMFLOAT2& pivot)
{
    const int idx = (int)kind;
    const int pat = gAssets[idx].patternId;
    if (pat < 0) return;

    int playId = SpriteAnim_CreatePlayer(pat);
    if (playId < 0) return;

    EffectInstance ins{};
    ins.kind = kind;
    ins.playId = playId;
    ins.pos = pos;
    ins.scale = scale;
    ins.pivot = pivot;

    gInstances.push_back(ins);
}

void SpriteEffect_SpawnHit(const XMFLOAT3& pos, const XMFLOAT2& scale, const XMFLOAT2& pivot)
{
    Spawn(SpriteEffectKind::Hit, pos, scale, pivot);
}

void SpriteEffect_SpawnParry(const XMFLOAT3& pos, const XMFLOAT2& scale, const XMFLOAT2& pivot)
{
    Spawn(SpriteEffectKind::Parry, pos, scale, pivot);
}

void SpriteEffect_Update(double /*dt*/)
{
    // dt 不需要用：SpriteAnim_Update 由你全局调用，这里只做清理
    for (int i = (int)gInstances.size() - 1; i >= 0; --i)
    {
        const int pid = gInstances[i].playId;
        if (pid >= 0 && SpriteAnim_IsStopped(pid))
        {
            SpriteAnim_DestroyPlayer(pid);
            gInstances.erase(gInstances.begin() + i);
        }
    }
}

void SpriteEffect_Draw()
{
    for (const auto& it : gInstances)
    {
        if (it.playId < 0) continue;
        BillboardAnim_Draw(it.playId, it.pos, it.scale, it.pivot);
    }
}
