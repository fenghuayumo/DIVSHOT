#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace diverse
{

    struct GlobalTransform
    {
        glm::mat4 world_matrix{1.0f};
        bool dirty = true;

        GlobalTransform() = default;

        explicit GlobalTransform(const glm::mat4& matrix)
            : world_matrix(matrix)
            , dirty(true)
        {
        }

        // Extract world properties
        glm::vec3 position() const
        {
            return world_matrix[3];
        }

        glm::quat rotation() const
        {
            glm::vec3 scale_;
            glm::quat rot;
            glm::vec3 pos;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(world_matrix, scale_, rot, pos, skew, perspective);
            return rot;
        }

        glm::vec3 scale() const
        {
            glm::vec3 scale_;
            glm::quat rot;
            glm::vec3 pos;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(world_matrix, scale_, rot, pos, skew, perspective);
            return scale_;
        }

        // Direction vectors (from world space)
        glm::vec3 up() const
        {
            return rotation() * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        glm::vec3 right() const
        {
            return rotation() * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        glm::vec3 forward() const
        {
            return rotation() * glm::vec3(0.0f, 0.0f, 1.0f);
        }

        // Not serialized - computed from Transform + hierarchy
    };

}
