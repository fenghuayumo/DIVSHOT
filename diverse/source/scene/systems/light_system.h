#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

namespace diverse
{

    // Forward declaration
    struct LightShaderData;

    class LightSystem
    {
    public:
        // Prepare light data for rendering
        // Returns number of lights prepared
        static size_t prepare_render_data(entt::registry& registry, LightShaderData* light_data, size_t max_lights);

        // Get total light count
        static size_t get_light_count(entt::registry& registry);

    private:
        // Helper functions for each light type
        static void prepare_directional_light(const glm::vec3& radiance, float intensity, float angular_size,
            const glm::mat4& world_matrix, LightShaderData& data);
        static void prepare_point_light(const glm::vec3& radiance, float intensity, float radius,
            const glm::mat4& world_matrix, LightShaderData& data);
        static void prepare_spot_light(const glm::vec3& radiance, float intensity, float radius,
            float inner_angle, float outer_angle, int profile_texture_index,
            const glm::mat4& world_matrix, LightShaderData& data);
        static void prepare_rect_light(const glm::vec3& radiance, float intensity,
            float width, float height, float radius,
            const glm::mat4& world_matrix, LightShaderData& data);
        static void prepare_disk_light(const glm::vec3& radiance, float intensity, float radius,
            const glm::mat4& world_matrix, LightShaderData& data);
        static void prepare_cylinder_light(const glm::vec3& radiance, float intensity,
            float radius, float length,
            const glm::mat4& world_matrix, LightShaderData& data);
    };

}
