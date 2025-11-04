/*==============================================================================

   mesh field描画用頂点シェーダー [shader_vertex_field.hlsl]

--------------------------------------------------------------------------------

==============================================================================*/

// 定数バッファ

// 投影変換行列
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
};

// ワールド座標変換行列
cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
};


cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
};



struct VS_IN
{
    float4 posL : POSITION0;
    float4 normalL : NORMAL0;
    float4 blend : COLOR0; //blend color
    float2 uv : TEXCOORD0;
};


struct VS_OUT
{
    float4 posH : SV_POSITION; // システム定義の頂点位置（クリップ空間座標）
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
};


//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    
    
    //float4 posW = mul(vi.posL, world);
    //float4 powWV = mul(posW, view);
    //vo.posH = mul(powWV, proj);
    
    // coordinate change
    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);
    vo.posH = mul(vi.posL, mtxWVP);
    
    float4 normalW = mul(float4(vi.normalL.xyz, 0.0f), world);
    vo.normalW = normalize(normalW);
    //specular calculation
    vo.posW = mul(vi.posL, world);
    
    vo.blend = vi.blend;
    
    //float3 color = direction_world_color.rgb * dl + ambient_color.rgb;
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
