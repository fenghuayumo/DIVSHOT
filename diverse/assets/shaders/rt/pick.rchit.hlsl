#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/mesh.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/rt.hlsl"

[[vk::binding(0, 3)]] RaytracingAccelerationStructure acceleration_structure;

struct RayHitAttrib {
    float2 bary;
};


[shader("closesthit")]
void main(inout GbufferRayPayload payload: SV_RayPayload, in RayHitAttrib attrib: SV_IntersectionAttributes) {
    float3 hit_point = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    const float hit_dist = length(hit_point - WorldRayOrigin());

    float3 barycentrics = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    //Mesh mesh = meshes[InstanceIndex() / 2];
    Mesh mesh = meshes[InstanceID()];

    // Indices of the triangle
    uint3 ind = uint3(
        vertices.Load((PrimitiveIndex() * 3 + 0) * sizeof(uint) + mesh.index_offset),
        vertices.Load((PrimitiveIndex() * 3 + 1) * sizeof(uint) + mesh.index_offset),
        vertices.Load((PrimitiveIndex() * 3 + 2) * sizeof(uint) + mesh.index_offset)
    );
    uint material_id = vertices.Load(ind.x * sizeof(uint) + mesh.vertex_mat_offset);

    GbufferData gbuffer = GbufferData::create_zero();
    gbuffer.albedo = float3(InstanceID(), material_id, 0);
    gbuffer.normal = 0;
    gbuffer.roughness = 0;
    gbuffer.metalness = 0;
    gbuffer.emissive = 0;

    payload.gbuffer_packed = gbuffer.pack();
    payload.t = RayTCurrent();
}
