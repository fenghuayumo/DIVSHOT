#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

#if MLP_LAYER0
[[vk::binding(0)]] StructuredBuffer<float> color_mlp_weights0;//feat_dim x (feat_dim + 3)
[[vk::binding(1)]] StructuredBuffer<float> color_mlp_bias0;
[[vk::binding(2)]] ByteAddressBuffer       splat_pos_buf;
[[vk::binding(3)]] RWStructuredBuffer<float> splat_feature_layer_buf;
[[vk::binding(4)]] cbuffer _ {
    float4x4 transform;

    uint buf_id;
    uint feat_dim;
    uint n_feat_offsets;
    uint num_gaussians;
};

#define SPLAT_META_ALLOC_COUNT (0 * sizeof(uint))
void get_mlp_color_layer0(float3 direction, uint index,uint feat_idx)
{
    // layer0
    {
        float color = 0.0;
        for (uint j = 0; j < feat_dim; j++)
        {
            float f2 = bindless_gaussians_color(buf_id).Load<float>((index * feat_dim + j) * sizeof(float));
            // float2 f2 = unpack_half2(f);
            color += f2.x * color_mlp_weights0[feat_idx * (feat_dim + 3) + j + 0];
            // color += f2.y * color_mlp_weights0[i * (feat_dim + 3) + j * 2 + 1];
        }
        color += direction.x * color_mlp_weights0[feat_idx * (feat_dim + 3) + feat_dim];
        color += direction.y * color_mlp_weights0[feat_idx * (feat_dim + 3) + feat_dim + 1];
        color += direction.z * color_mlp_weights0[feat_idx * (feat_dim + 3) + feat_dim + 2];
        color += color_mlp_bias0[feat_idx];
        color = relu(color);
        splat_feature_layer_buf[index * feat_dim + feat_idx] = color;
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
        get_mlp_color_layer0(direction.xyz, global_id, feat_idx);
    }
}
#else

[[vk::binding(0)]] StructuredBuffer<float> color_mlp_weights1; // 3* n_feat_offsets x feat_dim
[[vk::binding(1)]] StructuredBuffer<float> color_mlp_bias1;
[[vk::binding(2)]] StructuredBuffer<float> splat_feat_layer_buf;
[[vk::binding(3)]] RWStructuredBuffer<float> splat_colors_buf;
[[vk::binding(4)]] cbuffer _ {
    float4 gs_translation;
    float4 gs_scaling;
    float4 gs_rotation;

    uint buf_id;
    uint feat_dim;
    uint n_feat_offsets;
    uint num_gaussians;
};

void get_mlp_color_layer1(uint index, uint i)
{
    // layer1
    // for (uint i = 0; i < n_feat_offsets * 3; i++)
    {
        float color = 0.0;
        for (uint j = 0; j < feat_dim; j++)
        {
            color += splat_feat_layer_buf[index * feat_dim + j] * color_mlp_weights1[i * feat_dim + j];
        }
        color += color_mlp_bias1[i + 0];
        color = sigmoid(color);
        uint address_id = index * n_feat_offsets * 3 + i;
        // splat_colors.Store<float>(address_id * sizeof(float), color);
        splat_colors_buf[address_id] = color;
    }
}

[numthreads(GROUP_SIZE, GROUP_HEIGHT, 1)]
void main(uint2 px: SV_DispatchThreadID)
{
    const uint global_id = px.x;
    const uint idx = px.y;
    if (global_id < num_gaussians && idx < (n_feat_offsets * 3))
    {
        get_mlp_color_layer1(global_id, idx);
    }
}

#endif