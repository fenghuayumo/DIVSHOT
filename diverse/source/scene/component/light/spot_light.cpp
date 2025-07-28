#include "spot_light.h"
#include "renderer/light.h"

namespace diverse
{
    auto SpotLightComponent::get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void
    {
        float projectedArea = glm::pi<float>() * radius * radius;
        glm::vec3 color = radiance * intensity / projectedArea;
        float softness = glm::clamp(1.f - inner_angle / outer_angle,0.0f,1.0f);
        auto position = transform.get_world_position();
        auto direction = transform.get_forward_direction();
        light_data->colorTypeAndFlags = (uint32_t)LightType::kSphere << kPolymorphicLightTypeShift;
        light_data->colorTypeAndFlags |= kPolymorphicLightShapingEnableBit;
        packLightColor(color, *light_data);
        light_data->center = position;
        light_data->scalars = fp32ToFp16(radius);
        light_data->primaryAxis = packNormalizedVector(glm::normalize(direction));
        light_data->cosConeAngleAndSoftness = fp32ToFp16(cosf(glm::radians(outer_angle)));
        light_data->cosConeAngleAndSoftness |= fp32ToFp16(softness) << 16;

        if (profileTextureIndex >= 0)
        {
            light_data->iesProfileIndex = profileTextureIndex;
            light_data->colorTypeAndFlags |= kPolymorphicLightIesProfileEnableBit;
        }

    }
}