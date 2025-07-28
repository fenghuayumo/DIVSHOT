#include "point_light.h"
#include "renderer/light.h"

namespace diverse
{
    auto PointLightComponent::get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void
    {
        auto position = transform.get_world_position();
        if (radius == 0.f)
        {
            glm::vec3 flux = radiance * intensity;

            light_data->colorTypeAndFlags = (uint32_t)LightType::kPoint << kPolymorphicLightTypeShift;
            packLightColor(flux, *light_data);
            light_data->center = position;
        }
        else
        {
            float projectedArea = glm::pi<float>() * radius * radius;
            glm::vec3 color = radiance * intensity / projectedArea;

            light_data->colorTypeAndFlags = (uint32_t)LightType::kSphere << kPolymorphicLightTypeShift;
            packLightColor(color, *light_data);
            light_data->center = position;
            light_data->scalars = fp32ToFp16(radius);
        }
    }
}