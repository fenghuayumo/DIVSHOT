#pragma once

#include "light.h"

namespace diverse
{
    class DiskLightComponent : public LightComponent
    {
    public:
        DiskLightComponent() = default;
        ~DiskLightComponent() = default;

        auto radius_ref() -> float& { return radius; }
        
    public:
        auto get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void override;

    private:
        float radius = 1.0f;
    };
}