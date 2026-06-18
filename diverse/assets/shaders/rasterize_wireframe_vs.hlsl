#include "inc/binding.hlsl"
#include "inc/frame_constants.hlsl"
#include "inc/mesh.hlsl"
#include "inc/bindless.hlsl"
#include "inc/mesh_draw.hlsl"

DS_RESOURCE(0) StructuredBuffer<InstanceTransform> instance_transforms_dyn;
DS_RESOURCE(1) StructuredBuffer<MeshDrawGpuData> mesh_draws;

struct VsOut {
	float4 position: SV_Position;
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

    float3 ws_pos = mul(instance_transforms_dyn[draw_index].current, float4(v.position, 1.0)).xyz;

    float4 vs_pos = mul(float4(ws_pos, 1.0),frame_constants.view_constants.world_to_view);
    float4 cs_pos = mul(vs_pos, frame_constants.view_constants.view_to_sample);

    float3 prev_ws_pos = mul(instance_transforms_dyn[draw_index].previous, float4(v.position, 1.0)).xyz;
    float4 prev_vs_pos = mul(float4(prev_ws_pos, 1.0), frame_constants.view_constants.world_to_view);
    // float4 prev_cs_pos = mul(frame_constants.view_constants.view_to_sample, prev_vs_pos);
    vsout.position = cs_pos;

    return vsout;
}
