#pragma once
#include <glm/mat4x4.hpp>

namespace diverse
{
    class Plane
    {
    public:
        Plane();
        Plane(const glm::vec3& normal, float distance);
        Plane(const glm::vec3& point, const glm::vec3& normal);
        Plane(const glm::vec3& point1, const glm::vec3& point2, const glm::vec3& point3);
        Plane(const glm::vec4& plane);
        Plane(float a, float b, float c, float d);
        ~Plane();

        void set(const glm::vec3& normal, float distance);
        void set(const glm::vec3& point, const glm::vec3& normal);
        void set(const glm::vec3& point1, const glm::vec3& point2, const glm::vec3& point3);
        void set(const glm::vec4& plane);
        void set_normal(const glm::vec3& normal);
        void set_distance(float distance);
        void transform(const glm::mat4& transform);
        Plane transformed(const glm::mat4& transform) const;

        void normalise();

        float distance(const glm::vec3& point) const;
        float distance(const glm::vec4& point) const;

        bool is_point_on_plane(const glm::vec3& point) const;
        bool is_point_on_plane(const glm::vec4& point) const;

        glm::vec3 project(const glm::vec3& point) const { return point - distance(point) * m_Normal; }

        bool intersects_ray(const glm::vec3& origin, const glm::vec3& direction, glm::vec3& isect_points) const;
        inline glm::vec3 normal() const { return m_Normal; }
        inline float distance() const { return m_Distance; }

    private:
        glm::vec3 m_Normal;
        float m_Distance;
    };
}
