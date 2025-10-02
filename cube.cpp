/*==============================================================================

　 cubeの表示

--------------------------------------------------------------------------------

==============================================================================*/

#include "cube.h"
#include <DirectXMath.h>
#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "shader3d.h"
#include "texture.h"
#include "sampler.h"
#include <cmath>


using namespace DirectX;

static constexpr int NUM_VERTEX = 4 * 6; // 頂点数
static constexpr int NUM_INDEX = 3 * 2 * 6; // 頂点数

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
static ID3D11Buffer* g_pIndexBuffer = nullptr;

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

static float g_x = 0.0f;
static float g_angle = 0.0f;
static float g_scale = 1.0f;
static float g_AccumulatedTime = 0.0f;
static int g_CubeTexId = -1;

struct Vertex3d
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT3 normal; //法線
	XMFLOAT4 color;    // 色
	XMFLOAT2 texcoord;
};

static Vertex3d g_CubeVertex[24]
{
	// Front (red)
	{{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1,1,1,1}, {0.0f, 0.0f}}, //0
	{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1,1,1,1}, {0.25f, 0.25f}}, //1
	{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1,1,1,1}, {0.0f, 0.25f}}, //2
//	{{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1,1,1,1}, {0.0f, 0.0f}},
	{{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1,1,1,1}, {0.25f, 0.0f}}, //3
//	{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1,1,1,1}, {0.25f, 0.25f}},

	// Back (+Z, green) — CCW 正面（FrontCounterClockwise = TRUE）
	{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0,1,0,1}, {0.25f, 0.0f}},  //4
	{{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0,1,0,1}, {0.50f, 0.25f}}, //5
	{{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0,1,0,1}, {0.25f, 0.25f}},  //6
//	{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0,1,0,1}, {0.25f, 0.0f}},
	{{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0,1,0,1}, {0.50f, 0.0f}}, //7
//	{{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0,1,0,1}, {0.50f, 0.25f}},

	// Left (blue)
	{{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0,0,1,1}, {0.75f, 0.0f}},
	{{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0,0,1,1}, {1.0f, 0.25f}},
	{{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0,0,1,1}, {0.75f, 0.25f}},
//	{{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {0,0,1,1}, {0.75f, 0.0f}},
	{{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0,0,1,1}, { 1.0f, 0.0f}},
//	{{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0,0,1,1}, { 1.0f, 0.25f}},

	// Right (yellow)
	{{ 0.5f,  0.5f, -0.5f},{1.0f, 0.0f, 0.0f}, {1,1,0,1} , {0.25f, 0.0f}},
	{{ 0.5f, -0.5f,  0.5f},{1.0f, 0.0f, 0.0f}, {1,1,0,1} , {0.50f, 0.25f}},
	{{ 0.5f, -0.5f, -0.5f},{1.0f, 0.0f, 0.0f}, {1,1,0,1} , {0.25f, 0.25f}},
//	{{ 0.5f,  0.5f, -0.5f},{1.0f, 0.0f, 0.0f}, {1,1,0,1} , {0.25f, 0.0f}},
	{{ 0.5f,  0.5f,  0.5f},{1.0f, 0.0f, 0.0f}, {1,1,0,1} , {0.50f, 0.0f}},
//	{{ 0.5f, -0.5f,  0.5f},{1.0f, 0.0f, 0.0f}, {1,1,0,1} , {0.50f, 0.25f}},

	// Top (cyan)
	{{-0.5f,  0.5f,  0.5f},{0.0f, 1.0f, 0.0f}, {0,1,1,1}, {0.0f, 0.25f}},
	{{ 0.5f,  0.5f, -0.5f},{0.0f, 1.0f, 0.0f}, {0,1,1,1}, {0.25f, 0.50f}},
	{{-0.5f,  0.5f, -0.5f},{0.0f, 1.0f, 0.0f}, {0,1,1,1}, {0.0f, 0.50f}},
//	{{-0.5f,  0.5f,  0.5f},{0.0f, 1.0f, 0.0f}, {0,1,1,1}, {0.0f, 0.25f}},
	{{ 0.5f,  0.5f,  0.5f},{0.0f, 1.0f, 0.0f}, {0,1,1,1}, {0.25f, 0.25f}},
//	{{ 0.5f,  0.5f, -0.5f},{0.0f, 1.0f, 0.0f}, {0,1,1,1}, {0.25f, 0.50f}},

	// Bottom (magenta)
	{{-0.5f, -0.5f, -0.5f},{0.0f, -1.0f, 0.0f}, {1,0,1,1} , {0.25f, 0.25f}},
	{{ 0.5f, -0.5f,  0.5f},{0.0f, -1.0f, 0.0f}, {1,0,1,1} , {0.50f, 0.50f}},
	{{-0.5f, -0.5f,  0.5f},{0.0f, -1.0f, 0.0f}, {1,0,1,1} , {0.25f, 0.50f}},
//	{{-0.5f, -0.5f, -0.5f},{0.0f, -1.0f, 0.0f}, {1,0,1,1} , {0.25f, 0.25f}},
	{{ 0.5f, -0.5f, -0.5f},{0.0f, -1.0f, 0.0f}, {1,0,1,1} , {0.50f, 0.25f}},
//	{{ 0.5f, -0.5f,  0.5f},{0.0f, -1.0f, 0.0f}, {1,0,1,1} , {0.50f, 0.25f}},
};

static unsigned short g_CubeIndex[]{
	0, 1, 2, 0, 3, 1,
	4, 5, 6, 4, 7, 5,
	8, 9, 10,8, 11,9,
	12,13,14,12,15,13,
	16,17,18,16,19,17,
	20,21,22,20,23,21
};

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストのチェック
	if (!pDevice || !pContext) {
		hal::dout << "Polygon_Initialize() : 与えられたデバイスかコンテキストが不正です" << std::endl;
		return;
	}

	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3d) * NUM_VERTEX; //sizeof(&g_CubeVertex) is ok
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

	//index buffer
	bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX; //sizeof(&g_CubeVertex) is ok
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	sd.pSysMem = g_CubeIndex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);

	g_CubeTexId = Texture_Load(L"resources/UV_Texture_Pattern.png");
}

void Cube_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
	SAFE_RELEASE(g_pIndexBuffer);
}

void Cube_Update(double elapsed_time)
{
	g_AccumulatedTime += elapsed_time;
	g_x = sin(g_AccumulatedTime) * 4.5f;
	g_angle = g_AccumulatedTime * 3.0f;
	g_scale = abs(sin(g_AccumulatedTime) * 3.0f);
}


void Cube_Draw(const DirectX::XMMATRIX& mtxWorld)
{
	Shader3d_Begin();

	Texture_SetTexture(g_CubeTexId);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//set index buffer pipeline
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//XMMATRIX mtxWorld = XMMatrixIdentity(); // 如需自转可插 XMMatrixRotationY(θ)

	Shader3d_SetWorldMatrix(mtxWorld);

	g_pContext->DrawIndexed(NUM_INDEX, 0, 0);

	//int baseCount = 5;
	//float spacing = 1.0f;
	//float baseY = 0.5f;
	//XMMATRIX S = XMMatrixIdentity();

	//XMMATRIX mtxTrans = XMMatrixTranslation(2.0f, 0.0f, 2.0f);
	//for (int level = 0; level < baseCount; level++)
	//{
	//	const int side = baseCount - level;                 // 这一层一边几个方块
	//	const float y = baseY + level * spacing;           // 层高（每层抬高 spacing）
	//	const float start = (side - 1) * -0.5f * spacing;   // 让这一层以原点居中

	//	for (int i = 0; i < side; i++)
	//	{
	//		for (int j = 0; j < side; j++)
	//		{
	//			float x = start + i * spacing;
	//			float z = start + j * spacing;

	//			XMMATRIX T = XMMatrixTranslation(x, y, z);
	//			XMMATRIX R = XMMatrixRotationY(g_angle);
	//			XMMATRIX World = S * T * R * mtxTrans; // 如需自转可插 XMMatrixRotationY(θ)

	//			Shader3d_SetWorldMatrix(World);
	//			g_pContext->Draw(NUM_VERTEX, 0);
	//		}
	//	}
	//}
}
