/*==============================================================================

　 コリジョン判定 [collision.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/03
--------------------------------------------------------------------------------

==============================================================================*/
#include "collision.h"
#include "direct3d.h"
#include "texture.h"
#include "shader.h"

#include <DirectXMath.h>
using namespace DirectX;

// Debug用変数
static constexpr int NUM_VERTEX = 5000; // 最大頂点数、半径1600の円の周長
static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

static int g_WhiteTexId = -1;


// 頂点構造体
struct Vertex
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;    // 色
	XMFLOAT2 uv;       // UV
};


bool Collision_OverlapCircle(const Circle& a, const Circle& b)
{
    float x1 = b.center.x - a.center.x;
    float y1 = b.center.y - a.center.y;

    return (x1 * x1 + y1 * y1) < (a.radius + b.radius) * (a.radius + b.radius);

    //XMVECTOR ac = XMLoadFloat2(&a.center);
    //XMVECTOR bc = XMLoadFloat2(&b.center);
    //XMVECTOR lsq = XMVector2LengthSq(bc - ac);

    //return { XMVe(lsq) < (a.radius + b.radius) * (a.radius + b.radius) };
}

bool Collision_OverlapBox(const Box& a, const Box& b)
{
    float at = a.center.y - a.half_height;
    float ab = a.center.y + a.half_height;
    float al = a.center.x - a.half_width;
    float ar = a.center.x + a.half_width;
    float bt = b.center.y - b.half_height;
    float bb = b.center.y + b.half_height;
    float bl = b.center.x - b.half_width;
    float br = b.center.x + b.half_width;
    return al < br && ar > bl && at < bb && ab > bt;
}

bool Collision_OverlapAABB(const BOXAABB& a, const BOXAABB& b)
{
	return a.min.x < b.max.x
		&& a.max.x > b.min.x
		&& a.min.y < b.max.y
		&& a.max.y > b.min.y
		&& a.min.z < b.max.z
		&& a.max.z > b.min.z
		;
}

void Collision_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);

	g_WhiteTexId = Texture_Load(L"resources/white.png");
}

void Collision_DebugFinalize()
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Collision_DebugDraw(const Circle& circle, const XMFLOAT4& color)
{
	// 点の数を算出
	int NumVertex = (int)circle.radius * 2.0f * XM_PI + 1;

	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 円の描画
	const float rad = XM_2PI / NumVertex; // 円周を頂点数で等分したときの、1つあたりのラジアン角を計算
	for (int i = 0; i < NumVertex; i++)
	{
		v[i].position.x = cosf(rad * i) * circle.radius + circle.center.x;
		v[i].position.y = sinf(rad * i) * circle.radius + circle.center.y;
		v[i].position.z = 0.0f;
		v[i].color = color;
		v[i].uv = XMFLOAT2{ 0.0, 0.0f }; // UV座標の仮設定（中央に固定）
	}

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	Texture_SetTexture(g_WhiteTexId);

	// ポリゴン描画命令発行
	g_pContext->Draw(NumVertex, 0);
}

void Collision_DebugDraw(const Box& box, const DirectX::XMFLOAT4& color)
{
	// 点の数を算出
	int NumVertex = 5;

	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 頂点情報を書き込み
	v[0].position = { box.center.x - box.half_width, box.center.y - box.half_height, 0.0f };
	v[1].position = { box.center.x + box.half_width, box.center.y - box.half_height, 0.0f };
	v[2].position = { box.center.x + box.half_width, box.center.y + box.half_height, 0.0f };
	v[3].position = { box.center.x - box.half_width, box.center.y + box.half_height, 0.0f };
	v[4].position = { box.center.x - box.half_width, box.center.y - box.half_height, 0.0f };
	
	for (int i = 0; i < NumVertex; i++)
	{
		v[i].color = color;
		v[i].uv = XMFLOAT2{ 0.0, 0.0f }; // UV座標の仮設定（中央に固定）
	}
	
	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	Texture_SetTexture(g_WhiteTexId);

	// ポリゴン描画命令発行
	g_pContext->Draw(NumVertex, 0);
}
