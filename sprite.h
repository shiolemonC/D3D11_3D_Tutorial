/*==============================================================================

   スプライト表示 [sprite.h]
														 Author : Youhei Sato
														 Date   : 2025/06/12
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SPRITE_H
#define SPRITE_H

#include <d3d11.h>
#include <DirectXMath.h>

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sprite_Finalize(void);

// 2D用カメラの設定
void Sprite_Begin();

// Draw a solid-color rectangle (uses an internal 1x1 white texture).
void Sprite_DrawRect(float dx, float dy, float dw, float dh, const DirectX::XMFLOAT4& color);

// デフォルト背景色は白（引数XMFLOAT4 color）
// テクスチャ全表示
void Sprite_Draw(int texid, float dx, float dy,
	bool isFlipX = false, const DirectX::XMFLOAT4& color = {1.0f, 1.0f, 1.0f, 1.0f});

// テクスチャ全表示（表示サイズ変更）
void Sprite_Draw(int texid, float dx, float dy, float dw, float dh,
	bool isFlipX = false, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// UVカット
void Sprite_Draw(int texid, float dx, float dy, int px, int py, int pw, int ph,
	bool isFlipX = false, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// UVカット（表示サイズ変更）
void Sprite_Draw(int texid, float dx, float dy, float dw, float dh, int px, int py, int pw, int ph,
	bool isFlipX = false, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// 二つのバッファ、回転もできる
void Sprite_Draw(int texid, float dx, float dy, float dw, float dh, int px, int py, int pw, int ph, float angle,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

#endif // SPRITE_H
