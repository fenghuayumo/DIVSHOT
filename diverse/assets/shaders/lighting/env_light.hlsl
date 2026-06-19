#ifndef LIGHTING_ENV_LIGHT_HLSL
#define LIGHTING_ENV_LIGHT_HLSL

#include "../inc/math.hlsl"

static const float ENVIRONMENT_LIGHT_SELECTION_PDF = 1.0;

struct EnvironmentLightSample
{
    float3 Li;
    float3 direction;
    float pdf;
    float distance;

    static EnvironmentLightSample invalid()
    {
        EnvironmentLightSample ret;
        ret.Li = 0.0.xxx;
        ret.direction = float3(0.0, 0.0, 1.0);
        ret.pdf = 0.0;
        ret.distance = FLT_MAX;
        return ret;
    }

    bool valid()
    {
        return pdf > 0.0 && any(Li > 0.0);
    }
};

/// Sample environment map light
float3 sample_environment_light(float3 direction)
{
    return sky_cube_tex.SampleLevel(sampler_llc, direction, 0).rgb;
}

float nee_balance_mis(float this_pdf, float other_pdf)
{
    return this_pdf / max(this_pdf + other_pdf, 1e-8);
}

float3 sample_cosine_hemisphere_direction(float2 urand)
{
    float phi = M_TAU * urand.x;
    float cos_theta = sqrt(max(0.0, 1.0 - urand.y));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    return float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

float environment_nee_pdf(float3 normal, float3 direction)
{
    return max(0.0, dot(normalize(normal), normalize(direction))) * M_FRAC_1_PI;
}

EnvironmentLightSample sample_environment_light_nee(float3 normal, float2 urand)
{
    EnvironmentLightSample ret;
    float3x3 tangent_to_world = build_orthonormal_basis(normalize(normal));
    ret.direction = normalize(mul(tangent_to_world, sample_cosine_hemisphere_direction(urand)));
    ret.pdf = environment_nee_pdf(normal, ret.direction) * ENVIRONMENT_LIGHT_SELECTION_PDF;
    ret.distance = FLT_MAX;
    ret.Li = sample_environment_light(ret.direction);
    return ret;
}

float3 apply_firefly_filter(float3 signal_in, float threshold, float filter_k)
{
    float max_signal = (signal_in.x + signal_in.y + signal_in.z) * (1.0 / 3.0);
    float cap = threshold * max(filter_k, 1e-5);
    if (max_signal > cap)
        signal_in *= cap / max(max_signal, 1e-8);

    return signal_in;
}

#endif // LIGHTING_ENV_LIGHT_HLSL
