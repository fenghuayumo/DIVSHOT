#pragma once

#include "camera_controller.h"

#include "maths/ray.h"
#include "maths/frustum.h"

#include "scene/serialisation.h"

namespace diverse
{
    class DS_EXPORT Camera
    {
    public:
        Camera();
        Camera(float FOV, float Near, float Far, float aspect);
        Camera(float aspectRatio, float scale);
        Camera(float aspectRatio, float near, float far);
        ~Camera() = default;

        void set_mouse_sensitivity(float value)
        {
            mouse_sensitivity = value;
        }

        void set_orthographic(bool ortho)
        {
            frustum_dirty    = true;
            projection_dirty = true;
            orthographic    = ortho;
        }

        bool is_orthographic() const
        {
            return orthographic;
        }

        float get_aspect_ratio() const
        {
            return aspect_ratio;
        }

        void set_aspect_ratio(float y)
        {
            aspect_ratio     = y;
            projection_dirty = true;
            frustum_dirty    = true;
        };

        const glm::mat4& get_projection_matrix();

        float get_far() const
        {
            return far_plane;
        }
        float get_near() const
        {
            return near_plane;
        }

        void set_far(float pFar)
        {
            far_plane = pFar;
            projection_dirty = true;
            frustum_dirty = true;
        }

        void set_near(float pNear)
        {
            near_plane            = pNear;
            projection_dirty = true;
            frustum_dirty    = true;
        }

        float get_fov() const
        {
            return fov;
        }

        float get_scale() const
        {
            return scale;
        }

        void set_scale(float scale)
        {
            this->scale = scale;
            projection_dirty = true;
            frustum_dirty    = true;
        }

        void set_fov(float fov)
        {
            this->fov             = fov;
            projection_dirty = true;
            frustum_dirty = true;
        }

        maths::Frustum& get_frustum(const glm::mat4& viewMatrix);

        maths::Ray get_screen_ray(float x, float y, const glm::mat4& viewMatrix, bool flipY) const;

        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("Scale", scale), cereal::make_nvp("Aspect", aspect_ratio), cereal::make_nvp("FOV", fov), cereal::make_nvp("Near", near_plane), cereal::make_nvp("Far", far_plane));

            archive(cereal::make_nvp("Aperture", aperture), cereal::make_nvp("ShutterSpeed", shutter_speed), cereal::make_nvp("Sensitivity", sensitivity));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            archive(cereal::make_nvp("Scale", scale), cereal::make_nvp("Aspect", aspect_ratio), cereal::make_nvp("FOV", fov), cereal::make_nvp("Near", near_plane), cereal::make_nvp("Far", far_plane));

            if(Serialisation::CurrentSceneVersion > 11)
            {
                archive(cereal::make_nvp("Aperture", aperture), cereal::make_nvp("ShutterSpeed", shutter_speed), cereal::make_nvp("Sensitivity", sensitivity));
            }

            frustum_dirty    = true;
            projection_dirty = true;
        }

        float get_aperture() const { return aperture; }
        void set_aperture(const float aperture) { this->aperture = aperture; }

        float get_shutter_speed() const { return shutter_speed; }
        void set_shutter_speed(const float shutterSpeed) { shutter_speed = shutterSpeed; }

        float get_sensitivity() const { return sensitivity; }
        void set_sensitivity(const float sensitivity) { this->sensitivity = sensitivity; }

        // https://google.github.io/filament/Filament.html
        float get_ev100() const { return std::log2((aperture * aperture) / shutter_speed * 100.0f / shutter_speed); }
        float get_exposure() const { return 1.0f / (std::pow(2.0f, get_ev100()) * 1.2f); }

    protected:
        void update_projection_matrix();

        float aspect_ratio          = 1.0f;
        float scale                = 1.0f;
        float zoom                 = 1.0f;

        glm::vec2 projection_offset = glm::vec2(0.0f, 0.0f);

        glm::mat4 proj_matrix;

        maths::Frustum frustum;
        bool frustum_dirty     = true;
        bool projection_dirty  = false;
        bool custom_projection = false;

        float fov = 60.0f, near_plane = 0.001f, far_plane = 1000.0f;
        float mouse_sensitivity = 0.1f;
        float aperture         = 50.0f;
        float shutter_speed     = 1.0f / 60.0f;
        float sensitivity      = 250.0f;

        bool orthographic = false;
    };
}
