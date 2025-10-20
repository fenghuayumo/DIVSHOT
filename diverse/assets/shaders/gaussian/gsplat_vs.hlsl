#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"
#include "gsplat_sh.hlsl"
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 colour : COLOR0;
    [[vk::location(1)]] float2 pix_direction : COLOR2;
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
    float4 color_offset;//w: splat_scale_size
};
[[vk::binding(1)]] StructuredBuffer<uint> point_list_value_buffer;
#if VISUALIZE_RINGS || VISUALIZE_ELLIPSOIDS
static const float stddev = 3.5;
#else
static const float stddev = 3.33;
#endif

static const float minAlpha = 1.0 / 255.0;

static const float2 vPosition[4] = {
    float2(-1, -1),
    float2(-1, 1),
    float2(1, -1),
    float2(1, 1)
};

float3 get_color(uint index, float3 direction) {
    float3 color = read_splat_color(buf_id, index);
#if SH_DEGREE > 0
    color += read_splat_sh_color(buf_id, index, direction);
#endif
    return color;   
}

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

float3 computeCov2D_ortho(float3 p_view, float focal_x, float focal_y,float3x3 cov3D, float4x4 viewmatrix)
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

float3x3 computeCov2D(float3 center_view, float3 covA, float3 covB, float4x4 viewmatrix,float focal_x,bool is_ortho)
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

#if VISUALIZE_NORMAL
void computeCov3D(float3 scale, float4 rot, out float3 covA, out float3 covB, out float3 normal)
#else
void computeCov3D(float3 scale, float4 rot, out float3 covA, out float3 covB)
#endif
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

float ndc2Pix(float v, int S)
{
    return ((v + 1.0) * S - 1.0) * 0.5;
}

VS_OUTPUT main(uint vertexID : SV_VertexID,uint instanceID : SV_InstanceID)
{
    VS_OUTPUT output;
    uint global_id = point_list_value_buffer[instanceID];
    if(global_id >= num_gaussians)
    {
        output.colour.w = 0.0;
        return output;
    }
    Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
    uint state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
    float4 gs_position = float4(gaussian.position.xyz,asfloat(state));
    uint gs_state = asuint(gs_position.w);
    uint op_state = getOpState(gs_state);
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
    float4 p_view = mul(world_pos, view);
    if(p_view.z >= 0.0f)
    {
        output.colour.w = 0.0;
        return output;
    }
    // perspective projection
    float4 p_hom = mul(p_view, proj);
    float p_w = 1.0f / (p_hom.w + 0.0000001f);
    float4 p_proj = p_hom * p_w;
    float3 center_view = p_view.xyz / p_view.w;
#if OUTLINE_PASS
    if (op_state != SELECT_STATE)
    {
        output.colour.w = 0.0;
        return output;
    }
#endif

    // Gaussian rotation, scale, and opacity
    uint4 rotation_scale = gaussian.rotation_scale;
    float4 scale_opacity = unpack_uint2(rotation_scale.zw);
    float opac = scale_opacity.w;
    if(opac <= 1.0 / 255.0) 
    {
        output.colour.w = 0.0;
        return output;
    }
    float4 q = unpack_uint2(rotation_scale.xy);
    float quat_norm_sqr = dot(q, q);
    if (quat_norm_sqr < 1e-6) 
    {
        output.colour.w = 0.0;
        return output;
    }
    float3 s = scale_opacity.xyz * saturate(color_offset.w);

    float3 covA, covB;
#if VISUALIZE_NORMAL
    float3 normal;
    computeCov3D(s, q, covA, covB, normal);
#else
    computeCov3D(s, q, covA, covB);
#endif
    //const float tan_fovy = 1.0 / abs(proj[1][1]);
    //const float focal_y = surface_height / (2.0f * tan_fovy);
    const float tan_fovx = 1.0 / proj[0][0];
    const float focal_x = surface_width / (2.0f * tan_fovx);
    const bool is_ortho = proj[2][3] == 0.0f;
    float3x3 cov2D = computeCov2D(center_view, covA, covB, model_view, focal_x, is_ortho);
#if GSPLAT_AA
    // calculate AA factor
    float detOrig = cov2D[0][0] * cov2D[1][1] - cov2D[0][1] * cov2D[0][1];
    float detBlur = (cov2D[0][0] + 0.3) * (cov2D[1][1] + 0.3) - cov2D[0][1] * cov2D[0][1];
    float corner_aaFactor = sqrt(max(detOrig / detBlur, 0.0));
#endif

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
        output.colour.a = 0.0; // will not emit things
        return output;
    }
    float2 c = 2.0f / float2(surface_width, surface_height);
    // cull against frustum x/y axes
    float l = max(l1, l2);
    if (any((abs(p_proj.xy) - float2(l,l) * c - float2(1,1)) > 0.0)) {
        output.colour.a = 0.0; // will not emit things
        return output;
    }
    
    float2 diagonalVector = normalize(float2(offDiagonal, lambda1 - diagonal1));
    float2 v1 = l1 * diagonalVector;
    float2 v2 = l2 * float2(diagonalVector.y, -diagonalVector.x);

    int vi = vertexID % 4;
    float2 corner_uv = vPosition[vi];
    float2 corner_offset = (corner_uv.x * v1 + corner_uv.y * v2) * c;
    // float3 direction = normalize(world_pos.xyz - get_eye_position());
    float3x3 model_view_3x3 = float3x3(model_view[0][0],model_view[0][1],model_view[0][2],
                                    model_view[1][0],model_view[1][1],model_view[1][2],
                                    model_view[2][0],model_view[2][1],model_view[2][2]);
    float3 direction = normalize(mul(model_view_3x3,center_view));

#if VISUALIZE_NORMAL
    output.colour = float4( normalize(normal) * 0.5 + 0.5, opac);
#elif VISUALIZE_DEPTH

    float3 first_pos = bindless_gaussians(buf_id).Load<Gaussian>(point_list_value_buffer[0] * sizeof(Gaussian)).position.xyz;
    float3 last_pos = bindless_gaussians(buf_id).Load<Gaussian>(point_list_value_buffer[(num_gaussians - 1)] * sizeof(Gaussian)).position.xyz;

    float3 min_pos = mul(float4(first_pos, 1), transform).xyz;
    float3 max_pos = mul(float4(last_pos, 1), transform).xyz;
    float min_dist = length(min_pos - get_eye_position());
    float max_dist = length(max_pos - get_eye_position());
    float normalized_depth = (length(world_pos.xyz - get_eye_position()) - min_dist) / (max_dist - min_dist);
    float r = smoothstep(0.5, 1.0, normalized_depth);
    float g = 1.0 - abs(normalized_depth - 0.5) * 2.0;
    float b = 1.0 - smoothstep(0.0, 0.5, normalized_depth);
    output.colour = float4(float3(r, g, b), opac);
#elif VISUALIZE_OPACITY
    output.colour = float4(float3(1,1,1), opac);
#else
    if (op_state & DELETE_STATE)
        output.colour = float4(0, 0, 0, 0);
    else if (op_state & HIDE_STATE)
        output.colour = float4(get_color(global_id, direction) * locked_color.xyz, opac);
    else if (op_state & SELECT_STATE)
        output.colour = float4(lerp(get_color(global_id, direction), select_color.xyz * 0.8, select_color.w), opac);
    else
        output.colour = float4(get_color(global_id, direction) * tintColor.xyz + color_offset.xyz, tintColor.w * opac);
#endif
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
    return output;
}