#ifndef LIGHTING_ENV_LIGHT_HLSL
#define LIGHTING_ENV_LIGHT_HLSL

#include "../inc/math.hlsl"

/// Sample environment map light
float3 sample_environment_light(float3 direction)
{
    return sky_cube_tex.SampleLevel(sampler_llc, direction, 0).rgb;
}

/// PDF for environment map sampling (uniform)
float environment_map_pdf(float3 direction, uint cube_map_resolution)
{
    // Uniform sampling over sphere
    return 1.0 / (4.0 * M_PI);
}

/// Sample environment map with MIS
float3 sample_environment_with_mis(
    float3 direction,
    float bsdf_pdf,
    float light_selection_pdf,
    out float mis_weight)
{
    float3 radiance = sample_environment_light(direction);
    float env_pdf = environment_map_pdf(direction, 512);  // Assume 512x512

    // MIS weight (balance heuristic)
    mis_weight = env_pdf / (env_pdf + bsdf_pdf * light_selection_pdf);

    return radiance * mis_weight;
}

#endif // LIGHTING_ENV_LIGHT_HLSL
