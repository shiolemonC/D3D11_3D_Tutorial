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

float4 main(PS_IN pi) : SV_TARGET
{
    // 只采样一次，rgb/alpha 共用
    float4 texC = tex.Sample(samp, pi.uv);

    float3 material_color = texC.rgb * pi.color.rgb * diffuse_color.rgb;
    float alpha = texC.a * pi.color.a * diffuse_color.a;

    // -------------------------
    // 1) 计算世界空间法线（normal mapping）
    // -------------------------
    float3 nW = normalize(pi.normalW.xyz);

    // 尝试用 VS 输出的 tangent
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
        // tangent 不可靠时：用屏幕导数推导 TBN（防止未定义值导致闪烁）
        float3 dp1 = ddx(pi.posW.xyz);
        float3 dp2 = ddy(pi.posW.xyz);
        float2 duv1 = ddx(pi.uv);
        float2 duv2 = ddy(pi.uv);

        float3 t = dp1 * duv2.y - dp2 * duv1.y;
        tW = normalize(t - nW * dot(nW, t)); // 施密特正交化
        bW = normalize(cross(nW, tW));
    }

    // normal map 采样（t1）
    float3 nTex = normalTex.Sample(samp, pi.uv).xyz;

    // 如果 t1 没绑定，通常会采到 (0,0,0)，用默认平面法线兜底
    if (dot(nTex, nTex) < 1e-6f)
        nTex = float3(0.5f, 0.5f, 1.0f);

    float3 nTS = nTex * 2.0f - 1.0f;

    // 如果你发现凹凸是反的，就把这一行打开（OpenGL/DX 绿通道差异）
    nTS.y = -nTS.y;

    float3 normalW = normalize(nTS.x * tW + nTS.y * bW + nTS.z * nW);

    // -------------------------
    // 2) 光照：把原来的 normalW 全部换成上面的 normalW
    // -------------------------
    float3 toEye = normalize(eye_posW - pi.posW.xyz);

    // directional light（你当前的约定：direction_world_vector 是“从光源射向物体”的入射方向）
    float3 dirI = normalize(direction_world_vector.xyz);
    float dl = max(0.0f, dot(-dirI, normalW));
    float3 diffuse = material_color * direction_world_color.rgb * dl;

    // ambient
    float3 ambient = material_color * ambient_color.rgb;

    // specular（保持你原逻辑：reflect(入射向量, 法线)）
    float3 r = reflect(dirI, normalW);
    float t = pow(max(dot(r, toEye), 0.0f), specular_power);
    float3 specular = specular_color.rgb * t;

    float3 color = ambient + diffuse + specular;

    // rim
    float rim = 1.0f - max(dot(normalW, toEye), 0.0f);
    rim = pow(rim, 3.2f);
    float3 rimColor = float3(0.1f, 0.1f, 0.1f);
    color += rimColor * rim * 0.6f;

    // point lights
    for (int i = 0; i < point_light_count; i++)
    {
        float3 lightToPixel = pi.posW.xyz - point_light[i].posW;
        float distance = length(lightToPixel);
        float distance_factor = pow(max(1.0f - 1.0f / point_light[i].range * distance, 0.0f), 2.0f);

        float3 I = normalize(lightToPixel); // 入射：从光源到像素
        float pdl = max(0.0f, dot(-I, normalW));
        color += material_color * point_light[i].color.rgb * distance_factor * pdl;

        float3 pr = reflect(I, normalW);
        float pt = pow(max(dot(pr, toEye), 0.0f), specular_power);
        color += point_light[i].color.rgb * pt;
    }

    return float4(color, alpha);
}
