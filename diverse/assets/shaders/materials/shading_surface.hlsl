#ifndef MATERIALS_SHADING_SURFACE_HLSL
#define MATERIALS_SHADING_SURFACE_HLSL

#include "../inc/gbuffer.hlsl"
#include "../inc/bindless.hlsl"
#include "standard_bsdf.hlsl"

/// Shaded surface context shared by path tracing, raster deferred, and ReSTIR passes.
struct ShadingSurface
{
    float3 position;
    float3 normal;
    GbufferData gbuffer;
    StandardBSDF bsdf;
    float3x3 tangent_to_world;
    float3 wo;

    bool has_active_bsdf()
    {
        return bsdf.active_lobes != (uint)LOBE_NONE;
    }

    static ShadingSurface from_gbuffer(
        float3 position_ws,
        GbufferData gbuffer_data,
        float3 wo_ws,
        MaterialData material)
    {
        ShadingSurface surface;
        surface.position = position_ws;
        surface.gbuffer = gbuffer_data;
        surface.normal = gbuffer_data.normal;
        if (dot(surface.normal, wo_ws) <= 0.0)
        {
            surface.normal *= -1.0;
            surface.gbuffer.normal = surface.normal;
        }

        surface.tangent_to_world = build_orthonormal_basis(surface.normal);
        surface.wo = normalize(mul(wo_ws, surface.tangent_to_world));
        surface.bsdf = StandardBSDF::from_surface(material, surface.gbuffer, surface.normal, wo_ws);

        return surface;
    }

    static ShadingSurface from_rt_hit(GbufferPathVertex hit, float3 ray_direction, uint material_id)
    {
        GbufferData gbuffer_data = hit.gbuffer_packed.unpack();
        if (dot(gbuffer_data.normal, ray_direction) > 0.0)
            gbuffer_data.normal *= -1.0;

        return from_gbuffer(
            hit.position,
            gbuffer_data,
            -ray_direction,
            materials_buffer[material_id]);
    }
};

#endif // MATERIALS_SHADING_SURFACE_HLSL
