#pragma once
#include "light.h"

namespace diverse
{
    class PointLightComponent : public LightComponent
    {
    public:
        PointLightComponent() = default;
        ~PointLightComponent() = default;

        auto get_radius() const -> float { return radius; }
        auto set_radius(float r) -> void { radius = r; }
        auto radius_ref() -> float& { return radius; }
        auto is_delta_light() const -> bool override { return true; }
        auto get_render_light_data(const maths::Transform& transform,struct LightShaderData* light_data) -> void;
    public:
        float radius = 1.0f;
    };
}