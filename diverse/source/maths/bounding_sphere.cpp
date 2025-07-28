#include "precompile.h"
#include "bounding_sphere.h"
#include "bounding_box.h"
#include "frustum.h"

#include <glm/mat4x4.hpp>
#include <glm/gtx/norm.hpp>

namespace diverse
{
    namespace maths
    {
        BoundingSphere::BoundingSphere()
        {
            m_Center = glm::vec3(0.0f);
            m_Radius = 0.0f;
        }

        BoundingSphere::BoundingSphere(const glm::vec3& center, float radius)
        {
            m_Center = center;
            m_Radius = radius;
        }

        BoundingSphere::BoundingSphere(const BoundingSphere& copy)
        {
            m_Center = copy.m_Center;
            m_Radius = copy.m_Radius;
        }

        BoundingSphere::BoundingSphere(BoundingSphere&& other)
        {
            m_Center = std::move(other.m_Center);
            m_Radius = std::move(other.m_Radius);
        }

        BoundingSphere::BoundingSphere(const glm::vec3* points, unsigned int count)
        {
            if(count == 0)
            {
                m_Center = glm::vec3(0.0f);
                m_Radius = 0.0f;
                return;
            }

            m_Center = glm::vec3(0.0f);
            m_Radius = 0.0f;

            for(unsigned int i = 0; i < count; i++)
            {
                m_Center += points[i];
            }

            m_Center /= (float)count;

            float maxDist = 0.0f;
            for(unsigned int i = 0; i < count; i++)
            {
                float dist = glm::distance(points[i], m_Center);
                if(dist > maxDist)
                {
                    maxDist = dist;
                }
            }

            m_Radius = maxDist;
        }

        BoundingSphere::BoundingSphere(const glm::vec3* points, unsigned int count, const glm::vec3& center)
        {
            if(count == 0)
            {
                m_Center = glm::vec3(0.0f);
                m_Radius = 0.0f;
                return;
            }

            m_Center = center;
            m_Radius = 0.0f;

            for(unsigned int i = 0; i < count; i++)
            {
                m_Center += points[i];
            }

            m_Center /= (float)count;

            float maxDistSq = 0.0f;
            for(unsigned int i = 0; i < count; i++)
            {
                float dist = glm::length(points[i] - m_Center);
                if(dist > maxDistSq)
                {
                    maxDistSq = dist;
                }
            }

            m_Radius = maxDistSq;
        }

        BoundingSphere::BoundingSphere(const glm::vec3* points, unsigned int count, const glm::vec3& center, float radius)
        {
            if(count == 0)
            {
                m_Center = glm::vec3(0.0f);
                m_Radius = 0.0f;
                return;
            }

            m_Center = center;
            m_Radius = radius;

            for(unsigned int i = 0; i < count; i++)
            {
                m_Center += points[i];
            }

            m_Center /= (float)count;

            float maxDistSq = 0.0f;
            for(unsigned int i = 0; i < count; i++)
            {
                float dist = glm::length(points[i] - m_Center);
                if(dist > maxDistSq)
                {
                    maxDistSq = dist;
                }
            }

            m_Radius = maxDistSq;
        }

        BoundingSphere& BoundingSphere::operator=(const BoundingSphere& rhs)
        {
            if(this == &rhs)
                return *this;

            m_Center = rhs.m_Center;
            m_Radius = rhs.m_Radius;

            return *this;
        }

        BoundingSphere& BoundingSphere::operator=(BoundingSphere&& other)
        {
            if (this == &other)
                return *this;

            m_Center = std::move(other.m_Center);
            m_Radius = std::move(other.m_Radius);
            return *this;
        }

        bool BoundingSphere::is_inside(const glm::vec3& point) const
        {
            return glm::length2(point - m_Center) <= m_Radius * m_Radius;
        }

        bool BoundingSphere::is_inside(const BoundingSphere& sphere) const
        {
            return glm::length2(sphere.m_Center - m_Center) <= (m_Radius + sphere.m_Radius) * (m_Radius + sphere.m_Radius);
        }

        bool BoundingSphere::is_inside(const BoundingBox& box) const
        {
            return box.is_inside(*this);
        }

        bool BoundingSphere::is_inside(const Frustum& frustum) const
        {
            return frustum.is_inside(*this);
        }

        const glm::vec3& BoundingSphere::get_center() const
        {
            return m_Center;
        }

        float BoundingSphere::get_radius() const
        {
            return m_Radius;
        }
        float& BoundingSphere::radius()
        {
            return m_Radius;
        }
        glm::vec3& BoundingSphere::center()
        {
            return m_Center;
        }
        void BoundingSphere::set_center(const glm::vec3& center)
        {
            m_Center = center;
        }

        void BoundingSphere::set_radius(float radius)
        {
            m_Radius = radius;
        }

        bool BoundingSphere::contains(const glm::vec3& point) const
        {
            return glm::length2(point - m_Center) <= m_Radius * m_Radius;
        }

        bool BoundingSphere::contains(const BoundingSphere& other) const
        {
            return glm::distance2(other.m_Center, m_Center) <= (m_Radius + other.m_Radius) * (m_Radius + other.m_Radius);
        }
        bool BoundingSphere::intersects(const BoundingSphere& other) const
        {
            return glm::distance2(other.m_Center, m_Center) <= (m_Radius + other.m_Radius) * (m_Radius + other.m_Radius);
        }
        bool BoundingSphere::intersects(const glm::vec3& point) const
        {
            return glm::distance2(point, m_Center) <= m_Radius * m_Radius;
        }
        bool BoundingSphere::intersects(const glm::vec3& point, float radius) const
        {
            return glm::distance2(point, m_Center) <= (m_Radius + radius) * (m_Radius + radius);
        }

        void BoundingSphere::merge(const BoundingSphere& other)
        {
            float distance = glm::distance(other.m_Center, m_Center);

            if(distance > m_Radius + other.m_Radius)
                return;

            if(distance <= m_Radius - other.m_Radius)
            {
                m_Center = other.m_Center;
                m_Radius = other.m_Radius;
                return;
            }

            if(distance <= other.m_Radius - m_Radius)
                return;

            float half  = (distance + m_Radius + other.m_Radius) * 0.5f;
            float scale = half / distance;
            m_Center    = (m_Center + other.m_Center) * scale;
            m_Radius    = half;
        }

        void BoundingSphere::merge(const glm::vec3& point)
        {
            float distance = glm::distance(point, m_Center);

            if(distance > m_Radius)
                return;

            if(distance <= 0.0f)
            {
                m_Center = point;
                m_Radius = 0.0f;
                return;
            }

            float half  = (distance + m_Radius) * 0.5f;
            float scale = half / distance;
            m_Center    = (m_Center + point) * scale;
            m_Radius    = half;
        }

        void BoundingSphere::merge(const glm::vec3* points, unsigned int count)
        {
            if(count == 0)
                return;

            float radius     = 0.0f;
            glm::vec3 center = points[0];

            for(unsigned int i = 1; i < count; i++)
            {
                float distance = glm::distance(points[i], center);

                if(distance > radius)
                    radius = distance;

                center += points[i];
            }

            center /= (float)count;

            float distance = glm::distance(center, m_Center);

            if(distance > m_Radius)
                return;

            if(distance <= 0.0f)
            {
                m_Center = center;
                m_Radius = 0.0f;
                return;
            }

            float half  = (distance + m_Radius + radius) * 0.5f;
            float scale = half / distance;
            m_Center    = (m_Center + center) * scale;
            m_Radius    = half;
        }

        void BoundingSphere::transform(const glm::mat4& transform)
        {
            glm::vec3 center = transform * glm::vec4(m_Center, 1.0f);

            m_Center = center;
        }
        maths::BoundingSphere BoundingSphere::transformed(const glm::mat4& transform)
        {
            glm::vec3 center = transform * glm::vec4(m_Center, 1.0f);

            return maths::BoundingSphere(center, m_Radius);
        }
    }
}
