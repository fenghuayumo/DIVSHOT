#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

// [[vk::push_constant]]
// struct {
//     uint instance_id;
//     uint buf_id;
// } push_constants;

// [[vk::binding(0)]] RWStructuredBuffer<Gaussian> gaussians_buffer;
[[vk::binding(0)]] RWStructuredBuffer<uint> point_list_key_buffer;
[[vk::binding(1)]] RWStructuredBuffer<uint> point_list_value_buffer;
[[vk::binding(2)]] cbuffer _ {
    float4x4 model_matrix;

    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
};

float ndc2Pix(float v, int S)
{
    return ((v + 1.0) * S - 1.0) * 0.5;
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint2 px: SV_DispatchThreadID) {

    uint global_id = px.x;
    if (global_id < num_gaussians) {

        float4x4 view = frame_constants.view_constants.world_to_view;
        float4x4 proj = frame_constants.view_constants.view_to_clip;
        Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
        uint state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
        float4 gs_position = float4(gaussian.position.xyz, asfloat(state));
        uint gs_state = asuint(gs_position.w);
        uint op_state = getOpState(gs_state);
        if (op_state & DELETE_STATE) return;

        float4 world_pos = float4(gs_position.xyz, 1);
        // get transform from gs_translation, gs_scaling
#if SPLAT_EDIT
        uint transform_index = getTransformIndex(gs_state);
        float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
        transform = mul(transform, model_matrix);
#else
        float4x4 transform = model_matrix;
#endif
        world_pos.xyz = mul(world_pos, transform).xyz;
        float3 p_view = mul(world_pos, view).xyz;
        float4 p_hom = mul(float4(p_view, 1.0), proj);
        float p_w = 1.0f / (p_hom.w + 0.0000001f);
        float4 p_proj = p_hom * p_w;
        // if (p_proj.z <= 0.0 || p_proj.z >= 1.0)
        // {
        //     return;
        // }
        float view_z = p_view.z;

        point_list_key_buffer[global_id] = FloatToSortableUint(view_z);
        point_list_value_buffer[global_id] = global_id;
    }
}