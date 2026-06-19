#ifndef MATERIALS_MATERIAL_DATA_HLSL
#define MATERIALS_MATERIAL_DATA_HLSL

// Material flags - compatible with RTXPT style
static const uint MATERIAL_FLAG_USE_SPECULAR_GLOSS_MODEL        = 0x00000001;
static const uint MATERIAL_FLAG_USE_METAL_ROUGH_OR_SPEC_TEXTURE = 0x00000004;
static const uint MATERIAL_FLAG_USE_BASE_OR_DIFFUSE_TEXTURE      = 0x00000008;
static const uint MATERIAL_FLAG_USE_EMISSIVE_TEXTURE             = 0x00000010;
static const uint MATERIAL_FLAG_USE_NORMAL_TEXTURE               = 0x00000020;
static const uint MATERIAL_FLAG_USE_TRANSMISSION_TEXTURE        = 0x00000080;
static const uint MATERIAL_FLAG_METALNESS_IN_RED_CHANNEL         = 0x00000100;
static const uint MATERIAL_FLAG_THIN_SURFACE                     = 0x00000200;
static const uint MATERIAL_FLAG_USE_OPENPBR_MATERIAL_MODEL        = 0x00008000;

/// MaterialData - Unified GPU material structure
/// This structure aligns with RTXPT for compatibility and works across
/// all rendering pipelines: Path Tracing, Hybrid, and Rasterization
/// CPU-side equivalent: MaterialProperties in source/assets/material.h
struct MaterialData
{
    // === Base color and textures (16 bytes) ===
    float4 base_color_mult;          // RGBA base color

    // === Texture bindings (32 bytes) ===
    uint albedo_map;
    uint metallic_map;
    uint normal_map;
    uint emissive_map;
    uint roughness_map;
    uint ao_map;
    uint transmission_map;
    uint normal_detail_map;

    // === Material parameters (64 bytes) ===
    float roughness_mult;
    float metalness_factor;
    float ior;
    float anisotropy;

    float specular_weight;
    float specular_ior;
    float transmission_weight;
    float thickness;

    float fuzz_weight;
    float fuzz_roughness;
    float subsurface_scale;
    float sheen;

    // === Extended colors (64 bytes) ===
    float4 specular_color;
    float4 fuzz_color;
    float4 sheen_color;
    float4 transmission_color;

    // === UV transforms (144 bytes) ===
    // 6 channels × 6 floats (2x2 matrix + offset per channel)
    float map_transforms[6 * 6];

    // === Texture factors (32 bytes) ===
    float roughness_map_factor;
    float metallic_map_factor;
    float normal_map_factor;
    float ao_map_factor;
    float emissive_map_factor;
    float transmission_map_factor;
    uint work_flow;
    uint material_flags;

    // === Emission, alpha and padding (32 bytes) ===
    float3 emissive;
    float alpha_cutoff;
    float ao_mult;
    uint3 padding;
};

#endif // MATERIALS_MATERIAL_DATA_HLSL
