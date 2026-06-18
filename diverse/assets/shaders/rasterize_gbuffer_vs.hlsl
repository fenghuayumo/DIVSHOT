#include "inc/frame_constants.hlsl"
#include "inc/mesh.hlsl"
#include "inc/bindless.hlsl"
#include "inc/mesh_draw.hlsl"

[[vk::binding(0)]] StructuredBuffer<InstanceTransform> instance_transforms_dyn;
[[vk::binding(1)]] StructuredBuffer<MeshDrawGpuData> mesh_draws;

struct VsOut {
	float4 position: SV_Position;
    [[vk::location(0)]] float4 color: TEXCOORD0;
    [[vk::location(1)]] float2 uv: TEXCOORD1;
    [[vk::location(2)]] float3 normal: TEXCOORD2;
    [[vk::location(3)]] float3 tangent: TEXCOORD4;
    [[vk::location(4)]] float3 bitangent: TEXCOORD5;
    [[vk::location(5)]] float3 vs_pos: TEXCOORD6;
    [[vk::location(6)]] float3 prev_vs_pos: TEXCOORD7;
    [[vk::location(7)]] nointerpolation uint draw_index: TEXCOORD8;
    [[vk::location(8)]] nointerpolation uint mesh_index: TEXCOORD9;
};

VsOut main(uint vid: SV_VertexID, uint instance_index: SV_InstanceID) {
    VsOut vsout;
    const MeshDrawGpuData draw = mesh_draws[instance_index];
    const uint mesh_index = draw.mesh_id;
    const uint draw_index = draw.mesh_instance_id;

    const Mesh mesh = meshes_buffer[mesh_index];
    const uint vertex_index = bindless_ib[mesh.index_buf_id].Load(vid * sizeof(uint));

    // TODO: replace with Load<float4> once there's a fast path for NV
    // https://github.com/microsoft/DirectXShaderCompiler/issues/2193
    VertexPacked vp = VertexPacked(asfloat(bindless_vb[mesh.vertex_buf_id].Load4(vertex_index * sizeof(float4) + mesh.vertex_pos_nor_offset)));
    Vertex v = unpack_vertex(vp);

    uint color_packed = mesh.vertex_color_offset != 0
                            ? bindless_vb[mesh.vertex_buf_id].Load(vertex_index * sizeof(uint) + mesh.vertex_color_offset)
                            : 0xFFFFFFFF;
    float4 v_color = unpack_color_8888(color_packed);
    uint tangent_packed = mesh.vertex_tangent_offset != 0
        ? bindless_vb[mesh.vertex_buf_id].Load(vertex_index * sizeof(uint) + mesh.vertex_tangent_offset) : 0xFFFF0000;
    float3 v_tangent_packed = unpack_unit_direction_11_10_11(tangent_packed);


    float2 uv = asfloat(bindless_vb[mesh.vertex_buf_id].Load2(vertex_index * sizeof(float2) + mesh.vertex_uv_offset));

    float3 ws_pos = mul(instance_transforms_dyn[draw_index].current, float4(v.position, 1.0)).xyz;

    float4 vs_pos = mul(float4(ws_pos, 1.0),frame_constants.view_constants.world_to_view);
    float4 cs_pos = mul(vs_pos, frame_constants.view_constants.view_to_sample);

    float3 prev_ws_pos = mul(instance_transforms_dyn[draw_index].previous, float4(v.position, 1.0)).xyz;
    float4 prev_vs_pos = mul(float4(prev_ws_pos, 1.0), frame_constants.view_constants.world_to_view);
    // float4 prev_cs_pos = mul(frame_constants.view_constants.view_to_sample, prev_vs_pos);
    vsout.position = cs_pos;
    vsout.color = v_color;
    vsout.uv = uv;
    vsout.normal = v.normal;
    vsout.tangent = v_tangent_packed.xyz;
    vsout.bitangent = normalize(cross(v.normal, vsout.tangent));

    vsout.vs_pos = vs_pos.xyz / vs_pos.w;
    vsout.prev_vs_pos = prev_vs_pos.xyz / prev_vs_pos.w;
    vsout.draw_index = draw_index;
    vsout.mesh_index = mesh_index;

    return vsout;
}
