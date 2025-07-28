#pragma once
#include <glm/vec3.hpp>
#include <glm/fwd.hpp>

namespace diverse
{
    namespace maths
    {
        class BoundingBox;
        class Frustum;

        class BoundingSphere
        {
            friend class Frustum;
            friend class BoundingBox;

        public:
            BoundingSphere();
            BoundingSphere(const glm::vec3& center, float radius);
            BoundingSphere(const glm::vec3* points, unsigned int count);
            BoundingSphere(const glm::vec3* points, unsigned int count, const glm::vec3& center);
            BoundingSphere(const glm::vec3* points, unsigned int count, const glm::vec3& center, float radius);
            BoundingSphere(const BoundingSphere& other);
            BoundingSphere(BoundingSphere&& other);
            BoundingSphere& operator=(const BoundingSphere& other);
            BoundingSphere& operator=(BoundingSphere&& other);
            ~BoundingSphere() = default;

            const glm::vec3& get_center() const;
            float get_radius() const;
            float& radius();
            glm::vec3& center();
            void set_center(const glm::vec3& center);
            void set_radius(float radius);

            bool is_inside(const glm::vec3& point) const;
            bool is_inside(const BoundingSphere& sphere) const;
            bool is_inside(const BoundingBox& box) const;
            bool is_inside(const Frustum& frustum) const;

            bool contains(const glm::vec3& point) const;
            bool contains(const BoundingSphere& other) const;
            bool intersects(const BoundingSphere& other) const;
            bool intersects(const glm::vec3& point) const;
            bool intersects(const glm::vec3& point, float radius) const;

            void merge(const BoundingSphere& other);
            void merge(const glm::vec3& point);
            void merge(const glm::vec3* points, unsigned int count);

            void transform(const glm::mat4& transform);
            maths::BoundingSphere transformed(const glm::mat4& transform);
        private:
            glm::vec3 m_Center;
            float m_Radius;
        };
    }
}
