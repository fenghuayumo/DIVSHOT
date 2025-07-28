#include "precompile.h"
#include "ray.h"
#include "bounding_box.h"

namespace diverse
{
    namespace maths
    {
        Ray::Ray()
        {
            Origin    = glm::vec3(0.0f);
            Direction = glm::vec3(0.0f);
        }

        Ray::Ray(const glm::vec3& origin, const glm::vec3& direction)
        {
            Origin    = origin;
            Direction = direction;
        }

        bool Ray::intersects(const BoundingBox& bb, float& t) const
        {
            DS_PROFILE_FUNCTION();
            t = 0.0f;
            // Check for ray origin being inside the bb
            if(bb.is_inside(Origin))
                return true;

            float dist = maths::M_INFINITY;

            // Check for intersecting in the X-direction
            if(Origin.x < bb.min().x && Direction.x > 0.0f)
            {
                float x = (bb.min().x - Origin.x) / Direction.x;
                if(x < dist)
                {
                    glm::vec3 point = Origin + x * Direction;
                    if(point.y >= bb.min().y && point.y <= bb.max().y && point.z >= bb.min().z && point.z <= bb.max().z)
                        dist = x;
                }
            }
            if(Origin.x > bb.max().x && Direction.x < 0.0f)
            {
                float x = (bb.max().x - Origin.x) / Direction.x;
                if(x < dist)
                {
                    glm::vec3 point = Origin + x * Direction;
                    if(point.y >= bb.min().y && point.y <= bb.max().y && point.z >= bb.min().z && point.z <= bb.max().z)
                        dist = x;
                }
            }
            // Check for intersecting in the Y-direction
            if(Origin.y < bb.min().y && Direction.y > 0.0f)
            {
                float x = (bb.min().y - Origin.y) / Direction.y;
                if(x < dist)
                {
                    glm::vec3 point = Origin + x * Direction;
                    if(point.x >= bb.min().x && point.x <= bb.max().x && point.z >= bb.min().z && point.z <= bb.max().z)
                        dist = x;
                }
            }
            if(Origin.y > bb.max().y && Direction.y < 0.0f)
            {
                float x = (bb.max().y - Origin.y) / Direction.y;
                if(x < dist)
                {
                    glm::vec3 point = Origin + x * Direction;
                    if(point.x >= bb.min().x && point.x <= bb.max().x && point.z >= bb.min().z && point.z <= bb.max().z)
                        dist = x;
                }
            }
            // Check for intersecting in the Z-direction
            if(Origin.z < bb.min().z && Direction.z > 0.0f)
            {
                float x = (bb.min().z - Origin.z) / Direction.z;
                if(x < dist)
                {
                    glm::vec3 point = Origin + x * Direction;
                    if(point.x >= bb.min().x && point.x <= bb.max().x && point.y >= bb.min().y && point.y <= bb.max().y)
                        dist = x;
                }
            }
            if(Origin.z > bb.max().z && Direction.z < 0.0f)
            {
                float x = (bb.max().z - Origin.z) / Direction.z;
                if(x < dist)
                {
                    glm::vec3 point = Origin + x * Direction;
                    if(point.x >= bb.min().x && point.x <= bb.max().x && point.y >= bb.min().y && point.y <= bb.max().y)
                        dist = x;
                }
            }

            t = dist;
            return dist < maths::M_INFINITY;
            ;
        }

        bool Ray::intersects(const BoundingBox& bb) const
        {
            float distance;
            return intersects(bb, distance);
        }

        bool Ray::intersects_triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t) const
        {
            DS_PROFILE_FUNCTION();
            const glm::vec3 E1  = b - a;
            const glm::vec3 E2  = c - a;
            const glm::vec3 N   = cross(E1, E2);
            const float det     = -glm::dot(Direction, N);
            const float invdet  = 1.f / det;
            const glm::vec3 AO  = Origin - a;
            const glm::vec3 DAO = glm::cross(AO, Direction);
            const float u       = glm::dot(E2, DAO) * invdet;
            const float v       = -glm::dot(E1, DAO) * invdet;
            t                   = glm::dot(AO, N) * invdet;
            return (det >= 1e-6f && t >= 0.0f && u >= 0.0f && v >= 0.0f && (u + v) <= 1.0f);
        }
    }
}
