#ifndef MATERIALS_MATERIAL_TYPES_HLSL
#define MATERIALS_MATERIAL_TYPES_HLSL

#include "../inc/math.hlsl"
#include "material_data.hlsl"

/// Material properties at surface (after texture sampling)
/// This is used during shading/BSDF evaluation
struct MaterialProperties
{
    float3 shading_normal;
    float3 geometry_normal;
    float3 diffuse_albedo;
    float3 specular_f0;
    float3 emissive_color;
    float  opacity;
    float  roughness;
    float  base_weight;
    float  specular_weight;
    float  anisotropy;
    float  fuzz_weight;
    float3 fuzz_color;
    float  fuzz_roughness;
    float3 base_color;
    float  metalness;
    float  transmission;
    float  ior;
    uint   flags;

    static MaterialProperties make()
    {
        MaterialProperties result;
        result.shading_normal = float3(0, 1, 0);
        result.geometry_normal = float3(0, 1, 0);
        result.diffuse_albedo = 0;
        result.specular_f0 = 0.04;
        result.emissive_color = 0;
        result.opacity = 1;
        result.roughness = 0.5;
        result.base_weight = 1;
        result.specular_weight = 1;
        result.anisotropy = 0;
        result.fuzz_weight = 0;
        result.fuzz_color = 1;
        result.fuzz_roughness = 0.6;
        result.base_color = 1;
        result.metalness = 0;
        result.transmission = 0;
        result.ior = 1.5;
        result.flags = 0;
        return result;
    }
};

/// Helper to compute dielectric F0 from IOR
float dielectric_f0_from_ior(float ior)
{
    const float f = (ior - 1.0) / (ior + 1.0);
    return f * f;
}

/// Compute specular F0 following OpenPBR spec
float3 compute_specular_f0(
    float3 base_color,
    float metalness,
    float specular_ior,
    float3 specular_color,
    float specular_weight,
    bool use_openpbr)
{
    const float dielectric_f0 = dielectric_f0_from_ior(specular_ior);
    const float3 specular_tint = use_openpbr ? specular_color : float3(1.0, 1.0, 1.0);
    const float3 f0_dielectric = dielectric_f0 * specular_weight * specular_tint;
    return lerp(f0_dielectric, base_color, metalness);
}

#endif // MATERIALS_MATERIAL_TYPES_HLSL
