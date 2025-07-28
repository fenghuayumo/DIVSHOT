#include "precompile.h"
#include "maths/plane.h"
#include "maths/maths_utils.h"
namespace diverse
{
    Plane::Plane()
    {
        m_Normal   = glm::vec3(0.0f, 1.0f, 0.0f);
        m_Distance = 0.0f;
    }

    Plane::Plane(const glm::vec3& normal, float distance)
    {
        m_Normal   = glm::normalize(normal);
        m_Distance = distance;
    }

    Plane::Plane(const glm::vec3& point, const glm::vec3& normal)
    {
        m_Normal   = glm::normalize(normal);
        m_Distance = glm::dot(normal, point);
    }

    Plane::Plane(const glm::vec3& point1, const glm::vec3& point2, const glm::vec3& point3)
    {
        glm::vec3 edge1 = point2 - point1;
        glm::vec3 edge2 = point3 - point1;
        m_Normal        = glm::normalize(glm::cross(edge1, edge2));
        m_Distance      = glm::dot(m_Normal, point1);
    }

    Plane::Plane(const glm::vec4& plane)
    {
        m_Normal   = glm::vec3(plane.x, plane.y, plane.z);
        m_Distance = plane.w;
    }

    Plane::Plane(float a, float b, float c, float d)
    {
        m_Normal   = glm::vec3(a, b, c);
        m_Distance = d;
    }

    Plane::~Plane()
    {
    }

    void Plane::set(const glm::vec3& normal, float distance)
    {
        m_Normal   = normal;
        m_Distance = distance;
    }

    void Plane::set(const glm::vec3& point, const glm::vec3& normal)
    {
        m_Normal   = normal;
        m_Distance = glm::dot(m_Normal, point);
    }

    void Plane::set(const glm::vec3& point1, const glm::vec3& point2, const glm::vec3& point3)
    {
        glm::vec3 vec1 = point2 - point1;
        glm::vec3 vec2 = point3 - point1;
        m_Normal       = glm::cross(vec1, vec2);
        m_Normal       = glm::normalize(m_Normal);
        m_Distance     = glm::dot(m_Normal, point1);
    }

    void Plane::set(const glm::vec4& plane)
    {
        m_Normal   = glm::vec3(plane.x, plane.y, plane.z);
        m_Distance = plane.w;
    }

    void Plane::normalise()
    {
        float magnitude = glm::length(m_Normal);
        m_Normal /= magnitude;
        m_Distance /= magnitude;
    }

    float Plane::distance(const glm::vec3& point) const
    {
        return glm::dot(point, m_Normal) + m_Distance;
    }

    float Plane::distance(const glm::vec4& point) const
    {
        return glm::dot(glm::vec3(point), m_Normal) + m_Distance;
    }

    bool Plane::is_point_on_plane(const glm::vec3& point) const
    {
        return distance(point) >= -0.0001f;
    }

    bool Plane::is_point_on_plane(const glm::vec4& point) const
    {
        return distance(point) >= -maths::M_EPSILON;
    }

    void Plane::set_normal(const glm::vec3& normal)
    {
        m_Normal = normal;
    }

    void Plane::set_distance(float distance)
    {
        m_Distance = distance;
    }

    void Plane::transform(const glm::mat4& matrix)
    {
        glm::vec4 plane = glm::vec4(m_Normal, m_Distance);
        plane           = matrix * plane;
        m_Normal        = glm::vec3(plane.x, plane.y, plane.z);
        m_Distance      = plane.w;
    }

    Plane Plane::transformed(const glm::mat4& matrix) const
    {
        glm::vec4 plane = glm::vec4(m_Normal, m_Distance);
        plane           = matrix * plane;
        return Plane(glm::vec3(plane.x, plane.y, plane.z), plane.w);
    }

    bool Plane::intersects_ray(const glm::vec3& origin, const glm::vec3& direction, glm::vec3& isect_points) const
    {
        float denom = glm::dot(m_Normal, direction);
        if (denom > -maths::M_EPSILON && denom < maths::M_EPSILON)
        {
            return false;
        }

        float t = -(glm::dot(m_Normal, origin) + m_Distance) / denom;
        if (t < 0.0f)
        {
            return false;
        }

        isect_points = origin + t * direction;
        return true;
    }
}
