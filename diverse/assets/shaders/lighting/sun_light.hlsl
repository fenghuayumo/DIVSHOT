#ifndef LIGHTING_SUN_LIGHT_HLSL
#define LIGHTING_SUN_LIGHT_HLSL

#include "../inc/sun.hlsl"
#include "light_types.hlsl"

/// RTXPT-style directional sun adapter over inc/sun.hlsl.
/// All radiance, disk sampling, and atmosphere live in sun.hlsl; this struct
/// only bridges into the polymorphic LightSample / NEE interface.
struct SunLight
{
    float distance;

    static SunLight from_frame()
    {
        SunLight s;
        s.distance = FLT_MAX;
        return s;
    }

    LightSample sample(float2 urand)
    {
        LightSample result;
        result.light_index = 0xFFFFFFFF;
        result.distance = distance;
        result.selection_pdf = 1.0;
        result.light_sampleable_by_bsdf = false;
        result.from_local_distribution = false;
        result.solid_angle_pdf = sun_disk_solid_angle_pdf();
        result.direction = sample_sun_direction(urand, true);
        result.Li = sun_color_in_direction(result.direction);
        return result;
    }

    float3 evaluate(float3 direction)
    {
        return sun_color_in_direction(direction);
    }

    bool hits_sun(float3 ray_dir)
    {
        return dot(normalize(ray_dir), normalize(SUN_DIRECTION)) > frame_constants.sun_angular_radius_cos;
    }
};

#endif // LIGHTING_SUN_LIGHT_HLSL
