#pragma once

#include "maths/maths_serialisation.h"
#include <cereal/cereal.hpp>

namespace diverse
{
    namespace maths
    {
        class DS_EXPORT Transform
        {
        public:
            Transform();
            Transform(const glm::mat4& matrix);
            Transform(const glm::vec3& position);
            ~Transform();

            void set_world_matrix(const glm::mat4& mat);

            void set_local_transform(const glm::mat4& localMat);

            void set_local_position(const glm::vec3& localPos);
            void set_local_scale(const glm::vec3& localScale);
            void set_local_orientation(const glm::quat& quat);
            void set_local_orientation(const glm::vec3& angle);

            const glm::mat4& get_world_matrix() const;
            glm::mat4 get_local_matrix() const;

            const glm::vec3 get_world_position() const;
            const glm::quat get_world_orientation() const;

            const glm::vec3& get_local_position() const;
            const glm::vec3& get_local_scale() const;
            const glm::vec3& get_local_orientation() const;

            // Updates Local Matrix from R,T and S vectors
            void update_matrices();

            // Sets R,T and S vectors from Local Matrix
            void apply_transform();

            glm::vec3 get_up_direction() const
            {
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                up           = get_world_orientation() * up;
                return up;
            }

            glm::vec3 get_right_direction() const
            {
                glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
                right           = get_world_orientation() * right;
                return right;
            }

            glm::vec3 get_forward_direction() const
            {
                glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f);
                forward           = get_world_orientation() * forward;
                return forward;
            }

            maths::Transform operator*(const maths::Transform& other) const
            {
                maths::Transform result;
                result.set_world_matrix(m_WorldMatrix * other.m_WorldMatrix);
                return result;
            }

            maths::Transform& translate(const glm::vec3& pos);
            
            // Set orientation to look at a target point
            void look_at(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

            template <typename Archive>
            void save(Archive& archive) const
            {
                archive(cereal::make_nvp("Position", m_LocalPosition), cereal::make_nvp("Rotation", m_LocalOrientation), cereal::make_nvp("Scale", m_LocalScale));
            }

            template <typename Archive>
            void load(Archive& archive)
            {
                archive(cereal::make_nvp("Position", m_LocalPosition), cereal::make_nvp("Rotation", m_LocalOrientation), cereal::make_nvp("Scale", m_LocalScale));
            }

        protected:
            glm::mat4 m_WorldMatrix;

            glm::vec3 m_LocalPosition;
            glm::vec3 m_LocalScale;
            //glm::quat m_LocalOrientation;
            glm::vec3 m_LocalOrientation;
        };
    }
}
