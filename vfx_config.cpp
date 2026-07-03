#include "vfx_config.h"
#include "texture.h"

// ------------------------------------------------------------
// NOTE: You can freely edit this file to tune VFX without JSON.
// ------------------------------------------------------------

static VfxPreset g_presets[(int)VfxId::Count];

void VfxConfig_Initialize()
{
    const int neutralFireTex = Texture_Load(L"resources/fx/particle_fire_neutral.png");

    // ---- Spark (parry / guard) ----
    {
        VfxPreset& p = g_presets[(int)VfxId::SparkParry];

        // Prepare a small spark texture (white/yellow on transparent background).
        // If you don't have one yet, create: resources/fx/particle_spark.png
        p.texId = Texture_Load(L"resources/fx/particle_spark.png");

        p.blend = VfxBlend::Add;

        p.burstCount = { 30, 45 };
        p.lifetime = { 0.16f, 0.182f };

        p.speed = { 20.5f, 25.5f };
        p.useCone = true;
        p.coneAngleDeg = 60.0f;

        p.sizeStart = { 0.50f, 0.6f };
        p.sizeEnd = { 0.2f, 0.25f };

        p.colorStart = { 1.0f, 0.95f, 0.65f, 1.0f };
        p.colorEnd   = { 1.0f, 0.30f, 0.00f, 0.0f };

        p.gravityScale = 1.5f;
        p.drag = 12.0f; // sparks slow down quickly

        p.uvScale = { 1.0f, 1.0f };
        p.uvOffset = { 0.0f, 0.0f };

        p.alignToVelocity = true; // 火花开，血默认关
        p.rotationBias = 0.0f;                                // 需要时再调 ±90°/180°
        p.streakMul = 0.25f; // 火花拉伸强度
        p.streakMax = 4.0f;                                   // 最长 4 倍
        p.rotRad = 0.0f;

    }

    // ---- Blood (slash / hit) ----
    {
        VfxPreset& p = g_presets[(int)VfxId::BloodSlash];

        // Prepare a small blood droplet texture (red on transparent background).
        // If you don't have one yet, create: resources/fx/particle_blood.png
        p.texId = Texture_Load(L"resources/fx/particle_blood.png");

        p.blend = VfxBlend::Alpha;

        p.burstCount = { 15, 20 };
        p.lifetime = { 0.26f, 0.32f };

        p.speed = { 15.5f, 18.5f };
        p.useCone = true;
        p.coneAngleDeg = 30.0f;

        p.sizeStart = { 0.50f, 0.75f };
        p.sizeEnd = { 0.1f, 0.15f };

        p.colorStart = { 1.0f, 1.0f, 1.0f, 1.0f };
        p.colorEnd = { 1.0f, 0.30f, 0.00f, 0.0f };

        p.gravityScale = 4.50f;
        p.drag = 0.0f; // sparks slow down quickly

        p.uvScale = { 1.0f, 1.0f };
        p.uvOffset = { 0.0f, 0.0f };

        p.alignToVelocity = true; // 火花开，血默认关
        p.rotationBias = 0.0f;                                // 需要时再调 ±90°/180°
        p.streakMul = 0.95f; // 火花拉伸强度
        p.streakMax = 12.0f;                                   // 最长 4 倍
        p.rotRad = 0.0f;
    }

    // ---- Expanding green warning fire ----
    {
        VfxPreset& p = g_presets[(int)VfxId::FireRingGreen];
        p.texId = neutralFireTex;
        p.blend = VfxBlend::Alpha;
        p.burstCount = { 1, 1 };
        p.lifetime = { 0.30f, 0.42f };
        p.speed = { 0.45f, 0.90f };
        p.useCone = true;
        p.coneAngleDeg = 24.0f;
        p.sizeStart = { 0.62f, 0.82f };
        p.sizeEnd = { 0.24f, 0.36f };
        p.colorStart = { 0.18f, 1.00f, 0.30f, 0.58f };
        p.colorEnd = { 0.04f, 0.35f, 0.08f, 0.0f };
        p.gravityScale = 0.0f;
        p.drag = 1.8f;
        p.alignToVelocity = false;
        p.streakMul = 0.0f;
        p.streakMax = 1.0f;
    }

    // ---- Bright green confirmation at maximum range ----
    {
        VfxPreset& p = g_presets[(int)VfxId::FireRingGreenHighlight];
        p.texId = neutralFireTex;
        p.blend = VfxBlend::Add;
        p.burstCount = { 1, 1 };
        p.lifetime = { 0.20f, 0.30f };
        p.speed = { 0.65f, 1.20f };
        p.useCone = true;
        p.coneAngleDeg = 20.0f;
        p.sizeStart = { 0.82f, 1.05f };
        p.sizeEnd = { 0.30f, 0.48f };
        p.colorStart = { 0.55f, 1.00f, 0.58f, 0.90f };
        p.colorEnd = { 0.05f, 0.75f, 0.12f, 0.0f };
        p.gravityScale = 0.0f;
        p.drag = 2.0f;
        p.alignToVelocity = false;
        p.streakMul = 0.0f;
        p.streakMax = 1.0f;
    }

    // ---- Red damaging explosion ----
    {
        VfxPreset& p = g_presets[(int)VfxId::FireRingRedBurst];
        p.texId = neutralFireTex;
        p.blend = VfxBlend::Add;
        p.burstCount = { 1, 2 };
        p.lifetime = { 0.28f, 0.45f };
        p.speed = { 3.5f, 6.5f };
        p.useCone = true;
        p.coneAngleDeg = 38.0f;
        p.sizeStart = { 0.85f, 1.20f };
        p.sizeEnd = { 0.18f, 0.35f };
        p.colorStart = { 1.00f, 0.12f, 0.05f, 1.0f };
        p.colorEnd = { 0.45f, 0.01f, 0.00f, 0.0f };
        p.gravityScale = 0.0f;
        p.drag = 2.8f;
        p.alignToVelocity = false;
        p.streakMul = 0.0f;
        p.streakMax = 1.0f;
    }
}

void VfxConfig_Finalize()
{
    // Texture module likely owns texture lifetime; no-op here.
    // If you later add Texture_Unload(), call it here.
}

const VfxPreset& VfxConfig_Get(VfxId id)
{
    return g_presets[(int)id];
}
