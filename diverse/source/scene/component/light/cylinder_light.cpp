#include "cylinder_light.h"
#include "renderer/light.h"
namespace diverse
{
    auto  CylinderLightComponent::get_render_light_data(const maths::Transform& transform,struct LightShaderData* light_data) -> void
    {
        float surfaceArea = 2.f * glm::pi<float>() * (radius * length);
        glm::vec3 color = radiance * intensity / surfaceArea;
        auto position = transform.get_world_position();
        auto direction = transform.get_forward_direction();
        light_data->colorTypeAndFlags = (uint32_t)LightType::kCylinder << kPolymorphicLightTypeShift;
        packLightColor(color, *light_data);
        light_data->center = position;
        light_data->scalars = fp32ToFp16(radius) | (fp32ToFp16(length) << 16);
        light_data->direction1 = packNormalizedVector(direction);
    }
}