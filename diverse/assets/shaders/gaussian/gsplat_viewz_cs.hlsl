#include "../inc/binding.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"
#include "threedgs_covariance.hlsl"

// [[vk::push_constant]]
// struct {
//     uint instance_id;
//     uint buf_id;
// } push_constants;

// DS_RESOURCE(0) RWStructuredBuffer<Gaussian> gaussians_buffer;
DS_RESOURCE(0) RWStructuredBuffer<uint> point_list_key_buffer;
DS_RESOURCE(1) RWStructuredBuffer<uint> point_list_value_buffer;
DS_RESOURCE(2) RWByteAddressBuffer num_visible_buffer;
DS_CBUFFER(3) cbuffer _ {
    float4x4 model_matrix;
    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
    float4 locked_color;
    float4 select_color;
    float4 tintColor;    // w: transparency
    float4 color_offset; // w: splat_scale_size
};

float ndc2Pix(float v, int S)
{
    return ((v + 1.0) * S - 1.0) * 0.5;
}

static const float alphaCullThreshold = 1.0 / 255.0;

// VK-style covariance projection with proper focal_x and focal_y
float3 computeCov2D_vk(float3x3 cov3D, float4 viewCenter, float2 focal, float4x4 viewmatrix)
{
    // Jacobian of affine approximation of projection
    // This avoids the non-linear perspective division
    const float s = 1.0 / (viewCenter.z * viewCenter.z);
    const float3x3 J = float3x3(
        focal.x / viewCenter.z, 0.0, -(focal.x * viewCenter.x) * s,
        0.0, focal.y / viewCenter.z, -(focal.y * viewCenter.y) * s,
        0.0, 0.0, 0.0
    );
    
    // Extract rotation part of view matrix (transpose to get row-major)
    const float3x3 W = float3x3(
        viewmatrix[0][0], viewmatrix[1][0], viewmatrix[2][0],
        viewmatrix[0][1], viewmatrix[1][1], viewmatrix[2][1],
        viewmatrix[0][2], viewmatrix[1][2], viewmatrix[2][2]
    );
    
    // T = J * W
    const float3x3 T = mul(J, W);
    
    // cov2D = T * cov3D * T^T
    const float3x3 cov2D = mul(mul(T, cov3D), transpose(T));
    
    // Return upper 2x2 as vector: (xx, xy, yy)
    return float3(cov2D[0][0], cov2D[0][1], cov2D[1][1]);
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
        float4 p_view = mul(world_pos, view);
        if(p_view.z >= 0.0f)
            return;
        float4 p_hom = mul(float4(p_view.xyz, 1.0), proj);
        float p_w = 1.0f / (p_hom.w + 0.0000001f);
        float4 p_proj = p_hom * p_w;
        
        // Frustum culling with dilation to avoid flickering at boundaries
        // Like vk_gaussian_splatting, we add a small margin
        const float frustumDilation = 0.15f;  // 15% margin
        const float clip = 1.0f + frustumDilation;
        if(abs(p_proj.x) > clip || abs(p_proj.y) > clip || 
           p_proj.z < (0.0f - frustumDilation) || p_proj.z > 1.0f)
            return;
        float view_z = p_view.z;

        // Gaussian rotation, scale, and opacity
        uint4 rotation_scale = gaussian.rotation_scale;
        float4 scale_opacity = unpack_uint2(rotation_scale.zw);
        float opac = scale_opacity.w;
        if (opac <= 1.0 / 255.0)
            return;
        float4 q = unpack_uint2(rotation_scale.xy);
        float quat_norm_sqr = dot(q, q);
        if (quat_norm_sqr < 1e-6) return;
        float3 s = scale_opacity.xyz * saturate(color_offset.w);

        float3 covA, covB;
        threedgs_compute_covariance_3d(s, q, covA, covB);
        float3x3 cov3D = float3x3(
            covA.x, covA.y, covA.z,
            covA.y, covB.x, covB.y,
            covA.z, covB.y, covB.z
        );
        const float tan_fovx = 1.0 / proj[0][0];
        const float tan_fovy = 1.0 / abs(proj[1][1]);
        const float focal_x = surface_width / (2.0f * tan_fovx);
        const float focal_y = surface_height / (2.0f * tan_fovy);
        float2 focal = float2(focal_x, focal_y);
        const bool is_ortho = proj[2][3] == 0.0f;
        // Computes the projected covariance
        const float4x4 model_view = mul(transform, view);
        const float3 cov2Dv = threedgs_covariance_projection(cov3D, p_view, focal, model_view, is_ortho);
        float2 basisVector1, basisVector2;
        if(!threedgs_projected_extent_basis(cov2Dv, sqrt8, 1.0f, opac, basisVector1, basisVector2))
        {
            return;
        }

        uint write_idx = 0;
        num_visible_buffer.InterlockedAdd(0, 1u, write_idx);
        point_list_key_buffer[write_idx] = encodeMinMaxFp32(view_z);
        point_list_value_buffer[write_idx] = global_id;
    }
}