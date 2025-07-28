#pragma once
#include "maths/rect.h"
#include "maths/maths_utils.h"
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

namespace diverse
{
    namespace maths
    {

        class BoundingSphere;
        class BoundingBox
        {
            friend class BoundingSphere;

        public:
            BoundingBox();
            BoundingBox(const glm::vec3& min, const glm::vec3& max);
            BoundingBox(const glm::vec3* points, uint32_t numPoints);
            BoundingBox(const Rect& rect, const glm::vec3& center = glm::vec3(0.0f));
            BoundingBox(const BoundingBox& other);
            BoundingBox(BoundingBox&& other);
            ~BoundingBox();

            void clear();

            BoundingBox& operator=(const BoundingBox& other);
            BoundingBox& operator=(BoundingBox&& other);

            void set(const glm::vec3& min, const glm::vec3& max);
            void set(const glm::vec3* points, uint32_t numPoints);

            void set_from_points(const glm::vec3* points, uint32_t numPoints);
            void set_from_points(const glm::vec3* points, uint32_t numPoints, const glm::mat4& transform);

            void set_from_transformed_aabb(const BoundingBox& aabb, const glm::mat4& transform);

            void translate(const glm::vec3& translation);
            void translate(float x, float y, float z);

            void scale(const glm::vec3& scale);
            void scale(float x, float y, float z);

            void rotate(const glm::mat3& rotation);

            void transform(const glm::mat4& transform);
            BoundingBox transformed(const glm::mat4& transform) const;

            void merge(const BoundingBox& other);
            void merge(const glm::vec3& point);

            void merge(const BoundingBox& other, const glm::mat4& transform);
            void merge(const glm::vec3& point, const glm::mat4& transform);

            void merge(const BoundingBox& other, const glm::mat3& transform);
            void merge(const glm::vec3& point, const glm::mat3& transform);

            Intersection is_inside(const glm::vec3& point) const;
            Intersection is_inside(const BoundingBox& box) const;
            Intersection is_inside(const BoundingSphere& sphere) const;

            bool is_inside_fast(const BoundingBox& box) const;
            bool defined() const;
            
            glm::vec3 size() const;
            glm::vec3 center() const;
            glm::vec3 min() const;
            glm::vec3 max() const;

            glm::vec3 get_extents() const { return m_Max - m_Min; }

            glm::vec3 m_Min;
            glm::vec3 m_Max;
        };
    }
}
