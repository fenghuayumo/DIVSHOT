#include "precompile.h"
#include "maths/frustum.h"
#include "maths/bounding_box.h"
#include "maths/bounding_sphere.h"
#include "maths/rect.h"
#include "maths/ray.h"

#include <glm/gtc/matrix_transform.hpp>
#define REVERSED 1

namespace diverse
{
    namespace maths
    {
        Frustum::Frustum()
        {
            define(glm::mat4(1.0f));
        }

        Frustum::Frustum(const glm::mat4& transform)
        {
            define(transform);
        }

        Frustum::Frustum(const glm::mat4& projection, const glm::mat4& view)
        {
            glm::mat4 m = projection * view;
            define(m);
        }

        Frustum::~Frustum()
        {
        }

        void Frustum::define(const glm::mat4& projection, const glm::mat4& view)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::mat4 m = projection * view;
            define(m);
        }

        void Frustum::transform(const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                m_Planes[i].transform(transform);
            }

            for(int i = 0; i < 6; i++)
            {
                m_Planes[i].normalise();
            }

            calculate_vertices(transform);
        }

        void Frustum::define(const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            auto& m               = transform;
            m_Planes[PLANE_LEFT]  = Plane(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);
            m_Planes[PLANE_RIGHT] = Plane(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);
            m_Planes[PLANE_DOWN]  = Plane(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);
            m_Planes[PLANE_UP]    = Plane(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);
#if REVERSED
            m_Planes[PLANE_FAR]  = Plane(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]);
            m_Planes[PLANE_NEAR]   = Plane(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);
#else
            m_Planes[PLANE_NEAR]  = Plane(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]);
            m_Planes[PLANE_FAR]   = Plane(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);
#endif
            for(int i = 0; i < 6; i++)
            {
                m_Planes[i].normalise();
            }

            calculate_vertices(transform);
        }

        void Frustum::define_ortho(float scale, float aspectRatio, float n, float f, const glm::mat4& viewMatrix)
        {
            DS_PROFILE_FUNCTION_LOW();
            glm::mat4 m = glm::ortho(-scale * aspectRatio, scale * aspectRatio, -scale, scale, n, f);
            m           = m * viewMatrix;
            define(m);
        }
        void Frustum::define(float fov, float aspectRatio, float n, float f, const glm::mat4& viewMatrix)
        {
            DS_PROFILE_FUNCTION_LOW();
            float tangent = tan(fov * 0.5f);
            float height  = n * tangent;
            float width   = height * aspectRatio;

            glm::mat4 m = glm::frustum(-width, width, -height, height, n, f);
            m           = m * viewMatrix;
            define(m);
        }

        bool Frustum::is_inside(const glm::vec3& point) const
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                if(m_Planes[i].distance(point) < 0.0f)
                {
                    return false;
                }
            }

            return true;
        }

        bool Frustum::is_inside(const BoundingSphere& sphere) const
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                if(m_Planes[i].distance(sphere.get_center()) < -sphere.get_radius())
                {
                    return false;
                }
            }

            return true;
        }

        bool Frustum::is_inside(const BoundingBox& box) const
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                glm::vec3 p = box.min(), n = box.max();
                glm::vec3 N = m_Planes[i].normal();
                if(N.x >= 0)
                {
                    p.x = box.max().x;
                    n.x = box.min().x;
                }
                if(N.y >= 0)
                {
                    p.y = box.max().y;
                    n.y = box.min().y;
                }
                if(N.z >= 0)
                {
                    p.z = box.max().z;
                    n.z = box.min().z;
                }

                if(m_Planes[i].distance(p) < 0)
                {
                    return false;
                }
            }
            return true;
        }

        bool Frustum::is_inside(const Rect& rect) const
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                glm::vec3 N = m_Planes[i].normal();
                if(N.x >= 0)
                {
                    if(m_Planes[i].distance(glm::vec3(rect.get_position(), 0)) < 0)
                    {
                        return false;
                    }
                }
                else
                {
                    if(m_Planes[i].distance(glm::vec3(rect.get_position().x + rect.get_size().x, rect.get_position().y, 0)) < 0)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        bool Frustum::is_inside(const Ray& ray) const
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                if(m_Planes[i].distance(ray.Origin) < 0.0f)
                {
                    return false;
                }
            }

            return true;
        }

        bool Frustum::is_inside(const Plane& plane) const
        {
            DS_PROFILE_FUNCTION_LOW();
            for(int i = 0; i < 6; i++)
            {
                if(m_Planes[i].distance(plane.normal()) < 0.0f)
                {
                    return false;
                }
            }

            return true;
        }

        BoundingBox Frustum::get_bounding_box() const
        {
            maths::BoundingBox box;
            for (int i = 0; i < 8; i++)
            {
            	box.merge(m_Verticies[i]);
            }
            return box;
        }

        const Plane& Frustum::get_plane(FrustumPlane plane) const
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Planes[plane];
        }

        glm::vec3* Frustum::get_verticies()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Verticies;
        }

        void Frustum::calculate_vertices(const glm::mat4& transform)
        {
            DS_PROFILE_FUNCTION_LOW();
            static const bool zerotoOne = false;
            static const bool leftHand  = true;

            glm::mat4 transformInv = glm::inverse(transform);
#if REVERSED
            m_Verticies[0] = glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f);
            m_Verticies[1] = glm::vec4(1.0f, -1.0f, 1.0f, 1.0f);
            m_Verticies[2] = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            m_Verticies[3] = glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f);

            m_Verticies[4] = glm::vec4(-1.0f, -1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
            m_Verticies[5] = glm::vec4(1.0f, -1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
            m_Verticies[6] = glm::vec4(1.0f, 1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
            m_Verticies[7] = glm::vec4(-1.0f, 1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
#else
            m_Verticies[0] = glm::vec4(-1.0f, -1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
            m_Verticies[1] = glm::vec4(1.0f, -1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
            m_Verticies[2] = glm::vec4(1.0f, 1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);
            m_Verticies[3] = glm::vec4(-1.0f, 1.0f, zerotoOne ? 0.0f : -1.0f, 1.0f);

            m_Verticies[4] = glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f);
            m_Verticies[5] = glm::vec4(1.0f, -1.0f, 1.0f, 1.0f);
            m_Verticies[6] = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            m_Verticies[7] = glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f);
#endif

            glm::vec4 temp;
            for(int i = 0; i < 8; i++)
            {
                temp           = transformInv * glm::vec4(m_Verticies[i], 1.0f);
                m_Verticies[i] = temp / temp.w;
            }
        }
    }
}
