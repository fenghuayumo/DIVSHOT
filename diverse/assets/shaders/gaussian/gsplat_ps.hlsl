#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
// #include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"
struct PsIn
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 colour : COLOR0;
    [[vk::location(1)]] float2 pix_direction : COLOR2;
};
struct PsOut {
    float4 color : SV_TARGET0;
};

[[vk::binding(0)]] cbuffer _ {
    float4x4 model_matrix;
    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
    float4 locked_color;
    float4 select_color;
    float4 tintColor;    // w: transparency
    float4 color_offset; // w: splat_scale_size
};

static const float stddev = sqrt(8);
static const float square = 8;
static const float minAlpha = 1.0 / 255.0;

#ifdef SA_GS
float erf_approx(float x)
{
    float sign = x < 0.0 ? -1.0 : 1.0;
    x = abs(x);
    float a1 = 0.254829592;
    float a2 = -0.284496736;
    float a3 = 1.421413741;
    float a4 = -1.453152027;
    float a5 = 1.061405429;
    float p = 0.3275911;
    float t = 1.0 / (1.0 + p * x);
    float y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * exp(-x * x);
    return sign * y;
}

float erfc(float x)
{
    return 1.0 - erf_approx(x);
}
#endif

PsOut main(PsIn ps)
{
    PsOut ps_out;
    if (ps.colour.w <= 0) discard;

    // float power = dot(ps.cov2d.xyz, ps.pix_direction.yxx * ps.pix_direction.yxy);
    float A = dot(ps.pix_direction, ps.pix_direction);
    if (A > 1) discard;
    float power = -4 * A;
    float alpha =  min(0.999f,ps.colour.w * exp(power));

    if (alpha < minAlpha)
        discard;
#if OUTLINE_PASS
    const int mode = locked_color.w;
    ps_out.color = float4(1.0, 1.0, 1.0, mode == 1 ? 1.0f : alpha);
    return ps_out;
#endif
#if VISUALIZE_RINGS
    const float ringSize = 0.04;
    if (A < 1.0 - ringSize) {
        alpha = max(0.05, alpha);
    } else {
        alpha = 0.6;
    }
    ps_out.color = float4(ps.colour.xyz * alpha, alpha);
    return ps_out;
#elif VISUALIZE_ELLIPSOIDS
    ps_out.color = float4(ps.colour.xyz, 1);
    return ps_out;
#endif
    ps_out.color = float4(ps.colour.xyz * alpha, alpha);
    return ps_out;
}