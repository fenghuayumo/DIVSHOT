
#include "../inc/color.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/brdf.hlsl"
#include "../inc/brdf_lut.hlsl"
#include "../inc/layered_brdf.hlsl"
#include "../inc/uv.hlsl"
#include "../inc/hash.hlsl"
#include "../inc/reservoir.hlsl"
#include "../inc/blue_noise.hlsl"
#include "near_field_settings.hlsl"
#include "rtdgi_restir_settings.hlsl"
#include "rtdgi_common.hlsl"

#define USE_SSAO  1

[[vk::binding(0)]] Texture2D<float4> radiance_tex;
[[vk::binding(1)]] Texture2D<float4> gbuffer_tex;
[[vk::binding(2)]] Texture2D<float> depth_tex;
[[vk::binding(3)]] Texture2D<float4> half_view_normal_tex;
[[vk::binding(4)]] Texture2D<float> half_depth_tex;
[[vk::binding(5)]] Texture2D<float4> ssao_tex;
[[vk::binding(6)]] RWTexture2D<float4> irradiance_output_tex;
[[vk::binding(7)]] cbuffer _ {
    float4 gbuffer_tex_size;
    float4 output_tex_size;
};

static float ggx_ndf_unnorm(float a2, float cos_theta) {
	float denom_sqrt = cos_theta * cos_theta * (a2 - 1.0) + 1.0;
	return a2 / (denom_sqrt * denom_sqrt);
}

[numthreads(8, 8, 1)]
void main(uint2 px : SV_DispatchThreadID) {
    const int2 hi_px_offset = HALFRES_SUBSAMPLE_OFFSET;

    float depth = depth_tex[px];
    if (0 == depth) {
        irradiance_output_tex[px] = 0;
        return;
    }

    const uint seed = frame_constants.frame_index;
    uint rng = hash3(uint3(px, seed));

    const float2 uv = get_uv(px, gbuffer_tex_size);
    const ViewRayContext view_ray_context = ViewRayContext::from_uv_and_depth(uv, depth);

    GbufferData gbuffer = GbufferDataPacked::from_uint4(asuint(gbuffer_tex[px])).unpack();

    const float3 center_normal_ws = gbuffer.normal;
    const float3 center_normal_vs = direction_world_to_view(center_normal_ws);
    const float center_depth = depth;
    const float center_ssao = ssao_tex[px].r;

    //const float3 center_bent_normal_ws = normalize(direction_view_to_world(ssao_tex[px * 2].gba));

    const uint frame_hash = hash1(frame_constants.frame_index);
    const uint px_idx_in_quad = (((px.x & 1) | (px.y & 1) * 2) + frame_hash) & 3;
    const float4 blue = blue_noise_for_pixel(px, frame_constants.frame_index) * M_TAU;
    
    float3 total_irradiance = 0;
    bool sharpen_gi_kernel = false;
    const float kernel_scale = sharpen_gi_kernel ? 0.5 : 1.0;
    float3 weighted_irradiance = 0;
    float w_sum = 0;
    for (uint sample_i = 0; sample_i < (RTDGI_RESTIR_USE_RESOLVE_SPATIAL_FILTER ? 4 : 1); ++sample_i) {
        const float ang = (sample_i + blue.x) * GOLDEN_ANGLE + (px_idx_in_quad / 4.0) * M_TAU;
        const float radius =
            RTDGI_RESTIR_USE_RESOLVE_SPATIAL_FILTER
            ? (pow(float(sample_i), 0.666) * 1.0 * kernel_scale + 0.4 * kernel_scale)
            : 0.0;

        const float2 reservoir_px_offset = float2(cos(ang), sin(ang)) * radius;
        const int2 rpx = int2(floor(float2(px) * 0.5 + reservoir_px_offset));
        
        float3 contribution = radiance_tex[rpx].xyz;
        const float rpx_depth = half_depth_tex[rpx];

        float3 sample_normal_vs = half_view_normal_tex[rpx].rgb;
        const float sample_ssao = ssao_tex[rpx * 2 + HALFRES_SUBSAMPLE_OFFSET].r;

        float w = 1;
        w *= ggx_ndf_unnorm(0.01, saturate(dot(center_normal_vs, sample_normal_vs)));
        w *= exp2(-200.0 * abs(center_normal_vs.z * (center_depth / rpx_depth - 1.0)));
        
        #if USE_SSAO
        w *= exp2(-20.0 * abs(center_ssao - sample_ssao));
        #endif

        weighted_irradiance += contribution * w;
        w_sum += w;
    }
     total_irradiance += weighted_irradiance / max(1e-20, w_sum);

     irradiance_output_tex[px] = float4(total_irradiance, 1);
}