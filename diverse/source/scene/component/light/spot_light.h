#pragma once
#include "light.h"

namespace diverse
{
    class SpotLightComponent : public LightComponent
    {
    public:
        SpotLightComponent() = default;
        ~SpotLightComponent() = default;
        auto get_inner_angle() const -> float { return inner_angle; }
        auto set_inner_angle(float angle) -> void { inner_angle = angle; }
        auto get_outer_angle() const -> float { return outer_angle; }
        auto set_outer_angle(float angle) -> void { outer_angle = angle; }
        auto get_radius() const -> float { return radius; }
        auto set_radius(float r) -> void { radius = r; }
        auto is_delta_light() const -> bool override { return true; }

        auto radius_ref() -> float& { return radius; }
        auto inner_angle_ref() -> float& { return inner_angle; }
        auto outer_angle_ref() -> float& { return outer_angle; }
    public:
        auto get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void override;
    private:
        float inner_angle = 180.0f;
        float outer_angle = 180.0f;
        float radius = 0.01f;
        int profileTextureIndex = -1;
    };
}