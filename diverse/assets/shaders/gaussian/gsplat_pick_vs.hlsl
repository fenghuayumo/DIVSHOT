#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] nointerpolation float4 colour : COLOR0;
    [[vk::location(1)]] float2 pix_direction : COLOR2;
    [[vk::location(2)]] nointerpolation uint vert_id : COLOR3;
};
// struct InstanceTransform {
//     row_major float3x4 current;
//     row_major float3x4 previous;
// };

// [[vk::binding(0)]] StructuredBuffer<InstanceTransform> instance_transforms_dyn;
[[vk::binding(0)]] cbuffer _ {
    float4x4 model_matrix;
    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
    float4 locked_color;
    float4 select_color;
    float4 tintColor; // w: transparency
    float4 color_offset;
};
[[vk::binding(1)]] StructuredBuffer<uint> point_list_value_buffer;

static const float minAlpha = 1.0 / 255.0;

static const float2 vPosition[4] = {
    float2(-1, -1),
    float2(-1, 1),
    float2(1, -1),
    float2(1, 1)
};

float3 transformPoint4x3(float3 p, float4x4 t_matrix)
{
    float3 transformed = {
        t_matrix[0][0] * p.x + t_matrix[1][0] * p.y + t_matrix[2][0] * p.z + t_matrix[3][0],
        t_matrix[0][1] * p.x + t_matrix[1][1] * p.y + t_matrix[2][1] * p.z + t_matrix[3][1],
        t_matrix[0][2] * p.x + t_matrix[1][2] * p.y + t_matrix[2][2] * p.z + t_matrix[3][2],
    };
    return transformed;
}

float4 transformPoint4x4(float3 p, float4x4 t_matrix) {
    float4 transformed = {
        t_matrix[0][0] * p.x + t_matrix[1][0] * p.y + t_matrix[2][0] * p.z + t_matrix[3][0],
        t_matrix[0][1] * p.x + t_matrix[1][1] * p.y + t_matrix[2][1] * p.z + t_matrix[3][1],
        t_matrix[0][2] * p.x + t_matrix[1][2] * p.y + t_matrix[2][2] * p.z + t_matrix[3][2],
        t_matrix[0][3] * p.x + t_matrix[1][3] * p.y + t_matrix[2][3] * p.z + t_matrix[3][3]
    };
    return transformed;
}

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

#if VISUALIZE_NORMAL
void computeCov3D(float3 scale, float mod, float4 rot, out float3x3 cov3D, out float3 normal)
#else
void computeCov3D(float3 scale, float mod, float4 rot, out float3x3 cov3D)
#endif
{
    // Create scaling matrix
    float3x3 S = float3x3(1.0f, 0.0, 0.0,
                          0.0, 1.0, 0.0,
                          0.0, 0.0, 1.0);
    S[0][0] = mod * scale.x;
    S[1][1] = mod * scale.y;
    S[2][2] = mod * scale.z;
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
    // Compute 3D world covariance matrix Sigma
    // float3x3 Sigma = transpose(M) * M;
    float3x3 Sigma = mul(M, transpose(M));
    // Covariance is symmetric, only store upper right
    cov3D[0][0] = Sigma[0][0];
    cov3D[0][1] = Sigma[0][1];
    cov3D[0][2] = Sigma[0][2];
    cov3D[1][1] = Sigma[1][1];
    cov3D[1][2] = Sigma[1][2];
    cov3D[2][2] = Sigma[2][2];
}

float ndc2Pix(float v, int S)
{
    return ((v + 1.0) * S - 1.0) * 0.5;
}

float2 computeNDCOffset(float2 basisVector0, float2 basisVector1, float2 vPosition)
{
    float2 screen_pixel_size = 1.0 / float2(surface_width, surface_height);
    return (vPosition.x * basisVector0 + vPosition.y * basisVector1) * screen_pixel_size * 2.0;
}

float4 get_bounding_box(float2 direction, float radius_px)
{
    float2 screen_size = float2(surface_width, surface_height);
    float2 screen_pixel_size = 1.0 / screen_size;
    float2 radius_ndc = screen_pixel_size * radius_px * 2;
    return float4(
        radius_ndc * direction,
        radius_px * direction
    );
}

VS_OUTPUT main(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    uint global_id = point_list_value_buffer[instanceID];
    Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
    uint state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
    float4 gs_position = float4(gaussian.position.xyz, asfloat(state));
    uint gs_state = asuint(gs_position.w);
    uint op_state = getOpState(gs_state);
    VS_OUTPUT output;
    if (op_state & DELETE_STATE)
    {
        output.colour.w = 0.0;
        return output;
    }
#if SPLAT_EDIT
    uint transform_index = getTransformIndex(gs_state);
    float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
    transform = mul(transform, model_matrix);
#else
    float4x4 transform = model_matrix;
#endif
    float4 world_pos = mul(float4(gs_position.xyz, 1.0), transform);

    float4x4 view = frame_constants.view_constants.world_to_view;
    float4x4 model_view = mul(transform, view);
    float4x4 proj = frame_constants.view_constants.view_to_clip;
    float3 p_view = mul(world_pos, view).xyz;
    if (p_view.z > 0)
    {
        output.colour.w = 0.0;
        return output;
    }
    // perspective projection
    float4 p_hom = mul(float4(p_view, 1.0), proj);
    float p_w = 1.0f / (p_hom.w + 0.0000001f);
    float4 p_proj = p_hom * p_w;
    if (p_proj.z <= 0.0 || p_proj.z >= 1.0)
    {
        output.colour.w = 0.0;
        return output;
    }
#if OUTLINE_PASS
    if (op_state != SELECT_STATE)
    {
        output.colour.w = 0.0;
        return output;
    }
#endif

    // Gaussian rotation, scale, and opacity
    float4 rotation_scale = gaussian.rotation_scale;
    float4 scale_opacity = unpack_half4(rotation_scale.zw);
    float4 q = unpack_half4(rotation_scale.xy);

    float3 s = scale_opacity.xyz * saturate(color_offset.w);

    float3x3 cov3D;
#if VISUALIZE_NORMAL
    float3 normal;
    computeCov3D(s, 1.0, q, cov3D, normal);
#else
    computeCov3D(s, 1.0, q, cov3D);
#endif
    const float tan_fovy = 1.0 / abs(proj[1][1]);
    const float tan_fovx = 1.0 / abs(proj[0][0]);
    const float focal_y = surface_height / (2.0f * tan_fovy);
    const float focal_x = surface_width / (2.0f * tan_fovx);
    const bool is_ortho = proj[2][3] == 0.0f;
    float3 covariance = is_ortho ? computeCov2D_ortho(p_view.xyz, abs(proj[0][0]) * surface_width / 2.0, abs(proj[1][1]) * surface_height / 2.0f, cov3D, model_view) :
                                computeCov2D_persp(p_view.xyz, focal_x, focal_y, tan_fovx, tan_fovy, cov3D, model_view);
#if GSPLAT_AA
    // calculate AA factor
    float detOrig = covariance.x * covariance.y - covariance.z * covariance.z;
    float detBlur = (covariance.x + 0.3) * (covariance.y + 0.3) - covariance.z * covariance.z;
    float corner_aaFactor = sqrt(max(detOrig / detBlur, 0.0));
#endif

    // Gaussian region
    float diagonal1 = covariance.x + 0.3;
    float offDiagonal = covariance.z;
    float diagonal2 = covariance.y + 0.3;

    float mid = 0.5 * (diagonal1 + diagonal2);
    float radius = length(float2((diagonal1 - diagonal2) / 2.0, offDiagonal));
    float lambda1 = mid + radius;
    float lambda2 = max(mid - radius, 0.1);

    // Use the smaller viewport dimension to limit the kernel size relative to the screen resolution.
    float vmin = min(1024.0, min(surface_width, surface_height));

    float l1 = 2.0 * min(sqrt(2.0 * lambda1), vmin);
    float l2 = 2.0 * min(sqrt(2.0 * lambda2), vmin);
    if (l1 < 2.0 || l2 < 2.0) {
        output.colour.a = 0.0; // will not emit things
        return output;
    }
    float2 diagonalVector = normalize(float2(offDiagonal, lambda1 - diagonal1));
    float2 v1 = l1 * diagonalVector;
    float2 v2 = l2 * float2(diagonalVector.y, -diagonalVector.x);

    int vi = vertexID % 4;
    float2 corner_uv = vPosition[vi];
    float2 c = 2.0 / float2(surface_width, surface_height);
    float2 corner_offset = (corner_uv.x * v1 + corner_uv.y * v2) * c;
    // float3 direction = normalize(world_pos.xyz - get_eye_position());
    output.colour = float4(float3(1,1,1), 1);
#if GSPLAT_AA
    // apply AA compensation
    output.colour.a *= corner_aaFactor;
#endif
    float4 ndcCenter = p_proj;
    float clip = min(1.0, sqrt(-log(1.0 / 255.0 / output.colour.a)) / 2.0);
    corner_offset *= clip;
    corner_uv *= clip;
    output.pix_direction = corner_uv;
    output.position = float4(ndcCenter.xy + corner_offset, ndcCenter.zw);
    // output.colour = output.colour;
    output.vert_id = global_id;
    return output;
}