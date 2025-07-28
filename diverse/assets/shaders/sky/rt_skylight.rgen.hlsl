#include "../inc/atmosphere.hlsl"
#include "../inc/bindless_textures.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/rt.hlsl"
#include "../inc/sun.hlsl"
#include "../inc/uv.hlsl"

#include "../inc/blue_noise.hlsl"
#include "../inc/math.hlsl"
#include "../inc/monte_carlo.hlsl"
#include "../rt/light/rt_light_common.hlsl"
#include "../inc/brdf.hlsl"
#include "../inc/brdf_lut.hlsl"
#include "../inc/layered_brdf.hlsl"

[[vk::binding(0, 3)]] RaytracingAccelerationStructure acceleration_structure;
[[vk::binding(0)]] Texture2D<float4> gbuffer_tex;
[[vk::binding(1)]] Texture2D<float> depth_tex;
[[vk::binding(2)]] Texture2D<float3> geometric_normal_tex;
[[vk::binding(3)]] Texture2D<float4> sky_light_tex;
[[vk::binding(4)]] Texture2D<float> sky_light_pdf;
[[vk::binding(5)]] RWTexture2D<float4> output_diff_tex;
// [[vk::binding(5)]] RWTexture2D<float4> output_spec_tex;

[[vk::binding(6)]] cbuffer _ {
    int samples_per_pixel;
    int sky_light_mip_count;
    float2 sky_light_inv_res;
};

#include "../rt/light/rt_skylight.hlsl"

[shader("raygeneration")]
void main() {
    const uint2 px = DispatchRaysIndex().xy;

    const float2 pixel_center = px + 0.5.xx;
    const float2 uv = pixel_center / DispatchRaysDimensions().xy;

    float z_over_w = depth_tex[px];
    if (0.0 == z_over_w) {
        output_diff_tex[px] = 1.0;
        return;
    }

    float4 pt_cs = float4(uv_to_cs(uv), z_over_w, 1.0);
    float4 pt_vs = mul(pt_cs, frame_constants.view_constants.sample_to_view);
    float4 pt_ws = mul(pt_vs,frame_constants.view_constants.view_to_world);
    pt_ws /= pt_ws.w;
    pt_vs /= pt_vs.w;

    float4 eye_ws = mul(float4(0, 0, 0, 1),frame_constants.view_constants.view_to_world);

    const float3 normal_vs = geometric_normal_tex[px] * 2.0 - 1.0;
    const float3 normal_ws = mul(float4(normal_vs, 0.0),frame_constants.view_constants.view_to_world).xyz;

    const float3 bias_dir = normal_ws;
    const float bias_amount = (-pt_vs.z + length(pt_ws.xyz)) * 1e-5;
    const float3 ray_origin = pt_ws.xyz + bias_dir * bias_amount;

    GbufferData gbuffer = GbufferDataPacked::from_uint4(asuint(gbuffer_tex[px])).unpack();
    const float3x3 tangent_to_world = build_orthonormal_basis(gbuffer.normal);

    float3 view_direction = normalize(eye_ws.xyz - ray_origin);
    float3 wo = mul(view_direction, tangent_to_world);
    float3 total_diffuse = 0.0;
    float dist = 1.0;
    //for (int sample_index = 0; sample_index < samples_per_pixel; sample_index++) 
    {

        float2 rand = blue_noise_for_pixel(px, frame_constants.frame_index).xy;

        SkyLightSample sample = skyLight_sample_light(rand);
        const float3 wi = mul(sample.direction, tangent_to_world);
        const bool is_shadowed = rt_is_shadowed(
            acceleration_structure,
            new_ray(
                ray_origin,
                sample.direction,
                0,
                FLT_MAX
        ));
                
        if (!is_shadowed) 
        {
            LayeredBrdf brdf = LayeredBrdf::from_gbuffer_ndotv(gbuffer, wo.z);
            // const BrdfValue brdf_value = brdf.evaluate(wo, wi);
            const float nol = max(0.0, wi.z);
            float3 diff = 0.0, spec = 0.0;
            brdf.eval_diff_spec(wo, wi,diff, spec);

            const float3 light_radiance = sample.radiance / sample.pdf;
            total_diffuse += diff * light_radiance * nol;
        }
    }

    output_diff_tex[px] = float4(total_diffuse, dist);
}