/*==============================================================================
   Skinned Outline VS (Inverted Hull)
==============================================================================*/

cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
};
cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
};
cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
};

cbuffer VS_CONSTANT_BUFFER : register(b5)
{
    float4x4 Bones[128];
};

// ★ outline 参数（16-byte 对齐）
cbuffer OUTLINE_CB : register(b6)
{
    float outlineWidth;
    float3 _pad0;
};

struct VS_IN
{
    float3 posL : POSITION;
    float3 nrmL : NORMAL;
    float4 tangL : TANGENT; // 不用也行，但保持输入一致
    float2 uv : TEXCOORD0;
    uint4 idx4 : BLENDINDICES;
    float4 w4 : BLENDWEIGHT;
};

struct VS_OUT
{
    float4 posH : SV_Position;
};

float3x3 Upper3x3(float4x4 m)
{
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

VS_OUT main(VS_IN vi)
{
    VS_OUT o;

    float wsum = max(1e-6f, vi.w4.x + vi.w4.y + vi.w4.z + vi.w4.w);
    float4 w = vi.w4 / wsum;

    float4 skinnedPos = 0.0.xxxx;
    float3 skinnedNrm = 0.0.xxx;

    uint bi0 = vi.idx4.x;
    uint bi1 = vi.idx4.y;
    uint bi2 = vi.idx4.z;
    uint bi3 = vi.idx4.w;

    if (w.x > 0)
    {
        float4x4 M = Bones[bi0];
        skinnedPos += mul(float4(vi.posL, 1), M) * w.x;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.x;
    }
    if (w.y > 0)
    {
        float4x4 M = Bones[bi1];
        skinnedPos += mul(float4(vi.posL, 1), M) * w.y;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.y;
    }
    if (w.z > 0)
    {
        float4x4 M = Bones[bi2];
        skinnedPos += mul(float4(vi.posL, 1), M) * w.z;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.z;
    }
    if (w.w > 0)
    {
        float4x4 M = Bones[bi3];
        skinnedPos += mul(float4(vi.posL, 1), M) * w.w;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.w;
    }

    // 世界法线
    float3 nW = mul(float4(normalize(skinnedNrm), 0.0f), world).xyz;
    nW = normalize(nW);

    // 世界坐标 + 外扩
    float4 posW = mul(skinnedPos, world);
    posW.xyz += nW * outlineWidth;

    // MVP
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    return o;
}
