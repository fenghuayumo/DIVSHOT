#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cereal/cereal.hpp>

namespace diverse
{

    struct Transform
    {
        glm::vec3 local_position{0.0f};
        glm::quat local_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 local_scale{1.0f};

        Transform() = default;

        explicit Transform(const glm::vec3& position)
            : local_position(position)
        {
        }

        Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
            : local_position(position)
            , local_rotation(rotation)
            , local_scale(scale)
        {
        }

        // Computed local matrix (TRS)
        glm::mat4 compute_local_matrix() const
        {
            glm::mat4 result = glm::mat4(1.0f);
            result = glm::translate(result, local_position);
            result = result * glm::toMat4(local_rotation);
            result = glm::scale(result, local_scale);
            return result;
        }

        // Direction helpers (in local space, assuming identity parent)
        glm::vec3 up() const
        {
            return local_rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        glm::vec3 right() const
        {
            return local_rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        glm::vec3 forward() const
        {
            return local_rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        }

        // Modifiers
        Transform& translate(const glm::vec3& delta)
        {
            local_position += delta;
            return *this;
        }

        Transform& rotate(const glm::quat& delta_rotation)
        {
            local_rotation = local_rotation * delta_rotation;
            local_rotation = glm::normalize(local_rotation);
            return *this;
        }

        Transform& scale(const glm::vec3& scale_factor)
        {
            local_scale *= scale_factor;
            return *this;
        }

        void look_at(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f))
        {
            glm::mat4 look_matrix = glm::lookAt(local_position, target, up);
            local_rotation = glm::quat_cast(glm::inverse(look_matrix));
            local_rotation = glm::normalize(local_rotation);
        }

        // Serialization support
        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(
                cereal::make_nvp("position", local_position),
                cereal::make_nvp("rotation", local_rotation),
                cereal::make_nvp("scale", local_scale)
            );
        }
    };

}
