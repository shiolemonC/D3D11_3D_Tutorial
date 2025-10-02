#include "grid.h"

#include <DirectXMath.h>
#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "shader3d.h"
using namespace DirectX;

static constexpr int GRID_H_COUNT = 10;
static constexpr int GRID_V_COUNT = 10;
static constexpr int GRID_H_LINE_COUNT = GRID_H_COUNT + 1;
static constexpr int GRID_V_LINE_COUNT = GRID_V_COUNT + 1;

static constexpr int NUM_VERTEX = GRID_H_LINE_COUNT * 2 + GRID_V_LINE_COUNT * 2; // 頂点数

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

struct Vertex3d
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;    // 色
};

static Vertex3d g_GridVertex[NUM_VERTEX]{};

void Grid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストのチェック
	if (!pDevice || !pContext) {
		hal::dout << "Polygon_Initialize() : 与えられたデバイスかコンテキストが不正です" << std::endl;
		return;
	}

	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	int   idx = 0;
	float halfW = GRID_H_COUNT * 0.5f; // X 方向半幅
	float halfD = GRID_V_COUNT * 0.5f; // Z 方向半深

	// 统一颜色（可改）
	XMFLOAT4 col = XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f };

	// 1) 水平线（平行 X 轴，Z 从 -halfD 到 +halfD），共 GRID_V_LINE_COUNT 条
	for (int i = 0; i < GRID_V_LINE_COUNT; ++i)
	{
		float z = -halfD + (float)i; // 每格 1.0

		g_GridVertex[idx].position = XMFLOAT3(-halfW, 0.0f, z);
		g_GridVertex[idx].color = col;
		++idx;

		g_GridVertex[idx].position = XMFLOAT3(halfW, 0.0f, z);
		g_GridVertex[idx].color = col;
		++idx;
	}

	// 2) 垂直线（平行 Z 轴，X 从 -halfW 到 +halfW），共 GRID_H_LINE_COUNT 条
	for (int i = 0; i < GRID_H_LINE_COUNT; ++i)
	{
		float x = -halfW + (float)i; // 每格 1.0

		g_GridVertex[idx].position = XMFLOAT3(x, 0.0f, -halfD);
		g_GridVertex[idx].color = col;
		++idx;

		g_GridVertex[idx].position = XMFLOAT3(x, 0.0f, halfD);
		g_GridVertex[idx].color = col;
		++idx;
	}

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3d) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_GridVertex;
	//g_GridVertex[0] = { { 0.0f, 0.0f, 0.0f }, {0.0f, 1.0f, 0.0f, 1.0f } };
	//g_GridVertex[0].position = { 0.0f, 0.0f, 5.0f };
	//g_GridVertex[0].color = { 0.0f, 1.0f, 0.0f, 1.0f };

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Grid_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Grid_Draw(void)
{
	Shader3d_Begin();

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//world matrix
	XMMATRIX mtxWorld = XMMatrixIdentity();
	Shader3d_SetWorldMatrix(mtxWorld);

	////view matrix
	//XMVECTOR eye = XMVectorSet(2.0f, 2.0f, -5.0f, 1.0f);
	//XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);  // 盯着 cube 的中心
	//XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//XMMATRIX mtxView = XMMatrixLookAtLH(eye, at, up);
	//Shader3d_SetViewMatrix(mtxView);

	////Perspective array
	////NearZ一定要大于0 是距离
	//constexpr float fovAngleY = XMConvertToRadians(60.0f);
	//float aspectRatio = (float)Direct3D_GetBackBufferWidth() / Direct3D_GetBackBufferHeight();
	//float nearZ = 0.1f;
	//float farZ = 100.0f;
	//XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, nearZ, farZ);

	//Shader3d_SetProjectionMatrix(mtxPerspective);

	// プリミティブトポロジ設定
	//g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0); //TO DELETE

}
