struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D gScene : register(t0);
SamplerState gSamp : register(s0);

cbuffer PostFxCB : register(b0)
{
    float2 gInvTexSize; // FXAA 单像素 UV 尺寸
    float gStrength; // 0~1
    float gRadius; // UV半径，如 0.02~0.15

    float2 gCenterUV; // 0~1
    float gEnable; // 0/1
    float gSampleCount; // e.g. 8/12/16

    float4 _pad;
};

float Luma(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

float4 SampleFxaa(float2 uv)
{
    float4 center = gScene.Sample(gSamp, uv);
    float2 texel = gInvTexSize;

    float3 rgbNW = gScene.Sample(gSamp, uv + float2(-texel.x, -texel.y)).rgb;
    float3 rgbNE = gScene.Sample(gSamp, uv + float2( texel.x, -texel.y)).rgb;
    float3 rgbSW = gScene.Sample(gSamp, uv + float2(-texel.x,  texel.y)).rgb;
    float3 rgbSE = gScene.Sample(gSamp, uv + float2( texel.x,  texel.y)).rgb;

    float lumaM = Luma(center.rgb);
    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    float3 result = center.rgb;

    // 平坦区域保持原色，只在确实存在高对比边缘时追加采样。
    if (lumaRange >= max(0.0312f, lumaMax * 0.125f))
    {
        float2 dir;
        dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
        dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

        float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.0078125f, 0.0009765625f);
        float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
        dir = clamp(dir * rcpDirMin, -8.0f, 8.0f) * texel;

        float3 rgbA = 0.5f * (
            gScene.Sample(gSamp, uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
            gScene.Sample(gSamp, uv + dir * (2.0f / 3.0f - 0.5f)).rgb);

        float3 rgbB = rgbA * 0.5f + 0.25f * (
            gScene.Sample(gSamp, uv + dir * -0.5f).rgb +
            gScene.Sample(gSamp, uv + dir *  0.5f).rgb);

        float lumaB = Luma(rgbB);
        result = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
    }

    return float4(result, center.a);
}

float4 PS_RadialBlur(VSOut i) : SV_TARGET
{
    float2 uv = i.uv;
    float4 baseCol = SampleFxaa(uv);

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
