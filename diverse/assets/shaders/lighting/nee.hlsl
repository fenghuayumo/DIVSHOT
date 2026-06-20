#ifndef LIGHTING_NEE_HLSL
#define LIGHTING_NEE_HLSL

#include "../inc/sun.hlsl"
#include "../materials/shading_surface.hlsl"
#include "light_types.hlsl"
#include "lighting_config.hlsl"
#include "light_sampling.hlsl"

namespace DirectLighting
{
    float3 evaluate_sun(
        RaytracingAccelerationStructure acceleration_structure,
        ShadingSurface surface,
        inout Random rng)
    {
        float solid_angle_pdf = sun_disk_solid_angle_pdf();
        if (solid_angle_pdf <= 0.0)
            return 0.0.xxx;

        float3 direction = sample_sun_direction(
            float2(uniform_rand_float(rng), uniform_rand_float(rng)),
            true);
        float3 Li = sun_color_in_direction(direction);

        float3 wi = mul(direction, surface.tangent_to_world);
        if (wi.z <= 0.0)
            return 0.0.xxx;

        RayDesc shadow_ray = new_ray(
            offset_position(surface.position, surface.normal),
            direction,
            1e-3,
            FLT_MAX);
        if (!rt_visibility(acceleration_structure, shadow_ray))
            return 0.0.xxx;

        BsdfEvalData eval_data = BsdfEvalData::create(surface.wo, wi, float3(0, 0, 1), float3(0, 0, 1));
        BsdfEvalResult eval = surface.bsdf.evaluate(eval_data);
        return eval.value * Li / max(solid_angle_pdf, 1e-8);
    }

    float3 evaluate_neeat(
        RaytracingAccelerationStructure acceleration_structure,
        ShadingSurface surface,
        float firefly_filter_k,
        inout Random rng)
    {
        uint scene_light_count = frame_constants.scene_lights_count;
        uint direct_light_choice_count = scene_light_count + 1;
        if (direct_light_choice_count == 0)
            return 0.0.xxx;

        float light_selection_pdf = 1.0 / (float)direct_light_choice_count;
        NeeReservoir reservoir = NeeReservoir::make();

        [unroll]
        for (uint candidate_index = 0; candidate_index < LIGHTING_NEE_CANDIDATE_COUNT; ++candidate_index)
        {
            float light_selector = uniform_rand_float(rng);
            uint light_choice = min((uint)(light_selector * direct_light_choice_count), direct_light_choice_count - 1);

            LightSample candidate;
            if (light_choice < scene_light_count)
            {
                candidate = sample_scene_light(
                    light_choice,
                    surface.position,
                    float2(uniform_rand_float(rng), uniform_rand_float(rng)),
                    light_selection_pdf);
            }
            else
            {
                candidate = sample_environment_light_candidate(
                    surface.normal,
                    float2(uniform_rand_float(rng), uniform_rand_float(rng)),
                    light_selection_pdf);
            }

            float light_pdf = candidate.combined_pdf();
            if (light_pdf <= 0.0)
                continue;

            float3 wi = mul(candidate.direction, surface.tangent_to_world);
            if (wi.z <= 0.0)
                continue;

            BsdfEvalData eval_data = BsdfEvalData::create(surface.wo, wi, float3(0, 0, 1), float3(0, 0, 1));
            BsdfEvalResult eval = surface.bsdf.evaluate(eval_data);
            if (eval.pdf <= 0.0 || !any(eval.value > 0.0))
                continue;

            float candidate_weight = max3(candidate.Li.x, candidate.Li.y, candidate.Li.z) * eval.pdf / max(light_pdf, 1e-8);
            reservoir.add(uniform_rand_float(rng), candidate, candidate_weight);
        }

        if (!reservoir.valid())
            return 0.0.xxx;

        LightSample light_sample = reservoir.sample;
        float light_pdf = light_sample.combined_pdf();
        if (light_pdf <= 0.0)
            return 0.0.xxx;

        float3 wi = mul(light_sample.direction, surface.tangent_to_world);
        if (wi.z <= 0.0)
            return 0.0.xxx;

        RayDesc shadow_ray = new_ray(
            offset_position(surface.position, surface.normal),
            light_sample.direction,
            1e-3,
            max(1e-3, light_sample.distance * 0.9985));
        if (!rt_visibility(acceleration_structure, shadow_ray))
            return 0.0.xxx;

        BsdfEvalData eval_data = BsdfEvalData::create(surface.wo, wi, float3(0, 0, 1), float3(0, 0, 1));
        BsdfEvalResult eval = surface.bsdf.evaluate(eval_data);
        if (eval.pdf <= 0.0 || !any(eval.value > 0.0))
            return 0.0.xxx;

        float mis_weight = light_sample.light_sampleable_by_bsdf
            ? nee_balance_mis(light_pdf, eval.pdf)
            : 1.0;

        float neeat_correction = reservoir.weight_sum
            / max(reservoir.selected_weight * (float)LIGHTING_NEE_CANDIDATE_COUNT, 1e-8);
        float3 contribution = eval.value
            * light_sample.Li
            * mis_weight
            * neeat_correction
            / max(light_pdf, 1e-8);

        return apply_firefly_filter(contribution, LIGHTING_FIREFLY_FILTER_THRESHOLD, firefly_filter_k);
    }
}

#endif // LIGHTING_NEE_HLSL
