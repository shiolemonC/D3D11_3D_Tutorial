#ifndef BILLBOARD_H
#define BILLBOARD_H

#include <DirectXMath.h>
#include <d3d11.h>
#include "collision.h"

void Billboard_Initialize();
void Billboard_Finalize();

void Billboard_Draw(
	int texid, 
	const DirectX::XMFLOAT3& position, 
	const DirectX::XMFLOAT2& scale, 
	const DirectX::XMFLOAT2& pivot = {0.0f, 0.0f});

void Billboard_Draw(
	int texid, 
	const DirectX::XMFLOAT3& position, 
	const DirectX::XMFLOAT2& scale, 
	const DirectX::XMUINT4& tex_cut,
	const DirectX::XMFLOAT2& pivot = { 0.0f, 0.0f });

// Extended draw: allows per-instance color and UV.
// Recommended for particles (spark/blood) so they don't inherit the default UV offset in Billboard_Draw.
void Billboard_DrawEx(
    int texid,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT2& scale,
    const DirectX::XMFLOAT4& color,
    const DirectX::XMFLOAT2& uv_scale = { 1.0f, 1.0f },
    const DirectX::XMFLOAT2& uv_offset = { 0.0f, 0.0f },
    const DirectX::XMFLOAT2& pivot = { 0.0f, 0.0f });

// Extended draw with sprite-sheet cut (pixel units).
void Billboard_DrawEx(
    int texid,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT2& scale,
    const DirectX::XMUINT4& tex_cut,
    const DirectX::XMFLOAT4& color,
    const DirectX::XMFLOAT2& pivot = { 0.0f, 0.0f });

#endif // !BILLBOARD_H
