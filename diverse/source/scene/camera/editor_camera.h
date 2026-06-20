#pragma once
#include "camera_controller.h"
#include "scene/components/transform_component.h"

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

        void handle_mouse(Transform& transform, float dt, float xpos, float ypos);
        void handle_keyboard(Transform& transform, float dt);

        void mouse_pan(Transform& transform, const glm::vec2& delta);
        void mouse_rotate(Transform& transform, const glm::vec2& delta);
        void mouse_zoom(Transform& transform, float delta);
        void update_camera_view(Transform& transform, float dt);

        glm::vec3 calculate_position(Transform& transform) const;
        void      update_focal_point(Transform& transform,const glm::vec3& camera_pos);
        std::pair<float, float> pan_speed() const;
        float get_rotation_speed() const;
        float get_zoom_speed() const;

        void update_scroll(Transform& transform, float offset, float dt);

        void stop_movement();
        void set_speed(float speed) { camera_speed = speed; }
        float get_speed() { return camera_speed; }
        void set_current_mode(EditorCameraMode mode);
        EditorCameraMode get_current_mode() const { return camera_mode; }
        bool is_moving() const;
        void set_buttom_view(Transform& transform);
        void set_top_view(Transform& transform);
        void set_left_view(Transform& transform);
        void set_right_view(Transform& transform);
        void set_front_view(Transform& transform);
        void set_back_view(Transform& transform);
        
        // Initialize orthographic view center from current camera state
        void init_ortho_view_from_current(const Transform& transform);
        
        // Sync focal_point from ortho_view_center (when switching back to ARCBALL)
        void sync_focal_point_from_ortho_view(const Transform& transform);
        
    private:
        EditorCameraMode camera_mode = EditorCameraMode::ARCBALL;
        glm::vec2 stored_cursor_pos;
        float camera_speed = 20.0f;
        float rotation_speed = 0.3f;
        float pitch_delta { 0.0f }, yaw_delta { 0.0f };
        glm::vec3 position_delta {};
        
        // Orthographic view center point - all 6 views look at this point
        glm::vec3 ortho_view_center {0.0f, 0.0f, 0.0f};
        float ortho_view_distance = 10.0f;
    };
}
