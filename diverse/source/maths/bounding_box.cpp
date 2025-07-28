#include "precompile.h"
#include "bounding_box.h"
#include "bounding_sphere.h"

#include <glm/mat4x4.hpp>

namespace diverse
{
    namespace maths
    {
        BoundingBox::BoundingBox()
        {
            m_Min = glm::vec3(FLT_MAX);
            m_Max = glm::vec3(-FLT_MAX);
        }

        BoundingBox::BoundingBox(const glm::vec3& min, const glm::vec3& max)
        {
            m_Min = min;
            m_Max = max;
        }

        BoundingBox::BoundingBox(const Rect& rect, const glm::vec3& center)
        {
            m_Min = center + glm::vec3(rect.get_position(), 0.0f);
            m_Max = center + glm::vec3(rect.get_position().x + rect.get_size().x, rect.get_position().y + rect.get_size().y, 0.0f);
        }

        BoundingBox::BoundingBox(const BoundingBox& other)
        {
            m_Min = other.m_Min;
            m_Max = other.m_Max;
        }

        BoundingBox::BoundingBox(const glm::vec3* points, uint32_t numPoints)
        {
            set_from_points(points, numPoints);
        }

        BoundingBox::BoundingBox(BoundingBox&& other)
        {
            m_Min = other.m_Min;
            m_Max = other.m_Max;
        }

        BoundingBox::~BoundingBox()
        {
        }

        void BoundingBox::clear()
        {
            m_Min = glm::vec3(FLT_MAX);
            m_Max = glm::vec3(-FLT_MAX);
        }

        BoundingBox& BoundingBox::operator=(const BoundingBox& other)
        {
            m_Min = other.m_Min;
            m_Max = other.m_Max;

            return *this;
        }

        BoundingBox& BoundingBox::operator=(BoundingBox&& other)
        {
            m_Min = other.m_Min;
            m_Max = other.m_Max;

            return *this;
        }

        void BoundingBox::set(const glm::vec3& min, const glm::vec3& max)
        {
            m_Min = min;
            m_Max = max;
        }

        void BoundingBox::set(const glm::vec3* points, uint32_t numPoints)
        {
            m_Min = glm::vec3(FLT_MAX);
            m_Max = glm::vec3(-FLT_MAX);

            for(uint32_t i = 0; i < numPoints; i++)
            {
                m_Min = glm::min(m_Min, points[i]);
                m_Max = glm::max(m_Max, points[i]);
            }
        }

        void BoundingBox::set_from_points(const glm::vec3* points, uint32_t numPoints)
        {
            m_Min = glm::vec3(FLT_MAX);
            m_Max = glm::vec3(-FLT_MAX);

            for(uint32_t i = 0; i < numPoints; i++)
            {
                m_Min = glm::min(m_Min, points[i]);
                m_Max = glm::max(m_Max, points[i]);
            }
        }

        void BoundingBox::set_from_points(const glm::vec3* points, uint32_t numPoints, const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 min(FLT_MAX, FLT_MAX, FLT_MAX);
            glm::vec3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for(uint32_t i = 0; i < numPoints; i++)
            {
                glm::vec3 transformed = transform * glm::vec4(points[i], 1.0f);

                min.x = glm::min(min.x, transformed.x);
                min.y = glm::min(min.y, transformed.y);
                min.z = glm::min(min.z, transformed.z);

                max.x = glm::max(max.x, transformed.x);
                max.y = glm::max(max.y, transformed.y);
                max.z = glm::max(max.z, transformed.z);
            }

            m_Min = min;
            m_Max = max;
        }

        void BoundingBox::set_from_transformed_aabb(const BoundingBox& aabb, const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 min = aabb.m_Min;
            glm::vec3 max = aabb.m_Max;

            glm::vec3 minTransformed = transform * glm::vec4(min, 1.0f);
            glm::vec3 maxTransformed = transform * glm::vec4(max, 1.0f);

            m_Min = glm::min(minTransformed, maxTransformed);
            m_Max = glm::max(minTransformed, maxTransformed);
        }

        void BoundingBox::translate(const glm::vec3& translation)
        {
            DS_PROFILE_FUNCTION_LOW();
            m_Min += translation;
            m_Max += translation;
        }

        void BoundingBox::translate(float x, float y, float z)
        {
            DS_PROFILE_FUNCTION_LOW();
            translate(glm::vec3(x, y, z));
        }

        void BoundingBox::scale(const glm::vec3& scale)
        {
            DS_PROFILE_FUNCTION_LOW();
            m_Min *= scale;
            m_Max *= scale;
        }

        void BoundingBox::scale(float x, float y, float z)
        {
            DS_PROFILE_FUNCTION_LOW();
            m_Min.x *= x;
            m_Min.y *= y;
            m_Min.z *= z;

            m_Max.x *= x;
            m_Max.y *= y;
            m_Max.z *= z;
        }

        void BoundingBox::rotate(const glm::mat3& rotation)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 c  = center();
            glm::vec3 extents = get_extents();

            glm::vec3 rotatedExtents = glm::vec3(rotation * glm::vec4(extents, 1.0f));

            m_Min = c - rotatedExtents;
            m_Max = c + rotatedExtents;
        }

        void BoundingBox::transform(const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 newCenter = transform * glm::vec4(center(), 1.0f);
            glm::vec3 oldEdge   = size() * 0.5f;
            glm::vec3 newEdge   = glm::vec3(
                glm::abs(transform[0][0]) * oldEdge.x + glm::abs(transform[1][0]) * oldEdge.y + glm::abs(transform[2][0]) * oldEdge.z,
                glm::abs(transform[0][1]) * oldEdge.x + glm::abs(transform[1][1]) * oldEdge.y + glm::abs(transform[2][1]) * oldEdge.z,
                glm::abs(transform[0][2]) * oldEdge.x + glm::abs(transform[1][2]) * oldEdge.y + glm::abs(transform[2][2]) * oldEdge.z);

            m_Min = newCenter - newEdge;
            m_Max = newCenter + newEdge;
        }

        BoundingBox BoundingBox::transformed(const glm::mat4& transform) const
        {
            DS_PROFILE_FUNCTION_LOW();
            BoundingBox box(*this);
            box.transform(transform);
            return box;
        }

        void BoundingBox::merge(const BoundingBox& other)
        {
            DS_PROFILE_FUNCTION_LOW();
            if(other.m_Min.x < m_Min.x)
                m_Min.x = other.m_Min.x;
            if(other.m_Min.y < m_Min.y)
                m_Min.y = other.m_Min.y;
            if(other.m_Min.z < m_Min.z)
                m_Min.z = other.m_Min.z;
            if(other.m_Max.x > m_Max.x)
                m_Max.x = other.m_Max.x;
            if(other.m_Max.y > m_Max.y)
                m_Max.y = other.m_Max.y;
            if(other.m_Max.z > m_Max.z)
                m_Max.z = other.m_Max.z;
        }

        void BoundingBox::merge(const glm::vec3& point)
        {
            DS_PROFILE_FUNCTION_LOW();
            if(point.x < m_Min.x)
                m_Min.x = point.x;
            if(point.y < m_Min.y)
                m_Min.y = point.y;
            if(point.z < m_Min.z)
                m_Min.z = point.z;
            if(point.x > m_Max.x)
                m_Max.x = point.x;
            if(point.y > m_Max.y)
                m_Max.y = point.y;
            if(point.z > m_Max.z)
                m_Max.z = point.z;
        }

        void BoundingBox::merge(const BoundingBox& other, const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            merge(other.transformed(transform));
        }
        void BoundingBox::merge(const glm::vec3& point, const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 transformed = transform * glm::vec4(point, 1.0f);
            merge(transformed);
        }

        void BoundingBox::merge(const BoundingBox& other, const glm::mat3& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            merge(other.transformed(glm::mat4(transform)));
        }
        void BoundingBox::merge(const glm::vec3& point, const glm::mat3& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::vec3 transformed = transform * glm::vec4(point, 1.0f);
            merge(transformed);
        }

        Intersection BoundingBox::is_inside(const glm::vec3& point) const
        {
            DS_PROFILE_FUNCTION_LOW();
            if(point.x < m_Min.x || point.x > m_Max.x || point.y < m_Min.y || point.y > m_Max.y || point.z < m_Min.z || point.z > m_Max.z)
                return OUTSIDE;
            else
                return INSIDE;
        }

        Intersection BoundingBox::is_inside(const BoundingBox& box) const
        {
            DS_PROFILE_FUNCTION_LOW();
            if(box.m_Max.x < m_Min.x || box.m_Min.x > m_Max.x || box.m_Max.y < m_Min.y || box.m_Min.y > m_Max.y || box.m_Max.z < m_Min.z || box.m_Min.z > m_Max.z)
                return OUTSIDE;
            else if(box.m_Min.x < m_Min.x || box.m_Max.x > m_Max.x || box.m_Min.y < m_Min.y || box.m_Max.y > m_Max.y || box.m_Min.z < m_Min.z || box.m_Max.z > m_Max.z)
                return INTERSECTS;
            else
                return INSIDE;
        }

        Intersection BoundingBox::is_inside(const BoundingSphere& sphere) const
        {
            DS_PROFILE_FUNCTION_LOW();
            float distSquared = 0;
            float temp;
            const glm::vec3& center = sphere.get_center();

            if(center.x < m_Min.x)
            {
                temp = center.x - m_Min.x;
                distSquared += temp * temp;
            }
            else if(center.x > m_Max.x)
            {
                temp = center.x - m_Max.x;
                distSquared += temp * temp;
            }
            if(center.y < m_Min.y)
            {
                temp = center.y - m_Min.y;
                distSquared += temp * temp;
            }
            else if(center.y > m_Max.y)
            {
                temp = center.y - m_Max.y;
                distSquared += temp * temp;
            }
            if(center.z < m_Min.z)
            {
                temp = center.z - m_Min.z;
                distSquared += temp * temp;
            }
            else if(center.z > m_Max.z)
            {
                temp = center.z - m_Max.z;
                distSquared += temp * temp;
            }

            float radius = sphere.get_radius();
            if(distSquared > radius * radius)
                return OUTSIDE;
            else if(center.x - radius < m_Min.x || center.x + radius > m_Max.x || center.y - radius < m_Min.y || center.y + radius > m_Max.y || center.z - radius < m_Min.z || center.z + radius > m_Max.z)
                return INTERSECTS;
            else
                return INSIDE;
        }

        bool BoundingBox::is_inside_fast(const BoundingBox& box) const
        {
            DS_PROFILE_FUNCTION_LOW();
            if(box.m_Max.x < m_Min.x || box.m_Min.x > m_Max.x || box.m_Max.y < m_Min.y || box.m_Min.y > m_Max.y || box.m_Max.z < m_Min.z || box.m_Min.z > m_Max.z)
                return false;
            else
                return true;
        }

        glm::vec3 BoundingBox::size() const
        {
            return m_Max - m_Min;
        }

        glm::vec3 BoundingBox::center() const
        {
            return (m_Max + m_Min) * 0.5f;
        }

        glm::vec3 BoundingBox::min() const
        {
            return m_Min;
        }

        glm::vec3 BoundingBox::max() const
        {
            return m_Max;
        }

        bool BoundingBox::defined() const
        {
            return m_Max != glm::vec3(-FLT_MAX) && m_Min != glm::vec3(FLT_MAX);;
        }
    }
}
