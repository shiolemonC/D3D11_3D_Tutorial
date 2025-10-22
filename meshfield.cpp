#include "meshfield.h"

#include "texture.h"
#include "shader_field.h"
#include "direct3d.h"
#include "camera.h"
using namespace DirectX;

static constexpr float FIELD_MESH_SIZE = 1.0f; //一格的size

static constexpr int FIELD_MESH_H_COUNT = 50; //mesh horizontal
static constexpr int FIELD_MESH_V_COUNT = 25; // mesh vertical
static constexpr int FIELD_MESH_H_VERTEX_COUNT = FIELD_MESH_H_COUNT + 1;
static constexpr int FIELD_MESH_V_VERTEX_COUNT = FIELD_MESH_V_COUNT + 1;

static constexpr int NUM_VERTEX = FIELD_MESH_H_VERTEX_COUNT * FIELD_MESH_V_VERTEX_COUNT; // 頂点数
static constexpr int NUM_INDEX = 3 * 2 * FIELD_MESH_H_COUNT * FIELD_MESH_V_COUNT; // 一个面6个点 总共有H*V个面

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
static ID3D11Buffer* g_pIndexBuffer = nullptr;

static int g_Tex0Id = -1;
static int g_Tex1Id = -1;

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

struct Vertex3d
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT3 normal;
	XMFLOAT4 color;    // 色
	XMFLOAT2 texcoord;
};

static Vertex3d g_MeshFieldVertex[NUM_VERTEX];
static unsigned short g_MeshFieldIndex[NUM_INDEX];

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3d) * NUM_VERTEX; //sizeof(&g_CubeVertex) is ok
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	//顶点情报
	for(int z = 0; z < FIELD_MESH_V_VERTEX_COUNT; z++)
	{
		for (int x = 0; x < FIELD_MESH_H_VERTEX_COUNT; x++)
		{
			int index = x + FIELD_MESH_H_VERTEX_COUNT * z;
			g_MeshFieldVertex[index].position = { x * FIELD_MESH_SIZE, 0.0f, z * FIELD_MESH_SIZE };
			g_MeshFieldVertex[index].normal = { 0.0f, 1.0f, 0.0f };
			g_MeshFieldVertex[index].color = { 0.0f, 1.0f, 0.0f, 1.0f };
			g_MeshFieldVertex[index].texcoord = { x * 1.0f, z * 1.0f };
		}
	}

	for (int z = 0; z < FIELD_MESH_V_VERTEX_COUNT; z++)
	{
		int index = 26 + FIELD_MESH_H_VERTEX_COUNT * z;
		g_MeshFieldVertex[index].color = { 1.0f, 0.0f, 0.0f,1.0f };
		index = 25 + FIELD_MESH_H_VERTEX_COUNT * z;
		g_MeshFieldVertex[index].color = { 1.0f, 0.0f, 0.0f,1.0f };
		index = 27 + FIELD_MESH_H_VERTEX_COUNT * z;
		g_MeshFieldVertex[index].color = { 1.0f, 0.0f, 0.0f,1.0f };
	}

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_MeshFieldVertex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

	//index buffer
	bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX; //sizeof(&g_CubeVertex) is ok
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	//index情报
	int index = 0;

	for (int v = 0; v < FIELD_MESH_V_COUNT; v++)
	{
		for (int h = 0; h < FIELD_MESH_H_COUNT; h++)
		{
			//unsigned short v0 = (unsigned short) (h + v * FIELD_MESH_H_VERTEX_COUNT);
			//unsigned short v1 = (unsigned short) (h + 1 + v * FIELD_MESH_H_VERTEX_COUNT);
			//unsigned short v2 = (unsigned short) (h + (v + 1) * FIELD_MESH_H_VERTEX_COUNT);
			//unsigned short v3 = (unsigned short) (h + 1 + (v + 1) * FIELD_MESH_H_VERTEX_COUNT);

			// 两个三角形：从上看为 CW，法线朝 +Y
			//g_MeshFieldIndex[index++] = v0;  
			//g_MeshFieldIndex[index++] = v3;  
			//g_MeshFieldIndex[index++] = v1;  

			//g_MeshFieldIndex[index++] = v0;  
			//g_MeshFieldIndex[index++] = v2;  
			//g_MeshFieldIndex[index++] = v3;  

			g_MeshFieldIndex[index + 0] = (h + (v + 0) * FIELD_MESH_H_VERTEX_COUNT);
			g_MeshFieldIndex[index + 1] = (h + (v + 1) * FIELD_MESH_H_VERTEX_COUNT) + 1;
			g_MeshFieldIndex[index + 2] = g_MeshFieldIndex[index + 0] + 1;
			g_MeshFieldIndex[index + 3] = g_MeshFieldIndex[index + 0];
			g_MeshFieldIndex[index + 4] = g_MeshFieldIndex[index + 1] - 1;
			g_MeshFieldIndex[index + 5] = g_MeshFieldIndex[index + 1];

			index += 6;

		}
	}

	sd.pSysMem = g_MeshFieldIndex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);

	g_Tex0Id = Texture_Load(L"resources/ground_texture/Ground_Gravel_ukxmacclw_1K_BaseColor.jpg");
	g_Tex1Id = Texture_Load(L"resources/country_farwoods.png");
	ShaderField_Initialize(pDevice, pContext);
}

void MeshField_Finalize(void)
{
	ShaderField_Finalize();
	SAFE_RELEASE(g_pVertexBuffer);
	SAFE_RELEASE(g_pIndexBuffer);
}

void MeshField_Draw()
{
	ShaderField_Begin();

	Texture_SetTexture(g_Tex0Id, 0);
	Texture_SetTexture(g_Tex1Id, 1);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//set index buffer pipeline
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//XMMATRIX mtxWorld = XMMatrixIdentity(); // 如需自转可插 XMMatrixRotationY(θ)

	float offset_x = FIELD_MESH_H_COUNT * FIELD_MESH_SIZE * 0.5f;
	float offset_z = FIELD_MESH_V_COUNT * FIELD_MESH_SIZE * 0.5f;

	ShaderField_SetWorldMatrix(XMMatrixTranslation(-offset_x, 0.0f, -offset_z));

	ShaderField_SetViewMatrix(XMLoadFloat4x4(&Camera_GetMatrix()));
	ShaderField_SetProjectionMatrix(XMLoadFloat4x4(&Camera_GetPerspectiveMatrix()));

	g_pContext->DrawIndexed(NUM_INDEX, 0, 0);

	//add some comments to test git
	//add some comments to test git
}
