#ifndef LIGHTING_SUN_LIGHT_HLSL
#define LIGHTING_SUN_LIGHT_HLSL

#include "../inc/math.hlsl"
#include "../inc/monte_carlo.hlsl"
#include "light_types.hlsl"

/// Sun/directional light sampling
struct SunLight
{
    float3 direction;      ///< Direction to sun (normalized)
    float3 color;          ///< Sun color/intensity
    float angular_radius;  ///< Angular radius of sun (radians)
    float distance;        ///< Distance for ray tracing

    static SunLight make()
    {
        SunLight s;
        s.direction = normalize(float3(0.3, 0.5, 0.2));  // Default direction
        s.color = float3(1.0, 0.98, 0.95);               // Slightly warm sunlight
        s.angular_radius = 0.00465;  // ~0.27 degrees (actual sun)
        s.distance = 1e5;
        return s;
    }

    /// Sample sun light (uniform over solar disk, matching RTXPT DirectionalLight)
    LightSample sample(float2 urand)
    {
        LightSample result;
        result.light_index = 0xFFFFFFFF;  // Special index for sun
        result.distance = distance;
        result.selection_pdf = 1.0;     // Sun is always selected
        result.light_sampleable_by_bsdf = false;
        result.from_local_distribution = false;

        float solid_angle = 2.0 * M_PI * (1.0 - cos(angular_radius));
        result.solid_angle_pdf = 1.0 / max(solid_angle, 1e-8);

        float3x3 basis = build_orthonormal_basis(direction);
        float3 local_dir = uniform_sample_cone(urand, cos(angular_radius));
        result.direction = normalize(mul(basis, local_dir));

        // Radiance is constant over the solar disk.
        result.Li = color;

        return result;
    }

    /// Evaluate sun light (directional)
    float3 evaluate()
    {
        return color;
    }

    /// Check if ray hits sun (simple angular test)
    bool hits_sun(float3 ray_dir)
    {
        float cos_angle = dot(ray_dir, direction);
        return cos_angle > cos(angular_radius);
    }
};

#endif // LIGHTING_SUN_LIGHT_HLSL
