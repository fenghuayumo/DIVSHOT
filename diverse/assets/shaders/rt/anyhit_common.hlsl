#ifndef ANY_HIT_COMMON_HLSL
#define ANY_HIT_COMMON_HLSL

struct RayHitAttrib {
    float2 bary;
};

#if GBUFFERPAYLOAD_ANYHIT
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

// https://media.contentapi.ea.com/content/dam/ea/seed/presentations/2019-ray-tracing-gems-chapter-20-akenine-moller-et-al.pdf
BindlessTextureWithLod compute_texture_lod(uint bindless_texture_idx, float triangle_constant, float3 ray_direction, float3 surf_normal, float cone_width) {
    // Not using `GetDimensions` as it's buggy on AMD.
    float2 wh = bindless_texture_sizes[bindless_texture_idx].xy;

    float lambda = triangle_constant;
    lambda += log2(abs(cone_width));
    lambda += 0.5 * log2(wh.x * wh.y);

    // TODO: This blurs a lot at grazing angles; do aniso.
    lambda -= log2(abs(dot(normalize(ray_direction), surf_normal)));

    BindlessTextureWithLod res;
    res.tex = bindless_textures[NonUniformResourceIndex(bindless_texture_idx)];
    res.lod = lambda;
    return res;
}
#endif

float eval_non_opacity_material(
#if GBUFFERPAYLOAD_ANYHIT
    inout GbufferRayPayload payload, float2 barry, uint mesh_id){
#else
    inout ShadowPayload payload, float2 barry, uint mesh_id) {
#endif
    float3 hit_point = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    const float hit_dist = length(hit_point - WorldRayOrigin());

    float3 barycentrics = float3(1.0 - barry.x - barry.y, barry.x, barry.y);

    // Mesh mesh = meshes[InstanceIndex() / 2];
    Mesh mesh = meshes_buffer[mesh_id];

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
    // if (mesh.vertex_aux_offset != 0) {
    //     float4 vc0 = asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.x * sizeof(float4) + mesh.vertex_aux_offset));
    //     float4 vc1 = asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.y * sizeof(float4) + mesh.vertex_aux_offset));
    //     float4 vc2 = asfloat(bindless_vb[mesh.vertex_buf_id].Load4(ind.z * sizeof(float4) + mesh.vertex_aux_offset));
    //     v_color = vc0 * barycentrics.x + vc1 * barycentrics.y + vc2 * barycentrics.z;
    // }

    float2 uv0 = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.x * sizeof(float2) + mesh.vertex_uv_offset));
    float2 uv1 = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.y * sizeof(float2) + mesh.vertex_uv_offset));
    float2 uv2 = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(ind.z * sizeof(float2) + mesh.vertex_uv_offset));
    float2 uv = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    uint material_id = mesh.material_id;
    MeshMaterial material = materials_buffer[material_id];

    float2 albedo_uv = transform_material_uv(material, uv, 0);
    
#if GBUFFERPAYLOAD_ANYHIT
    const float cone_width = payload.ray_cone.width_at_t(hit_dist);
    const float3 v0_pos_ws = mul(ObjectToWorld3x4(), float4(v0.position, 1.0));
    const float3 v1_pos_ws = mul(ObjectToWorld3x4(), float4(v1.position, 1.0));
    const float3 v2_pos_ws = mul(ObjectToWorld3x4(), float4(v2.position, 1.0));
    const float lod_triangle_constant = 0.5 * log2(twice_uv_area(uv0, uv1, uv2) / twice_triangle_area(v0_pos_ws, v1_pos_ws, v2_pos_ws));
    const BindlessTextureWithLod albedo_tex =
    compute_texture_lod(material.albedo_map, lod_triangle_constant, WorldRayDirection(), surf_normal, cone_width);

    float4 albedo =
        albedo_tex.tex.SampleLevel(sampler_llr, albedo_uv, albedo_tex.lod);
#else
    Texture2D albedo_tex = bindless_textures[NonUniformResourceIndex(material.albedo_map)];
    float4 albedo =
        albedo_tex.SampleLevel(sampler_llr, albedo_uv, 0);
#endif

    return albedo.w;
}

#endif
