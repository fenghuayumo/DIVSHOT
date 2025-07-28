#include "precompile.h"
#include "fps_camera.h"
#include "engine/application.h"
#include "engine/input.h"
#include "engine/window.h"
#include "camera.h"
#include "maths/maths_utils.h"
#include "maths/transform.h"

namespace diverse
{

    FPSCameraController::FPSCameraController()
    {
    }

    FPSCameraController::~FPSCameraController() = default;

    void FPSCameraController::handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos)
    {
        if(Application::get().get_window()->get_window_focus())
        {
            {
                glm::vec2 windowCentre = glm::vec2();
                xpos -= windowCentre.x;
                ypos -= windowCentre.y;

                Application::get().get_window()->set_mouse_position(windowCentre);

                glm::vec3 euler = transform.get_local_orientation();
                float pitch     = euler.x;
                float yaw       = euler.y;

                pitch -= (ypos) * mouse_sensitivity;
                yaw -= (xpos) * mouse_sensitivity;

                transform.set_local_orientation(glm::quat(glm::vec3(pitch, yaw, euler.z)));
            }

            previous_curser_pos = glm::vec2(xpos, ypos);

            update_scroll(transform, Input::get().get_scroll_offset(), dt);
        }
    }

    void FPSCameraController::handle_keyboard(maths::Transform& transform, float dt)
    {
        camera_speed = 1000.0f * dt;

        if(Input::get().get_key_held(diverse::InputCode::Key::W))
        {
            velocity += transform.get_forward_direction() * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::S))
        {
            velocity -= transform.get_forward_direction() * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::A))
        {
            velocity -= transform.get_right_direction() * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::D))
        {
            velocity += transform.get_right_direction() * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::Space))
        {
            velocity -= transform.get_up_direction() * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::LeftShift))
        {
            velocity += transform.get_up_direction() * camera_speed;
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
