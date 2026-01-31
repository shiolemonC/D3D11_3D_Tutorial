/*==============================================================================

   テクスチャ管理 [texture.cpp]
														 Author : Youhei Sato
														 Date   : 2025/06/13
--------------------------------------------------------------------------------

==============================================================================*/
#include "texture.h"
#include "direct3d.h"
#include "WICTextureLoader11.h"
#include <string>
using namespace DirectX;


static constexpr int TEXTURE_MAX = 1024; // テクスチャ管理最大数

struct Texture // テクスチャ管理する用の構造体
{
	std::wstring filename;

	bool isSRGB = true;

	unsigned int width; // テクスチャの幅
	unsigned int height; // テクスチャの高さ

	ID3D11Resource* pTexture = nullptr;
	ID3D11ShaderResourceView* pTextureView = nullptr;
};

static Texture g_Textures[TEXTURE_MAX] = {};
static int g_SetTextureIndex = -1; // 管理番号、デフォルトは -1

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	for (Texture& t : g_Textures)
	{
		t.pTexture = nullptr;
	}

	g_SetTextureIndex = -1;

	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;
}

void Texture_Finalize(void)
{
	Texture_AllRelease();
}

int Texture_Load(const wchar_t* pFilename, bool srgb)
{
	// すでに読み込んでいるものは読み込まない
	for (int i = 0; i < TEXTURE_MAX; i++)
	{
		if (g_Textures[i].filename == pFilename && g_Textures[i].isSRGB == srgb)
		{
			return i;
		}
	}

	// 空いている管理領域を探す
	for (int i = 0; i < TEXTURE_MAX; i++)
	{
		if (g_Textures[i].pTexture)
			continue;

		HRESULT hr;

		// ★ここが本体：sRGB/Linear を明示
		// BaseColor: srgb=true  => FORCE_SRGB
		// Normal/Rough/Height: srgb=false => IGNORE_SRGB
		WIC_LOADER_FLAGS flags = srgb ? WIC_LOADER_FORCE_SRGB : WIC_LOADER_IGNORE_SRGB;

		hr = CreateWICTextureFromFileEx(
			g_pDevice, g_pContext,
			pFilename,
			0,                              // maxsize (0=制限なし)
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
			flags,
			&g_Textures[i].pTexture,
			&g_Textures[i].pTextureView
		);

		if (FAILED(hr))
		{
			MessageBoxW(nullptr, L"テクスチャの読み込みに失敗しました", pFilename, MB_OK | MB_ICONERROR);
			return -1;
		}

		ID3D11Texture2D* pTexture = (ID3D11Texture2D*)g_Textures[i].pTexture;
		D3D11_TEXTURE2D_DESC t2desc;
		pTexture->GetDesc(&t2desc);
		g_Textures[i].width = t2desc.Width;
		g_Textures[i].height = t2desc.Height;

		g_Textures[i].filename = pFilename;
		g_Textures[i].isSRGB = srgb;  // ★记录

		return i;
	}

	return -1;
}

void Texture_AllRelease()
{
	for (Texture& t : g_Textures)
	{
		t.filename.clear(); // ファイル名削除
		SAFE_RELEASE(t.pTexture);
		SAFE_RELEASE(t.pTextureView);
	}
}

void Texture_SetTexture(int texid, int slot)
{
	ID3D11ShaderResourceView* srv = nullptr;

	if (texid >= 0)
	{
		g_SetTextureIndex = texid;
		srv = g_Textures[texid].pTextureView;
	}
	else
	{
		g_SetTextureIndex = -1;
		srv = nullptr; // ★关键：清空
	}

	g_pContext->PSSetShaderResources(slot, 1, &srv);
}

unsigned int Texture_Width(int texid)
{
	if (texid < 0)
	{
		return 0;
	}
	return g_Textures[texid].width;
}

unsigned int Texture_Height(int texid)
{
	if (texid < 0)
	{
		return 0;
	}
	return g_Textures[texid].height;
}

ID3D11ShaderResourceView* Texture_GetSRV(int texid)
{
	if (texid < 0 || texid >= TEXTURE_MAX) return nullptr;
	return g_Textures[texid].pTextureView; // 仅返回指针，不做 AddRef
}
