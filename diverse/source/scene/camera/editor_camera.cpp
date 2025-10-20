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
    EditorCameraController::EditorCameraController()
    {
        focal_point            = glm::vec3();
        velocity              = glm::vec3(0.0f);
        rotate_velocity        = glm::vec2(0.0f);
        previous_curser_pos     = glm::vec3(0.0f);
        mouse_sensitivity      = 0.00001f;
        zoom_dampening_factor   = 0.00001f;
        dampening_factor       = 0.00001f;
        rotate_dampening_factor = 0.000001f;
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
                mouse_sensitivity = 0.1f;
                glm::vec3 position = transform.get_local_position();
                position.x -= (xpos - previous_curser_pos.x) /** camera->get_scale() */ * mouse_sensitivity * 0.5f;
                position.y += (ypos - previous_curser_pos.y) /** camera->get_scale() */ * mouse_sensitivity * 0.5f;
                transform.set_local_position(position);
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
                    mouse_sensitivity = 0.0002f;
                    rotate_velocity   = glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * mouse_sensitivity * 10.0f;
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
  
                if (Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight) )
                {
                    mouse_sensitivity = 0.0002f;
                    rotate_velocity = glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * mouse_sensitivity * 10.0f;
                }
                else
                {
                    if (mouseHeld)
                    {
                        mouseHeld = false;
                        Application::get().get_window()->set_mouse_position(stored_cursor_pos);
                    }
                }
                if (Input::get().get_mouse_held(InputCode::MouseKey::ButtonMiddle))
                    mouse_pan(transform, glm::vec2((xpos - previous_curser_pos.x), (ypos - previous_curser_pos.y)) * 0.001f);
            }

            if(glm::length(rotate_velocity) > maths::M_EPSILON || pitch_delta > maths::M_EPSILON || yaw_delta > maths::M_EPSILON)
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
        if(camera_mode == EditorCameraMode::TWODIM)
        {
            glm::vec3 up = glm::vec3(0, 1, 0), right = glm::vec3(1, 0, 0);

            float speed = /*camera->get_scale() **/dt * camera_speed;

            if(Input::get().get_key_held(diverse::InputCode::Key::A))
            {
                velocity -= right * speed;
            }

            if(Input::get().get_key_held(diverse::InputCode::Key::D))
            {
                velocity += right * speed;
            }

            if(Input::get().get_key_held(diverse::InputCode::Key::W))
            {
                velocity += up * speed;
            }

            if(Input::get().get_key_held(diverse::InputCode::Key::S))
            {
                velocity -= up * speed;
            }

            if(glm::length(velocity) > maths::M_EPSILON)
            {
                glm::vec3 position = transform.get_local_position();
                position += velocity * dt;
                velocity = velocity * pow(dampening_factor, dt);

                transform.set_local_position(position);
            }
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
                multiplier = 0.5f;
            }

            float speed = multiplier * dt * camera_speed;

            if(Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight))
            {
                if(Input::get().get_key_held(InputCode::Key::W))
                {
                    velocity -= transform.get_forward_direction() * speed;
                }

                if(Input::get().get_key_held(InputCode::Key::S))
                {
                    velocity += transform.get_forward_direction() * speed;
                }

                if(Input::get().get_key_held(InputCode::Key::A))
                {
                    velocity -= transform.get_right_direction() * speed;
                }

                if(Input::get().get_key_held(InputCode::Key::D))
                {
                    velocity += transform.get_right_direction() * speed;
                }

                if(Input::get().get_key_held(InputCode::Key::Q))
                {
                    velocity -= transform.get_up_direction() * speed;
                }

                if(Input::get().get_key_held(InputCode::Key::E))
                {
                    velocity += transform.get_up_direction() * speed;
                }
            }

            if(velocity.length() > maths::M_EPSILON)
            {
                glm::vec3 position = transform.get_local_position();
                position += velocity * dt;
                transform.set_local_position(position);
                velocity = velocity * pow(dampening_factor, dt);
            }
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
        float dist = distance * 0.4f;
        dist = glm::max(dist, 4.0f);
        float speed    = dist * dist;
        speed          = glm::min(speed, 50.0f); // max speed = 50
        return speed * camera_speed / 1000.0f;
    }
    void EditorCameraController::mouse_pan(maths::Transform& transform, const glm::vec2& delta)
    {
        auto [xSpeed, ySpeed] = pan_speed();
        focal_point -= transform.get_right_direction() * std::clamp(delta.x, -100.0f, 100.0f) * xSpeed * distance;
        focal_point += transform.get_up_direction() * std::clamp(delta.y, -100.0f, 100.0f) * ySpeed * distance;
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
            distance -= delta * get_zoom_speed();
            const glm::vec3 forwardDir = transform.get_forward_direction();
            //if(distance < 1.0f)
            //{
            //    //focal_point += forwardDir * distance;
            //    distance = 1.0f;
            //}
            position_delta += delta * get_zoom_speed() * forwardDir;
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

            float multiplier = camera_speed / 10.0f;
            if(Input::get().get_key_held(InputCode::Key::LeftShift))
            {
                multiplier = camera_speed / 2.0f;
            }

            if(offset != 0.0f)
            {
                zoom_velocity += dt * offset * multiplier;
            }

            if(!maths::Equals(zoom_velocity, 0.0f))
            {
                float scale = camera->get_scale();

                scale -= zoom_velocity;

                if(scale < 0.15f)
                {
                    scale          = 0.15f;
                    zoom_velocity = 0.0f;
                }
                else
                {
                    zoom_velocity = zoom_velocity * pow(zoom_dampening_factor, dt);
                }

                camera->set_scale(scale);
            }
        }
        else
        {

            if(offset != 0.0f)
            {
                zoom_velocity -= dt * offset * 10.0f;
            }

            if(!maths::Equals(zoom_velocity, 0.0f))
            {
                glm::vec3 pos = transform.get_local_position();
                pos += transform.get_forward_direction() * zoom_velocity;
                zoom_velocity = zoom_velocity * pow(zoom_dampening_factor, dt);

                transform.set_local_position(pos);
            }
        }
    }

    void EditorCameraController::stop_movement()
    {
        zoom_velocity   = 0.0f;
        velocity       = glm::vec3(0.0f);
        rotate_velocity = glm::vec2(0.0f);
    }

    bool EditorCameraController::is_moving() const
    {
		return glm::length(velocity) > 0.0f || glm::length(rotate_velocity) > 0.0f || zoom_velocity != 0.0f;
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
    void EditorCameraController::set_front_view(maths::Transform& transform)
    {
        // Front view: looking along negative Z-axis
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.set_local_orientation(rotation);
        
        // Update camera position for arcball mode
        if (camera_mode == EditorCameraMode::ARCBALL)
        {
            transform.set_local_position(calculate_position(transform));
        }
    }

    void EditorCameraController::set_back_view(maths::Transform& transform)
    {
        // Back view: looking along positive Z-axis (rotate 180 degrees around Y-axis)
        glm::quat rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        // Update camera position for arcball mode
        if (camera_mode == EditorCameraMode::ARCBALL)
        {
            transform.set_local_position(calculate_position(transform));
        }
    }

    void EditorCameraController::set_left_view(maths::Transform& transform)
    {
        // Left view: looking along positive X-axis (rotate 90 degrees around Y-axis)
        glm::quat rotation = glm::angleAxis(glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        // Update camera position for arcball mode
        if (camera_mode == EditorCameraMode::ARCBALL)
        {
            transform.set_local_position(calculate_position(transform));
        }
    }

    void EditorCameraController::set_right_view(maths::Transform& transform)
    {
        // Right view: looking along negative X-axis (rotate -90 degrees around Y-axis)
        glm::quat rotation = glm::angleAxis(-glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        // Update camera position for arcball mode
        if (camera_mode == EditorCameraMode::ARCBALL)
        {
            transform.set_local_position(calculate_position(transform));
        }
    }

    void EditorCameraController::set_top_view(maths::Transform& transform)
    {
        // Top view: looking along negative Y-axis (rotate -90 degrees around X-axis)
        glm::quat rotation = glm::angleAxis(-glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        // Update camera position for arcball mode
        if (camera_mode == EditorCameraMode::ARCBALL)
        {
            transform.set_local_position(calculate_position(transform));
        }
    }

    void EditorCameraController::set_buttom_view(maths::Transform& transform)
    {
        // Bottom view: looking along positive Y-axis (rotate 90 degrees around X-axis)
        glm::quat rotation = glm::angleAxis(glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        transform.set_local_orientation(rotation);
        
        // Update camera position for arcball mode
        if (camera_mode == EditorCameraMode::ARCBALL)
        {
            transform.set_local_position(calculate_position(transform));
        }
    }

}
