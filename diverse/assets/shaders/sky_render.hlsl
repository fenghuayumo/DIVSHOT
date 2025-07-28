#include "inc/samplers.hlsl"
#include "inc/frame_constants.hlsl"
#include "inc/pack_unpack.hlsl"

#include "inc/hash.hlsl"
#include "inc/color.hlsl"

[[vk::binding(0)]] RWTexture2D<float4> output_tex;
[[vk::binding(1)]] TextureCube<float4> convolved_sky_cube_tex;
[[vk::binding(2)]] cbuffer _ {
    float4 output_tex_size;
};

#include "inc/atmosphere.hlsl"
#include "inc/sun.hlsl"

[numthreads(8, 8, 1)]
void main(in uint2 px: SV_DispatchThreadID) {
    
    float2 uv = get_uv(px, output_tex_size);
    const ViewRayContext view_ray_context = ViewRayContext::from_uv(uv);
    // Allow the size to be changed, but don't go below the real sun's size,
    // so that we have something in the sky.
    const float real_sun_angular_radius = 0.53 * 0.5 * PI / 180.0;
    const float sun_angular_radius_cos = min(cos(real_sun_angular_radius), frame_constants.sun_angular_radius_cos);

    // Conserve the sun's energy by making it dimmer as it increases in size
    // Note that specular isn't quite correct with this since we're not using area lights.
    float current_sun_angular_radius = acos(sun_angular_radius_cos);
    float sun_radius_ratio = real_sun_angular_radius / current_sun_angular_radius;

    float3 output = convolved_sky_cube_tex.SampleLevel(sampler_llr, view_ray_context.ray_dir_ws(), 0).rgb;
    if (dot(view_ray_context.ray_dir_ws(), SUN_DIRECTION) > sun_angular_radius_cos) {
        // TODO: what's the correct value?
        output += 800 * sun_color_in_direction(view_ray_context.ray_dir_ws()) * sun_radius_ratio * sun_radius_ratio;
    }
    output_tex[px] = float4(output, 1.0);
}