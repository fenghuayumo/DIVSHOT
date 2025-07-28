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

[[vk::binding(0)]] StructuredBuffer<uint> image_mask;
[[vk::binding(1)]] RWStructuredBuffer<int> output_buffer;
[[vk::binding(2)]] cbuffer _ {
    int surface_width;
    int surface_height;
    int paint_tex_width;
    int paint_tex_height;
};
[shader("raygeneration")]
void main() {
    const uint2 px = DispatchRaysIndex().xy;
    const int pixel_id = px.y * surface_width + px.x;
    const bool inhit = image_mask[pixel_id] > 0;
    if (!inhit) {
        return;
    }
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

        outgoing_ray.TMax = 1e10;
        CustomRayPayload primary_hit = TextureRaytrace::with_ray(outgoing_ray)
                                            .with_cone(ray_cone)
                                            .with_cull_back_faces(false)
                                            .trace(acceleration_structure);

        if (primary_hit.is_hit()) {
            float2 uv = primary_hit.texture_pos - int2(primary_hit.texture_pos);
            int2 tex_dim = int2(paint_tex_width, paint_tex_height);
            int2 tex_pos0 = int2(primary_hit.texture_pos);
            int2 tex_pos1 = max(int2(tex_pos0 + int2(1, 0)), tex_dim);
            int2 tex_pos2 = max(int2(tex_pos0 + int2(0, 1)), tex_dim);
            int2 tex_pos3 = max(int2(tex_pos0 + int2(1, 1)), tex_dim);
            int tex_pos0_id = tex_pos0.y * tex_dim.x + tex_pos0.x;
            int tex_pos1_id = tex_pos1.y * tex_dim.x + tex_pos1.x;
            int tex_pos2_id = tex_pos2.y * tex_dim.x + tex_pos2.x;
            int tex_pos3_id = tex_pos3.y * tex_dim.x + tex_pos3.x;
            InterlockedAdd(output_buffer[tex_pos0_id], 1);
            InterlockedAdd(output_buffer[tex_pos1_id], 1);
            InterlockedAdd(output_buffer[tex_pos2_id], 1);
            InterlockedAdd(output_buffer[tex_pos3_id], 1);
        }
    }
}