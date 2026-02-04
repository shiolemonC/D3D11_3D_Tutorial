/*==============================================================================

   field描画用ピクセルシェーダー [shader_pixel_field.hlsl]

--------------------------------------------------------------------------------

==============================================================================*/
cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
    float4 matParams0; // x=uvScale, y=parallaxScale, z=roughMul, w=specMul
    float4 matParams1; // x=useNormal, y=useParallax, z=roughIsGloss, w=normalFlipY
    float4 matParams2; // x=normalStrength, y=roughBias, z=roughPow, w=heightMul
};

cbuffer PS_CONSTANT_BUFFER : register(b1)
{
    float4 ambient_color;
};

cbuffer PS_CONSTANT_BUFFER : register(b2)
{
    float4 direction_world_vector;
    float4 direction_world_color = { 1.0f, 1.0f, 1.0f, 1.0f };
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

//-------------------------------
// Shadow mapping CB (NEW)
// b5: lightViewProj is TRANSPOSED on CPU side
// shadowParams.x = bias (e.g. 0.001~0.005)
// shadowParams.y = strength (0..1)  1=use shadow fully
//-------------------------------
cbuffer PS_SHADOW_CB : register(b5)
{
    float4x4 lightViewProj;
    float4 shadowParams;
};

struct PS_IN
{
    float4 posH : SV_POSITION; // clip-space
    float4 posW : POSITION0; // world position
    float4 normalW : NORMAL0; // world normal
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D g_baseColor : register(t0);
Texture2D g_normalMap : register(t1);
Texture2D g_shadowMap : register(t2);
Texture2D g_roughMap : register(t3);
Texture2D g_heightMap : register(t4);
//Texture2D tex1 : register(t1);

// Shadow map (NEW)
//Texture2D g_shadowMap : register(t2);

SamplerState samp : register(s0);
SamplerComparisonState g_shadowSamp : register(s1);

// Compute shadow visibility for directional light.
// return: 1.0 = lit, 0.0 = in shadow
float ComputeShadowVisibility(float3 posW, float3 normalW3)
{
    // Project world position to light clip space
    float4 lp = mul(float4(posW, 1.0f), lightViewProj);

    // Avoid division by zero
    if (abs(lp.w) < 1e-6f)
        return 1.0f;

    float3 ndc = lp.xyz / lp.w;

    // NDC -> UV (D3D texture space: y down)
    float2 uv;
    uv.x = ndc.x * 0.5f + 0.5f;
    uv.y = -ndc.y * 0.5f + 0.5f;

    float depth = ndc.z; // D3D NDC z is [0..1] (assuming your light proj is LH)

    // Outside shadow map region => treat as lit
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || depth < 0.0f || depth > 1.0f)
        return 1.0f;

    // Slope-scaled bias based on N·L (simple & robust)
    float3 N = normalize(normalW3);
    float3 L = normalize(-direction_world_vector.xyz); // light direction toward surface
    float ndotl = saturate(dot(N, L));

    float baseBias = shadowParams.x;
    float bias = baseBias * (1.0f - ndotl) + baseBias * 0.25f;

    // SampleCmp: returns [0..1], 1=pass (lit), 0=blocked (shadowed)
    return g_shadowMap.SampleCmpLevelZero(g_shadowSamp, uv, depth - bias);
}

// ------------------------------
// No-tangent TBN from ddx/ddy
// ------------------------------
float3x3 CotangentFrame(float3 N, float3 p, float2 uv)
{
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);

    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invMax = rsqrt(max(dot(T, T), dot(B, B)));
    T *= invMax;
    B *= invMax;

    // 这里我们用 “行向量基” 约定：TBN 的三行分别是 T,B,N
    // World -> Tangent:  v_ts = mul(v_ws, TBN)
    // Tangent -> World: v_ws = mul(v_ts, transpose(TBN))
    return float3x3(T, B, N);
}

// ------------------------------
// Simple Parallax (UV offset)
// ------------------------------
float2 ParallaxUV(float2 uv, float3 Vws, float3x3 TBN, float scale)
{
    // Vws: pixel -> eye (world space)
    float3 Vts = mul(Vws, TBN); // world -> tangent
    float h = g_heightMap.Sample(samp, uv).r; // 0..1
    float height = (h * 2.0f - 1.0f) * matParams2.w; // -1..1

    float denom = max(Vts.z, 0.2f); // 避免斜视角爆炸
    float2 offset = (Vts.xy / denom) * (height * scale);
    return uv + offset;
}

float4 main(PS_IN pi) : SV_TARGET
{
    // ============================================================
    // (A) 先准备：uv0 / N0 / view / TBN
    // ============================================================
    float2 uv0 = pi.uv * matParams0.x; // uvScale
    float3 N0 = normalize(pi.normalW.xyz);
    float3 toEye = normalize(eye_posW - pi.posW.xyz);

    // “无需切线”的 TBN（用 ddx/ddy 自动构建）
    float3x3 TBN = CotangentFrame(N0, pi.posW.xyz, uv0);

    // ============================================================
    // (B) Parallax（可选）：用高度图偏移 UV
    // ============================================================
    float2 uv = uv0;
    if (matParams1.y > 0.5f)                      // useParallax
    {
        uv = ParallaxUV(uv, toEye, TBN, matParams0.y); // parallaxScale
    }

    // ============================================================
    // (C) 采样 BaseColor（用偏移后的 uv）
    // ============================================================
    float4 tex_color = g_baseColor.Sample(samp, uv);
    float3 material_color = tex_color.rgb * diffuse_color.rgb;

    // ============================================================
    // (D) Normal Mapping（可选）：用法线贴图更新 normalW3
    // ============================================================
    float3 normalW3 = N0;
    if (matParams1.x > 0.5f)                      // useNormal
    {
        float3 nTS = g_normalMap.Sample(samp, uv).xyz * 2.0f - 1.0f;

    // ★法线强度：1=原样，1.5~2 更明显，>3 容易“塑料/脏”
        nTS.xy *= matParams2.x; // ★

        if (matParams1.w > 0.5f) // normalFlipY
            nTS.y = -nTS.y;

        nTS = normalize(nTS); // ★放大后一定要 normalize

        normalW3 = normalize(mul(nTS, transpose(TBN))); // Tangent -> World
    }

    // ============================================================
    // (E) Roughness/Gloss -> Spec 参数（可选但强烈建议）
    // ============================================================
    float rm = g_roughMap.Sample(samp, uv).r;

    // 如果你的贴图其实是 gloss，就 rough = 1 - gloss
    if (matParams1.z > 0.5f)                      // roughnessIsGloss
        rm = 1.0f - rm;

// ★粗糙度基础强度（你已有 matParams0.z）
    float rough = rm * matParams0.z;

// ★偏移：+ 更粗糙（更哑光），- 更光滑（更亮）
    rough += matParams2.y; // ★ 推荐 -0.2 ~ +0.2

    rough = saturate(rough);

// ★对比曲线：>1 更“分层明显”，<1 更平
    rough = pow(rough, max(matParams2.z, 0.001f)); // ★ 推荐 0.8 ~ 2.0

    // roughness 映射到高光“锐利度”
    float specPow = lerp(256.0f, 8.0f, rough * rough);

    // roughness 越大，高光越弱
    float specIntensity = (1.0f - rough) * matParams0.w; // specularMul

    // ============================================================
    // 下面开始：你的原本光照逻辑（尽量少动）
    // ============================================================

    //-----------------------------
    // Directional light (with shadow)
    //-----------------------------
    float3 Ldir = -direction_world_vector.xyz;
    float dl = max(0.0f, dot(normalize(Ldir), normalW3));

    // Shadow visibility
    float vis = ComputeShadowVisibility(pi.posW.xyz, normalW3);
    float shadowTerm = lerp(1.0f, vis, shadowParams.y);

    float3 diffuse = material_color * direction_world_color.rgb * dl * shadowTerm;

    //-----------------------------
    // Ambient light
    //-----------------------------
    float3 ambient = material_color * ambient_color.rgb;
    ambient *= lerp(1.0f, vis, shadowParams.y * 0.6f);

    //-----------------------------
    // Specular (use specPow + specIntensity)
    //-----------------------------
    float3 rDir = reflect(direction_world_vector.xyz, normalW3);
    float tDir = pow(max(dot(normalize(rDir), toEye), 0.0f), specPow);
    float3 specular = specular_color.rgb * tDir * shadowTerm * specIntensity;

    float3 color = ambient + diffuse + specular;

    //-----------------------------
    // Point lights
    //-----------------------------
    for (int i = 0; i < point_light_count; i++)
    {
        float3 lightToPixel = pi.posW.xyz - point_light[i].posW;
        float distance = length(lightToPixel);

        float distance_factor = pow(max(1.0f - 1.0f / point_light[i].range * distance, 0.0f), 2.0f);

        float dlp = max(0.0f, dot(-normalize(lightToPixel), normalW3));
        color += material_color * point_light[i].color.rgb * distance_factor * dlp;

        // specular of point light（建议也乘 distance_factor，更自然）
        float3 rP = reflect(normalize(lightToPixel), normalW3);
        float tP = pow(max(dot(normalize(rP), toEye), 0.0f), specPow);
        float3 point_specular = point_light[i].color.rgb * tP * specIntensity;

        color += point_specular * distance_factor;
    }

    return float4(color, 1.0f);
}
