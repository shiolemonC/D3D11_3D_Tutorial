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
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex0 : register(t0); // テクスチャ
Texture2D tex1 : register(t1); // テクスチャ

SamplerState samp; // テクスチャサンプラ

float4 main(PS_IN pi) : SV_TARGET
{
    //UV handle, sample of rotating
    float2 uv;
    float angle = 3.14159f * 45 / 180.0f;
    uv.x = pi.uv.x * cos(angle) + pi.uv.y * sin(angle);
    uv.y = -pi.uv.x * sin(angle) + pi.uv.y * cos(angle);
    
    //color handle, sample of color blending
    float4 tex_color = tex0.Sample(samp, pi.uv) * pi.blend.g 
                     + tex1.Sample(samp, pi.uv) * pi.blend.r;
    
    //material color
    float3 material_color = tex_color.rgb * diffuse_color.rgb;
    
    //directional light
    float4 normalW = normalize(pi.normalW);
    float dl = max(0.0f, dot(-direction_world_vector, normalW));
    float3 diffuse = material_color * direction_world_color.rgb * dl;
    
    //ambient light
    float3 ambient = material_color * ambient_color.rgb;
    
    //specular light
    float3 toEye = normalize(eye_posW - pi.posW.xyz);
    float3 r = reflect(direction_world_vector, normalW).xyz;
    float t = pow(max(dot(r, toEye), 0.0f), specular_power); // power of specular
    float3 specular = specular_color * t;
    
    
    //float alpha = tex_color.a * pi.blend.a * diffuse_color.a;
    float3 color = ambient + diffuse + specular; //final color
    
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
    
    return float4(color, 1.0f);
}
