#ifndef MATERIALS_FUZZ_HLSL
#define MATERIALS_FUZZ_HLSL

#include "../inc/math.hlsl"

// Fuzz (sheen) BRDF for cloth/fabric-like materials
// Based on OpenPBR specification

struct FuzzBrdf
{
    float3 color;
    float weight;
    float roughness;

    // Charlie/sheen BRDF approximation
    float3 evaluate(float3 wo, float3 wi)
    {
        if (wo.z <= 0.0 || wi.z <= 0.0)
            return 0.0;

        float ndotl = saturate(wi.z);
        float ndotv = saturate(wo.z);

        // Simplified Charlie distribution
        float ndoth = saturate(dot(normalize(wo + wi), float3(0, 0, 1)));
        float sheen = pow(ndoth, roughness) * (1.0 + roughness);

        // View-dependent sheen intensity
        float view_factor = pow(1.0 - ndotv, 3.0);
        float light_factor = pow(1.0 - ndotl, 3.0);

        return color * weight * sheen * view_factor * light_factor * M_FRAC_1_PI;
    }

    float pdf(float3 wo, float3 wi)
    {
        return wi.z > 0.0 ? M_FRAC_1_PI : 0.0;
    }

    // Cosine-weighted sampling with sheen distribution
    float3 sample(float3 wo, float2 urand, out float3 wi, out float pdf_val)
    {
        float phi = urand.x * M_TAU;
        float cos_theta = pow(max(0.0, 1.0 - urand.y), 1.0 / (roughness + 1.0));
        float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

        wi = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
        pdf_val = M_FRAC_1_PI;

        return evaluate(wo, wi);
    }
};

#endif // MATERIALS_FUZZ_HLSL
