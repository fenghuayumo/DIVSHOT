#include "rect_light.h"
#include "maths/rect.h"
#include "renderer/light.h"

namespace diverse
{
    auto RectLightComponent::get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void
    {
        float surfaceArea = width * height;
        glm::vec3 color = radiance * intensity / surfaceArea;
        auto position = transform.get_world_position();
        auto normal = transform.get_forward_direction();
        auto right = transform.get_right_direction();
        auto up = transform.get_up_direction();
        light_data->colorTypeAndFlags = (uint32_t)LightType::kRect << kPolymorphicLightTypeShift;
        packLightColor(color, *light_data);
        light_data->center = position;
        light_data->scalars = fp32ToFp16(width) | (fp32ToFp16(height) << 16);
        light_data->direction1 = packNormalizedVector(glm::normalize(right));
        light_data->direction2 = packNormalizedVector(glm::normalize(up));
    }
}