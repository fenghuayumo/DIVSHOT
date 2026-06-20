#ifndef MATERIALS_BSDF_SCATTER_HLSL
#define MATERIALS_BSDF_SCATTER_HLSL

#include "bsdf_types.hlsl"

static const float BSDF_SCATTER_MIN_COS_THETA = 1e-6;

/// Cosine term for path throughput after BSDF sampling.
float bsdf_scatter_cosine(LobeType lobe, float3 wi_tangent)
{
    if (lobe_has((uint)lobe, LOBE_TRANSMISSION))
        return max(BSDF_SCATTER_MIN_COS_THETA, abs(wi_tangent.z));
    return max(0.0, wi_tangent.z);
}

/// Surface offset direction when spawning the next path segment.
float3 bsdf_scatter_offset_normal(float3 shading_normal, LobeType lobe)
{
    return lobe_has((uint)lobe, LOBE_TRANSMISSION) ? -shading_normal : shading_normal;
}

/// Whether a scatter event should increment the diffuse-bounce counter (env mip, etc.).
bool bsdf_scatter_is_diffuse_bounce(LobeType lobe, float roughness, float specular_roughness_diffuse_threshold)
{
    return lobe_has((uint)lobe, LOBE_DIFFUSE)
        || (lobe_has((uint)lobe, LOBE_SPECULAR_REFLECTION) && roughness > specular_roughness_diffuse_threshold);
}

#endif // MATERIALS_BSDF_SCATTER_HLSL
