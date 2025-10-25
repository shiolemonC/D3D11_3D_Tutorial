/*==============================================================================
   Skinned 3D 頂点シェーダー（VS 只做蒙皮与坐标系转换，光照交给 PS）
==============================================================================*/

// ---- 常量缓冲：与通用 VS 完全一致 ----
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

// ---- 骨矩阵调色板：final = invBind * animatedGlobal（你在 CPU 已经算好） ----
cbuffer VS_CONSTANT_BUFFER : register(b5)
{
    float4x4 Bones[128];
};

// ---- 顶点输入：与 D3D11 输入布局一一对应 ----
struct VS_IN
{
    float3 posL : POSITION; // R32G32B32_FLOAT
    float3 nrmL : NORMAL; // R32G32B32_FLOAT
    float4 tangL : TANGENT; // R32G32B32A32_FLOAT（目前未使用，但保留对齐）
    float2 uv : TEXCOORD0; // R32G32_FLOAT
    uint4 idx4 : BLENDINDICES; // R8G8B8A8_UINT   ->  VS 看成 uint4
    float4 w4 : BLENDWEIGHT; // R8G8B8A8_UNORM  ->  VS 看成 float4 (0..1)
};

// ---- VS 输出：与通用 VS 完全一致（给 PS 做光照） ----
struct VS_OUT
{
    float4 posH : SV_Position; // クリップ空間
    float4 posW : POSITION0; // ワールド座標（PS 用于光照/視線ベクトル等）
    float4 normalW : NORMAL0; // ワールド法線（w=0）
    float4 color : COLOR0; // 顶点色；蒙皮网格没有顶点色，这里填白
    float2 uv : TEXCOORD0;
};

// 取 4x4 的左上 3x3
float3x3 Upper3x3(float4x4 m)
{
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

VS_OUT main(VS_IN vi)
{
    VS_OUT o;

    // ------ 1) 线性蒙皮 ------
    // 规范化权重（有些导出会不精确，防止漂移）
    float wsum = max(1e-6f, vi.w4.x + vi.w4.y + vi.w4.z + vi.w4.w);
    float4 w = vi.w4 / wsum;

    float4 skinnedPos = 0.0.xxxx; // 累加位置
    float3 skinnedNrm = 0.0.xxx; // 累加法线

    // 骨索引（uint4）直接用
    uint bi0 = vi.idx4.x;
    uint bi1 = vi.idx4.y;
    uint bi2 = vi.idx4.z;
    uint bi3 = vi.idx4.w;

    // 逐权重累加（位置用 4x4，法线用 3x3）
    if (w.x > 0.0f)
    {
        float4x4 M = Bones[bi0];
        skinnedPos += mul(float4(vi.posL, 1.0f), M) * w.x;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.x;
    }
    if (w.y > 0.0f)
    {
        float4x4 M = Bones[bi1];
        skinnedPos += mul(float4(vi.posL, 1.0f), M) * w.y;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.y;
    }
    if (w.z > 0.0f)
    {
        float4x4 M = Bones[bi2];
        skinnedPos += mul(float4(vi.posL, 1.0f), M) * w.z;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.z;
    }
    if (w.w > 0.0f)
    {
        float4x4 M = Bones[bi3];
        skinnedPos += mul(float4(vi.posL, 1.0f), M) * w.w;
        skinnedNrm += mul(vi.nrmL, Upper3x3(M)) * w.w;
    }

    // ------ 2) 转到世界/裁剪空间（光照交给 PS） ------
    // 世界坐标
    o.posW = mul(skinnedPos, world);

    // 视图/投影
    float4 posV = mul(o.posW, view);
    o.posH = mul(posV, proj);

    // 世界法线（把蒙皮后的法线当作方向向量乘 world，再归一化）
    float3 nW = mul(float4(normalize(skinnedNrm), 0.0f), world).xyz;
    o.normalW = float4(normalize(nW), 0.0f);

    // 颜色（你的蒙皮网格没有顶点色，这里给 1）
    o.color = 1.0.xxxx;

    // 透传 UV
    o.uv = vi.uv;

    return o;
}
