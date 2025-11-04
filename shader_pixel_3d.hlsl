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
    float4 direction_world_color = {1.0f, 1.0f, 1.0f, 1.0f};
    
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
    float4 posH       : SV_POSITION; // システム定義の頂点位置（クリップ空間座標）
    float4 posW       : POSITION0;
    float4 normalW    : NORMAL0;
    float4 color      : COLOR0;
    float2 uv         : TEXCOORD0;
};

Texture2D tex: register(t0); // PS 纹理槽 t0
SamplerState samp: register(s0); // PS 采样器槽 s0

float4 main(PS_IN pi) : SV_TARGET
{
    float3 material_color = tex.Sample(samp, pi.uv).rgb * pi.color.rgb * diffuse_color.rgb;
    
    //directional light
    float4 normalW = normalize(pi.normalW);
    float dl = max(0.0f, dot(-direction_world_vector, normalW)); 
    float3 diffuse = material_color * direction_world_color.rgb * dl;
    
    //ambient light
    float3 ambient = material_color * ambient_color.rgb;
    
    //specular
    float3 toEye = normalize(eye_posW - pi.posW.xyz);
    float3 r = reflect(direction_world_vector, normalW).xyz;
    float t = pow(max(dot(r, toEye), 0.0f), specular_power); // power of specular
    float3 specular = specular_color.rgb * t;
    
    float alpha = tex.Sample(samp, pi.uv).a * pi.color.a * diffuse_color.a;
    float3 color = ambient + diffuse + specular; //final color
    
    float rim = 1.0f - max(dot(normalW.xyz, toEye), 0.0f);
    rim = pow(rim, 3.2f);
    // color += float3(rim, rim, rim);
    
    // point light calculation

    for (int i = 0; i < point_light_count; i++)
    {
        // have to calculate distance to point light source  
        float3 lightToPixel = pi.posW.xyz - point_light[i].posW;
        float distance = length(lightToPixel);
        // caculate distance factor , reverse to Distance
        float distance_factor = pow(max(1.0f - 1.0f / point_light[i].range * distance, 0.0f),
        2.0f);
        
        
        float dl = max(0.0f, dot(-normalize(lightToPixel), normalW.xyz));
        //color += point_light[0].color.rgb * distance_factor; //float3(distance_factor, distance_factor, distance_factor);
        color += material_color * point_light[i].color.rgb * distance_factor * dl;
        
        //specular of point light
        float3 r = reflect(normalize(lightToPixel), normalW.xyz);
        float t = pow(max(dot(r, toEye), 0.0f), specular_power); // power of specular
        float3 point_specular = point_light[i].color.rgb * t;
        
        color += point_specular;
    }
    return float4(color, alpha);
    //return pi.color * color;
   }
