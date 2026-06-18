#include "../inc/binding.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
// #include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"
struct PsIn
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 colour : COLOR0;
    [[vk::location(1)]] float2 fragPos : TEXCOORD1;
};
struct PsOut {
    float4 color : SV_TARGET0;
};

DS_CBUFFER(0) cbuffer _ {
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


PsOut main(PsIn ps)
{
    PsOut ps_out;
    if (ps.colour.w <= 0) discard;

    // Compute the positional squared distance from the center of the splat to the current fragment.
    const float A = dot(ps.fragPos, ps.fragPos);
    
    if (A > 8.0) discard;
    const float power = -0.5 * A;
    float alpha = min(0.999f, ps.colour.w * exp(power));

    // if (alpha < 1.0 / 255.0)
    //     discard;
#if OUTLINE_PASS
    const int mode = locked_color.w;
    ps_out.color = float4(1.0, 1.0, 1.0, mode == 1 ? 1.0f : alpha);
    return ps_out;
#endif
#if VISUALIZE_RINGS
    // Ring visualization effect parameters
    const float ringCenter = 1.0;      // Ring center position (A = 1.0 is standard ellipse boundary)
    const float ringSize = 0.1;        // Ring half-width (controls thickness, smaller = thinner)
    const float centerAlpha = 0.4;     // Center region opacity (0-1, smaller = more transparent)
    const float ringAlpha = 0.90;      // Ring region opacity (0-1, larger = brighter)
    
    // Calculate distance to ring center
    float distToRing = abs(A - ringCenter);
    
    if (distToRing < ringSize) {
        // Inside ring band: highlight display
        alpha = ringAlpha;
    } else if (A < ringCenter) {
        // Center region: semi-transparent
        alpha = alpha * centerAlpha;
    } else {
        // Outer region: attenuated with reduced intensity
        alpha = alpha * 0.15;
    }
    
    ps_out.color = float4(ps.colour.xyz, alpha);
    return ps_out;
#elif VISUALIZE_ELLIPSOIDS
    ps_out.color = float4(ps.colour.xyz, 1);
    return ps_out;
#endif
    ps_out.color = float4(ps.colour.xyz, alpha);
    return ps_out;
}