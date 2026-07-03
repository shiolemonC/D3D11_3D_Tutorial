#ifndef VFX_CONFIG_H
#define VFX_CONFIG_H

#include <DirectXMath.h>

// ------------------------------------------------------------
// VFX Presets (hard-coded; no JSON as requested)
// ------------------------------------------------------------

enum class VfxId : int
{
    SparkParry = 0,
    BloodSlash,
    FireRingGreen,
    FireRingGreenHighlight,
    FireRingRedBurst,
    Count
};

enum class VfxBlend : int
{
    Alpha = 0,
    Add
};

struct RangeF
{
    float minV = 0.0f;
    float maxV = 0.0f;
};

struct RangeI
{
    int minV = 0;
    int maxV = 0;
};

struct VfxPreset
{
    int texId = -1;

    VfxBlend blend = VfxBlend::Alpha;

    RangeI burstCount{ 1, 1 };
    RangeF lifetime{ 0.1f, 0.2f };

    // velocity
    RangeF speed{ 1.0f, 3.0f };
    bool   useCone = true;          // true: cone around direction, false: random sphere
    float  coneAngleDeg = 30.0f;    // only used when useCone==true

    // size (billboard scale XY)
    RangeF sizeStart{ 0.15f, 0.25f };
    RangeF sizeEnd{ 0.02f, 0.06f };

    // color over lifetime (linear)
    DirectX::XMFLOAT4 colorStart{ 1,1,1,1 };
    DirectX::XMFLOAT4 colorEnd{ 1,1,1,0 };

    // simple physics
    float gravityScale = 0.0f;      // 0 = no gravity, 1 = ~9.8m/s^2
    float drag = 0.0f;              // 0 = no drag, bigger = faster slowdown

    // UV (for atlas / sprite-sheet usage)
    DirectX::XMFLOAT2 uvScale{ 1.0f, 1.0f };
    DirectX::XMFLOAT2 uvOffset{ 0.0f, 0.0f };

    float alignToVelocity = false; // 火花开，血默认关
    float rotationBias = 0.0f;                                // 需要时再调 ±90°/180°
    float streakMul = 0.0f; // 火花拉伸强度
    float streakMax = 0.0f;                                   // 最长 4 倍
    float rotRad = 0.0f;
};

void VfxConfig_Initialize();
void VfxConfig_Finalize();

const VfxPreset& VfxConfig_Get(VfxId id);

#endif // VFX_CONFIG_H
