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

        p.burstCount = { 10, 18 };
        p.lifetime = { 2.06f, 3.12f };

        p.speed = { 3.5f, 7.5f };
        p.useCone = true;
        p.coneAngleDeg = 40.0f;

        p.sizeStart = { 1.10f, 1.16f };
        p.sizeEnd = { 1.00f, 1.04f };

        p.colorStart = { 1.0f, 0.95f, 0.65f, 1.0f };
        p.colorEnd   = { 1.0f, 0.30f, 0.00f, 0.0f };

        p.gravityScale = 2.0f;
        p.drag = 5.0f; // sparks slow down quickly

        p.uvScale = { 1.0f, 1.0f };
        p.uvOffset = { 0.0f, 0.0f };
    }

    // ---- Blood (slash / hit) ----
    {
        VfxPreset& p = g_presets[(int)VfxId::BloodSlash];

        // Prepare a small blood droplet texture (red on transparent background).
        // If you don't have one yet, create: resources/fx/particle_blood.png
        p.texId = Texture_Load(L"resources/fx/particle_blood.png");

        p.blend = VfxBlend::Alpha;

        p.burstCount = { 8, 14 };
        p.lifetime = { 0.25f, 0.45f };

        p.speed = { 1.5f, 4.0f };
        p.useCone = true;
        p.coneAngleDeg = 55.0f;

        p.sizeStart = { 1.10f, 1.18f };
        p.sizeEnd = { 2.12f, 2.22f }; // grow slightly

        p.colorStart = { 0.85f, 0.05f, 0.05f, 0.9f };
        p.colorEnd   = { 0.35f, 0.02f, 0.02f, 0.0f };

        p.gravityScale = 1.0f;
        p.drag = 1.5f;

        p.uvScale = { 1.0f, 1.0f };
        p.uvOffset = { 0.0f, 0.0f };
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
