#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace diverse
{
    const uint PBR_WORKFLOW_SEPARATE_TEXTURES = 0;
    const uint PBR_WORKFLOW_METALLIC_ROUGHNESS = 1;
    const uint PBR_WORKFLOW_SPECULAR_GLOSINESS = 2;

    struct MaterialProperties
    {
        // GPU ABI: keep this layout byte-for-byte in sync with
        // diverse/assets/shaders/materials/material_data.hlsl.
        glm::vec4 base_color_mult = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        uint albedo_map = 0xffffffff;
        uint metallic_map = 0xffffffff;
        uint normal_map = 0xffffffff;
        uint emissive_map = 0xffffffff;
        uint roughness_map = 0xffffffff;
        uint ao_map = 0xffffffff;
        uint transmission_map = 0xffffffff;
        uint normal_detail_map = 0xffffffff;

        float roughness_mult = 0.7f;
        float metalness_factor = 0.7f;
        float ior = 1.5f;
        float anisotropy = 0.0f;

        float specular_weight = 1.0f;
        float specular_ior = 1.5f;
        float transmission_weight = 0.0f;
        float thickness = 0.0f;

        float fuzz_weight = 0.0f;
        float fuzz_roughness = 0.5f;
        float subsurface_scale = 0.0f;
        float sheen = 0.0f;

        glm::vec4 specular_color = glm::vec4(1.0f);
        glm::vec4 fuzz_color = glm::vec4(1.0f);
        glm::vec4 sheen_color = glm::vec4(1.0f);
        glm::vec4 transmission_color = glm::vec4(1.0f);

        std::array<std::array<f32, 6>, 6> map_transforms = {
            std::array<f32, 6>{1.0, 0.0, 0.0, 1.0, 0.0, 0.0},
            {1.0, 0.0, 0.0, 1.0, 0.0, 0.0},
            {1.0, 0.0, 0.0, 1.0, 0.0, 0.0},
            {1.0, 0.0, 0.0, 1.0, 0.0, 0.0},
            {1.0, 0.0, 0.0, 1.0, 0.0, 0.0},
            {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}};

        float roughness_map_factor = 0.0f;
        float metallic_map_factor = 0.0f;
        float normal_map_factor = 0.0f;
        float ao_map_factor = 0.0f;
        float emissive_map_factor = 0.0f;
        float transmission_map_factor = 0.0f;
        uint work_flow = 0;
        uint32_t material_flags = 0;

        glm::vec3 emissive = glm::vec3(0.0f, 0.0f, 0.0f);
        float alpha_cutoff = 0.4f;
        float ao_mult = 1.0f;
        uint padding[3] = {};
    };

    static_assert(std::is_trivially_copyable_v<MaterialProperties>);
    static_assert(sizeof(glm::vec3) == sizeof(float) * 3);
    static_assert(offsetof(MaterialProperties, base_color_mult) == 0);
    static_assert(offsetof(MaterialProperties, roughness_mult) == 48);
    static_assert(offsetof(MaterialProperties, specular_color) == 96);
    static_assert(offsetof(MaterialProperties, map_transforms) == 160);
    static_assert(offsetof(MaterialProperties, roughness_map_factor) == 304);
    static_assert(offsetof(MaterialProperties, emissive) == 336);
    static_assert(sizeof(MaterialProperties) == 368);
}
