#include "../inc/color.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/rt.hlsl"
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


[[vk::binding(0, 3)]] RaytracingAccelerationStructure acceleration_structure;

[[vk::binding(0)]] Texture2D<float4> radiance_tex;
[[vk::binding(1)]] Texture2D<uint2> reservoir_input_tex;
[[vk::binding(2)]] Texture2D<float4> gbuffer_tex;
[[vk::binding(3)]] Texture2D<float> depth_tex;
[[vk::binding(4)]] Texture2D<float4> half_view_normal_tex;
[[vk::binding(5)]] Texture2D<float> half_depth_tex;
[[vk::binding(6)]] Texture2D<float4> ssao_tex;
[[vk::binding(7)]] Texture2D<uint4> temporal_reservoir_packed_tex;
[[vk::binding(8)]] RWTexture2D<float4> eval_output_tex;
[[vk::binding(9)]] cbuffer _ {
    float4 gbuffer_tex_size;
    float4 output_tex_size;
};

static float ggx_ndf_unnorm(float a2, float cos_theta) {
	float denom_sqrt = cos_theta * cos_theta * (a2 - 1.0) + 1.0;
	return a2 / (denom_sqrt * denom_sqrt);
}

uint2 reservoir_payload_to_px(uint payload) {
    return uint2(payload & 0xffff, payload >> 16);
}


[shader("raygeneration")]
void main() {
    const uint2 px = DispatchRaysIndex().xy;
    const int2 hi_px_offset = HALFRES_SUBSAMPLE_OFFSET;
    const uint2 hi_px = px * 2 + hi_px_offset;
    
    float depth = depth_tex[hi_px];

    if (0.0 == depth) {
        eval_output_tex[px] = 0;
        return;
    }

    const float2 uv = get_uv(hi_px, gbuffer_tex_size);
    const ViewRayContext view_ray_context = ViewRayContext::from_uv_and_biased_depth(uv, depth);
    
    GbufferData gbuffer = GbufferDataPacked::from_uint4(asuint(gbuffer_tex[hi_px])).unpack();
    const float3 center_normal_ws = gbuffer.normal;

    Reservoir1spp r = Reservoir1spp::from_raw(reservoir_input_tex[px]);
    const uint2 spx = reservoir_payload_to_px(r.payload);

    const TemporalReservoirOutput spx_packed = TemporalReservoirOutput::from_raw(temporal_reservoir_packed_tex[spx]);

    const float2 spx_uv = get_uv(
        spx * 2 + HALFRES_SUBSAMPLE_OFFSET,
        gbuffer_tex_size);
    const ViewRayContext spx_ray_ctx = ViewRayContext::from_uv_and_depth(spx_uv, spx_packed.depth);
    const float3 spx_pos_ws = spx_ray_ctx.ray_hit_ws();

    float3 contribution = 0.0;
    const float3 trace_origin_ws =
        //view_ray_context.ray_hit_ws();
        view_ray_context.biased_secondary_ray_origin_ws();
    {
        const float3 hit_ws = spx_packed.ray_hit_offset_ws + spx_ray_ctx.ray_hit_ws();
        const float3 sample_offset = hit_ws - view_ray_context.ray_hit_ws();
        const float sample_dist = length(sample_offset);
        const float3 sample_dir = sample_offset / sample_dist;

        const float geometric_term =
            // TODO: fold the 2 into the PDF
            2 * max(0.0, dot(center_normal_ws, sample_dir));

        float3 radiance = radiance_tex[spx].xyz;
        // const float3 trace_vec = sample_dir;

        if (!rt_is_shadowed(
            acceleration_structure,
            new_ray(
                trace_origin_ws,
                sample_dir,
                0.0,
                min(5 *length(spx_pos_ws - trace_origin_ws), sample_dist * 0.999)
        )))
        {
            contribution = radiance * geometric_term * r.W;
        }
        // if( RESTIR_FEEDBACK_VIS )
        // {

        // }
    }
    eval_output_tex[px] = float4(contribution,1.0);   
}