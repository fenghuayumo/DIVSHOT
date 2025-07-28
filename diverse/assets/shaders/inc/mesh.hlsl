#ifndef MESH_HLSL
#define MESH_HLSL

#include "pack_unpack.hlsl"

struct VertexPacked {
	float4 data0;
};

struct Mesh
{
    uint vertex_pos_nor_offset;
    uint vertex_uv_offset;
    uint vertex_tangent_offset;
    uint vertex_color_offset;
    uint vertex_buf_id;
    uint index_buf_id;
    uint material_id;
};

struct Vertex {
    float3 position;
    float3 normal;
};

// TODO: nuke
float3 unpack_unit_direction_11_10_11(uint pck) {
    return float3(
        float(pck & ((1u << 11u)-1u)) * (2.0f / float((1u << 11u)-1u)) - 1.0f,
        float((pck >> 11u) & ((1u << 10u)-1u)) * (2.0f / float((1u << 10u)-1u)) - 1.0f,
        float((pck >> 21u)) * (2.0f / float((1u << 11u)-1u)) - 1.0f
    );
}

Vertex unpack_vertex(VertexPacked p) {
    Vertex res;
    res.position = p.data0.xyz;
    res.normal = unpack_unit_direction_11_10_11(asuint(p.data0.w));
    return res;
}

VertexPacked pack_vertex(Vertex v) {
    VertexPacked p;
    p.data0.xyz = v.position;
    p.data0.w = pack_normal_11_10_11(v.normal);
    return p;
}

static const uint MESH_MATERIAL_FLAG_EMISSIVE_USED_AS_LIGHT = 1;
static const uint PBR_WORKFLOW_SEPARATE_TEXTURES = 0;
static const uint PBR_WORKFLOW_METALLIC_ROUGHNESS = 1;
static const uint PBR_WORKFLOW_SPECULAR_GLOSINESS = 2;

struct MeshMaterial {
    float4 base_color_mult;

    uint albedo_map;
    uint metallic_map;
    uint normal_map;
    uint emissive_map;

    float roughness_mult;
    float metalness_factor;
    float emissive_map_factor;
    float ao_map_factor;

    uint  work_flow;
    float metallic_map_factor;
    float roughness_map_factor;
    float normal_map_factor;

    float reflectance;
    float ao_mult;
    float alpha_cutoff;
    uint ao_map;

    float3 emissive;
    uint roughness_map;

    float map_transforms[6 * 6];
};

float2 transform_material_uv(MeshMaterial mat, float2 uv, uint map_idx) {
    uint xo = map_idx * 6;
    float2x2 rot_scl = float2x2(mat.map_transforms[xo+0], mat.map_transforms[xo+1], mat.map_transforms[xo+2], mat.map_transforms[xo+3]);
    float2 offset = float2(mat.map_transforms[xo+4], mat.map_transforms[xo+5]);
    return mul(rot_scl, uv) + offset;
}


#endif