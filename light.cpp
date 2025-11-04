#include "light.h"
#include "direct3d.h"
using namespace DirectX;

static ID3D11Buffer* g_pPSConstantBuffer1 = nullptr; // 定数バッファb3
static ID3D11Buffer* g_pPSConstantBuffer2 = nullptr; // 定数バッファb4
static ID3D11Buffer* g_pPSConstantBuffer3 = nullptr;
static ID3D11Buffer* g_pPSConstantBuffer4 = nullptr;

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

struct DirectionalLight
{
	XMFLOAT4 Directional;
	XMFLOAT4 Color;
};

struct SpecularLight
{
	XMFLOAT3 CameraPosition;
	float Power;
	XMFLOAT4 Color;
};

struct PointLight
{
	XMFLOAT3 LightPosition;
	float Range;
	XMFLOAT4 Color;
};

struct PointLightList
{
	PointLight pointlights[4];
	int count;
	XMFLOAT3 dummy;
};

static PointLightList g_PointLights{};

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

	buffer_desc.ByteWidth = sizeof(DirectionalLight); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer2);

	buffer_desc.ByteWidth = sizeof(SpecularLight); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer3);

	buffer_desc.ByteWidth = sizeof(PointLightList); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer4);

	//g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);

}

void Light_Finalize()
{
	SAFE_RELEASE(g_pPSConstantBuffer4);
	SAFE_RELEASE(g_pPSConstantBuffer3);
	SAFE_RELEASE(g_pPSConstantBuffer2);
	SAFE_RELEASE(g_pPSConstantBuffer1);
}

void Light_SetAmbient(const DirectX::XMFLOAT3& color)
{
	// 定数バッファにAmbientをセット
	g_pContext->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &color, 0, 0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);
}


void Light_SetDirectionWorld(
	const DirectX::XMFLOAT4& world_directional, 
	const DirectX::XMFLOAT4& color)
{
	DirectionalLight light
	{
		world_directional,
		color,
	};
	g_pContext->UpdateSubresource(g_pPSConstantBuffer2, 0, nullptr, &light, 0, 0);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pPSConstantBuffer2);
}

void Light_SetSpecularWorld(const DirectX::XMFLOAT3& camera_position, float power, const DirectX::XMFLOAT4& color)
{
	SpecularLight light
	{
		camera_position,
		power,
		color
	};

	g_pContext->UpdateSubresource(g_pPSConstantBuffer3, 0, nullptr, &light, 0, 0);
	g_pContext->PSSetConstantBuffers(3, 1, &g_pPSConstantBuffer3);
}

void Light_SetPointCount(int count)
{
	g_PointLights.count = count;

	g_pContext->UpdateSubresource(g_pPSConstantBuffer4, 0, nullptr, &g_PointLights, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);
}

void Light_SetPointWorld(int n, const DirectX::XMFLOAT3& light_position, float range, const DirectX::XMFLOAT3& color)
{
	g_PointLights.pointlights[n].LightPosition = light_position;
	g_PointLights.pointlights[n].Range = range;
	g_PointLights.pointlights[n].Color = {color.x, color.y, color.z, 1.0f};

	g_pContext->UpdateSubresource(g_pPSConstantBuffer4, 0, nullptr, &g_PointLights, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);
}


