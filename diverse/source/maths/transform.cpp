#include "precompile.h"
#include "transform.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/ext.hpp>

namespace diverse
{
    namespace maths
    {
        Transform::Transform()
        {
            DS_PROFILE_FUNCTION_LOW();
            m_LocalPosition    = glm::vec3(0.0f, 0.0f, 0.0f);
            //m_LocalOrientation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
            m_LocalOrientation = glm::vec3(0.0f, 0.0f, 0.0f);
            m_LocalScale       = glm::vec3(1.0f, 1.0f, 1.0f);
            // m_LocalMatrix      = glm::mat4(1.0f);
            m_WorldMatrix = glm::mat4(1.0f);
            // m_ParentMatrix     = glm::mat4(1.0f);
        }

        Transform::Transform(const glm::mat4& matrix)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::quat rot;
            glm::decompose(matrix, m_LocalScale, rot, m_LocalPosition, skew, perspective);
            m_LocalOrientation = glm::eulerAngles(rot);
            // m_LocalMatrix  = matrix;
            m_WorldMatrix = matrix;
            // m_ParentMatrix = glm::mat4(1.0f);
        }

        Transform::Transform(const glm::vec3& position)
        {
            DS_PROFILE_FUNCTION_LOW();
            m_LocalPosition    = position;
            m_LocalOrientation = glm::vec3(0.0f, 0.0f, 0.0f);
            m_LocalScale       = glm::vec3(1.0f, 1.0f, 1.0f);
            m_WorldMatrix      = glm::mat4(1.0f);
            set_local_position(position);
        }

        Transform::~Transform() = default;

        void Transform::update_matrices()
        {
            DS_PROFILE_FUNCTION_LOW();
            // m_LocalMatrix =

            // m_WorldMatrix = m_ParentMatrix * m_LocalMatrix;
        }

        void Transform::apply_transform()
        {
            DS_PROFILE_FUNCTION_LOW();
            // glm::vec3 skew;
            // glm::vec4 perspective;
            //  glm::decompose(m_LocalMatrix, m_LocalScale, m_LocalOrientation, m_LocalPosition, skew, perspective);
        }

        void Transform::set_world_matrix(const glm::mat4& mat)
        {
            DS_PROFILE_FUNCTION_LOW();
            // if(m_Dirty)
            //   update_matrices();
            // m_ParentMatrix = mat;
            m_WorldMatrix = mat * glm::translate(glm::mat4(1.0), m_LocalPosition) * glm::toMat4(glm::quat(m_LocalOrientation)) * glm::scale(glm::mat4(1.0), m_LocalScale);
        }

        void Transform::set_local_transform(const glm::mat4& localMat)
        {
            DS_PROFILE_FUNCTION_LOW();
            // m_LocalMatrix = localMat;
            // m_HasUpdated  = true;

            glm::vec3 skew;
            glm::vec4 perspective;
            glm::quat rot;
            glm::decompose(localMat, m_LocalScale, rot, m_LocalPosition, skew, perspective);
            m_LocalOrientation = glm::eulerAngles(rot);
            apply_transform();

            // m_WorldMatrix = m_ParentMatrix * m_LocalMatrix;
        }

        void Transform::set_local_position(const glm::vec3& localPos)
        {
            // m_Dirty         = true;
            m_LocalPosition = localPos;
        }

        void Transform::set_local_scale(const glm::vec3& newScale)
        {
            // m_Dirty      = true;
            m_LocalScale = newScale;
            if(m_LocalScale.x == 0) m_LocalScale.x = 1e-3;
            if(m_LocalScale.y == 0) m_LocalScale.y = 1e-3;
            if(m_LocalScale.z == 0) m_LocalScale.z = 1e-3;
        }

        void Transform::set_local_orientation(const glm::quat& quat)
        {
            // m_Dirty            = true;
            m_LocalOrientation = glm::eulerAngles(quat);
        }

        void Transform::set_local_orientation(const glm::vec3& angle)
        {
            m_LocalOrientation = angle;
        }

        const glm::mat4& Transform::get_world_matrix() const
        {
            DS_PROFILE_FUNCTION_LOW();
            // if(m_Dirty)
            // update_matrices();

            return m_WorldMatrix;
        }

        glm::mat4 Transform::get_local_matrix() const
        {
            DS_PROFILE_FUNCTION_LOW();
            // if(m_Dirty)
            //   update_matrices();

            return glm::translate(glm::mat4(1.0), m_LocalPosition) * glm::toMat4(glm::quat(m_LocalOrientation)) * glm::scale(glm::mat4(1.0), m_LocalScale);
        }

        const glm::vec3 Transform::get_world_position() const
        {
            // if(m_Dirty)
            // update_matrices();

            return m_WorldMatrix[3];
        }

        const glm::quat Transform::get_world_orientation() const
        {
            // if(m_Dirty)
            // update_matrices();

            return glm::toQuat(m_WorldMatrix);
        }

        const glm::vec3& Transform::get_local_position() const
        {
            return m_LocalPosition;
        }

        const glm::vec3& Transform::get_local_scale() const
        {
            return m_LocalScale;
        }

        const glm::vec3& Transform::get_local_orientation() const
        {
            return m_LocalOrientation;
        }

        maths::Transform& Transform::translate(const glm::vec3& pos)
        {
            //set_world_matrix(glm::translate(glm::mat4(1.0), pos));
            m_LocalPosition += pos;
            set_world_matrix(glm::mat4(1.0f));
            return *this;
        }

        void Transform::look_at(const glm::vec3& target, const glm::vec3& up)
        {
            DS_PROFILE_FUNCTION_LOW();
            
            // Calculate the direction from position to target
            glm::vec3 direction = glm::normalize(target - m_LocalPosition);
            
            // Handle edge case where position equals target
            if (glm::length(direction) < 1e-6f)
            {
                direction = glm::vec3(0.0f, 0.0f, -1.0f); // Default forward direction
            }
            
            // Calculate the right vector
            glm::vec3 right = glm::normalize(glm::cross(up, direction));
            
            // Recalculate the up vector to ensure orthogonality
            glm::vec3 newUp = glm::cross(direction, right);
            
            // Create rotation matrix
            // Note: In OpenGL/GLM convention, -Z is forward
            // So we need to negate the direction for the Z column
            glm::mat4 lookMatrix;
            lookMatrix[0] = glm::vec4(right, 0.0f);        // Right vector
            lookMatrix[1] = glm::vec4(newUp, 0.0f);        // Up vector
            lookMatrix[2] = glm::vec4(-direction, 0.0f);   // Forward vector (negated for OpenGL)
            lookMatrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            
            // Extract quaternion from matrix and convert to euler angles
            glm::quat rotation = glm::quat_cast(lookMatrix);
            m_LocalOrientation = glm::eulerAngles(rotation);
            
            // Update world matrix
            set_world_matrix(glm::mat4(1.0f));
        }
    }
}
