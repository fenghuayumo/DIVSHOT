#include "inc/math.hlsl"
#include "inc/quasi_random.hlsl"
#include "inc/monte_carlo.hlsl"
#include "inc/samplers.hlsl"
#include "inc/cube_map.hlsl"
#include "inc/color/srgb.hlsl"

[[vk::binding(0)]] TextureCube<float4> sky_cube_tex;
[[vk::binding(1)]] RWTexture2D<float> sky_pdf_tex;
[[vk::binding(2)]] RWTexture2D<float4> sky_env_tex;
[[vk::binding(3)]] cbuffer _ {
    float4 sky_light_tex_size; // Used to calculate the inverse resolution
};

[numthreads(8, 8, 1)]
void main(in uint3 px : SV_DispatchThreadID) {

    float2 uv = (px.xy + 0.5) * sky_light_tex_size.zw;
    // Highest resolution level -- capture the actual cube map(s) data
    float3 sky_dir = equi_area_spherical_mapping(uv).zxy;
    float4 output = sky_cube_tex.SampleLevel(sampler_llr, sky_dir, 0);
    // Calculate the PDF for this direction
    float pdf = sRGB_to_luminance(output.xyz);
    // Store the PDF in a separate texture for later use
    sky_pdf_tex[px.xy] = pdf;
    sky_env_tex[px.xy] = output;
}
