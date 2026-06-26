#pragma once

#include <DirectXMath.h>
#include "vfx_config.h"

enum class BossProjectileVisualType
{
    Sphere,
    VelocityBillboard,
    ParticleTrail,
    Model,
};

struct BossProjectileVisualDesc
{
    BossProjectileVisualType type = BossProjectileVisualType::Sphere;

    float visualRadius = 0.35f;
    DirectX::XMFLOAT4 color{ 0.55f, 0.85f, 1.0f, 1.0f };

    VfxBlend blend = VfxBlend::Add;
    int textureId = -1;
    const wchar_t* texturePath = nullptr;

    float baseWidth = 0.35f;
    float baseLength = 0.55f;
    float streakMul = 0.0f;
    float streakMax = 1.0f;
    float rotationBias = 0.0f;

    DirectX::XMFLOAT2 uvScale{ 1.0f, 1.0f };
    DirectX::XMFLOAT2 uvOffset{ 0.0f, 0.0f };
};

bool BossProjectileVisual_Initialize();
void BossProjectileVisual_Finalize();

void BossProjectileVisual_Draw(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& velocity,
    const BossProjectileVisualDesc& visual);
