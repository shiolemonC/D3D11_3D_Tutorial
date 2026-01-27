#include <DirectXMath.h>
#include "billboard.h"
#include "shader_billboard.h"
#include "texture.h"
#include "sampler.h"
#include "direct3d.h"
#include "camera.h"
using namespace DirectX;

static constexpr int NUM_VERTEX = 4; // 頂点数

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

struct Vertex3d
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;    // 色
	XMFLOAT2 texcoord; //UV
};


void Billboard_Initialize()
{
	ShaderBillboard_Initialize();
	Vertex3d vertex[]
	{
		// Front (red)
		{{-0.5f,  0.5f, 0.0f},  {1,1,1,1}, {0.0f,  0.0f}}, //0
		{{ 0.5f,  0.5f, 0.0f},  {1,1,1,1}, {1.0f,  0.0f}}, //1
		{{-0.5f, -0.5f, 0.0f},  {1,1,1,1}, {0.0f,  1.0f}}, //2
		{{ 0.5f, -0.5f, 0.0f},  {1,1,1,1}, {1.0f,  1.0f}}, //3
	};

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3d) * NUM_VERTEX; //sizeof(&g_CubeVertex) is ok
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &g_pVertexBuffer);


}

void Billboard_Finalize()
{
	ShaderBillboard_Finalize();
	SAFE_RELEASE(g_pVertexBuffer);
}




void Billboard_Draw(int texid, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT2& pivot)
{
	ShaderBillboard_Begin();

	ShaderBillboard_SetUVParameter({{1.0f, 1.0f}, {0.6f, 0.5f}});


	//set PSShader color
	ShaderBillboard_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	Texture_SetTexture(texid);




	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//XMMATRIX mtxWorld = XMMatrixIdentity(); // 如需自转可插 XMMatrixRotationY(θ)

	XMFLOAT4X4 mtxCamera = Camera_GetMatrix();
	mtxCamera._41 = mtxCamera._42 = mtxCamera._43 = 0.0f;
	XMMATRIX iv = XMMatrixTranspose(XMLoadFloat4x4(&mtxCamera));//重い！

	XMMATRIX pivot_offset = XMMatrixTranslation(-pivot.x, -pivot.y, 1.0f);
	XMMATRIX s = XMMatrixScaling(scale.x, scale.y, 1.0f);
	XMMATRIX t = XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);
	ShaderBillboard_SetWorldMatrix(pivot_offset * s * iv * t);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);
}

void Billboard_Draw(int texid, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMUINT4& tex_cut, const DirectX::XMFLOAT2& pivot)
{
	ShaderBillboard_Begin();
	float uv_x = (float)tex_cut.x / Texture_Width(texid);
	float uv_y = (float)tex_cut.y / Texture_Height(texid);
	float uv_w = (float)tex_cut.z / Texture_Width(texid);
	float uv_h = (float)tex_cut.w / Texture_Height(texid);

	ShaderBillboard_SetUVParameter({ {uv_w, uv_h}, {uv_x, uv_y} });


	//set PSShader color
	ShaderBillboard_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	Texture_SetTexture(texid);




	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//XMMATRIX mtxWorld = XMMatrixIdentity(); // 如需自转可插 XMMatrixRotationY(θ)

	XMFLOAT4X4 mtxCamera = Camera_GetMatrix();
	mtxCamera._41 = mtxCamera._42 = mtxCamera._43 = 0.0f;
	XMMATRIX iv = XMMatrixTranspose(XMLoadFloat4x4(&mtxCamera));//重い！

	XMMATRIX pivot_offset = XMMatrixTranslation(-pivot.x, -pivot.y, 1.0f);
	XMMATRIX s = XMMatrixScaling(scale.x, scale.y, 1.0f);
	XMMATRIX t = XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);
	ShaderBillboard_SetWorldMatrix(pivot_offset * s * iv * t);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);
}

// ------------------------------------------------------------
// Extended draw (per-instance color & UV) for particles, etc.
// ------------------------------------------------------------
void Billboard_DrawEx(
	int texid,
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT2& scale,
	const DirectX::XMFLOAT4& color,
	const DirectX::XMFLOAT2& uv_scale,
	const DirectX::XMFLOAT2& uv_offset,
	const DirectX::XMFLOAT2& pivot)
{
	ShaderBillboard_Begin();

	ShaderBillboard_SetUVParameter({ uv_scale, uv_offset });
	ShaderBillboard_SetColor(color);

	Texture_SetTexture(texid);

	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	DirectX::XMFLOAT4X4 mtxCamera = Camera_GetMatrix();
	mtxCamera._41 = mtxCamera._42 = mtxCamera._43 = 0.0f;
	DirectX::XMMATRIX iv = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&mtxCamera)); // "billboard" rotation only

	DirectX::XMMATRIX pivot_offset_m = DirectX::XMMatrixTranslation(-pivot.x, -pivot.y, 1.0f);
	DirectX::XMMATRIX s = DirectX::XMMatrixScaling(scale.x, scale.y, 1.0f);
	DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);
	ShaderBillboard_SetWorldMatrix(pivot_offset_m * s * iv * t);

	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);
}

void Billboard_DrawEx(
	int texid,
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT2& scale,
	const DirectX::XMUINT4& tex_cut,
	const DirectX::XMFLOAT4& color,
	const DirectX::XMFLOAT2& pivot)
{
	float uv_x = (float)tex_cut.x / Texture_Width(texid);
	float uv_y = (float)tex_cut.y / Texture_Height(texid);
	float uv_w = (float)tex_cut.z / Texture_Width(texid);
	float uv_h = (float)tex_cut.w / Texture_Height(texid);

	Billboard_DrawEx(texid, position, scale, color, { uv_w, uv_h }, { uv_x, uv_y }, pivot);
}

void Billboard_DrawExRot(
	int texid,
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT2& scale,
	const DirectX::XMFLOAT4& color,
	float rotationRad,
	const DirectX::XMFLOAT2& uv_scale,
	const DirectX::XMFLOAT2& uv_offset,
	const DirectX::XMFLOAT2& pivot)
{
	ShaderBillboard_Begin();

	ShaderBillboard_SetUVParameter({ uv_scale, uv_offset });
	ShaderBillboard_SetColor(color);

	Texture_SetTexture(texid);

	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	DirectX::XMFLOAT4X4 mtxCamera = Camera_GetMatrix();
	mtxCamera._41 = mtxCamera._42 = mtxCamera._43 = 0.0f;
	DirectX::XMMATRIX iv = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&mtxCamera));

	DirectX::XMMATRIX pivot_offset_m = DirectX::XMMatrixTranslation(-pivot.x, -pivot.y, 1.0f);
	DirectX::XMMATRIX s = DirectX::XMMatrixScaling(scale.x, scale.y, 1.0f);
	DirectX::XMMATRIX r = DirectX::XMMatrixRotationZ(rotationRad);
	DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);

	// ★ 重要：r 必须在 iv 之前（在屏幕平面内转，再面向相机）
	ShaderBillboard_SetWorldMatrix(pivot_offset_m * s * r * iv * t);

	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);
}
