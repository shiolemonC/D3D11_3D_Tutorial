struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D gSource : register(t0);
SamplerState gSamp : register(s0);

cbuffer BloomBlurCB : register(b0)
{
    float2 gTexelStep;
    float2 _pad;
};

float4 PS_BloomBlur(VSOut i) : SV_TARGET
{
    float3 color = gSource.Sample(gSamp, i.uv).rgb * 0.227027f;
    color += gSource.Sample(gSamp, i.uv + gTexelStep * 1.384615f).rgb * 0.316216f;
    color += gSource.Sample(gSamp, i.uv - gTexelStep * 1.384615f).rgb * 0.316216f;
    color += gSource.Sample(gSamp, i.uv + gTexelStep * 3.230769f).rgb * 0.070270f;
    color += gSource.Sample(gSamp, i.uv - gTexelStep * 3.230769f).rgb * 0.070270f;
    return float4(color, 1.0f);
}
