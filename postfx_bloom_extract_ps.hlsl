struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D gScene : register(t0);
SamplerState gSamp : register(s0);

float4 PS_BloomExtract(VSOut i) : SV_TARGET
{
    float3 color = gScene.Sample(gSamp, i.uv).rgb;
    float brightness = max(color.r, max(color.g, color.b));

    const float threshold = 1.00f;
    const float knee = 0.14f;

    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1e-4f);

    float contribution = max(brightness - threshold, soft);
    contribution /= max(brightness, 1e-4f);

    return float4(color * contribution, 1.0f);
}
