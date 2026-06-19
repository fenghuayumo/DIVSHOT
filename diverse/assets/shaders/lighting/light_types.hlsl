#ifndef LIGHTING_LIGHT_TYPES_HLSL
#define LIGHTING_LIGHT_TYPES_HLSL

#include "../inc/math.hlsl"

/// Light sample result structure
struct LightSample
{
    float3 Li;              ///< Incident radiance at shading point (unshadowed)
    float3 direction;       ///< Ray direction (normalized)
    float distance;         ///< Ray distance for visibility evaluation
    uint light_index;       ///< Identifier of the source light
    float selection_pdf;    ///< Pdf of light selection
    float solid_angle_pdf;  ///< Pdf with respect to solid angle
    bool light_sampleable_by_bsdf;  ///< Can BSDF sample this light
    bool from_local_distribution;  ///< Was drawn from local distribution

    static LightSample make()
    {
        LightSample ret;
        ret.Li = float3(0, 0, 0);
        ret.direction = float3(0, 0, 0);
        ret.distance = 0;
        ret.light_index = 0xFFFFFFFF;
        ret.selection_pdf = 0;
        ret.solid_angle_pdf = 0;
        ret.light_sampleable_by_bsdf = false;
        ret.from_local_distribution = false;
        return ret;
    }

    bool Valid()
    {
        return any(Li > 0);
    }
};

/// Polymorphic light types
enum LightType : uint
{
    POINT = 0,
    SPOT = 1,
    DIRECTIONAL = 2,
    TRIANGLE = 3,
    ENVIRONMENT = 4,
};

/// Light data structure
struct LightData
{
    float3 position;       ///< For point/spot lights
    float3 direction;      ///< For spot/directional lights
    float3 color;          ///< Light color/intensity
    float range;           ///< For point/spot lights
    float spot_angle;      ///< For spot lights (cos of angle)
    LightType type;        ///< Light type
    uint padding;          ///< Alignment padding

    static LightData make()
    {
        LightData ret;
        ret.position = float3(0, 0, 0);
        ret.direction = float3(0, -1, 0);
        ret.color = float3(1, 1, 1);
        ret.range = 1000.0;
        ret.spot_angle = 0.5;
        ret.type = POINT;
        ret.padding = 0;
        return ret;
    }
};

#endif // LIGHTING_LIGHT_TYPES_HLSL
