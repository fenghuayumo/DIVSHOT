#pragma once
#include "maths/plane.h"
#include <glm/mat4x4.hpp>

namespace diverse
{
    namespace maths
    {
        class Ray;
        class Rect;
        class BoundingBox;
        class BoundingSphere;

        enum FrustumPlane
        {
            PLANE_NEAR = 0,
            PLANE_LEFT,
            PLANE_RIGHT,
            PLANE_UP,
            PLANE_DOWN,
            PLANE_FAR,
        };

        class Frustum
        {
        public:
            Frustum();
            Frustum(const glm::mat4& transform);
            Frustum(const glm::mat4& projection, const glm::mat4& view);
            ~Frustum();

            void transform(const glm::mat4& transform);
            void define(const glm::mat4& projection, const glm::mat4& view);
            void define(const glm::mat4& transform);
            void define_ortho(float scale, float aspectRatio, float n, float f, const glm::mat4& viewMatrix);
            void define(float fov, float aspectRatio, float n, float f, const glm::mat4& viewMatrix);

            bool is_inside(const glm::vec3& point) const;
            bool is_inside(const BoundingSphere& sphere) const;
            bool is_inside(const BoundingBox& box) const;
            bool is_inside(const Rect& rect) const;
            bool is_inside(const Plane& plane) const;
            bool is_inside(const Ray& ray) const;
            BoundingBox get_bounding_box() const;
            const Plane& get_plane(FrustumPlane plane) const;
            const Plane& get_plane(int index) const { return m_Planes[index]; }
            glm::vec3* get_verticies();

        private:
            void calculate_vertices(const glm::mat4& transform);

            Plane m_Planes[6];
            glm::vec3 m_Verticies[8];
        };
    }
}
