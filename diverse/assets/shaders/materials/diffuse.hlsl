#ifndef MATERIALS_DIFFUSE_HLSL
#define MATERIALS_DIFFUSE_HLSL

#include "../inc/math.hlsl"

// Frostbite Diffuse BRDF
// Based on "Moving Frostbite to PBR v3.2"

struct FrostbiteDiffuseBrdf
{
    float3 albedo;
    float roughness;

    // Energy-conserving diffuse with Fresnel attenuation
    float3 evaluate(float3 wo, float3 wi)
    {
        if (wo.z <= 0.0 || wi.z <= 0.0)
            return 0.0;

        // Disney/Burley diffuse approximation
        float ndotl = saturate(wi.z);
        float ndotv = saturate(wo.z);
        float ldoth = saturate(dot(wi, normalize(wo + wi)));

        float fd90 = 0.5 + 2.0 * ldoth * ldoth * roughness;
        float light_scatter = fresnel_schlick(ndotl, 1.0, fd90);
        float view_scatter = fresnel_schlick(ndotv, 1.0, fd90);

        return albedo * M_FRAC_1_PI * light_scatter * view_scatter;
    }

    float pdf(float3 wo, float3 wi)
    {
        return wi.z > 0.0 ? M_FRAC_1_PI : 0.0;
    }

    // Cosine-weighted hemisphere sampling
    float3 sample(float3 wo, float2 urand, out float3 wi, out float pdf_val)
    {
        float phi = urand.x * M_TAU;
        float cos_theta = sqrt(max(0.0, 1.0 - urand.y));
        float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

        wi = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
        pdf_val = M_FRAC_1_PI;

        return evaluate(wo, wi);
    }

    // Simple Schlick Fresnel approximation
    float fresnel_schlick(float ndotv, float f0, float fd90)
    {
        float f = pow(1.0 - ndotv, 5.0);
        return f0 + (fd90 - f0) * f;
    }
};

#endif // MATERIALS_DIFFUSE_HLSL
