#include "precompile.h"
#include "light_system.h"

#include "scene/components/light_component.h"
#include "scene/components/global_transform_component.h"
#include "renderer/light.h"
#include "utility/pack_utils.h"
#include "core/profiler.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{

    // Utility functions (moved from light components)
    namespace
    {
        uint float_to_uint(float v, float scale)
        {
            return static_cast<uint>(std::floor(v * scale + 0.5f));
        }

        float saturate(float x)
        {
            return glm::clamp(x, 0.0f, 1.0f);
        }

        uint float3_to_r8g8b8_unorm(float x, float y, float z)
        {
            return (float_to_uint(saturate(x), 0xFF) & 0xFF) |
                   ((float_to_uint(saturate(y), 0xFF) & 0xFF) << 8) |
                   ((float_to_uint(saturate(z), 0xFF) & 0xFF) << 16);
        }

        void pack_light_color(const glm::vec3& color, LightShaderData& light_info)
        {
            float max_radiance = std::max(color.x, std::max(color.y, color.z));

            if (max_radiance <= 0.0f)
                return;

            float log_radiance = (std::log2(max_radiance) - kPolymorphicLightMinLog2Radiance) /
                                (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance);
            log_radiance = saturate(log_radiance);
            uint32_t packed_radiance = std::min(uint32_t(std::ceil(log_radiance * 65534.0f)) + 1, 0xFFFFu);
            float unpacked_radiance = std::exp2((float(packed_radiance - 1) / 65534.0f) *
                                               (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance) +
                                               kPolymorphicLightMinLog2Radiance);

            light_info.colorTypeAndFlags |= float3_to_r8g8b8_unorm(
                color.x / unpacked_radiance,
                color.y / unpacked_radiance,
                color.z / unpacked_radiance);
            light_info.logRadiance = packed_radiance;
        }

        glm::vec2 unit_vector_to_octahedron(const glm::vec3& n)
        {
            float m = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
            glm::vec2 xy = { n.x, n.y };
            xy.x /= m;
            xy.y /= m;
            if (n.z <= 0.0f)
            {
                glm::vec2 signs;
                signs.x = xy.x >= 0.0f ? 1.0f : -1.0f;
                signs.y = xy.y >= 0.0f ? 1.0f : -1.0f;
                float x = (1.0f - std::abs(xy.y)) * signs.x;
                float y = (1.0f - std::abs(xy.x)) * signs.y;
                xy.x = x;
                xy.y = y;
            }
            return xy;
        }

        uint32_t pack_normalized_vector(const glm::vec3& x)
        {
            glm::vec2 xy = unit_vector_to_octahedron(x);
            xy.x = xy.x * 0.5f + 0.5f;
            xy.y = xy.y * 0.5f + 0.5f;
            uint x_val = float_to_uint(glm::clamp(xy.x, 0.0f, 1.0f), (1 << 16) - 1);
            uint y_val = float_to_uint(glm::clamp(xy.y, 0.0f, 1.0f), (1 << 16) - 1);
            uint packed_output = x_val;
            packed_output |= y_val << 16;
            return packed_output;
        }

        uint16_t fp32_to_fp16(float v)
        {
            union { uint ui; float f; } multiple = { 0x07800000 }; // 2**-112
            union { uint ui; float f; } biased_float;
            biased_float.f = v * multiple.f;
            const uint u = biased_float.ui;
            const uint sign = u & 0x80000000;
            uint body = u & 0x0fffffff;
            return static_cast<uint16_t>((sign >> 16) | (body >> 13)) & 0xFFFF;
        }

        glm::vec3 extract_direction(const glm::mat4& world_matrix)
        {
            // Forward vector from transformation matrix (column 2)
            return glm::normalize(glm::vec3(world_matrix[2]));
        }

        glm::vec3 extract_position(const glm::mat4& world_matrix)
        {
            return glm::vec3(world_matrix[3]);
        }
    }

    void LightSystem::prepare_directional_light(const glm::vec3& radiance, float intensity, float angular_size,
        const glm::mat4& world_matrix, LightShaderData& data)
    {
        glm::vec3 direction = -extract_direction(world_matrix); // Light points in -Z direction
        float half_angular_size_rad = 0.5f * glm::radians(angular_size);
        float solid_angle = 2.0f * glm::pi<float>() * (1.0f - std::cos(half_angular_size_rad));
        glm::vec3 color = radiance * intensity / solid_angle;

        data.colorTypeAndFlags = (static_cast<uint>(LightType::kDirectional) << kPolymorphicLightTypeShift);
        pack_light_color(color, data);
        data.direction1 = pack_normalized_vector(glm::normalize(direction));
        // Can't pass cosines of small angles reliably with fp16
        data.scalars = fp32_to_fp16(half_angular_size_rad) | (fp32_to_fp16(solid_angle) << 16);
    }

    void LightSystem::prepare_point_light(const glm::vec3& radiance, float intensity, float radius,
        const glm::mat4& world_matrix, LightShaderData& data)
    {
        glm::vec3 position = extract_position(world_matrix);

        if (radius == 0.0f)
        {
            glm::vec3 flux = radiance * intensity;

            data.colorTypeAndFlags = (static_cast<uint>(LightType::kPoint) << kPolymorphicLightTypeShift);
            pack_light_color(flux, data);
            data.center = position;
        }
        else
        {
            float projected_area = glm::pi<float>() * radius * radius;
            glm::vec3 color = radiance * intensity / projected_area;

            data.colorTypeAndFlags = (static_cast<uint>(LightType::kSphere) << kPolymorphicLightTypeShift);
            pack_light_color(color, data);
            data.center = position;
            data.scalars = fp32_to_fp16(radius);
        }
    }

    void LightSystem::prepare_spot_light(const glm::vec3& radiance, float intensity, float radius,
        float inner_angle, float outer_angle, int profile_texture_index,
        const glm::mat4& world_matrix, LightShaderData& data)
    {
        float projected_area = glm::pi<float>() * radius * radius;
        glm::vec3 color = radiance * intensity / projected_area;
        float softness = glm::clamp(1.0f - inner_angle / outer_angle, 0.0f, 1.0f);
        glm::vec3 position = extract_position(world_matrix);
        glm::vec3 direction = -extract_direction(world_matrix); // Light points in -Z direction

        data.colorTypeAndFlags = (static_cast<uint>(LightType::kSphere) << kPolymorphicLightTypeShift);
        data.colorTypeAndFlags |= kPolymorphicLightShapingEnableBit;
        pack_light_color(color, data);
        data.center = position;
        data.scalars = fp32_to_fp16(radius);
        data.primaryAxis = pack_normalized_vector(glm::normalize(direction));
        data.cosConeAngleAndSoftness = fp32_to_fp16(std::cos(glm::radians(outer_angle)));
        data.cosConeAngleAndSoftness |= fp32_to_fp16(softness) << 16;

        if (profile_texture_index >= 0)
        {
            data.iesProfileIndex = profile_texture_index;
            data.colorTypeAndFlags |= kPolymorphicLightIesProfileEnableBit;
        }
    }

    void LightSystem::prepare_rect_light(const glm::vec3& radiance, float intensity,
        float width, float height, float radius,
        const glm::mat4& world_matrix, LightShaderData& data)
    {
        glm::vec3 position = extract_position(world_matrix);
        float area = width * height;
        glm::vec3 color = radiance * intensity / area;

        data.colorTypeAndFlags = (static_cast<uint>(LightType::kRect) << kPolymorphicLightTypeShift);
        pack_light_color(color, data);
        data.center = position;
        // Encode width and height in scalars
        data.scalars = fp32_to_fp16(width) | (fp32_to_fp16(height) << 16);
    }

    void LightSystem::prepare_disk_light(const glm::vec3& radiance, float intensity, float radius,
        const glm::mat4& world_matrix, LightShaderData& data)
    {
        glm::vec3 position = extract_position(world_matrix);
        float area = glm::pi<float>() * radius * radius;
        glm::vec3 color = radiance * intensity / area;

        data.colorTypeAndFlags = (static_cast<uint>(LightType::kDisk) << kPolymorphicLightTypeShift);
        pack_light_color(color, data);
        data.center = position;
        data.scalars = fp32_to_fp16(radius);
    }

    void LightSystem::prepare_cylinder_light(const glm::vec3& radiance, float intensity,
        float radius, float length,
        const glm::mat4& world_matrix, LightShaderData& data)
    {
        glm::vec3 position = extract_position(world_matrix);
        float area = 2.0f * glm::pi<float>() * radius * length;
        glm::vec3 color = radiance * intensity / area;

        data.colorTypeAndFlags = (static_cast<uint>(LightType::kCylinder) << kPolymorphicLightTypeShift);
        pack_light_color(color, data);
        data.center = position;
        // Encode radius and length in scalars
        data.scalars = fp32_to_fp16(radius) | (fp32_to_fp16(length) << 16);
    }

    size_t LightSystem::get_light_count(entt::registry& registry)
    {
        size_t count = 0;

        // Count all light types - iterate over each view to count
        {
            auto view = registry.view<LightCommon, DirectionalLight, GlobalTransform>();
            for (auto entity : view) { (void)entity; count++; }
        }
        {
            auto view = registry.view<LightCommon, PointLight, GlobalTransform>();
            for (auto entity : view) { (void)entity; count++; }
        }
        {
            auto view = registry.view<LightCommon, SpotLight, GlobalTransform>();
            for (auto entity : view) { (void)entity; count++; }
        }
        {
            auto view = registry.view<LightCommon, RectLight, GlobalTransform>();
            for (auto entity : view) { (void)entity; count++; }
        }
        {
            auto view = registry.view<LightCommon, DiskLight, GlobalTransform>();
            for (auto entity : view) { (void)entity; count++; }
        }
        {
            auto view = registry.view<LightCommon, CylinderLight, GlobalTransform>();
            for (auto entity : view) { (void)entity; count++; }
        }

        return count;
    }

    size_t LightSystem::prepare_render_data(entt::registry& registry, LightShaderData* light_data, size_t max_lights)
    {
        DS_PROFILE_FUNCTION();

        size_t light_index = 0;

        // Helper lambda to add light
        auto add_light = [&](LightShaderData data) {
            if (light_index < max_lights)
            {
                light_data[light_index++] = data;
            }
        };

        // Process directional lights
        {
            auto view = registry.view<LightCommon, DirectionalLight, GlobalTransform>();
            for (auto entity : view)
            {
                if (light_index >= max_lights)
                    break;

                auto& light_common = view.get<LightCommon>(entity);
                auto& directional = view.get<DirectionalLight>(entity);
                auto& global = view.get<GlobalTransform>(entity);

                if (!light_common.enabled)
                    continue;

                LightShaderData data = {};
                prepare_directional_light(light_common.radiance, light_common.intensity,
                    directional.angular_size, global.world_matrix, data);
                add_light(data);
            }
        }

        // Process point lights
        {
            auto view = registry.view<LightCommon, PointLight, GlobalTransform>();
            for (auto entity : view)
            {
                if (light_index >= max_lights)
                    break;

                auto& light_common = view.get<LightCommon>(entity);
                auto& point = view.get<PointLight>(entity);
                auto& global = view.get<GlobalTransform>(entity);

                if (!light_common.enabled)
                    continue;

                LightShaderData data = {};
                prepare_point_light(light_common.radiance, light_common.intensity,
                    point.radius, global.world_matrix, data);
                add_light(data);
            }
        }

        // Process spot lights
        {
            auto view = registry.view<LightCommon, SpotLight, GlobalTransform>();
            for (auto entity : view)
            {
                if (light_index >= max_lights)
                    break;

                auto& light_common = view.get<LightCommon>(entity);
                auto& spot = view.get<SpotLight>(entity);
                auto& global = view.get<GlobalTransform>(entity);

                if (!light_common.enabled)
                    continue;

                LightShaderData data = {};
                prepare_spot_light(light_common.radiance, light_common.intensity,
                    spot.radius, spot.inner_angle, spot.outer_angle, spot.profile_texture_index,
                    global.world_matrix, data);
                add_light(data);
            }
        }

        // Process rect lights
        {
            auto view = registry.view<LightCommon, RectLight, GlobalTransform>();
            for (auto entity : view)
            {
                if (light_index >= max_lights)
                    break;

                auto& light_common = view.get<LightCommon>(entity);
                auto& rect = view.get<RectLight>(entity);
                auto& global = view.get<GlobalTransform>(entity);

                if (!light_common.enabled)
                    continue;

                LightShaderData data = {};
                prepare_rect_light(light_common.radiance, light_common.intensity,
                    rect.width, rect.height, rect.radius, global.world_matrix, data);
                add_light(data);
            }
        }

        // Process disk lights
        {
            auto view = registry.view<LightCommon, DiskLight, GlobalTransform>();
            for (auto entity : view)
            {
                if (light_index >= max_lights)
                    break;

                auto& light_common = view.get<LightCommon>(entity);
                auto& disk = view.get<DiskLight>(entity);
                auto& global = view.get<GlobalTransform>(entity);

                if (!light_common.enabled)
                    continue;

                LightShaderData data = {};
                prepare_disk_light(light_common.radiance, light_common.intensity,
                    disk.radius, global.world_matrix, data);
                add_light(data);
            }
        }

        // Process cylinder lights
        {
            auto view = registry.view<LightCommon, CylinderLight, GlobalTransform>();
            for (auto entity : view)
            {
                if (light_index >= max_lights)
                    break;

                auto& light_common = view.get<LightCommon>(entity);
                auto& cylinder = view.get<CylinderLight>(entity);
                auto& global = view.get<GlobalTransform>(entity);

                if (!light_common.enabled)
                    continue;

                LightShaderData data = {};
                prepare_cylinder_light(light_common.radiance, light_common.intensity,
                    cylinder.radius, cylinder.length, global.world_matrix, data);
                add_light(data);
            }
        }

        return light_index;
    }

}
