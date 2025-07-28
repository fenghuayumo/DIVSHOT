
#include "../inc/bindless.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/frame_constants.hlsl"


#include "gaussian_common.hlsl"

[[vk::binding(0)]] RWByteAddressBuffer splat_pos_buf; // feat_dim x (feat_dim + 3)

[[vk::binding(1)]] cbuffer _ {
    float4x4 model_matrix;
    uint buf_id;
    uint feat_dim;
    uint n_feat_offsets;
    uint num_gaussians;
};

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint2 px: SV_DispatchThreadID)
{
    const uint global_id = px.x;
    if (global_id < num_gaussians)
    {
        float4x4 view = frame_constants.view_constants.world_to_view;
        float4x4 proj = frame_constants.view_constants.view_to_clip;

        Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
        uint gs_state = asuint(gaussian.position.w);
        uint op_state = getOpState(gs_state);
        if (op_state & DELETE_STATE) return;

        float4 world_pos = float4(gaussian.position.xyz, 1);
// get transform from gs_translation, gs_scaling
#if SPLAT_EDIT
        uint transform_index = getTransformIndex(gs_state);
        float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
        transform = mul(transform, model_matrix);
#else
        float4x4 transform = model_matrix;
#endif
        world_pos.xyz = mul(world_pos, transform).xyz;
        float3 direction = (world_pos.xyz - get_eye_position());
        float len = length(direction);
        float3 direction_normed = direction / len;
        splat_pos_buf.Store<float4>(global_id * sizeof(float4), float4(direction_normed, len));
    }
}
