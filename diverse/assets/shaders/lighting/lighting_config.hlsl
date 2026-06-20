#ifndef LIGHTING_LIGHTING_CONFIG_HLSL
#define LIGHTING_LIGHTING_CONFIG_HLSL

#include "../inc/math_const.hlsl"

#ifndef LIGHTING_NEE_CANDIDATE_COUNT
#define LIGHTING_NEE_CANDIDATE_COUNT 8
#endif

#ifndef LIGHTING_FIREFLY_FILTER_THRESHOLD
#define LIGHTING_FIREFLY_FILTER_THRESHOLD 256.0
#endif

#ifndef LIGHTING_SPECULAR_ROUGHNESS_DIFFUSE_THRESHOLD
#define LIGHTING_SPECULAR_ROUGHNESS_DIFFUSE_THRESHOLD 0.25
#endif

float lighting_update_firefly_filter_k(float current_k, float bounce_pdf)
{
    if (bounce_pdf <= 0.0)
        return current_k;

    float angle = 2.0 * acos(max(-1.0, 1.0 - (1.0 / bounce_pdf) / (2.0 * M_PI)));
    float p = 32.0 / (32.0 + angle * angle);
    return max(1e-5, current_k * p);
}

#endif // LIGHTING_LIGHTING_CONFIG_HLSL
