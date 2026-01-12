/*==============================================================================

   field描画用ピクセルシェーダー [shader_pixel_field.hlsl]

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

Texture2D tex0 : register(t0);
//Texture2D tex1 : register(t1);

// Shadow map (NEW)
Texture2D g_shadowMap : register(t2);

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

float4 main(PS_IN pi) : SV_TARGET
{
    // UV handle, sample of rotating (NOTE: you compute 'uv' but original samples used pi.uv.
    // Keep original behavior to avoid changing visuals.)
    //float2 uv;
    //float angle = 3.14159f * 45.0f / 180.0f;
    //uv.x = pi.uv.x * cos(angle) + pi.uv.y * sin(angle);
    //uv.y = -pi.uv.x * sin(angle) + pi.uv.y * cos(angle);

    // color handle, sample of color blending (original behavior: sample by pi.uv)
    float4 tex_color = tex0.Sample(samp, pi.uv);// * pi.blend.g;
                     //+ tex1.Sample(samp, pi.uv) * pi.blend.r;

    // material color
    float3 material_color = tex_color.rgb * diffuse_color.rgb;

    // Normalize normal
    float3 normalW3 = normalize(pi.normalW.xyz);

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
    // Ambient light (not shadowed in v1)
    //-----------------------------
    float3 ambient = material_color * ambient_color.rgb;

    //-----------------------------
    // Specular (shadowed together with directional diffuse)
    //-----------------------------
    float3 toEye = normalize(eye_posW - pi.posW.xyz);
    float3 rDir = reflect(direction_world_vector.xyz, normalW3);
    float tDir = pow(max(dot(normalize(rDir), toEye), 0.0f), specular_power);
    float3 specular = specular_color.rgb * tDir * shadowTerm;

    float3 color = ambient + diffuse + specular;

    //-----------------------------
    // Point lights (not shadowed in v1)
    //-----------------------------
    for (int i = 0; i < point_light_count; i++)
    {
        float3 lightToPixel = pi.posW.xyz - point_light[i].posW;
        float distance = length(lightToPixel);

        float distance_factor = pow(max(1.0f - 1.0f / point_light[i].range * distance, 0.0f), 2.0f);

        float dlp = max(0.0f, dot(-normalize(lightToPixel), normalW3));
        color += material_color * point_light[i].color.rgb * distance_factor * dlp;

        // specular of point light
        float3 rP = reflect(normalize(lightToPixel), normalW3);
        float tP = pow(max(dot(normalize(rP), toEye), 0.0f), specular_power);
        float3 point_specular = point_light[i].color.rgb * tP;

        color += point_specular;
    }

    return float4(color, 1.0f);
}
