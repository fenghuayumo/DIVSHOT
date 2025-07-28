#include "precompile.h"
#include "camera2D.h"
#include "engine/input.h"
#include "maths/maths_utils.h"
#include "maths/transform.h"
#include "camera.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace diverse
{
    CameraController2D::CameraController2D()
    {
        velocity         = glm::vec3(0.0f);
        mouse_sensitivity = 0.005f;
    }

    CameraController2D::~CameraController2D() = default;

    void CameraController2D::handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos)
    {
        if(Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight))
        {
            glm::vec3 position = transform.get_local_position();
            position.x -= (xpos - previous_curser_pos.x) * mouse_sensitivity * 0.5f;
            position.y += (ypos - previous_curser_pos.y) * mouse_sensitivity * 0.5f;
            transform.set_local_position(position);
        }

        previous_curser_pos = glm::vec2(xpos, ypos);
    }

    void CameraController2D::handle_keyboard(maths::Transform& transform, float dt)
    {
        glm::vec3 up = glm::vec3(0, 1, 0), right = glm::vec3(1, 0, 0);

        camera_speed = dt * 20.0f; // camera->get_scale() *

        if(Input::get().get_key_held(diverse::InputCode::Key::A))
        {
            velocity -= right * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::D))
        {
            velocity += right * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::W))
        {
            velocity += up * camera_speed;
        }

        if(Input::get().get_key_held(diverse::InputCode::Key::S))
        {
            velocity -= up * camera_speed;
        }

        if(velocity.length() > maths::M_EPSILON)
        {
            glm::vec3 position = transform.get_local_position();
            position += velocity * dt;
            velocity = velocity * pow(dampening_factor, dt);

            transform.set_local_position(position);
        }

        update_scroll(transform, Input::get().get_scroll_offset(), dt);
    }

    void CameraController2D::update_scroll(maths::Transform& transform, float offset, float dt)
    {
        float multiplier = 2.0f;
        if(Input::get().get_key_held(InputCode::Key::LeftShift))
        {
            multiplier = 10.0f;
        }

        if(offset != 0.0f)
        {
            zoom_velocity += dt * offset * multiplier;
        }

        if(!maths::Equals(zoom_velocity, 0.0f))
        {
            float scale = 1.0f; // camera->get_scale();

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

            // camera->set_scale(scale); TODO
        }
    }
}
