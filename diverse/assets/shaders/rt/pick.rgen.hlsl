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

// [[vk::binding(0)]] RWTexture2D<float4> output_tex;

[[vk::binding(0)]] cbuffer _ {
    uint2 ray_pixel;
};

[[vk::binding(1)]] RWStructuredBuffer<int> output_buffer;

[shader("raygeneration")]
void main() {
    const uint2 px = ray_pixel;//DispatchRaysIndex().xy;

    output_buffer[0] = int2(-1, -1);
    {
        float4 radiance_sample_count_packed = 0.0;
        uint rng = hash_combine2(hash_combine2(px.x, hash1(px.y)), frame_constants.frame_index);

        static const uint sample_count = 1;

        float px_off0 = 0.5;
        float px_off1 = 0.5;

        const float2 pixel_center = px + float2(px_off0, px_off1);
        const float2 uv = pixel_center / DispatchRaysDimensions().xy;

        RayDesc outgoing_ray;
        {
            const ViewRayContext view_ray_context = ViewRayContext::from_uv(uv);
            const float3 ray_dir_ws = view_ray_context.ray_dir_ws();

            outgoing_ray = new_ray(
                view_ray_context.ray_origin_ws(),
                normalize(ray_dir_ws.xyz),
                0.0,
                FLT_MAX
            );
        }

        float3 throughput = 1.0.xxx;
        float3 total_radiance = 0.0.xxx;

        float roughness_bias = 0.0;

        RayCone ray_cone = pixel_ray_cone_from_image_height(
            DispatchRaysDimensions().y
        );

        // Bias for texture sharpness
        ray_cone.spread_angle *= 0.3;


        outgoing_ray.TMax = MAX_RAY_LENGTH;
        GbufferPathVertex primary_hit = GbufferRaytrace::with_ray(outgoing_ray)
                                            .with_cone(ray_cone)
                                            .with_cull_back_faces(false)
                                            .with_path_length(1)
                                            .trace(acceleration_structure);

        if (primary_hit.is_hit) {

            GbufferData gbuffer = primary_hit.gbuffer_packed.unpack();
            output_buffer[0] = uint2(gbuffer.albedo.x, gbuffer.albedo.y);
        }
    }
}