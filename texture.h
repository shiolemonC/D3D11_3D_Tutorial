/*==============================================================================

   テクスチャ管理 [texture.h]
														 Author : Youhei Sato
														 Date   : 2025/06/13
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef TEXTURE_H
#define TEXTURE_H

#include <d3d11.h>

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Texture_Finalize(void);

// テクスチャ画像の読み込み
//
// 戻り値：管理番号。読み込めなかった場合 -1
//
int Texture_Load(const wchar_t* pFilename);

void Texture_AllRelease();

void Texture_SetTexture(int texid, int slot = 0);
unsigned int Texture_Width(int texid); // テクスチャの幅
unsigned int Texture_Height(int texid); // テクスチャの高さ

ID3D11ShaderResourceView* Texture_GetSRV(int texid);


#endif //TEXTURE_H