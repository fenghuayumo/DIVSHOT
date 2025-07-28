#pragma once
#include "light.h"

namespace diverse
{
    class DirectionalLightComponent : public LightComponent
    {
    public:
        DirectionalLightComponent() = default;
        ~DirectionalLightComponent() = default;

    public:
        auto is_delta_light() const -> bool override { return true; }
        auto get_render_light_data(const maths::Transform& transform,struct LightShaderData* light_data) -> void override;

        float angularSize  = 0.1;
    };
}