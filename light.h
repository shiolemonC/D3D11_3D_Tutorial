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

void Light_SetDirectionWorld(const DirectX::XMFLOAT4& world_directional, const DirectX::XMFLOAT4& color);


#endif // !LIGHT_H
