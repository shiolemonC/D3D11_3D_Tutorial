/*==============================================================================

   2D描画用ピクセルシェーダー [shader_pixel_billboard.hlsl]
--------------------------------------------------------------------------------

==============================================================================*/
cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
};

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D tex; // テクスチャ
SamplerState samp; // テクスチャサンプラ

float4 main(PS_IN pi) : SV_TARGET
{
    // a * b
    // a.r * b.r a.g * b.g a.b * b.b a.a * b.a
    return tex.Sample(samp, pi.uv) * diffuse_color;
}