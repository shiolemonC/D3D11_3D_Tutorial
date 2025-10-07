// ==== 常量，与现有一致 ====
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
}
cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
}
cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
}
cbuffer VS_CONSTANT_BUFFER : register(b3)
{
    float4 ambient_color;
}
cbuffer VS_CONSTANT_BUFFER : register(b4)
{
    float4 direction_world_vector;
    float4 direction_world_color;
}

// 骨矩阵：模型空间的 final palette（global * offset）
cbuffer VS_CONSTANT_BUFFER : register(b5)
{
    float4x4 Bones[128];
};

struct VS_IN
{
    float3 posL : POSITION;
    float3 nrmL : NORMAL;
    float4 tangL : TANGENT;
    float2 uv : TEXCOORD0;
    uint4 idx4 : BLENDINDICES; // u8x4 -> VS 里按 uint4 接
    float4 w4 : BLENDWEIGHT;
};

struct VS_OUT
{
    float4 posH : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float3x3 Get3x3(float4x4 m)
{
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

VS_OUT main(VS_IN vi)
{
    VS_OUT o;

    float4 p = float4(0, 0, 0, 0);
    float3 n = float3(0, 0, 0);

    // ▼▼▼ 在这里加入索引检查 ▼▼▼
    bool has_bad_index = false;
    uint indices[4] = { vi.idx4.x, vi.idx4.y, vi.idx4.z, vi.idx4.w };

    [unroll]
    for (int k_check = 0; k_check < 4; ++k_check)
    {
        // 我们的骨骼总数是 70，所以有效索引是 0-69
        if (indices[k_check] >= 70u)
        {
            has_bad_index = true;
        }
    }

    // 如果发现无效索引，直接输出亮粉色，让我们能看到是哪些顶点出了问题
    if (has_bad_index)
    {
        o.posH = mul(float4(vi.posL, 1.0), mul(world, mul(view, proj)));
        o.color = float4(1.0, 0.0, 1.0, 1.0); // 亮粉色
        o.uv = vi.uv;
        return o;
    }
    // ▲▲▲ 检查代码结束 ▲▲▲


    // 如果索引都有效，才执行正常的蒙皮计算
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        uint bi = indices[k]; // 使用我们检查过的索引
        float w = vi.w4[k]; // 直接使用 vi.w4
        
        float4x4 M = Bones[bi];
        p += mul(float4(vi.posL, 1), M) * w;
        n += mul(vi.nrmL, Get3x3(M)) * w;
    }

    n = normalize(n);

    // ... (后续的光照和变换代码不变) ...
    float4 posW = mul(p, world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);

    float3 nW = normalize(mul(float4(n, 0), world).xyz);
    float dl = saturate(dot(-direction_world_vector.xyz, nW));
    float3 lit = direction_world_color.rgb * dl + ambient_color.rgb;
    o.color = float4(lit, 1.0);

    o.uv = vi.uv;
    return o;
}