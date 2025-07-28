#pragma once
#include <glm/vec3.hpp>

namespace diverse
{
    namespace maths
    {
        class BoundingBox;
        class Ray
        {
        public:
            Ray();
            Ray(const glm::vec3& origin, const glm::vec3& direction);

            bool intersects(const BoundingBox& bb) const;
            bool intersects(const BoundingBox& bb, float& t) const;
            bool intersects_triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t) const;

            glm::vec3 Origin, Direction;
        };
    }
}
