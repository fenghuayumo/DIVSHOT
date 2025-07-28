#pragma once

#include "camera_controller.h"

namespace diverse
{

    class DS_EXPORT CameraController2D : public CameraController
    {
    public:
        CameraController2D();
        virtual ~CameraController2D() override;

        virtual void handle_mouse(maths::Transform& transform, float dt, float xpos, float ypos) override;
        virtual void handle_keyboard(maths::Transform& transform, float dt) override;

        void update_scroll(maths::Transform& transform, float offset, float dt) override;
    };
}
