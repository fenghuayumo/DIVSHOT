#include "precompile.h"
#include "editor_camera.h"
#include "camera.h"
#include "engine/application.h"
#include "engine/input.h"
#include "engine/window.h"
#include "maths/maths_utils.h"
#include "maths/transform.h"
#include "maths/maths_log.hpp"
namespace diverse
{
    namespace
    {
        constexpr float kMoveResponse = 12.0f;
        constexpr float kStopEpsilon = 0.001f;
        constexpr float kMinOrbitDistance = 0.25f;
        constexpr float kMinOrthoScale = 0.15f;
        constexpr float kMaxOrthoScale = 10000.0f;
        constexpr float kArcballPanMouseScale = 0.01f;
        constexpr float kOrthoPanMouseScale = 0.03f;
        constexpr float kArcballRotateMouseScale = 0.008f;
        constexpr float kFlycamRotateMouseScale = 0.002f;

        glm::vec3 smooth_velocity(const glm::vec3& current, const glm::vec3& target, float dt)
        {
            const float blend = glm::clamp(dt * kMoveResponse, 0.0f, 1.0f);
            return glm::mix(current, target, blend);
        }

        float get_ortho_scale(const Camera* camera)
        {
            return camera ? glm::max(camera->get_scale(), kMinOrthoScale) : 1.0f;
        }
    }

    EditorCameraController::EditorCameraController()
    {
        focal_point            = glm::vec3();
        velocity              = glm::vec3(0.0f);
        rotate_velocity        = glm::vec2(0.0f);
        previous_curser_pos     = glm::vec3(0.0f);
        mouse_sensitivity      = 0.00001f;
        zoom_dampening_factor   = 0.00001f;
        dampening_factor       = 0.00001f;  // Smooth keyboard movement
        rotate_dampening_factor = 0.000001f;  // Smooth rotation
        camera_mode            = EditorCameraMode::ARCBALL;
        distance = 10.0f;
    }

    EditorCameraController::~EditorCameraController()
    {
    }

    void EditorCameraController::update_camera_view(maths::Transform& transform, float dt)
    {
        const float yawSign = transform.get_up_direction().y < 0 ? -1.0f : 1.0f;

        // Extra step to handle the problem when the camera direction is the same as the up vector
        const float cosAngle = glm::dot(transform.get_forward_direction(), transform.get_up_direction());
        if(cosAngle * yawSign > 0.99f)
            pitch_delta = 0.f;

        // damping for smooth camera
        yaw_delta *= pow(dampening_factor, dt);
        pitch_delta *= pow(dampening_factor, dt);
        position_delta *= pow(dampening_factor, dt);
    }
    void EditorCameraController::handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos)
    {
        DS_PROFILE_FUNCTION();
        
        // Camera Mouse Controls:
        // ARCBALL Mode:
        //   - Right Mouse Button: Orbit camera around focal point
        //   - Middle Mouse Button: Pan camera (translate view)
        //   - Middle + Right Mouse Button: Pan camera (alternative)
        //   - Mouse Scroll Wheel: Dolly/Zoom camera
        // FLYCAM Mode:
        //   - Right Mouse Button: FPS-style rotation
        //   - Mouse Scroll Wheel: Move camera forward/backward
        // TWODIM Mode:
        //   - Middle Mouse Button: Pan camera in 2D

        // distance = glm::distance(transform.get_local_position(), focal_point);

        if(camera_mode == EditorCameraMode::TWODIM)
        {
            static bool mouseHeld = false;
            if(Input::get().get_mouse_clicked(InputCode::MouseKey::ButtonMiddle))
            {
                mouseHeld = true;
                Application::get().get_window()->hide_mouse(true);
                Input::get().set_mouse_mode(MouseMode::Captured);
                stored_cursor_pos   = glm::vec2(xpos, ypos);
                previous_curser_pos = stored_cursor_pos;
            }

            if(Input::get().get_mouse_held(InputCode::MouseKey::ButtonMiddle))
            {
                mouse_sensitivity = get_ortho_scale(camera) * kOrthoPanMouseScale;
                
                // Use camera's actual right and up directions for panning
                // This works correctly for all 6 orthographic views
                glm::vec3 right = transform.get_right_direction();
                glm::vec3 up = transform.get_up_direction();
                
                glm::vec3 position = transform.get_local_position();
                position -= right * (xpos - previous_curser_pos.x) * mouse_sensitivity;
                position += up * (ypos - previous_curser_pos.y) * mouse_sensitivity;
                transform.set_local_position(position);
                
                // Also update ortho_view_center so switching views maintains the new position
                ortho_view_center = position - transform.get_forward_direction() * ortho_view_distance;
                
                previous_curser_pos = glm::vec2(xpos, ypos);
            }
            else
            {
                if(mouseHeld)
                {
                    mouseHeld = false;
                    Application::get().get_window()->hide_mouse(false);
                    Application::get().get_window()->set_mouse_position(stored_cursor_pos);
                    Input::get().set_mouse_mode(MouseMode::Visible);
                }
            }
        }
        else
        {
            if(camera_mode == EditorCameraMode::FLYCAM)
            { 
                static bool mouseHeld = false;
                if(Input::get().get_mouse_clicked(InputCode::MouseKey::ButtonRight))
                {
                    mouseHeld = true;
                    Application::get().get_window()->hide_mouse(true);
                    Input::get().set_mouse_mode(MouseMode::Captured);
                    stored_cursor_pos   = glm::vec2(xpos, ypos);
                    previous_curser_pos = stored_cursor_pos;
                }

                if(Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight))
                {
                    rotate_velocity = glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * kFlycamRotateMouseScale;
                }
                else
                {
                    if(mouseHeld)
                    {
                        mouseHeld = false;
                        Application::get().get_window()->hide_mouse(false);
                        Application::get().get_window()->set_mouse_position(stored_cursor_pos);
                        Input::get().set_mouse_mode(MouseMode::Visible);
                    }
                }
            }
            else if(camera_mode == EditorCameraMode::ARCBALL)
            {
                static bool mouseHeld = false;
                if (Input::get().get_mouse_clicked(InputCode::MouseKey::ButtonRight) || Input::get().get_mouse_clicked(InputCode::MouseKey::ButtonMiddle))
                {
                    mouseHeld = true;
                    stored_cursor_pos = glm::vec2(xpos, ypos);
                    previous_curser_pos = stored_cursor_pos;
                }
  
                // Check for Middle + Right mouse button combination for panning
                bool middleAndRight = Input::get().get_mouse_held(InputCode::MouseKey::ButtonMiddle) && 
                                     Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight);
                
                if (middleAndRight)
                {
                    // MMB + RMB = Pan camera
                    mouse_pan(transform, glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * kArcballPanMouseScale);
                    previous_curser_pos = glm::vec2(xpos, ypos);
                }
                else if (Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight))
                {
                    // RMB only = Orbit camera
                    float rotate_scale = kArcballRotateMouseScale;
                    if(Input::get().get_key_held(InputCode::Key::LeftShift))
                        rotate_scale *= 1.8f;
                    else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
                        rotate_scale *= 0.35f;

                    rotate_velocity = glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * rotate_scale;
                }
                else if (Input::get().get_mouse_held(InputCode::MouseKey::ButtonMiddle))
                {
                    // MMB only = Dolly camera (pan movement)
                    mouse_pan(transform, glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * kArcballPanMouseScale);
                    previous_curser_pos = glm::vec2(xpos, ypos);
                }
                else
                {
                    if (mouseHeld)
                    {
                        mouseHeld = false;
                        Application::get().get_window()->set_mouse_position(stored_cursor_pos);
                    }
                }
            }

            if(glm::length(rotate_velocity) > maths::M_EPSILON || glm::abs(pitch_delta) > maths::M_EPSILON || glm::abs(yaw_delta) > maths::M_EPSILON)
            {
                if(camera_mode == EditorCameraMode::ARCBALL)
                {
                    mouse_rotate(transform, rotate_velocity);
                    previous_curser_pos = glm::vec2(xpos, ypos);

                    glm::quat rotation  = transform.get_local_orientation();
                    glm::quat rotationX = glm::angleAxis(-pitch_delta, glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::quat rotationY = glm::angleAxis(-yaw_delta, glm::vec3(0.0f, 1.0f, 0.0f));

                    rotation = rotationY * rotation;
                    rotation = rotation * rotationX;
                    transform.set_local_orientation(rotation);
                    transform.set_local_position(calculate_position(transform));

                    yaw_delta = 0.0f;
                    pitch_delta = 0.0f;
                    rotate_velocity = glm::vec2(0.0f);
                }
                else if(camera_mode == EditorCameraMode::FLYCAM)
                {
                    glm::quat rotation  = transform.get_local_orientation();
                    glm::quat rotationX = glm::angleAxis(-rotate_velocity.y, glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::quat rotationY = glm::angleAxis(-rotate_velocity.x, glm::vec3(0.0f, 1.0f, 0.0f));

                    rotation = rotationY * rotation;
                    rotation = rotation * rotationX;

                    previous_curser_pos = glm::vec2(xpos, ypos);
                    transform.set_local_orientation(rotation);
                }
            }
        }

        if(camera_mode != EditorCameraMode::ARCBALL)
            rotate_velocity = rotate_velocity * pow(rotate_dampening_factor, dt);

        if(camera_mode == EditorCameraMode::ARCBALL)
        {
            mouse_zoom(transform, Input::get().get_scroll_offset());
            update_camera_view(transform, dt);
            if ( (glm::length(position_delta) > 0.001f) || !maths::Equals(glm::length(rotate_velocity), 0.0f))
            {
                transform.set_local_position(calculate_position(transform));
            }
        }
        else
        {
            update_scroll(transform, Input::get().get_scroll_offset(), dt);
        }
    }

    void EditorCameraController::handle_keyboard(maths::Transform& transform, float dt)
    {
        // Camera Keyboard Controls:
        // ARCBALL Mode:
        //   - W/S Keys: Dolly camera forwards/backwards
        //   - A/D Keys: Strafe camera left/right
        //   - Q/E Keys: Move camera upward/downward (world space)
        //   - Shift Key: Accelerate movement (3x speed)
        // FLYCAM Mode (only when RMB held):
        //   - W/S Keys: Move forward/backward
        //   - A/D Keys: Strafe left/right
        //   - Q/E Keys: Move down/up (camera space)
        //   - Shift Key: Accelerate movement (10x speed)
        //   - Alt Key: Slow movement (0.5x speed)
        // TWODIM Mode:
        //   - W/S Keys: Move up/down
        //   - A/D Keys: Move left/right
        
        glm::vec3 target_velocity(0.0f);
        bool move_focal_point = false;

        if(camera_mode == EditorCameraMode::TWODIM)
        {
            const glm::vec3 right = transform.get_right_direction();
            const glm::vec3 up = transform.get_up_direction();
            float multiplier = get_ortho_scale(camera);

            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier *= 3.0f;
            }
            else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
            {
                multiplier *= 0.35f;
            }

            const float speed = camera_speed * multiplier;

            if(Input::get().get_key_held(InputCode::Key::A))
                target_velocity -= right * speed;
            if(Input::get().get_key_held(InputCode::Key::D))
                target_velocity += right * speed;
            if(Input::get().get_key_held(InputCode::Key::W))
                target_velocity += up * speed;
            if(Input::get().get_key_held(InputCode::Key::S))
                target_velocity -= up * speed;
        }
        else if(camera_mode == EditorCameraMode::ARCBALL)
        {
            float multiplier = 1.0f;
            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier = 3.0f;
            }
            else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
            {
                multiplier = 0.35f;
            }

            const float speed = camera_speed * glm::max(distance * 0.15f, 1.0f) * multiplier;

            if(Input::get().get_key_held(InputCode::Key::W))
                target_velocity -= transform.get_forward_direction() * speed;
            if(Input::get().get_key_held(InputCode::Key::S))
                target_velocity += transform.get_forward_direction() * speed;
            if(Input::get().get_key_held(InputCode::Key::A))
                target_velocity -= transform.get_right_direction() * speed;
            if(Input::get().get_key_held(InputCode::Key::D))
                target_velocity += transform.get_right_direction() * speed;
            if(Input::get().get_key_held(InputCode::Key::Q))
                target_velocity -= glm::vec3(0.0f, 1.0f, 0.0f) * speed;
            if(Input::get().get_key_held(InputCode::Key::E))
                target_velocity += glm::vec3(0.0f, 1.0f, 0.0f) * speed;

            move_focal_point = true;
        }
        else if(camera_mode == EditorCameraMode::FLYCAM)
        {
            float multiplier = 1.0f;

            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier = 10.0f;
            }
            else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
            {
                multiplier = 0.35f;
            }

            const float speed = camera_speed * multiplier;

            if(Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight))
            {
                if(Input::get().get_key_held(InputCode::Key::W))
                    target_velocity -= transform.get_forward_direction() * speed;
                if(Input::get().get_key_held(InputCode::Key::S))
                    target_velocity += transform.get_forward_direction() * speed;
                if(Input::get().get_key_held(InputCode::Key::A))
                    target_velocity -= transform.get_right_direction() * speed;
                if(Input::get().get_key_held(InputCode::Key::D))
                    target_velocity += transform.get_right_direction() * speed;
                if(Input::get().get_key_held(InputCode::Key::Q))
                    target_velocity -= transform.get_up_direction() * speed;
                if(Input::get().get_key_held(InputCode::Key::E))
                    target_velocity += transform.get_up_direction() * speed;
            }
        }

        velocity = smooth_velocity(velocity, target_velocity, dt);
        if(glm::length(velocity) < kStopEpsilon)
            velocity = glm::vec3(0.0f);

        if(glm::length(velocity) > maths::M_EPSILON)
        {
            const glm::vec3 delta = velocity * dt;
            transform.set_local_position(transform.get_local_position() + delta);

            if(camera_mode == EditorCameraMode::TWODIM)
                ortho_view_center += delta;
            else if(move_focal_point)
                focal_point += delta;
        }
    }

    std::pair<float, float> EditorCameraController::pan_speed() const
    {
        const float x       = glm::min(float(1920) / 1000.0f, 2.4f); // max = 2.4f
        const float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

        const float y       = glm::min(float(1080) / 1000.0f, 2.4f); // max = 2.4f
        const float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

        return { xFactor * camera_speed / 40.0f, yFactor * camera_speed / 40.0f};
    }

    float EditorCameraController::get_rotation_speed() const
    {
        return rotation_speed;
    }

    float EditorCameraController::get_zoom_speed() const
    {
        float dist = glm::max(distance, kMinOrbitDistance) * 0.4f;
        dist = glm::max(dist, 4.0f);
        float speed    = dist * dist;
        speed          = glm::min(speed, 50.0f); // max speed = 50
        return speed * camera_speed / 1000.0f;
    }
    void EditorCameraController::mouse_pan(maths::Transform& transform, const glm::vec2& delta)
    {
        // Apply shift key acceleration for faster panning
        float multiplier = 1.0f;
        if(Input::get().get_key_held(InputCode::Key::LeftShift))
        {
            multiplier = 3.0f;  // Accelerate panning
        }
        
        auto [xSpeed, ySpeed] = pan_speed();
        focal_point -= transform.get_right_direction() * std::clamp(delta.x, -100.0f, 100.0f) * xSpeed * distance * multiplier;
        focal_point += transform.get_up_direction() * std::clamp(delta.y, -100.0f, 100.0f) * ySpeed * distance * multiplier;
        transform.set_local_position(calculate_position(transform));
    }

    void EditorCameraController::mouse_rotate(maths::Transform& transform, const glm::vec2& delta)
    {
        const float yawSign = transform.get_up_direction().y < 0.0f ? -1.0f : 1.0f;
        yaw_delta += yawSign * delta.x * get_rotation_speed();
        pitch_delta += delta.y * get_rotation_speed();
    }

    void EditorCameraController::mouse_zoom(maths::Transform& transform, float delta)
    {
        if (Input::get().get_key_held(InputCode::Key::LeftControl))
        {
            return;
        }
        if( delta != 0 )
        {
            // Apply shift key acceleration for faster zoom
            float multiplier = 1.0f;
            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier = 3.0f;  // Accelerate zoom
            }
            else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
            {
                multiplier = 0.35f;
            }
            
            distance = glm::max(distance - delta * get_zoom_speed() * multiplier, kMinOrbitDistance);
            position_delta = glm::vec3(0.0f);
            transform.set_local_position(calculate_position(transform));
        }
    }

    void EditorCameraController::update_scroll(maths::Transform& transform, float offset, float dt)
    {
        if (Input::get().get_key_held(InputCode::Key::LeftControl))
        {
            return;
        }
        if(camera_mode == EditorCameraMode::TWODIM)
        {
            if(!camera)
                return;

            float multiplier = 1.0f;
            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier = 2.0f;
            }
            else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
            {
                multiplier = 0.5f;
            }

            if(offset != 0.0f)
            {
                const float zoom_factor = glm::pow(0.88f, offset * multiplier);
                const float scale = glm::clamp(camera->get_scale() * zoom_factor, kMinOrthoScale, kMaxOrthoScale);
                camera->set_scale(scale);
            }
        }
        else
        {
            // Apply shift key acceleration for faster scroll zoom
            float multiplier = 1.0f;
            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier = 3.0f;  // Accelerate zoom
            }
            else if(Input::get().get_key_held(InputCode::Key::LeftAlt))
            {
                multiplier = 0.35f;
            }

            if(offset != 0.0f)
            {
                const glm::vec3 delta = -transform.get_forward_direction() * offset * camera_speed * 0.25f * multiplier;
                transform.set_local_position(transform.get_local_position() + delta);
            }
        }
    }

    void EditorCameraController::stop_movement()
    {
        zoom_velocity   = 0.0f;
        velocity       = glm::vec3(0.0f);
        rotate_velocity = glm::vec2(0.0f);
        pitch_delta = 0.0f;
        yaw_delta = 0.0f;
        position_delta = glm::vec3(0.0f);
    }

    void EditorCameraController::set_current_mode(EditorCameraMode mode)
    {
        if(camera_mode == mode)
            return;

        camera_mode = mode;
        stop_movement();
    }

    bool EditorCameraController::is_moving() const
    {
        return glm::length(velocity) > 0.0f ||
               glm::length(rotate_velocity) > 0.0f ||
               glm::length(position_delta) > 0.0f ||
               glm::abs(pitch_delta) > 0.0f ||
               glm::abs(yaw_delta) > 0.0f ||
               zoom_velocity != 0.0f;
    }

    glm::vec3 EditorCameraController::calculate_position(maths::Transform& transform) const
    {
        auto forward = transform.get_forward_direction();
        auto right = transform.get_right_direction();
        auto up = transform.get_up_direction();
        auto camera_pos = focal_point + forward * distance;// + right * distance + up * distance;
        return camera_pos + position_delta;
    }

    void EditorCameraController::update_focal_point(maths::Transform& transform,const glm::vec3& camera_pos)
    {
        transform.set_local_position(camera_pos);
        focal_point = camera_pos - transform.get_forward_direction() * distance;
        //+ transform.get_right_direction() * distance  - transform.get_up_direction() * distance;
    }
    
    void EditorCameraController::init_ortho_view_from_current(const maths::Transform& transform)
    {
        // Calculate the center point that the current camera is looking at
        glm::vec3 forward = transform.get_forward_direction();
        glm::vec3 camera_pos = transform.get_local_position();
        
        // Use focal_point if available (for ARCBALL mode)
        if (glm::length(focal_point) > 0.01f && distance > 0.1f)
        {
            // Use the existing focal_point from ARCBALL mode
            ortho_view_center = focal_point;
            ortho_view_distance = distance;
        }
        else
        {
            // Calculate from current position and forward direction
            // Use a reasonable default distance
            ortho_view_distance = 10.0f;
            ortho_view_center = camera_pos - forward * ortho_view_distance;
        }
    }
    
    void EditorCameraController::sync_focal_point_from_ortho_view(const maths::Transform& transform)
    {
        // Sync focal_point and distance from ortho_view data
        // This is called when switching back to ARCBALL mode
        focal_point = ortho_view_center;
        distance = ortho_view_distance;
    }
    void EditorCameraController::set_front_view(maths::Transform& transform)
    {
        // Front view: looking along negative Z-axis (toward -Z)
        // Camera position: ortho_view_center + (0, 0, +distance)
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.set_local_orientation(rotation);
        
        glm::vec3 new_position = ortho_view_center + glm::vec3(0.0f, 0.0f, ortho_view_distance);
        transform.set_local_position(new_position);
    }

    void EditorCameraController::set_back_view(maths::Transform& transform)
    {
        // Back view: looking along positive Z-axis (toward +Z)
        // Camera position: ortho_view_center + (0, 0, -distance)
        glm::quat rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        glm::vec3 new_position = ortho_view_center + glm::vec3(0.0f, 0.0f, -ortho_view_distance);
        transform.set_local_position(new_position);
    }

    void EditorCameraController::set_left_view(maths::Transform& transform)
    {
        // Left view: camera on left side looking toward +X (toward center)
        // Camera position: ortho_view_center + (-distance, 0, 0)
        glm::quat rotation = glm::angleAxis(-glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        glm::vec3 new_position = ortho_view_center + glm::vec3(-ortho_view_distance, 0.0f, 0.0f);
        transform.set_local_position(new_position);
    }

    void EditorCameraController::set_right_view(maths::Transform& transform)
    {
        // Right view: camera on right side looking toward -X (toward center)
        // Camera position: ortho_view_center + (+distance, 0, 0)
        glm::quat rotation = glm::angleAxis(glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        glm::vec3 new_position = ortho_view_center + glm::vec3(ortho_view_distance, 0.0f, 0.0f);
        transform.set_local_position(new_position);
    }

    void EditorCameraController::set_top_view(maths::Transform& transform)
    {
        // Top view: looking along negative Y-axis (toward -Y, from top)
        // Camera position: ortho_view_center + (0, +distance, 0)
        glm::quat rotation = glm::angleAxis(-glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        glm::vec3 new_position = ortho_view_center + glm::vec3(0.0f, ortho_view_distance, 0.0f);
        transform.set_local_position(new_position);
    }

    void EditorCameraController::set_buttom_view(maths::Transform& transform)
    {
        // Bottom view: looking along positive Y-axis (toward +Y, from bottom)
        // Camera position: ortho_view_center + (0, -distance, 0)
        glm::quat rotation = glm::angleAxis(glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        glm::vec3 new_position = ortho_view_center + glm::vec3(0.0f, -ortho_view_distance, 0.0f);
        transform.set_local_position(new_position);
    }

}
