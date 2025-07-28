#include "disk_light.h"
#include "renderer/light.h"
namespace diverse
{
    auto DiskLightComponent::get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void
    {
        float surfaceArea = 2.f * glm::pi<float>() * (radius * radius);
        glm::vec3 color = radiance * intensity / surfaceArea;
        auto position = transform.get_world_position();
        auto direction = transform.get_forward_direction();
        light_data->colorTypeAndFlags = (uint32_t)LightType::kDisk << kPolymorphicLightTypeShift;
        packLightColor(color, *light_data);
        light_data->center = position;
        light_data->scalars = fp32ToFp16(radius);
        light_data->direction1 = packNormalizedVector(direction);

    }
}