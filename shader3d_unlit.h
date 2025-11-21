/*==============================================================================

shader3d without light
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SHADER3D_UNLIT_H
#define	SHADER3D_UNLIT_H

#include <d3d11.h>
#include <DirectXMath.h>


bool Shader3d_Unlit_Initialize();
void Shader3d_Unlit_Finalize();

void Shader3d_Unlit_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader3d_Unlit_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader3d_Unlit_SetProjectionMatrix(const DirectX::XMMATRIX& matrix);

void Shader3d_Unlit_SetColor(const DirectX::XMFLOAT4& color);
void Shader3d_Unlit_Begin();

#endif // SHADER_H
