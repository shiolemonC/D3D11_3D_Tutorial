/*==============================================================================

   2D描画用頂点シェーダー [shader_vertex_2d.hlsl]
														 Author : Youhei Sato
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/

// 定数バッファ

// 投影変換行列
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 proj;
};

// ワールド座標変換行列
cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 world;
};

// 頂点シェーダーの入力構造体（CPU側の頂点バッファから受け取る）
struct VS_IN
{
    float4 posL  : POSITION0;
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

// 頂点シェーダーの出力構造体（次のステージに渡すデータ）
struct VS_OUT
{
    float4 posH  : SV_Position; // システム定義の頂点位置（クリップ空間座標）
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};


//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    
    // 座標変換
    float4x4 mtx = mul(world, proj); // ワールド座標系と投影変換座標系の掛け算 → 変換マトリクス
    
    // vi.posL = mul(vi.posL, world);
    vo.posH = mul(vi.posL, mtx); // ローカル座標と変換座標の掛け算
    
    vo.color = vi.color;
    vo.uv = vi.uv;
    
	return vo;
}


//    CPU側の頂点バッファ（位置 + 色 + UV）
//    ↓
// → 頂点シェーダー関数（ VS_IN → VS_OUT）
//    ↓
// → ラスタライザ（位置は画面座標に変換、 色とUVは補間される）
//    ↓
// → ピクセルシェーダー
