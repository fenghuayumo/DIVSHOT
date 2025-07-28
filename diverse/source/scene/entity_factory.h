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
        glm::vec4 GenColour(float alpha);

        // Generates a default Sphere object with the parameters specified.
        Entity BuildSphereObject(
            Scene* scene,
            const std::string& name,
            const glm::vec3& pos,
            float radius,
            bool physics_enabled    = false,
            float inverse_mass      = 0.0f, // requires physics_enabled = true
            bool collidable         = true, // requires physics_enabled = true
            const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Generates a default Cuboid object with the parameters specified
        Entity BuildCuboidObject(
            Scene* scene,
            const std::string& name,
            const glm::vec3& pos,
            const glm::vec3& scale,
            bool physics_enabled    = false,
            float inverse_mass      = 0.0f, // requires physics_enabled = true
            bool collidable         = true, // requires physics_enabled = true
            const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Generates a default Cuboid object with the parameters specified
        Entity BuildPyramidObject(
            Scene* scene,
            const std::string& name,
            const glm::vec3& pos,
            const glm::vec3& scale,
            bool physics_enabled    = false,
            float inverse_mass      = 0.0f, // requires physics_enabled = true
            bool collidable         = true, // requires physics_enabled = true
            const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        void AddLightCube(Scene* scene, const glm::vec3& pos, const glm::vec3& dir);
        void AddSphere(Scene* scene, const glm::vec3& pos, const glm::vec3& dir);
        void AddPyramid(Scene* scene, const glm::vec3& pos, const glm::vec3& dir);
    };
}
