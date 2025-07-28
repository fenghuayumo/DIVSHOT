#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

#if MLP_LAYER0
[[vk::binding(0)]] StructuredBuffer<float> scale_rot_mlp_weights0; // feat_dim x (feat_dim + 4)
[[vk::binding(1)]] StructuredBuffer<float> scale_rot_mlp_bias0;
[[vk::binding(2)]] ByteAddressBuffer splat_pos_buf;
[[vk::binding(3)]] RWStructuredBuffer<float> splat_feat_layer_buf;
[[vk::binding(4)]] cbuffer _ {
    float4x4 transform;

    uint buf_id;
    uint feat_dim;
    uint n_feat_offsets;
    uint num_gaussians;
};

void get_mlp_scale_rot_layer0(float3 direction,float viewdir_len, uint index,uint feat_idx)
{
    // layer0
    // float feature_0[32]; // assert fat_dim == 32
    // for (uint feat_idx = 0; feat_idx < feat_dim; feat_idx++)
    {
        float value = 0.0;
        for (uint j = 0; j < feat_dim; j++)
        {
            float f2 = bindless_gaussians_color(buf_id).Load<float>((index * feat_dim + j) * sizeof(float));
            // float2 f2 = unpack_half2(f);
            value += f2.x * scale_rot_mlp_weights0[feat_idx * (feat_dim + 4) + j + 0];
        }
        value += direction.x * scale_rot_mlp_weights0[feat_idx * (feat_dim + 4) + feat_dim];
        value += direction.y * scale_rot_mlp_weights0[feat_idx * (feat_dim + 4) + feat_dim + 1];
        value += direction.z * scale_rot_mlp_weights0[feat_idx * (feat_dim + 4) + feat_dim + 2];
        value += viewdir_len * scale_rot_mlp_weights0[feat_idx * (feat_dim + 4) + feat_dim + 3];
        value += scale_rot_mlp_bias0[feat_idx];
        value = relu(value);
        // feature_0[i] = value;
        splat_feat_layer_buf[index * feat_dim + feat_idx] = value;
    }
}

[numthreads(GROUP_SIZE, GROUP_HEIGHT, 1)]
void main(uint2 px: SV_DispatchThreadID)
{
    const uint global_id = px.x;
    const uint feat_idx = px.y;
    if (global_id < num_gaussians && feat_idx < feat_dim)
    {
        float4 direction = splat_pos_buf.Load<float4>(sizeof(float4) * global_id);
        get_mlp_scale_rot_layer0(direction.xyz, direction.w, global_id,feat_idx);
    }
}

#else

[[vk::binding(0)]] StructuredBuffer<float> scale_rot_mlp_weights1; // 7 * n_feat_offsets x feat_dim
[[vk::binding(1)]] StructuredBuffer<float> scale_rot_mlp_bias1;
[[vk::binding(2)]] StructuredBuffer<float> splat_feat_layer_buf;
[[vk::binding(3)]] RWStructuredBuffer<float> splat_scale_rot_opacity_buf;
[[vk::binding(4)]] cbuffer _ {
    float4 gs_translation;
    float4 gs_scaling;
    float4 gs_rotation;

    uint buf_id;
    uint feat_dim;
    uint n_feat_offsets;
    uint num_gaussians;
};

void get_mlp_scale_rot_layer1( uint index, uint i)
{
    // layer1
    // for (uint i = 0; i < n_feat_offsets * 7; i++)
    {
        float value = 0.0f;
        for (uint j = 0; j < feat_dim; j++)
        {
            value += splat_feat_layer_buf[index* feat_dim + j] * scale_rot_mlp_weights1[i * feat_dim + j];
        }
        value += scale_rot_mlp_bias1[i];
        splat_scale_rot_opacity_buf[index * n_feat_offsets * 7 + i] = value;
    }
}

[numthreads(GROUP_SIZE, GROUP_HEIGHT, 1)]
void main(uint2 px: SV_DispatchThreadID)
{
    const uint global_id = px.x;
    const uint col = px.y;
    if (global_id < num_gaussians && col < (n_feat_offsets * 7))
    {
        get_mlp_scale_rot_layer1(global_id, col);
    }
}

#endif