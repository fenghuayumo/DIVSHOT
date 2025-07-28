#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace diverse
{
    namespace maths
    {
        class Transform;
    }
    class Camera;

    class DS_EXPORT CameraController
    {
    public:
        CameraController()          = default;
        virtual ~CameraController() = default;

        virtual void handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos) {};
        virtual void handle_keyboard(maths::Transform& transform, float dt) {};
        virtual void update_scroll(maths::Transform& transform, float offset, float dt) {};

        virtual void on_imgui() {};

        void set_mouse_sensitivity(float value) { mouse_sensitivity = value; }

        const glm::vec3& get_velocity() const { return velocity; }

        void set_camera(Camera* camera) { this->camera = camera; }

    protected:
        glm::vec3 velocity;
        glm::vec2 rotate_velocity;
        glm::vec2 arc_rotate_velocity;
        glm::vec3 focal_point;

        float zoom_velocity = 0.0f;
        float camera_speed  = 0.0f;
        float distance     = 0.0f;
        float zoom         = 1.0f;

        glm::vec2 projection_offset  = glm::vec2(0.0f, 0.0f);
        glm::vec2 previous_curser_pos = glm::vec2(0.0f, 0.0f);
        float mouse_sensitivity = 0.1f;

        float zoom_dampening_factor   = 0.00001f;
        float dampening_factor       = 0.00001f;
        float rotate_dampening_factor = 0.001f;

        Camera* camera = nullptr;
    };

}
