//--------------------------------------------------------------------------------------
// light controller
//
// 
//
//--------------------------------------------------------------------------------------

#ifndef LIGHT_H
#define LIGHT_H

#include <DirectXMath.h>
#include <d3d11.h>

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

void Light_Finalize();

void Light_SetAmbient(const DirectX::XMFLOAT3& color);

void Light_SetDirectionWorld(
	const DirectX::XMFLOAT4& world_directional, 
	const DirectX::XMFLOAT4& color);

void Light_SetSpecularWorld(
	const DirectX::XMFLOAT3& camera_position,
	float power,
	const DirectX::XMFLOAT4& color);

void Light_SetPointCount(int count);

void Light_SetPointWorld(
	int n,
	const DirectX::XMFLOAT3& light_position,
	float range,
	const DirectX::XMFLOAT3& color);

#endif // !LIGHT_H
