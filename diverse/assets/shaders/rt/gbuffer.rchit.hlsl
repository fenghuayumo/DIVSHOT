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

[shader("closesthit")]
void main(inout GbufferRayPayload payload: SV_RayPayload, in RayHitAttrib attrib: SV_IntersectionAttributes) {
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
    const float lod_triangle_constant = 0.5 * log2(twice_uv_area(uv0, uv1, uv2) / twice_triangle_area(v0_pos_ws, v1_pos_ws, v2_pos_ws));

    uint material_id = mesh.material_id;
    MeshMaterial material = materials_buffer[material_id];

    float2 albedo_uv = transform_material_uv(material, uv, 0);
    const BindlessTextureWithLod albedo_tex =
        compute_texture_lod(material.albedo_map, lod_triangle_constant, WorldRayDirection(), surf_normal, cone_width);

    float3 albedo = albedo_tex.tex.SampleLevel(sampler_llr, albedo_uv, albedo_tex.lod).xyz
        * float4(material.base_color_mult).xyz
        * v_color.rgb;

    float2 metallic_uv = transform_material_uv(material, uv, 1);
    const BindlessTextureWithLod metalness_tex = compute_texture_lod(material.metallic_map, lod_triangle_constant, WorldRayDirection(), surf_normal, cone_width);
    float roughness = 1.0, metalness = 0.0;
    if (material.work_flow == PBR_WORKFLOW_METALLIC_ROUGHNESS) {
        float2 metalness_roughness = metalness_tex.tex.SampleLevel(sampler_llr, metallic_uv, metalness_tex.lod).xy;
        roughness = metalness_roughness.y;
        metalness = metalness_roughness.x;
        float perceptual_roughness = (1.0f - material.roughness_map_factor) * material.roughness_mult + material.roughness_map_factor * roughness;
        roughness = clamp(perceptual_roughness_to_roughness(perceptual_roughness), 1e-4, 1.0);
        metalness = (1.0f - material.metallic_map_factor) * material.metalness_factor + material.metallic_map_factor * metalness;
    } else {
        float metalness = metalness_tex.tex.SampleLevel(sampler_llr, metallic_uv, metalness_tex.lod).x;
        float2 roughness_uv = transform_material_uv(material, uv, 5);
        const BindlessTextureWithLod roughness_tex = compute_texture_lod(material.roughness_map, lod_triangle_constant, WorldRayDirection(), surf_normal, cone_width);
        float roughness = roughness_tex.tex.SampleLevel(sampler_llr, roughness_uv, roughness_tex.lod).x;
        float perceptual_roughness = (1.0f - material.roughness_map_factor) * material.roughness_mult + material.roughness_map_factor * roughness;
        roughness = clamp(perceptual_roughness_to_roughness(perceptual_roughness), 1e-4, 1.0);
        metalness = (1.0f - material.metallic_map_factor) * material.metalness_factor + material.metallic_map_factor * metalness;
    }
   
    if (frame_constants.render_overrides.has_flag(RenderOverrideFlags::NO_METAL)) {
        metalness = 0;
    }

    if (frame_constants.render_overrides.material_roughness_scale <= 1) {
        roughness *= frame_constants.render_overrides.material_roughness_scale;
    } else {
        roughness = square(lerp(sqrt(roughness), 1.0, 1.0 - 1.0 / frame_constants.render_overrides.material_roughness_scale));
    }

    float2 emissive_uv = transform_material_uv(material, uv, 3);
    const BindlessTextureWithLod emissive_tex =
        compute_texture_lod(material.emissive_map, lod_triangle_constant, WorldRayDirection(), surf_normal, cone_width);

    float3 emissive = 0;

    // Only allow emissive if this is not a light
    // ... except then still allow it if the path is currently tracing from the eye,
    // since we need the direct contribution of the light's surface to the screen.
    if (0 == payload.path_length ) {
        emissive = 1.0.xxx
            * emissive_tex.tex.SampleLevel(sampler_llr, emissive_uv, emissive_tex.lod).rgb
            * float3(material.emissive)
            * instance_dynamic_parameters_dyn[InstanceIndex()].emissive_multiplier
            * frame_constants.pre_exposure;
    }

    GbufferData gbuffer = GbufferData::create_zero();
    gbuffer.albedo = albedo;
    gbuffer.normal = normalize(mul(ObjectToWorld3x4(), float4(normal, 0.0)));
    gbuffer.roughness = roughness;
    gbuffer.metalness = metalness;
    gbuffer.emissive = emissive;

    // Force double-sided
    if (dot(WorldRayDirection(), gbuffer.normal) > 0) {
        gbuffer.normal *= -1;
    }

    //gbuffer.albedo = float3(0.966653, 0.802156, 0.323968); // Au from Mitsuba

    payload.gbuffer_packed = gbuffer.pack();
    payload.t = RayTCurrent();
}
