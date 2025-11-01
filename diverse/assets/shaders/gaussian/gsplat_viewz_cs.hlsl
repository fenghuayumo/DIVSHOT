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
[[vk::binding(2)]] RWByteAddressBuffer num_visible_buffer;
[[vk::binding(3)]] cbuffer _ {
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

static const float minAlpha = 1.0 / 255.0;

float3 computeCov2D_persp(float3 p_view, float focal_x, float focal_y, float tan_fovx, float tan_fovy, float3x3 cov3D, float4x4 viewmatrix)
{
    // The following models the steps outlined by equations 29
    // and 31 in "EWA Splatting" (Zwicker et al., 2002).
    // Additionally considers aspect / scaling of viewport.
    // Transposes used to account for row-/column-major conventions.
    // float3 t = transformPoint4x3(mean, viewmatrix);

    const float limx = 1.3f * tan_fovx;
    const float limy = 1.3f * tan_fovy;
    const float txtz = p_view.x / p_view.z;
    const float tytz = p_view.y / p_view.z;
    p_view.x = min(limx, max(-limx, txtz)) * p_view.z;
    p_view.y = min(limy, max(-limy, tytz)) * p_view.z;

    float3x3 J = float3x3(
        focal_x / p_view.z, 0.0f, -(focal_x * p_view.x) / (p_view.z * p_view.z),
        0.0f, focal_y / p_view.z, -(focal_y * p_view.y) / (p_view.z * p_view.z),
        0, 0, 0);

    float3x3 W = float3x3(
        viewmatrix[0][0], viewmatrix[1][0], viewmatrix[2][0],
        viewmatrix[0][1], viewmatrix[1][1], viewmatrix[2][1],
        viewmatrix[0][2], viewmatrix[1][2], viewmatrix[2][2]);

    // float3x3 T = W * J;
    float3x3 T = mul(J, W);

    float3x3 Vrk = float3x3(
        cov3D[0][0], cov3D[0][1], cov3D[0][2],
        cov3D[0][1], cov3D[1][1], cov3D[1][2],
        cov3D[0][2], cov3D[1][2], cov3D[2][2]);

    // float3x3 cov = transpose(T) * transpose(Vrk) * T;
    float3x3 cov = mul(T, mul(transpose(Vrk), transpose(T)));
    return float3(float(cov[0][0]), float(cov[1][1]), float(cov[0][1]));
}

float3 computeCov2D_ortho(float3 p_view, float focal_x, float focal_y, float3x3 cov3D, float4x4 viewmatrix)
{
    // The following models the steps outlined by equations 29
    // and 31 in "EWA Splatting" (Zwicker et al., 2002).
    // Additionally considers aspect / scaling of viewport.
    // Transposes used to account for row-/column-major conventions.
    // float3 t = transformPoint4x3(mean, viewmatrix);
    float3x3 J = float3x3(
        focal_x, 0.0f, 0,
        0.0f, focal_y, 0,
        0, 0, 0);

    float3x3 W = float3x3(
        viewmatrix[0][0], viewmatrix[1][0], viewmatrix[2][0],
        viewmatrix[0][1], viewmatrix[1][1], viewmatrix[2][1],
        viewmatrix[0][2], viewmatrix[1][2], viewmatrix[2][2]);

    // float3x3 T = W * J;
    float3x3 T = mul(J, W);

    float3x3 Vrk = float3x3(
        cov3D[0][0], cov3D[0][1], cov3D[0][2],
        cov3D[0][1], cov3D[1][1], cov3D[1][2],
        cov3D[0][2], cov3D[1][2], cov3D[2][2]);

    // float3x3 cov = transpose(T) * transpose(Vrk) * T;
    float3x3 cov = mul(T, mul(transpose(Vrk), transpose(T)));
    return float3(float(cov[0][0]), float(cov[1][1]), float(cov[0][1]));
}

float3x3 computeCov2D(float3 center_view, float3 covA, float3 covB, float4x4 viewmatrix, float focal_x, bool is_ortho)
{
    float3x3 Vrk = float3x3(
        covA.x, covA.y, covA.z,
        covA.y, covB.x, covB.y,
        covA.z, covB.y, covB.z
    );
    float3 v = is_ortho ? float3(0.0, 0.0, 1.0) : center_view.xyz;
    float J1 = focal_x / v.z;
    float2 J2 = -J1 / v.z * v.xy;
    float3x3 J = float3x3(
        J1, 0.0, J2.x,
        0.0, J1, J2.y,
        0.0, 0.0, 0.0
    );
    // T = W * J;
    // cov = transpose(T) * Vrk * T;
    float3x3 W = float3x3(
        viewmatrix[0][0], viewmatrix[1][0], viewmatrix[2][0],
        viewmatrix[0][1], viewmatrix[1][1], viewmatrix[2][1],
        viewmatrix[0][2], viewmatrix[1][2], viewmatrix[2][2]);
    float3x3 T = mul(J, W);
    float3x3 cov = mul(T, mul(transpose(Vrk), transpose(T)));
    return cov;
}

void computeCov3D(float3 scale, float4 rot, out float3 covA, out float3 covB)
{
    // Create scaling matrix
    float3x3 S = float3x3(scale.x, 0.0, 0.0,
                          0.0, scale.y, 0.0,
                          0.0, 0.0, scale.z);
#if VISUALIZE_NORMAL
    float3 scale_axis = float3(0, 1, 0);
    if (scale.x < scale.y && scale.x < scale.z) {
        scale_axis = float3(1, 0, 0);
    } else if (scale.x > scale.y && scale.y < scale.z) {
        scale_axis = float3(0, 1, 0);
    } else {
        scale_axis = float3(0, 0, 1);
    }
#endif
    // Normalize quaternion to get valid rotation
    float4 q = rot; // / glm::length(rot);
    float r = q.x;
    float x = q.y;
    float y = q.z;
    float z = q.w;

    // Compute rotation matrix from quaternion
    float3x3 R = float3x3(
        1.f - 2.f * (y * y + z * z), 2.f * (x * y - r * z), 2.f * (x * z + r * y),
        2.f * (x * y + r * z), 1.f - 2.f * (x * x + z * z), 2.f * (y * z - r * x),
        2.f * (x * z - r * y), 2.f * (y * z + r * x), 1.f - 2.f * (x * x + y * y)
	);
#if VISUALIZE_NORMAL
    normal = normalize(mul(scale_axis, R));
#endif
    // float3x3 M = S * R;
    float3x3 M = mul(R, S);

    covA = float3(dot(M[0], M[0]), dot(M[0], M[1]), dot(M[0], M[2]));
    covB = float3(dot(M[1], M[1]), dot(M[1], M[2]), dot(M[2], M[2]));
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
        if (p_proj.z <= 1e-6f || p_proj.z >= 1.0f)
            return;
        float view_z = p_view.z;

        float3 center_view = p_view.xyz / p_view.w;
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
        computeCov3D(s, q, covA, covB);

        const float tan_fovx = 1.0 / proj[0][0];
        const float focal_x = surface_width / (2.0f * tan_fovx);
        const bool is_ortho = proj[2][3] == 0.0f;
        float4x4 model_view = mul(transform, view);
        float3x3 cov2D = computeCov2D(center_view, covA, covB, model_view, focal_x, is_ortho);

        // Gaussian region
        float diagonal1 = cov2D[0][0] + 0.3;
        float offDiagonal = cov2D[0][1];
        float diagonal2 = cov2D[1][1] + 0.3;

        float mid = 0.5 * (diagonal1 + diagonal2);
        float radius = length(float2((diagonal1 - diagonal2) / 2.0, offDiagonal));
        float lambda1 = mid + radius;
        float lambda2 = max(mid - radius, 0.1);

        // Use the smaller viewport dimension to limit the kernel size relative to the screen resolution.
        float vmin = min(1024.0, min(surface_width, surface_height));

        float l1 = 2.0 * min(sqrt(2.0 * lambda1), vmin);
        float l2 = 2.0 * min(sqrt(2.0 * lambda2), vmin);
        if (l1 < 0.1f || l2 < 0.1f) {
            return;
        }
        float2 c = 2.0f / float2(surface_width, surface_height);
        // cull against frustum x/y axes
        float l = max(l1, l2);
        if (any((abs(p_proj.xy) - float2(l, l) * c - float2(1, 1)) > 0.0)) {
            return;
        }

        uint write_idx = 0;
        num_visible_buffer.InterlockedAdd(0, 1u, write_idx);
        point_list_key_buffer[write_idx] = FloatToSortableUint(view_z);
        point_list_value_buffer[write_idx] = global_id;
    }
}