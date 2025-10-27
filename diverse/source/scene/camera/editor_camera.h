#pragma once
#include "camera_controller.h"

namespace diverse
{
    enum class EditorCameraMode
    {
        NONE,
        FLYCAM,
        ARCBALL,
        TWODIM
    };

    class EditorCameraController : public CameraController
    {
    public:
        EditorCameraController();
        ~EditorCameraController();

        virtual void handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos) override;
        virtual void handle_keyboard(maths::Transform& transform, float dt) override;

        void mouse_pan(maths::Transform& transform, const glm::vec2& delta);
        void mouse_rotate(maths::Transform& transform, const glm::vec2& delta);
        void mouse_zoom(maths::Transform& transform, float delta);
        void update_camera_view(maths::Transform& transform, float dt);

        glm::vec3 calculate_position(maths::Transform& transform) const;
        void      update_focal_point(maths::Transform& transform,const glm::vec3& camera_pos);
        std::pair<float, float> pan_speed() const;
        float get_rotation_speed() const;
        float get_zoom_speed() const;

        void update_scroll(maths::Transform& transform, float offset, float dt) override;

        void stop_movement();
        void set_speed(float speed) { camera_speed = speed; }
        float get_speed() { return camera_speed; }
        void set_current_mode(EditorCameraMode mode) { camera_mode = mode; }
        EditorCameraMode get_current_mode() const { return camera_mode; }
        bool is_moving() const;
        void set_buttom_view(maths::Transform& transform);
        void set_top_view(maths::Transform& transform);
        void set_left_view(maths::Transform& transform);
        void set_right_view(maths::Transform& transform);
        void set_front_view(maths::Transform& transform);
        void set_back_view(maths::Transform& transform);
    private:
        EditorCameraMode camera_mode = EditorCameraMode::ARCBALL;
        glm::vec2 stored_cursor_pos;
        float camera_speed = 20.0f;
        float rotation_speed = 0.3f;
        float pitch_delta { 0.0f }, yaw_delta { 0.0f };
        glm::vec3 position_delta {};
    };
}
