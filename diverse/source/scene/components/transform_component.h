#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
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

        glm::mat4 get_local_matrix() const
        {
            return compute_local_matrix();
        }

        glm::mat4 get_world_matrix() const
        {
            return compute_local_matrix();
        }

        glm::vec3 get_world_position() const
        {
            return local_position;
        }

        glm::quat get_world_orientation() const
        {
            return local_rotation;
        }

        const glm::vec3& get_local_position() const
        {
            return local_position;
        }

        const glm::quat& get_local_orientation() const
        {
            return local_rotation;
        }

        const glm::vec3& get_local_scale() const
        {
            return local_scale;
        }

        // Direction helpers (in local space, assuming identity parent)
        glm::vec3 up() const
        {
            return local_rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        glm::vec3 get_up_direction() const
        {
            return up();
        }

        glm::vec3 right() const
        {
            return local_rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        glm::vec3 get_right_direction() const
        {
            return right();
        }

        glm::vec3 forward() const
        {
            return local_rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::vec3 get_forward_direction() const
        {
            return forward();
        }

        // Modifiers
        Transform& translate(const glm::vec3& delta)
        {
            local_position += delta;
            return *this;
        }

        void set_local_position(const glm::vec3& position)
        {
            local_position = position;
        }

        void set_local_scale(const glm::vec3& scale_)
        {
            local_scale = scale_;
            if (local_scale.x == 0.0f) local_scale.x = 1e-3f;
            if (local_scale.y == 0.0f) local_scale.y = 1e-3f;
            if (local_scale.z == 0.0f) local_scale.z = 1e-3f;
        }

        void set_local_orientation(const glm::quat& rotation)
        {
            local_rotation = glm::normalize(rotation);
        }

        void set_local_orientation(const glm::vec3& euler)
        {
            local_rotation = glm::quat(euler);
        }

        void set_local_transform(const glm::mat4& local_matrix)
        {
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(local_matrix, local_scale, local_rotation, local_position, skew, perspective);
            local_rotation = glm::normalize(local_rotation);
        }

        void set_world_matrix(const glm::mat4& world_matrix)
        {
            if (world_matrix == glm::mat4(1.0f))
                return;
            set_local_transform(world_matrix);
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
