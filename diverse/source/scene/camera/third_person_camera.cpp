#include "precompile.h"
#include "third_person_camera.h"
#include "engine/input.h"
#include "camera.h"
#include "engine/application.h"
#include "engine/window.h"
#include "maths/maths_utils.h"
#include "maths/transform.h"

namespace diverse
{

    ThirdPersonCameraController::ThirdPersonCameraController()
    {
        velocity              = glm::vec3(0.0f);
        mouse_sensitivity      = 0.00001f;
        zoom_dampening_factor   = 0.00001f;
        dampening_factor       = 0.00001f;
        rotate_dampening_factor = 0.0000001f;
    }

    ThirdPersonCameraController::~ThirdPersonCameraController()
    {
    }

    void ThirdPersonCameraController::handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos)
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

        glm::quat rotation  = transform.get_local_orientation();
        glm::quat rotationX = glm::angleAxis(-rotate_velocity.y, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat rotationY = glm::angleAxis(-rotate_velocity.x, glm::vec3(0.0f, 1.0f, 0.0f));

        rotation = rotationY * rotation;
        rotation = rotation * rotationX;
        transform.set_local_orientation(rotation);

        previous_curser_pos = glm::vec2(xpos, ypos);
        rotate_velocity    = rotate_velocity * pow(rotate_dampening_factor, dt);

        update_scroll(transform, Input::get().get_scroll_offset(), dt);
    }

    void ThirdPersonCameraController::handle_keyboard(maths::Transform& transform, float dt)
    {
        // Temp
        return;
        float multiplier = 1000.0f;

        if(Input::get().get_key_held(InputCode::Key::LeftShift))
        {
            multiplier = 10000.0f;
        }

        camera_speed = multiplier * dt;

        if(Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight))
        {
            if(Input::get().get_key_held(InputCode::Key::W))
            {
                velocity -= transform.get_forward_direction() * camera_speed;
            }

            if(Input::get().get_key_held(InputCode::Key::S))
            {
                velocity += transform.get_forward_direction() * camera_speed;
            }

            if(Input::get().get_key_held(InputCode::Key::A))
            {
                velocity -= transform.get_right_direction() * camera_speed;
            }

            if(Input::get().get_key_held(InputCode::Key::D))
            {
                velocity += transform.get_right_direction() * camera_speed;
            }

            if(Input::get().get_key_held(InputCode::Key::Q))
            {
                velocity -= transform.get_up_direction() * camera_speed;
            }

            if(Input::get().get_key_held(InputCode::Key::E))
            {
                velocity += transform.get_up_direction() * camera_speed;
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
