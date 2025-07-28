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

float twice_triangle_area(float3 p0, float3 p1, float3 p2) {
    return length(cross(p1 - p0, p2 - p0));
}

float twice_uv_area(float2 t0, float2 t1, float2 t2) {
    return abs((t1.x - t0.x) * (t2.y - t0.y) - (t2.x - t0.x) * (t1.y - t0.y));
}

struct BindlessTextureWithLod {
    Texture2D tex;
    float lod;
};


[shader("closesthit")]
void main(inout CustomRayPayload payload: SV_RayPayload, in RayHitAttrib attrib: SV_IntersectionAttributes) {
    float3 hit_point = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    const float hit_dist = length(hit_point - WorldRayOrigin());

    float3 barycentrics = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    uint geometry_offset = instance_dynamic_parameters_dyn[InstanceIndex()].geometry_offset;
    Mesh mesh = meshes_buffer[geometry_offset + GeometryIndex()];

    // Indices of the triangle
    uint primitive_index = PrimitiveIndex() * 3;
    uint3 ind = uint3(
        bindless_ib[mesh.index_buf_id].Load((primitive_index + 0) * sizeof(uint)),
        bindless_ib[mesh.index_buf_id].Load((primitive_index + 1) * sizeof(uint)),
        bindless_ib[mesh.index_buf_id].Load((primitive_index + 2) * sizeof(uint))
    );

    Vertex v0 = unpack_vertex(VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.x * sizeof(float4) + mesh.vertex_pos_nor_offset))));
    Vertex v1 = unpack_vertex(VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.y * sizeof(float4) + mesh.vertex_pos_nor_offset))));
    Vertex v2 = unpack_vertex(VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.z * sizeof(float4) + mesh.vertex_pos_nor_offset))));
    float3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;

    const float3 surf_normal = normalize(cross(v1.position - v0.position, v2.position - v0.position));

    if (frame_constants.render_overrides.has_flag(RenderOverrideFlags::FORCE_FACE_NORMALS)) {
        normal = surf_normal;
    }

    float4 v_color = 1.0.xxxx;
    if (mesh.vertex_color_offset != 0) {
        float4 vc0 = unpack_color_8888(bindless_vb[mesh.vertex_buf_id].Load(ind.x * sizeof(uint) + mesh.vertex_color_offset));
        float4 vc1 = unpack_color_8888(bindless_vb[mesh.vertex_buf_id].Load(ind.y * sizeof(uint) + mesh.vertex_color_offset));
        float4 vc2 = unpack_color_8888(bindless_vb[mesh.vertex_buf_id].Load(ind.z * sizeof(uint) + mesh.vertex_color_offset));
        v_color = vc0 * barycentrics.x + vc1 * barycentrics.y + vc2 * barycentrics.z;
    }

    float2 uv0 = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.x * sizeof(float2) + mesh.vertex_uv_offset));
    float2 uv1 = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.y * sizeof(float2) + mesh.vertex_uv_offset));
    float2 uv2 = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.z * sizeof(float2) + mesh.vertex_uv_offset));
    float2 uv = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    const float cone_width = payload.ray_cone.width_at_t(hit_dist);
    const float3 v0_pos_ws = mul(ObjectToWorld3x4(), float4(v0.position, 1.0));
    const float3 v1_pos_ws = mul(ObjectToWorld3x4(), float4(v1.position, 1.0));
    const float3 v2_pos_ws = mul(ObjectToWorld3x4(), float4(v2.position, 1.0));
    // const float lod_triangle_constant = 0.5 * log2(twice_uv_area(uv0, uv1, uv2) / twice_triangle_area(v0_pos_ws, v1_pos_ws, v2_pos_ws));

    uint material_id = mesh.material_id;
    MeshMaterial material = materials_buffer[material_id];

    float2 albedo_uv = transform_material_uv(material, uv, 0);

    float4 tex_size = bindless_texture_sizes[NonUniformResourceIndex(material.albedo_map)];
    Texture2D tex = bindless_textures[NonUniformResourceIndex(material.albedo_map)];
    float2 tex_pos = float2(albedo_uv.x * tex_size.x, albedo_uv.y * tex_size.y);
    payload.texture_pos = tex_pos;
    payload.t = RayTCurrent();
}
