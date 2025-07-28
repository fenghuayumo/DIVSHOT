#include "directional_light.h"
#include "renderer/light.h"
#include "utility/pack_utils.h"
namespace diverse
{
    uint floatToUInt(float _V, float _Scale)
    {
        return (uint)floor(_V * _Scale + 0.5f);
    }

    float saturate(float x)
    {
        return glm::clamp(x,0.0f,1.0f);
    }

    uint FLOAT3_to_R8G8B8_UNORM(float unpackedInputX, float unpackedInputY, float unpackedInputZ)
    {
        return (floatToUInt(saturate(unpackedInputX), 0xFF) & 0xFF) |
            ((floatToUInt(saturate(unpackedInputY), 0xFF) & 0xFF) << 8) |
            ((floatToUInt(saturate(unpackedInputZ), 0xFF) & 0xFF) << 16);
    }

    void packLightColor(const glm::vec3& color, LightShaderData& lightInfo)
    {
        float maxRadiance = std::max(color.x, std::max(color.y, color.z));

        if (maxRadiance <= 0.f)
            return;

        float logRadiance = (::log2f(maxRadiance) - kPolymorphicLightMinLog2Radiance) / (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance);
        logRadiance = saturate(logRadiance);
        uint32_t packedRadiance = std::min(uint32_t(ceilf(logRadiance * 65534.f)) + 1, 0xffffu);
        float unpackedRadiance = ::exp2f((float(packedRadiance - 1) / 65534.f) * (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance) + kPolymorphicLightMinLog2Radiance);

        lightInfo.colorTypeAndFlags |= FLOAT3_to_R8G8B8_UNORM(color.x / unpackedRadiance, color.y / unpackedRadiance, color.z / unpackedRadiance);
        lightInfo.logRadiance |= packedRadiance;
    }

    float unpackLightRadiance(uint logRadiance)
    {
        return (logRadiance == 0) ? 0 : exp2((float(logRadiance - 1) / 65534.0) * (kPolymorphicLightMaxLog2Radiance - kPolymorphicLightMinLog2Radiance) + kPolymorphicLightMinLog2Radiance);
    }

    glm::vec3 unpackLightColor(LightShaderData lightInfo)
    {
        glm::vec3 color = unpack_color_8888(lightInfo.colorTypeAndFlags);
        float radiance = unpackLightRadiance(lightInfo.logRadiance & 0xffff);
        return color * radiance;
    }


    glm::vec2 unitVectorToOctahedron(const glm::vec3& N)
    {
        float m = abs(N.x) + abs(N.y) + abs(N.z);
        glm::vec2 XY = { N.x, N.y };
        XY.x /= m;
        XY.y /= m;
        if (N.z <= 0.0f)
        {
            glm::vec2 signs;
            signs.x = XY.x >= 0.0f ? 1.0f : -1.0f;
            signs.y = XY.y >= 0.0f ? 1.0f : -1.0f;
            float x = (1.0f - abs(XY.y)) * signs.x;
            float y = (1.0f - abs(XY.x)) * signs.y;
            XY.x = x;
            XY.y = y;
        }
        return { XY.x, XY.y };
    }

    uint32_t packNormalizedVector(const glm::vec3& x)
    {
        glm::vec2 XY = unitVectorToOctahedron(x);
        XY.x = XY.x * .5f + .5f;
        XY.y = XY.y * .5f + .5f;
        uint X = floatToUInt(glm::clamp(XY.x,0.0f,1.0f), (1 << 16) - 1);
        uint Y = floatToUInt(glm::clamp(XY.y,0.0f,1.0f), (1 << 16) - 1);
        uint packedOutput = X;
        packedOutput |= Y << 16;
        return packedOutput;
    }

    // Modified from original, based on the method from the DX fallback layer sample
    uint16_t fp32ToFp16(float v)
    {
        // Multiplying by 2^-112 causes exponents below -14 to denormalize
        static const union FU {
            uint ui;
            float f;
        } multiple = { 0x07800000 }; // 2**-112

        FU BiasedFloat;
        BiasedFloat.f = v * multiple.f;
        const uint u = BiasedFloat.ui;

        const uint sign = u & 0x80000000;
        uint body = u & 0x0fffffff;

        return (uint16_t)(sign >> 16 | body >> 13) & 0xFFFF;
    }
    auto DirectionalLightComponent::get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void
    {
        auto direction = transform.get_forward_direction();
        float halfAngularSizeRad = 0.5f * glm::radians(angularSize);
        float solidAngle = float(2 * glm::pi<float>() * (1.0 - cos(halfAngularSizeRad)));
        glm::vec3 color =  radiance / solidAngle;

        light_data->colorTypeAndFlags = (uint32_t)LightType::kDirectional << kPolymorphicLightTypeShift;
        packLightColor(color, *light_data);
        light_data->direction1 = packNormalizedVector(glm::vec3(glm::normalize(direction)));
        // Can't pass cosines of small angles reliably with fp16
        light_data->scalars = fp32ToFp16(halfAngularSizeRad) | (fp32ToFp16(solidAngle) << 16);
    }
}