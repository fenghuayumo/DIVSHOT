struct DATA
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float4 colour : COLOR0;
    [[vk::location(2)]] float2 size : TEXCOORD0;
    [[vk::location(3)]] float2 uv : TEXCOORD1;
};

struct Ps_Out
{
    float4 colour : SV_Target0;
};

Ps_Out main(DATA fs_in)
{
    float distSq = dot(fs_in.uv, fs_in.uv);
    if (distSq > 1.0)
    {
        discard;
    }
    Ps_Out ps_out;
    ps_out.colour = fs_in.colour;
    return ps_out;
}
