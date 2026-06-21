#pragma once

#include "camera_controller.h"

namespace diverse
{

    class DS_EXPORT FPSCameraController : public CameraController
    {
    public:
        FPSCameraController();
        virtual ~FPSCameraController() override;

        virtual void handle_mouse(Transform& transform, float dt, float xpos, float ypos) override;
        virtual void handle_keyboard(Transform& transform, float dt) override;
        void update_scroll(Transform& transform, float offset, float dt) override {};
    };

}
