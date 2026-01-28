struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D gScene : register(t0);
SamplerState gSamp : register(s0);

cbuffer PostFxCB : register(b0)
{
    float2 gInvTexSize; // (暂时没用到，但保留)
    float gStrength; // 0~1
    float gRadius; // UV半径，如 0.02~0.15

    float2 gCenterUV; // 0~1
    float gEnable; // 0/1
    float gSampleCount; // e.g. 8/12/16

    float4 _pad;
};

float4 PS_RadialBlur(VSOut i) : SV_TARGET
{
    float2 uv = i.uv;
    float4 baseCol = gScene.Sample(gSamp, uv);

    if (gEnable < 0.5 || gStrength <= 0.0001)
        return baseCol;

    const int MAX_SAMPLES = 32;
    int N = (int) gSampleCount;
    N = clamp(N, 2, MAX_SAMPLES);

    float2 dir = uv - gCenterUV;
    float4 acc = 0;

    [loop]
    for (int s = 0; s < MAX_SAMPLES; ++s)
    {
        if (s >= N)
            break;

        float t = (float) s / (float) (N - 1);
        float2 suv = uv - dir * (t * gRadius);
        acc += gScene.Sample(gSamp, suv);
    }

    float4 blurCol = acc / (float) N;
    return lerp(baseCol, blurCol, gStrength);
}
