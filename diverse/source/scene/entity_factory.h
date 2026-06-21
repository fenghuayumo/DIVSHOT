#pragma once

#include "entity.h"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace diverse
{
    class RigidBody;
    class Scene;

    namespace EntityFactory
    {
        glm::vec4 gen_colour(float alpha);

        // Generates a default Sphere object with the parameters specified.
        Entity build_sphere_object(
            Scene* scene,
            const std::string& name,
            const glm::vec3& pos,
            float radius,
            bool physics_enabled    = false,
            float inverse_mass      = 0.0f, // requires physics_enabled = true
            bool collidable         = true, // requires physics_enabled = true
            const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Generates a default Cuboid object with the parameters specified
        Entity build_cuboid_object(
            Scene* scene,
            const std::string& name,
            const glm::vec3& pos,
            const glm::vec3& scale,
            bool physics_enabled    = false,
            float inverse_mass      = 0.0f, // requires physics_enabled = true
            bool collidable         = true, // requires physics_enabled = true
            const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Generates a default Cuboid object with the parameters specified
        Entity build_pyramid_object(
            Scene* scene,
            const std::string& name,
            const glm::vec3& pos,
            const glm::vec3& scale,
            bool physics_enabled    = false,
            float inverse_mass      = 0.0f, // requires physics_enabled = true
            bool collidable         = true, // requires physics_enabled = true
            const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        void add_light_cube(Scene* scene, const glm::vec3& pos, const glm::vec3& dir);
        void add_sphere(Scene* scene, const glm::vec3& pos, const glm::vec3& dir);
        void add_pyramid(Scene* scene, const glm::vec3& pos, const glm::vec3& dir);
    };
}
