#pragma once
#include "core/core.h"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#define TASK_PRIMITIVE_LIGHT_BIT 0x80000000u
namespace diverse
{
    static const uint kPolymorphicLightTypeShift = 24;
    static const uint kPolymorphicLightTypeMask = 0xf;
    static const uint kPolymorphicLightShapingEnableBit = 1 << 28;
    static const uint kPolymorphicLightIesProfileEnableBit = 1 << 29;
    static const float kPolymorphicLightMinLog2Radiance = -8.f;
    static const float kPolymorphicLightMaxLog2Radiance = 40.f;

    enum LightType
    {
        kSphere = 0,
        kCylinder,
        kDisk,
        kRect,
        kTriangle,
        kDirectional,
        kEnvironment,
        kPoint
    };

    
    struct LightShaderData
    {
         // uint4[0]
        glm::vec3 center;
        uint colorTypeAndFlags; // RGB8 + uint8 (see the kPolymorphicLight... constants above)

        // uint4[1]
        uint direction1; // oct-encoded
        uint direction2; // oct-encoded
        uint scalars; // 2x float16
        uint logRadiance; // uint16

        // uint4[2] -- optional, contains only shaping data
        uint iesProfileIndex;
        uint primaryAxis; // oct-encoded
        uint cosConeAngleAndSoftness; // 2x float16
        uint padding;
    };

    inline LightType getLightType(LightShaderData lightInfo)
    {
        uint typeCode = (lightInfo.colorTypeAndFlags >> kPolymorphicLightTypeShift) 
            & kPolymorphicLightTypeMask;

        return (LightType)typeCode;
    }
}
