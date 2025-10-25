/*==============================================================================

   3D描画用ピクセルシェーダー [shader_pixel_3d.hlsl]

--------------------------------------------------------------------------------

==============================================================================*/
cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 color;
};

cbuffer PS_CONSTANT_BUFFER : register(b1)
{
    float4 ambient_color;
};

cbuffer PS_CONSTANT_BUFFER : register(b2)
{
    float4 direction_world_vector;
    float4 direction_world_color;
    
    float3 eye_posW;
    //float specular_power;
};

struct PS_IN
{
    float4 posH       : SV_POSITION; // システム定義の頂点位置（クリップ空間座標）
    float4 posW       : POSITION0;
    float4 normalW    : NORMAL0;
    float4 color      : COLOR0;
    float2 uv         : TEXCOORD0;
};

Texture2D tex: register(t0); // PS 纹理槽 t0
SamplerState samp: register(s0); // PS 采样器槽 s0

float4 main(PS_IN pi) : SV_TARGET
{

    //directional light
    float4 normalW = normalize(pi.normalW);
    float dl = max(0.0f, dot(-direction_world_vector, normalW)); 

        
    float3 toEye = normalize(eye_posW - pi.posW.xyz);
    float3 r = reflect(direction_world_vector, normalW).xyz;
    float t = pow(max(dot(r, toEye), 0.0f), 10.0f); // power of specular
    
    float3 light_color = pi.color.rgb * direction_world_color.rgb * dl + ambient_color.rgb * pi.color.rgb ; 
    light_color += float3(1.0f, 1.0f, 1.0f) * t;
    
    return tex.Sample(samp, pi.uv) * float4(light_color, 1.0f) * color;
    //return pi.color * color;
}
