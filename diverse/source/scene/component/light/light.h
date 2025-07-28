#pragma once
#include "core/core.h"
#include "maths/transform.h"
namespace diverse
{

    class LightComponent
    {
    public:
        LightComponent() = default;
        virtual ~LightComponent() = default;
        void set_intensity(float intensity) { this->intensity = intensity; }
        float get_intensity() const { return intensity; }
        float& get_intensity_ref() { return intensity; }
        void set_radiance(const glm::vec3& radiance) { this->radiance = radiance; }
        glm::vec3 get_radiance() const { return radiance; }
        glm::vec3& get_radiance_ref()  { return radiance; }
        virtual auto  is_delta_light() const ->bool{ return false; }
        virtual auto  get_render_light_data(const maths::Transform& transform,struct LightShaderData* light_data) -> void = 0;
    protected:
        float intensity = 1.0f;
        glm::vec3 radiance = glm::vec3(1.0f, 1.0f, 1.0f);
    };
    uint floatToUInt(float _V, float _Scale);
    uint FLOAT3_to_R8G8B8_UNORM(float unpackedInputX, float unpackedInputY, float unpackedInputZ);
    void packLightColor(const glm::vec3& color, LightShaderData& lightInfo);
    glm::vec2 unitVectorToOctahedron(const glm::vec3& N);
    uint32_t packNormalizedVector(const glm::vec3& x);
    uint16_t fp32ToFp16(float v);

    glm::vec3 unpackLightColor(LightShaderData lightInfo);
}