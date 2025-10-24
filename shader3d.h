/*==============================================================================

shader3d
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SHADER3D_H
#define	SHADER3D_H

#include <d3d11.h>
#include <DirectXMath.h>


bool Shader3d_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader3d_Finalize();

void Shader3d_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader3d_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader3d_SetProjectionMatrix(const DirectX::XMMATRIX& matrix);

void Shader3d_SetColor(const DirectX::XMFLOAT4& color);
void Shader3d_Begin();

#endif // SHADER_H
