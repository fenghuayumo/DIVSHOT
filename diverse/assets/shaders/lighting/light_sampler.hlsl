#ifndef LIGHTING_LIGHT_SAMPLER_HLSL
#define LIGHTING_LIGHT_SAMPLER_HLSL

#include "light_types.hlsl"
#include "../inc/random.hlsl"

/// Light sampler for NEE (Next Event Estimation)
struct LightSampler
{
    uint total_light_count;
    uint rng_state;

    static LightSampler make(uint rng_init_state)
    {
        LightSampler s;
        s.total_light_count = 0;  // To be set from constant buffer
        s.rng_state = rng_init_state;
        return s;
    }

    /// Sample a light by index
    LightSample sample_light(uint light_index, float3 shading_point, float2 urand)
    {
        LightSample result = LightSample::make();

        if (light_index >= total_light_count)
            return result;

        // TODO: Implement proper light sampling from light buffer
        // For now, return invalid sample
        result.light_index = light_index;
        result.selection_pdf = 1.0 / max(1.0, (float)total_light_count);

        return result;
    }

    /// Sample a random light
    LightSample sample_random_light(float3 shading_point, float2 urand)
    {
        uint light_index = uint_to_u01_float(hash1_mut(rng_state)) * total_light_count;
        return sample_light(light_index, shading_point, urand);
    }
};

#endif // LIGHTING_LIGHT_SAMPLER_HLSL
