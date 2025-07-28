#pragma once
#include "camera_controller.h"

namespace diverse
{

    class DS_EXPORT ThirdPersonCameraController : public CameraController
    {
    public:
        ThirdPersonCameraController();
        virtual ~ThirdPersonCameraController() override;

        virtual void handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos) override;
        virtual void handle_keyboard(maths::Transform& transform, float dt) override;

    private:
        bool m_Free;
        glm::vec2 stored_cursor_pos;
    };
}
