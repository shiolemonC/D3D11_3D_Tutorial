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


void Billboard_Draw(int texid, const DirectX::XMFLOAT3& position, float scale_x, float scale_y, const DirectX::XMFLOAT2& pivot)
{
	ShaderBillboard_SetUVParameter({{1.0f, 1.0f}, {0.0f, 0.0f}});
	ShaderBillboard_Begin();

	//set PSShader color
	ShaderBillboard_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	Texture_SetTexture(texid);




	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//XMMATRIX mtxWorld = XMMatrixIdentity(); // 如需自转可插 XMMatrixRotationY(θ)
	XMMATRIX pivot_offset = XMMatrixTranslation(-pivot.x, -pivot.y, 1.0f);
	XMFLOAT4X4 mtxCamera = Camera_GetMatrix();
	mtxCamera._41 = mtxCamera._42 = mtxCamera._43 = 0.0f;
	XMMATRIX iv = XMMatrixTranspose(XMLoadFloat4x4(&mtxCamera));//重い！

	XMMATRIX s = XMMatrixScaling(scale_x, scale_y, 1.0f);
	XMMATRIX t = XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);
	ShaderBillboard_SetWorldMatrix(pivot_offset * s * iv * t);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);
}
