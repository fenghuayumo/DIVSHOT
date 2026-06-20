#ifndef PATH_TRACER_PATH_INTEGRATOR_HLSL
#define PATH_TRACER_PATH_INTEGRATOR_HLSL

#include "../inc/math.hlsl"
#include "../materials/bsdf_scatter.hlsl"
#include "../materials/shading_surface.hlsl"
#include "../lighting/lighting_config.hlsl"
#include "../lighting/nee.hlsl"
#include "path_state.hlsl"

namespace PathTracer
{
    bool valid_radiance(float3 v)
    {
        return all(v >= 0.0) && all(v < 1e20);
    }

    void setup_primary_ray(inout PathTraceState path, float2 uv, uint2 dims)
    {
        ViewRayContext view_ray_context = ViewRayContext::from_uv(uv);
        path.ray = new_ray(
            view_ray_context.ray_origin_ws(),
            view_ray_context.ray_dir_ws(),
            0.0,
            FLT_MAX);
        path.ray_cone = pixel_ray_cone_from_image_height(dims.y);
        path.ray_cone.spread_angle *= 0.3;
    }

    void handle_miss(inout PathTraceState path)
    {
        float mis_weight = 1.0;
        if (path.previous_bsdf_pdf > 0.0 && path.previous_environment_nee_pdf > 0.0)
            mis_weight = nee_balance_mis(path.previous_bsdf_pdf, path.previous_environment_nee_pdf);

        float3 environment_emission = sample_environment_light(
            path.ray.Direction,
            environment_miss_sample_mip_level(path.diffuse_bounces)) * mis_weight;
        environment_emission = apply_firefly_filter(
            environment_emission,
            LIGHTING_FIREFLY_FILTER_THRESHOLD,
            path.firefly_filter_k);

        path.radiance += path.throughput * environment_emission;
        path.active = false;
    }

    bool generate_scatter_ray(
        inout PathTraceState path,
        ShadingSurface surface,
        out BsdfSampleResult bsdf_sample)
    {
        bsdf_sample = surface.bsdf.sample(
            surface.wo,
            float3(
                uniform_rand_float(path.rng),
                uniform_rand_float(path.rng),
                uniform_rand_float(path.rng)));

        if (!bsdf_sample.is_valid())
            return false;

        path.throughput *= bsdf_sample.value;

        if (!valid_radiance(path.throughput) || max3(path.throughput.x, path.throughput.y, path.throughput.z) <= 1e-5)
            return false;

        float3 next_direction = normalize(mul(surface.tangent_to_world, bsdf_sample.wi));
        path.previous_bsdf_pdf = bsdf_sample.pdf;
        path.previous_environment_nee_pdf = environment_nee_pdf(surface.normal, next_direction)
            / (float)(frame_constants.scene_lights_count + 1);
        path.firefly_filter_k = lighting_update_firefly_filter_k(path.firefly_filter_k, bsdf_sample.pdf);

        if (bsdf_scatter_is_diffuse_bounce(
                bsdf_sample.selected_lobe,
                surface.bsdf.roughness,
                LIGHTING_SPECULAR_ROUGHNESS_DIFFUSE_THRESHOLD))
        {
            path.diffuse_bounces++;
        }

        float3 offset_normal = bsdf_scatter_offset_normal(surface.normal, bsdf_sample.selected_lobe);
        path.ray = new_ray(
            offset_position(surface.position, offset_normal),
            next_direction,
            1e-3,
            FLT_MAX);

        return true;
    }

    bool apply_russian_roulette(inout PathTraceState path)
    {
        if (path.path_length < PATH_TRACER_RR_START_PATH_LENGTH)
            return false;

        float continue_p = saturate(max3(path.throughput.x, path.throughput.y, path.throughput.z));
        if (uniform_rand_float(path.rng) > continue_p)
            return true;

        path.throughput /= max(continue_p, 1e-3);
        return false;
    }

    void handle_hit(
        RaytracingAccelerationStructure acceleration_structure,
        inout PathTraceState path,
        GbufferPathVertex hit)
    {
        path.ray_cone = path.ray_cone.propagate(0.0, hit.ray_t);

        if (path.path_length == 0)
        {
            float4 clip = mul(float4(hit.position, 1.0), frame_constants.view_constants.world_to_clip);
            path.depth = clip.z / clip.w;
        }

        GbufferData gbuffer = hit.gbuffer_packed.unpack();
        if (dot(gbuffer.normal, path.ray.Direction) > 0.0)
            gbuffer.normal *= -1.0;

        path.radiance += path.throughput * gbuffer.emissive;

        if (hit.material_id == 0xffffffff)
        {
            path.active = false;
            return;
        }

        ShadingSurface surface = ShadingSurface::from_gbuffer(
            hit.position,
            gbuffer,
            -path.ray.Direction,
            materials_buffer[hit.material_id]);
        if (!surface.has_active_bsdf())
        {
            path.active = false;
            return;
        }

        path.radiance += path.throughput * DirectLighting::evaluate_sun(
            acceleration_structure, surface, path.rng);
        path.radiance += path.throughput * DirectLighting::evaluate_neeat(
            acceleration_structure, surface, path.firefly_filter_k, path.rng);

        BsdfSampleResult bsdf_sample;
        if (!generate_scatter_ray(path, surface, bsdf_sample) || apply_russian_roulette(path))
            path.active = false;
    }

    void trace_bounce(RaytracingAccelerationStructure acceleration_structure, inout PathTraceState path)
    {
        GbufferPathVertex hit = GbufferRaytrace::with_ray(path.ray)
            .with_cone(path.ray_cone)
            .with_path_length(path.path_length)
            .with_cull_back_faces(false)
            .trace(acceleration_structure);

        if (!hit.is_hit)
            handle_miss(path);
        else
            handle_hit(acceleration_structure, path, hit);

        path.path_length++;
    }

    void commit_pixel(
        PathTraceState path,
        float4 prev_output,
        RWTexture2D<float4> output_tex,
        RWTexture2D<float> depth_tex)
    {
        float3 radiance = valid_radiance(path.radiance) ? path.radiance : 0.0.xxx;
        float sample_count = prev_output.w + 1.0;

        output_tex[path.pixel] = float4(lerp(prev_output.rgb, radiance, 1.0 / sample_count), sample_count);
        depth_tex[path.pixel] = path.depth;
    }
}

#endif // PATH_TRACER_PATH_INTEGRATOR_HLSL
