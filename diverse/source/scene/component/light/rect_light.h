#pragma once
#include "light.h"

namespace diverse
{
    class RectLightComponent : public LightComponent
    {
    public:
        RectLightComponent() = default;
        ~RectLightComponent() = default;

        auto get_width() const -> float { return width; }
        auto set_width(float w) -> void { width = w; }
        auto get_height() const -> float { return height; }
        auto set_height(float h) -> void { height = h; }
        auto get_radius() const -> float { return radius; }
        auto set_radius(float r) -> void { radius = r; }

        auto width_ref() -> float& { return width; }
        auto height_ref() -> float& { return height; }
        auto radius_ref() -> float& { return radius; }
    public:
        auto get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void override;
    protected:
        float width = 1.0f;
        float height = 1.0f;
        float radius = 1.0f;
    };
}