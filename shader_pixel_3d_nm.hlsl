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

// 三段 toon：暗 / 中 / 亮。用 fwidth 做小过渡带，减少条带闪烁
float ToonDiffuse3(float x)
{
    x = saturate(x);

    // 阈值（可调）
    const float t1 = 0.35f;
    const float t2 = 0.70f;

    // 每段强度（可调：想更“卡通”就拉开差距）
    const float b0 = 0.20f;
    const float b1 = 0.60f;
    const float b2 = 1.00f;

    // 过渡带宽度（可调：越大越不闪，但阶梯感更软）
    float w = fwidth(x) * 1.5f;

    float s1 = smoothstep(t1 - w, t1 + w, x);
    float s2 = smoothstep(t2 - w, t2 + w, x);

    // 0->1 段插值：b0->b1->b2
    float v01 = lerp(b0, b1, s1);
    float v12 = lerp(b1, b2, s2);
    float v = (x < t2) ? v01 : v12;

    return v;
}

// toon 高光：阈值化（硬高光）
float ToonSpec(float s)
{
    s = saturate(s);

    // 高光阈值（越大高光越少、越硬）
    const float sth = 0.55f;

    float w = fwidth(s) * 2.0f;
    return smoothstep(sth - w, sth + w, s);
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
        tW = normalize(t - nW * dot(nW, tW));
        bW = normalize(cross(nW, tW));
    }

    float3 nTex = normalTex.Sample(samp, pi.uv).xyz;
    if (dot(nTex, nTex) < 1e-6f)
        nTex = float3(0.5f, 0.5f, 1.0f);

    float3 nTS = nTex * 2.0f - 1.0f;

    // 绿通道反了就开这一行（你的工程里之前就遇到过）
    // nTS.y = -nTS.y;

    float3 normalW = normalize(nTS.x * tW + nTS.y * bW + nTS.z * nW);

    // -------------------------
    // 2) Toon Lighting
    // -------------------------
    float3 toEye = normalize(eye_posW - pi.posW.xyz);

    // 你想更“卡通”的阴影色，可以调这个
    float3 shadowTint = float3(0.15f, 0.15f, 0.20f);

    // ambient（建议在 C++ 里把 ambient 调低，否则 toon 阶梯不明显）
    float3 ambient = baseColor * ambient_color.rgb;

    float3 color = ambient;

    // ---- directional light (toon diffuse + toon spec) ----
    {
        float3 dirI = normalize(direction_world_vector.xyz); // 入射方向（光->物体）
        float nl = saturate(dot(-dirI, normalW)); // N·L

        float toonDl = ToonDiffuse3(nl);

        // 阴影更“脏”：把 baseColor 在暗处往 shadowTint 拉
        float3 toonBase = baseColor * lerp(shadowTint, float3(1, 1, 1), toonDl);

        color += toonBase * direction_world_color.rgb * toonDl;

        // spec（硬高光）
        float3 r = reflect(dirI, normalW);
        float s = pow(max(dot(r, toEye), 0.0f), specular_power);
        float specToon = ToonSpec(s);
        color += specular_color.rgb * specToon;
    }

    // ---- rim（也做一点 toon 化，边缘更动画感） ----
    {
        float rim = 1.0f - saturate(dot(normalW, toEye));
        rim = pow(rim, 2.5f);

        // 量化/阈值化 rim
        float rw = fwidth(rim) * 2.0f;
        float rimToon = smoothstep(0.55f - rw, 0.55f + rw, rim);

        float3 rimColor = float3(0.05f, 0.05f, 0.05f);
        color += rimColor * rimToon;
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

        float3 toonBase = baseColor * lerp(shadowTint, float3(1, 1, 1), toonPl);
        color += toonBase * point_light[i].color.rgb * distFactor * toonPl;

        // spec（硬高光）
        float3 pr = reflect(I, normalW);
        float ps = pow(max(dot(pr, toEye), 0.0f), specular_power);
        float psToon = ToonSpec(ps);
        color += point_light[i].color.rgb * psToon * distFactor * 0.5f;
    }

    return float4(color, alpha);
}
