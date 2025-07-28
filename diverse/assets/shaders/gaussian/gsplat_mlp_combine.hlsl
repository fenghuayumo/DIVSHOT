#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

[[vk::binding(0)]] RWByteAddressBuffer splat_scale_rot_buf;
[[vk::binding(1)]] RWByteAddressBuffer splat_pos_buf;
[[vk::binding(2)]] cbuffer _ {
    float4x4 transform;
    
    uint buf_id;
    uint feat_dim;
    uint n_feat_offsets;
    uint num_gaussians;
};

float4 quaternion_multiply(float4 q1, float4 q2)
{
    float w1 = q1.x; float x1 = q1.y; float y1 = q1.z; float z1 = q1.w;
    float w2 = q2.x; float x2 = q2.y; float y2 = q2.z; float z2 = q2.w;
    float w = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2;
    float x = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2;
    float y = w1 * y2 + y1 * w2 + z1 * x2 - x1 * z2;
    float z = w1 * z2 + z1 * w2 + x1 * y2 - y1 * x2;
    return float4(w, x, y, z);
}

[numthreads(GROUP_SIZE, GROUP_HEIGHT, 1)]
void main(uint2 px: SV_DispatchThreadID)
{
    const uint gaussian_id = px.x;
    const uint feat_offset_id = px.y;
    // const uint n = num_gaussians * n_feat_offsets;
    if (gaussian_id < num_gaussians && feat_offset_id < n_feat_offsets)
    {
        const uint global_id = gaussian_id * n_feat_offsets + feat_offset_id;
        float3 neural_scale = float3(splat_scale_rot_buf.Load<float>((global_id * 7 + 0)* sizeof(float)),
                                     splat_scale_rot_buf.Load<float>((global_id * 7 + 1) * sizeof(float)),
                                     splat_scale_rot_buf.Load<float>((global_id * 7 + 2) * sizeof(float)));

        float4 neural_rot = float4(splat_scale_rot_buf.Load<float>((global_id * 7 + 3) * sizeof(float)),
                                   splat_scale_rot_buf.Load<float>((global_id * 7 + 4) * sizeof(float)),
                                   splat_scale_rot_buf.Load<float>((global_id * 7 + 5) * sizeof(float)),
                                   splat_scale_rot_buf.Load<float>((global_id * 7 + 6) * sizeof(float)));
        float3 anchor_scale_offset = bindless_anchor_scales_offset(buf_id).Load<float3>(gaussian_id * sizeof(float3));
        float3 splat_offset = bindless_splat_offset(buf_id).Load<float3>(global_id * sizeof(float3));
        float3 scale = sigmoid(neural_scale) * anchor_scale_offset;

        Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(gaussian_id * sizeof(Gaussian));
        uint gs_state = bindless_splat_state[buf_id].Load<uint>(gaussian_id * sizeof(uint));
        float4 anchor_rotation_scale = gaussian.rotation_scale;
        float4 anchor_scale_opacity = unpack_half4(anchor_rotation_scale.zw);
        float3 gaussian_scale = anchor_scale_opacity.xyz;
        // rotation matrix
        float4 anchor_q = unpack_half4(anchor_rotation_scale.xy);
        float4 q = normalize(quaternion_multiply(anchor_q, neural_rot));
        // float4 q = normalize(neural_rot);
        float3 offset = splat_offset * gaussian_scale;
        float3 mean = gaussian.position.xyz + offset;
        splat_pos_buf.Store<float4>(global_id * sizeof(float4), float4(mean, asfloat(gs_state)));
        splat_scale_rot_buf.Store<float>((global_id * 7 + 0) * sizeof(float), scale.x);
        splat_scale_rot_buf.Store<float>((global_id * 7 + 1) * sizeof(float), scale.y);
        splat_scale_rot_buf.Store<float>((global_id * 7 + 2) * sizeof(float), scale.z);

        splat_scale_rot_buf.Store<float>((global_id * 7 + 3) * sizeof(float), q.x);
        splat_scale_rot_buf.Store<float>((global_id * 7 + 4) * sizeof(float), q.y);
        splat_scale_rot_buf.Store<float>((global_id * 7 + 5) * sizeof(float), q.z);
        splat_scale_rot_buf.Store<float>((global_id * 7 + 6) * sizeof(float), q.w);
    }
}