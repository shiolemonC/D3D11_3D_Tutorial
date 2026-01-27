#include "vfx_config.h"
#include "texture.h"

// ------------------------------------------------------------
// NOTE: You can freely edit this file to tune VFX without JSON.
// ------------------------------------------------------------

static VfxPreset g_presets[(int)VfxId::Count];

void VfxConfig_Initialize()
{
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
