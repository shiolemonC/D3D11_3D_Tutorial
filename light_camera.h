#pragma once
#include <DirectXMath.h> 

void LightCamera_Initialize(const DirectX::XMFLOAT3& world_direction, const DirectX::XMFLOAT3& position);

void LightCamera_Finalize();

void LightCamera_SetPosition(const DirectX::XMFLOAT3& position);
void LightCamera_SetFront(const DirectX::XMFLOAT3& position);

const DirectX::XMFLOAT4X4& LightCamera_GetViewMatrix();
const DirectX::XMFLOAT4X4& LightCamera_GetProjectionMatrix();