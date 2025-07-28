#include "inc/frame_constants.hlsl"

struct PsIn
{
    [[vk::location(0)]] float4 colour : COLOR0;
};
struct PsOut {
    float4 color : SV_TARGET0;
};

PsOut main(PsIn ps)
{
    if (ps.colour.w <= 0) discard;
    PsOut ps_out;
    ps_out.color = float4(ps.colour.xyz,1);
    return ps_out;
}