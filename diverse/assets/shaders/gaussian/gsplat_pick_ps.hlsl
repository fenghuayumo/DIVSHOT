#include "../inc/frame_constants.hlsl"

struct PsIn
{
    [[vk::location(0)]] nointerpolation float4 colour : COLOR0;
    [[vk::location(1)]] float2 pix_direction : COLOR2;
    [[vk::location(2)]] nointerpolation uint vert_id : COLOR3;
};
struct PsOut {
    uint vert_id : SV_TARGET0;
    // float4 color : SV_TARGET0;
};

[[vk::binding(0)]] cbuffer _ {
    float4x4 transform;

    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
};

static const float stddev = sqrt(8);
static const float square = 8;
static const float minAlpha = 1.0 / 255.0;

PsOut main(PsIn ps)
{
    PsOut ps_out;
    if(ps.colour.w <= 0)
        discard;
    uint vert_id = ps.vert_id;
    float A = dot(ps.pix_direction, ps.pix_direction);
    if (A > 1) discard;
    float power = -4 * A;
    float alpha = ps.colour.w * exp(power);

    if (alpha < minAlpha)
        discard;
    ps_out.vert_id = vert_id;
    return ps_out;
}