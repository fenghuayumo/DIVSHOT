#include "precompile.h"
#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

#define REVERSED 1

namespace diverse
{
    Camera::Camera()
        : aspect_ratio(1.0f)
        , near_plane(0.1f)
        , far_plane(100.0f)
        , fov(60.0f)
        , projection_dirty(true)
        , frustum_dirty(true)
        , orthographic(false)
        , mouse_sensitivity(0.1f)
    {
    }

    Camera::Camera(float FOV, float Near, float Far, float aspect)
        : aspect_ratio(aspect)
        , frustum_dirty(true)
        , projection_dirty(true)
        , fov(FOV)
        , near_plane(Near)
        , far_plane(Far)
        , orthographic(false)
        , scale(1.0f) {};

    Camera::Camera(float aspectRatio, float scale)
        : aspect_ratio(aspectRatio)
        , scale(scale)
        , frustum_dirty(true)
        , projection_dirty(true)
        , fov(60)
        , near_plane(-10.0)
        , far_plane(10.0f)
        , orthographic(true)
    {
    }

    Camera::Camera(float aspectRatio, float nearf, float farf)
        : aspect_ratio(aspectRatio)
        , scale(1.0f)
        , frustum_dirty(true)
        , projection_dirty(true)
        , fov(60)
        , near_plane(nearf)
        , far_plane(farf)
        , orthographic(true)
    {
    }

    void Camera::update_projection_matrix()
    {
#if REVERSED
        if(orthographic)
            proj_matrix = glm::ortho(-aspect_ratio * scale, aspect_ratio * scale, -scale, scale, far_plane, near_plane);
        else
            proj_matrix = glm::perspective(glm::radians(fov), aspect_ratio, far_plane, near_plane);
#else
        if(orthographic)
            proj_matrix = glm::ortho(-aspect_ratio * scale, aspect_ratio * scale, -scale, scale, near_plane, far_plane);
        else
            proj_matrix = glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
#endif
         //proj_matrix[1][1] *= -1;
    }

    maths::Frustum& Camera::get_frustum(const glm::mat4& viewMatrix)
    {
        if(projection_dirty)
            update_projection_matrix();

        {
            frustum.define(proj_matrix, viewMatrix);
            frustum_dirty = false;
        }

        return frustum;
    }

    const glm::mat4& Camera::get_projection_matrix()
    {
        if(projection_dirty)
        {
            update_projection_matrix();
            projection_dirty = false;
        }
        return proj_matrix;
    }

    maths::Ray Camera::get_screen_ray(float x, float y, const glm::mat4& viewMatrix, bool flipY) const
    {
        maths::Ray ret;
        glm::mat4 viewProjInverse = glm::inverse(proj_matrix * viewMatrix);

        x = 2.0f * x - 1.0f;
        y = 2.0f * y - 1.0f;

        if(flipY)
            y *= -1.0f;
#if REVERSED
        const auto farz = 0.0f;
        const auto nearz = 1.0f;
#else
        const auto farz = 1.0f;
        const auto nearz = 0.0f;
#endif
        glm::vec4 n = viewProjInverse * glm::vec4(x, y, nearz, 1.0f);
        n /= n.w;

        glm::vec4 f = viewProjInverse * glm::vec4(x, y, farz, 1.0f);
        f /= f.w;

        ret.Origin    = glm::vec3(n);
        ret.Direction = glm::normalize(glm::vec3(f) - ret.Origin);

        return ret;
    }

}
