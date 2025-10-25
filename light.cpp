#include "light.h"
using namespace DirectX;

static ID3D11Buffer* g_pPSConstantBuffer1 = nullptr; // 定数バッファb3
static ID3D11Buffer* g_pPSConstantBuffer2 = nullptr; // 定数バッファb4


// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

struct PhongLight
{
	XMFLOAT4 Directional;
	XMFLOAT4 Color;
	XMFLOAT4 CameraPosition;
};

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{

	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点シェーダー用定数バッファの作成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4); // バッファのサイズ
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // バインドフラグ

	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer1);

	buffer_desc.ByteWidth = sizeof(PhongLight); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer2);
}

void Light_Finalize()
{
}

void Light_SetAmbient(const DirectX::XMFLOAT3& color)
{
	// 定数バッファにAmbientをセット
	g_pContext->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &color, 0, 0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);
}

void Light_SetDirectionWorld(
	const DirectX::XMFLOAT4& world_directional, 
	const DirectX::XMFLOAT4& color, 
	const DirectX::XMFLOAT3& camera_position)
{
	PhongLight light
	{
		world_directional,
		color,
		{
			camera_position.x,
			camera_position.y, 
			camera_position.z, 
			0.0f
		}
	};
	g_pContext->UpdateSubresource(g_pPSConstantBuffer2, 0, nullptr, &light, 0, 0);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pPSConstantBuffer2);
}
