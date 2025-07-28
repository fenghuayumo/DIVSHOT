#include "inc/math.hlsl"
#include "inc/samplers.hlsl"
#include "inc/frame_constants.hlsl"
#include "inc/mesh.hlsl"
#include "inc/pack_unpack.hlsl"
#include "inc/bindless.hlsl"
#include "inc/gbuffer.hlsl"

struct PsIn {
    [[vk::location(0)]] float4 color: TEXCOORD0;
    [[vk::location(1)]] float2 uv: TEXCOORD1;
    [[vk::location(2)]] float3 normal: TEXCOORD2;
    [[vk::location(3)]] float3 tangent: TEXCOORD4;
    [[vk::location(4)]] float3 bitangent: TEXCOORD5;
    [[vk::location(5)]] float3 vs_pos: TEXCOORD6;
    [[vk::location(6)]] float3 prev_vs_pos: TEXCOORD7;
};
[[vk::push_constant]]
struct PushConstants {
    uint draw_index;
    uint mesh_index;
} push_constants;


struct InstanceTransform {
    row_major float3x4 current;
    row_major float3x4 previous;
};

[[vk::binding(0)]] StructuredBuffer<InstanceTransform> instance_transforms_dyn;

struct PsOut {
    float3 geometric_normal: SV_TARGET0;
    float4 gbuffer: SV_TARGET1;
    float4 velocity: SV_TARGET2;
};

PsOut main(PsIn ps) {
    Mesh mesh = meshes_buffer[push_constants.mesh_index];
    MeshMaterial material = materials_buffer[mesh.material_id];

    const float lod_bias = -0.5;

    float2 albedo_uv = transform_material_uv(material, ps.uv, 0);
    Texture2D albedo_tex = bindless_textures[NonUniformResourceIndex(material.albedo_map)];
    float4 albedo_texel = albedo_tex.SampleBias(sampler_llr, albedo_uv, lod_bias);
    if (albedo_texel.a < material.alpha_cutoff) {
        discard;
    }

    float3 albedo =  material.base_color_mult.xyz * albedo_texel.xyz * ps.color.xyz;
    
    float roughness = 1.0, metalness = 0.0;
    float2 metallic_uv = transform_material_uv(material, ps.uv, 1);
    Texture2D metalness_tex = bindless_textures[NonUniformResourceIndex(material.metallic_map)];
    float2 metalness_roughness = metalness_tex.SampleBias(sampler_llr, metallic_uv, lod_bias).xy;
    if( material.work_flow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        roughness = metalness_roughness.y;
        metalness = metalness_roughness.x;
        float perceptual_roughness = (1.0f - material.roughness_map_factor) * material.roughness_mult + material.roughness_map_factor * roughness;
        roughness = clamp(perceptual_roughness_to_roughness(perceptual_roughness), 1e-4, 1.0);
        metalness = (1.0f - material.metallic_map_factor) * material.metalness_factor + material.metallic_map_factor * metalness;
    } else {
        metalness = metalness_roughness.x;
        float2 roughness_uv = transform_material_uv(material, ps.uv, 5);
        Texture2D roughness_tex = bindless_textures[NonUniformResourceIndex(material.roughness_map)];
        roughness = roughness_tex.SampleBias(sampler_llr, roughness_uv, lod_bias).x;
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

    float3 normal_ws; {
        float3 normal_os = ps.normal;

        [branch]
        if (!frame_constants.render_overrides.has_flag(RenderOverrideFlags::NO_NORMAL_MAPS)) {
            Texture2D normal_tex = bindless_textures[NonUniformResourceIndex(material.normal_map)];

#if 1
            float3 ts_normal = float3(normal_tex.SampleBias(sampler_llr, ps.uv, lod_bias).xy * 2.0 - 1.0, 0);
            ts_normal.z = sqrt(max(0.01, 1.0 - dot(ts_normal.xy, ts_normal.xy)));
#else
            float3 ts_normal = normal_tex.SampleBias(sampler_llr, ps.uv, lod_bias).xyz * 2.0 - 1.0;
#endif

            if (frame_constants.render_overrides.has_flag(RenderOverrideFlags::FLIP_NORMAL_MAP_YZ)) {
                ts_normal.zy *= -1;
            }

            if (dot(ps.bitangent, ps.bitangent) > 0.0) {
                float3x3 tbn = float3x3(ps.tangent, ps.bitangent, ps.normal);
                normal_os = mul(ts_normal, tbn);
            }
        }

        // Transform to world space
        normal_ws = normalize(mul(instance_transforms_dyn[push_constants.draw_index].current,float4(normal_os, 0.0)).xyz);
    }

    // Derive normal from depth
    float3 geometric_normal_vs; {
        float3 d1 = ddx(ps.vs_pos);
        float3 d2 = ddy(ps.vs_pos);
        geometric_normal_vs = normalize(cross(d2, d1));
    }
    float3 geometric_normal_ws = direction_view_to_world(geometric_normal_vs);

    // Fix invalid normals
    if (dot(normal_ws, geometric_normal_ws) < 0.0) {
        normal_ws *= -1;
    }

    if (frame_constants.render_overrides.has_flag(RenderOverrideFlags::FORCE_FACE_NORMALS)) {
        normal_ws = geometric_normal_ws;
    }

    float2 emissive_uv = transform_material_uv(material, ps.uv, 3);
    Texture2D emissive_tex = bindless_textures[NonUniformResourceIndex(material.emissive_map)];
    float3 emissive = ((1.0f - material.emissive_map_factor) * material.emissive +
                       material.emissive_map_factor * emissive_tex.SampleBias(sampler_llr, emissive_uv, lod_bias).rgb)
                       * frame_constants.pre_exposure;

    GbufferData gbuffer = GbufferData::create_zero();
    gbuffer.albedo = albedo;
    gbuffer.normal = normal_ws;
    gbuffer.roughness = roughness;
    //gbuffer.roughness = lerp(0.05, 0.15, roughness);  // kitchen hack
    gbuffer.metalness = metalness;
    gbuffer.emissive = emissive;

    PsOut ps_out;
    ps_out.geometric_normal =  geometric_normal_vs * 0.5 + 0.5;
    ps_out.gbuffer = asfloat(gbuffer.pack().data0);
    ps_out.velocity = float4(ps.prev_vs_pos - ps.vs_pos, 0);

    return ps_out;
}
