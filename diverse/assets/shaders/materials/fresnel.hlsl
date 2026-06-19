#ifndef MATERIALS_FRESNEL_HLSL
#define MATERIALS_FRESNEL_HLSL

#include "../inc/math.hlsl"

// Fresnel equations for BRDF

/// Schlick Fresnel approximation
float3 fresnel_schlick(float3 f0, float f90, float cos_theta)
{
    return lerp(f0, f90.xxx, pow(max(0.0, 1.0 - cos_theta), 5.0));
}

/// Schlick Fresnel approximation (scalar)
float fresnel_schlick(float f0, float f90, float cos_theta)
{
    return lerp(f0, f90, pow(max(0.0, 1.0 - cos_theta), 5.0));
}

/// Dielectric F0 from IOR
float dielectric_f0_from_ior(float ior)
{
    const float f = (ior - 1.0) / (ior + 1.0);
    return f * f;
}

/// Conductor Fresnel (complex IOR)
float3 fresnel_conductor(float3 f0, float cos_theta)
{
    return fresnel_schlick(f0, 1.0, cos_theta);
}

/// General Fresnel for layered materials
float3 fresnel_layered(float3 f0, float metalness, float cos_theta)
{
    float3 f_dielectric = dielectric_f0_from_ior(1.5);
    float3 f_metal = f0;
    return lerp(f_dielectric.xxx, f_metal, metalness);
}

#endif // MATERIALS_FRESNEL_HLSL
