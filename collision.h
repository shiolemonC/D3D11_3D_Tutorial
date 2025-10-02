/*==============================================================================

　 コリジョン判定 [collision.h]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/03
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef COLLISION_H
#define COLLISION_H

#include <d3d11.h>
#include <DirectXMath.h>

struct Circle // 丸の定義
{
    DirectX::XMFLOAT2 center; // 中心座標
    float radius; // 半径
};

struct Box // 四角の定義
{
    DirectX::XMFLOAT2 center;
    float half_width;
    float half_height;
};

bool Collision_OverlapCircle(const Circle& a, const Circle& b);
bool Collision_OverlapBox(const Box& a, const Box& b);

void Collision_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Collision_DebugFinalize(); // 頂点バッファー回収
void Collision_DebugDraw(const Circle& circle, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
void Collision_DebugDraw(const Box& box, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

#endif // COLLISION_H

