#ifndef SUN_HLSL
#define SUN_HLSL

#include "frame_constants.hlsl"
#include "math.hlsl"
#include "monte_carlo.hlsl"
#include "atmosphere_felix.hlsl"

#define SUN_DIRECTION (frame_constants.sun_direction.xyz)

#if 0
    float3 sun_color_in_direction(float3 dir) {
        return frame_constants.pre_exposure * 10.0;
    }
#else
    float3 sun_color_in_direction(float3 dir) {
        return
            20.0 *
            frame_constants.sun_color_multiplier.rgb *
            frame_constants.pre_exposure *
            Absorb(IntegrateOpticalDepth(0.0.xxx, dir));
    }

#endif

#define SUN_COLOR (sun_color_in_direction(SUN_DIRECTION))

float sun_disk_solid_angle()
{
    return 2.0 * M_PI * (1.0 - saturate(frame_constants.sun_angular_radius_cos));
}

float sun_disk_solid_angle_pdf()
{
    return 1.0 / max(sun_disk_solid_angle(), 1e-8);
}

float3 sample_sun_direction(float2 urand, bool soft) {
    if (soft) {
        if (frame_constants.sun_angular_radius_cos < 1.0) {
            const float3x3 basis = build_orthonormal_basis(normalize(SUN_DIRECTION));
            return mul(basis, uniform_sample_cone(urand, frame_constants.sun_angular_radius_cos));
        }
    }

    return SUN_DIRECTION;
}

#endif