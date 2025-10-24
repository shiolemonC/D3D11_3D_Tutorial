/*==============================================================================

   3D描画用ピクセルシェーダー [shader_pixel_3d.hlsl]

--------------------------------------------------------------------------------

==============================================================================*/
cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 color;
};

struct PS_IN
{
    float4 posH  : SV_Position;
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

Texture2D tex: register(t0); // PS 纹理槽 t0
SamplerState samp: register(s0); // PS 采样器槽 s0

float4 main(PS_IN pi) : SV_TARGET
{
    // a * b
    // a.r * b.r a.g * b.g a.b * b.b a.a * b.a
    return tex.Sample(samp, pi.uv) * pi.color * color;
    //return pi.color * color;
}
