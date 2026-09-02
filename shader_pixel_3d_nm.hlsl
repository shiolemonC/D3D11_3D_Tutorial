/*==============================================================================

   3D描画用ピクセルシェーダー [shader_pixel_3d.hlsl]

--------------------------------------------------------------------------------

==============================================================================*/
cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER : register(b1)
{
    float4 ambient_color;
};

cbuffer PS_CONSTANT_BUFFER : register(b2)
{
    float4 direction_world_vector;
    float4 direction_world_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    //float specular_power;
};

cbuffer PS_CONSTANT_BUFFER : register(b3)
{
    float3 eye_posW;
    float specular_power = 30.0f;
    float4 specular_color = { 0.1f, 0.1f, 0.1f, 1.0f };
};

struct PointLight
{
    float3 posW;
    float range;
    float4 color;
};

cbuffer PS_CONSTANT_BUFFER : register(b4)
{
    PointLight point_light[4];
    int point_light_count;
    float3 point_light_dummy;
};

struct PS_IN
{
    float4 posH : SV_POSITION; // システム定義の頂点位置（クリップ空間座標）
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 tangentW : TANGENT0; // ★新增：来自 VS
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex : register(t0); // PS 纹理槽 t0
Texture2D normalTex : register(t1); // ★新增：normal map
SamplerState samp : register(s0); // PS 采样器槽 s0

// -------------------------
// Toon helpers
// -------------------------

// 三段 toon：保留卡通层次，同时混入连续光照，避免大面积硬切。
float ToonDiffuse3(float x)
{
    x = saturate(x);

    // 阈值（可调）
    const float t1 = 0.35f;
    const float t2 = 0.70f;

    // 每段强度
    const float b0 = 0.24f;
    const float b1 = 0.62f;
    const float b2 = 1.00f;

    // 大三角形上的 fwidth 可能很小，因此保留一个固定的最小软化宽度。
    float w = max(fwidth(x) * 1.5f, 0.045f);

    float s1 = smoothstep(t1 - w, t1 + w, x);
    float s2 = smoothstep(t2 - w, t2 + w, x);

    // 两个阶梯直接累加，确保跨过 t2 时不会发生亮度跳变。
    float stepped = b0 + (b1 - b0) * s1 + (b2 - b1) * s2;
    float smoothDiffuse = lerp(b0, b2, x);
    return lerp(smoothDiffuse, stepped, 0.60f);
}

float4 main(PS_IN pi) : SV_TARGET
{
    // ---- base color ----
    float4 texC = tex.Sample(samp, pi.uv);
    float3 baseColor = texC.rgb * pi.color.rgb * diffuse_color.rgb;
    float alpha = texC.a * pi.color.a * diffuse_color.a;

    // -------------------------
    // 1) Normal Mapping：算世界法线 normalW
    // -------------------------
    float3 nW = normalize(pi.normalW.xyz);

    float3 tW = pi.tangentW.xyz;
    float tLen2 = dot(tW, tW);
    float3 bW;

    if (tLen2 > 1e-6f)
    {
        tW = normalize(tW);
        float sign = (pi.tangentW.w == 0.0f) ? 1.0f : pi.tangentW.w;
        bW = normalize(cross(nW, tW)) * sign;
    }
    else
    {
        // fallback：用导数推导 TBN，防止未定义 tangent 导致闪烁
        float3 dp1 = ddx(pi.posW.xyz);
        float3 dp2 = ddy(pi.posW.xyz);
        float2 duv1 = ddx(pi.uv);
        float2 duv2 = ddy(pi.uv);

        float3 t = dp1 * duv2.y - dp2 * duv1.y;
        tW = normalize(t - nW * dot(nW, t));
        bW = normalize(cross(nW, tW));
    }

    float3 nTex = normalTex.Sample(samp, pi.uv).xyz;
    if (dot(nTex, nTex) < 1e-6f)
        nTex = float3(0.5f, 0.5f, 1.0f);

    float3 nTS = nTex * 2.0f - 1.0f;

    // 角色法线贴图稍微收敛，避免细小法线变化频繁跨过 toon 阈值。
    nTS.xy *= 0.70f;
    nTS = normalize(nTS);

    // 绿通道反了就开这一行（你的工程里之前就遇到过）
    // nTS.y = -nTS.y;

    float3 normalW = normalize(nTS.x * tW + nTS.y * bW + nTS.z * nW);

    // -------------------------
    // 2) Toon Lighting
    // -------------------------
    float3 toEye = normalize(eye_posW - pi.posW.xyz);

    // 你想更“卡通”的阴影色，可以调这个
    float3 shadowTint = float3(0.24f, 0.25f, 0.30f);

    // 简单半球环境光：上方略冷、下方略暗，给轮廓提供连续的体积变化。
    float hemi = saturate(nW.y * 0.5f + 0.5f);
    float3 ambientGround = ambient_color.rgb * float3(0.62f, 0.60f, 0.58f);
    float3 ambientSky = ambient_color.rgb * float3(0.95f, 1.00f, 1.10f);
    float3 ambient = baseColor * lerp(ambientGround, ambientSky, hemi);

    float3 color = ambient;

    // ---- directional light (toon diffuse + toon spec) ----
    {
        float3 dirI = normalize(direction_world_vector.xyz); // 入射方向（光->物体）
        float nl = saturate(dot(-dirI, normalW)); // N·L

        float toonDl = ToonDiffuse3(nl);

        // 阴影更“脏”：把 baseColor 在暗处往 shadowTint 拉
        float3 toonBase = baseColor * lerp(shadowTint, float3(1, 1, 1), nl);

        color += toonBase * direction_world_color.rgb * toonDl;

        // 连续高光，避免肩膀和头部出现大块硬边。
        float3 r = reflect(dirI, normalW);
        float s = pow(max(dot(r, toEye), 0.0f), specular_power);
        color += specular_color.rgb * s * 0.75f;
    }

    // ---- soft rim ----
    {
        // 使用几何法线而不是 normal map 法线，让模型外轮廓稳定、柔和。
        float rim = pow(1.0f - saturate(dot(nW, toEye)), 3.0f);
        float3 rimColor = float3(0.28f, 0.36f, 0.55f);
        color += rimColor * rim * 0.12f;
    }

    // ---- point lights (toon diffuse + toon spec) ----
    for (int i = 0; i < point_light_count; i++)
    {
        float3 lightToPixel = pi.posW.xyz - point_light[i].posW;
        float dist = length(lightToPixel);

        float distFactor = pow(max(1.0f - dist / point_light[i].range, 0.0f), 2.0f);

        float3 I = normalize(lightToPixel); // 入射（光->像素）
        float nl = saturate(dot(-I, normalW)); // N·L
        float toonPl = ToonDiffuse3(nl);

        float3 toonBase = baseColor * lerp(shadowTint, float3(1, 1, 1), nl);
        color += toonBase * point_light[i].color.rgb * distFactor * toonPl;

        // 点光同样使用连续高光。
        float3 pr = reflect(I, normalW);
        float ps = pow(max(dot(pr, toEye), 0.0f), specular_power);
        color += point_light[i].color.rgb * ps * distFactor * 0.35f;
    }

    return float4(color, alpha);
}
