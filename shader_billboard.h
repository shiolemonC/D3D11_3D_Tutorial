#ifndef SHADER_BILLBOARD_H
#define	SHADER_BILLBOARD_H

#include <d3d11.h>
#include <DirectXMath.h>


struct UVParameter
{
	DirectX::XMFLOAT2 scale;
	DirectX::XMFLOAT2 translation;
};


bool ShaderBillboard_Initialize();
void ShaderBillboard_Finalize();

void ShaderBillboard_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void ShaderBillboard_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderBillboard_SetProjectionMatrix(const DirectX::XMMATRIX& matrix);

void ShaderBillboard_SetColor(const DirectX::XMFLOAT4& color);

void ShaderBillboard_SetUVParameter(const UVParameter& pamameter);

void ShaderBillboard_Begin();

#endif // 