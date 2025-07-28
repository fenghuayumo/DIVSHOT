#pragma once
#include "light.h"

namespace diverse
{
    class CylinderLightComponent : public LightComponent
    {
    public:
        CylinderLightComponent() = default;
        ~CylinderLightComponent() = default;
    
        auto get_length() const -> float { return length; }
        auto set_length(float len) -> void { length = len; }
        auto get_radius() const -> float { return radius; }
        auto set_radius(float r) -> void { radius = r; }
        float& radius_ref() {return radius;}
        float& length_ref() { return length; }
    public:
        auto  get_render_light_data(const maths::Transform& transform,struct LightShaderData* light_data) -> void;
    
    protected:
        float length = 1.f;
        float radius = 1.f;
    };
}
