struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};


VSOut VS_Fullscreen(uint id : SV_VertexID)
{
    float2 pos[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };

    float2 uv[3] =
    {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };

    VSOut o;
    o.pos = float4(pos[id], 0.0, 1.0);
    o.uv = uv[id];
    return o;
}
