#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

#define NONE_EDIT   0
#define BOX_EDIT    1
#define SPHERE_EDIT 2
#define RECT_EDIT   3
#define BRUSH_EDIT  4
#define POLYGON_EDIT 5
#define LASSO_EDIT 6
#define PAINT_EDIT 7
#define PICKER_EDIT 8

#define EDIT_OP_SET     0
#define EDIT_OP_REMOVE  1
#define EDIT_OP_ADD     2

// #define MODE_CENTERS 0
// #define MODE_RINGS   1
// [[vk::push_constant]]
// struct {
//     uint instance_id;
//     uint buf_id;
// } push_constants;

[[vk::binding(0)]] StructuredBuffer<uint> image_mask;
[[vk::binding(1)]] Texture2D<uint> pick_rect_image;
[[vk::binding(2)]] cbuffer _ {
    int surface_width;
    int surface_height;
    uint num_gaussians;
    float point_size;

    float4x4 model_matrix;

    float3 edit_box_min;
    uint edit_thick_ness;
    float3 edit_box_max;
    uint edit_value; //no use

    float edit_sphere_radius;
    float3 edit_sphere_center;
    
    float4 edit_rect;

    float2 mouse_pos;
    float gs_splat_size;
    uint buf_id;
};

static const float minAlpha = 1.0 / 255.0;

float2 clip_to_uv(float2 cs) 
{
    return cs * float2(0.5, -0.5) + float2(0.5, 0.5);
}

void computeCov3D(float3 scale, float mod, float4 rot, out float3x3 cov3D)
{
    // Create scaling matrix
    float3x3 S = float3x3(1.0f, 0.0, 0.0,
                          0.0, 1.0, 0.0,
                          0.0, 0.0, 1.0);
    S[0][0] = mod * scale.x;
    S[1][1] = mod * scale.y;
    S[2][2] = mod * scale.z;
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
float3 computeCov2D(float3 p_view, float focal_x, float focal_y, float tan_fovx, float tan_fovy, float3x3 cov3D, float4x4 viewmatrix, float kernel_ratio)
{
    // The following models the steps outlined by equations 29
    // and 31 in "EWA Splatting" (Zwicker et al., 2002).
    // Additionally considers aspect / scaling of viewport.
    // Transposes used to account for row-/column-major conventions.
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
    // Apply low-pass filter: every Gaussian should be at least
    // one pixel wide/high. Discard 3rd row and column.
    cov[0][0] += 0.3f;
    cov[1][1] += 0.3f;
    return float3(float(cov[0][0]), float(cov[1][1]), float(cov[0][1]));
}

float3 computeCov2D_ortho(float3 p_view, float focal_x, float focal_y, float3x3 cov3D, float4x4 viewmatrix, float kernel_ratio)
{
    // The following models the steps outlined by equations 29
    // and 31 in "EWA Splatting" (Zwicker et al., 2002).
    // Additionally considers aspect / scaling of viewport.
    // Transposes used to account for row-/column-major conventions.
    float3x3 J = float3x3(
        focal_x , 0.0f, 0,
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
    // Apply low-pass filter: every Gaussian should be at least
    // one pixel wide/high. Discard 3rd row and column.
    cov[0][0] += 0.3f;
    cov[1][1] += 0.3f;
    return float3(float(cov[0][0]), float(cov[1][1]), float(cov[0][1]));
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint2 px: SV_DispatchThreadID) 
{
    uint global_id = px.x;
    if (global_id < num_gaussians) 
    {
        Gaussian gs = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
        uint gs_state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
        uint op_state = getOpState(gs_state);

        // int edit_op = edit_type & 0x0000FFFF;
        int edit_tool_type = (edit_value >> 16) & 0xFF;

        if (op_state & DELETE_STATE || op_state & HIDE_STATE)
        {
            gs_state = setOpFlag(gs_state, 0);
            bindless_splat_state[buf_id].Store<uint>(global_id * sizeof(uint), gs_state);
            return;
        }
#if SPLAT_EDIT
        uint transform_index = getTransformIndex(gs_state);
        float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
        transform = mul(transform, model_matrix);
#else
        float4x4 transform = model_matrix;
#endif
        float4x4 view = frame_constants.view_constants.world_to_view;
        float4x4 proj = frame_constants.view_constants.view_to_clip;
        float4 world_pos = float4(gs.position.xyz, 1);
        // get transform from gs_translation, gs_scaling
        world_pos.xyz = mul(world_pos, transform).xyz;
        float3 p_view = mul(world_pos, view).xyz;
        // perspective projection
        float4 p_hom = mul(float4(p_view, 1.0), proj);
        float p_w = 1.0f / (p_hom.w + 0.0000001f);
        float3 p_proj = { p_hom.x * p_w, p_hom.y * p_w, p_hom.z * p_w };
        if (p_proj.z <= 0.0 || p_proj.z >= 1.0) return;
        float2 point_image = clip_to_uv(p_proj.xy) * float2(surface_width, surface_height);
        // point_image.y = surface_height - point_image.y - 1;
        // whether the point is in the crop bouding box region, where min is edit_box_min, max is edit_box_max
        bool in_crop = false;
#ifdef MODE_CENTERS
        if (edit_tool_type == BOX_EDIT) {
            in_crop = world_pos.x > edit_box_min.x && world_pos.x < edit_box_max.x &&
                      world_pos.y > edit_box_min.y && world_pos.y < edit_box_max.y &&
                      world_pos.z > edit_box_min.z && world_pos.z < edit_box_max.z;
        } else if (edit_tool_type == SPHERE_EDIT){ // whether the point is in the crop bouding sphere region, where center is edit_sphere_center, radius is edit_sphere_radius
            in_crop = length(world_pos.xyz - edit_sphere_center) < edit_sphere_radius;
        } else if (edit_tool_type == RECT_EDIT) { // whether the point is in the crop rect region, where the rect is edit_rect
            float2 region_min = max(point_image - point_size, 0);
            float2 region_max = min(point_image + point_size, float2(surface_width, surface_height));
            for (float i = region_min.x; i < region_max.x; i++) {
                for (float j = region_min.y; j < region_max.y; j++) {
                    float2 p = float2(i, j);
                    in_crop |= p.x >= edit_rect.x && p.x <= edit_rect.z &&
                              p.y >= edit_rect.y && p.y <= edit_rect.w;
                }
            }
        }
        else if (edit_tool_type >= BRUSH_EDIT && edit_tool_type <= PAINT_EDIT) {
            in_crop = image_mask[int(point_image.y) * surface_width + int(point_image.x)] > 0;
        }
        else if (edit_tool_type == PICKER_EDIT) {
            float2 p = mouse_pos;
            float2 region_min = max(point_image - point_size, 0);
            float2 region_max = min(point_image + point_size,float2(surface_width, surface_height));
            in_crop = p.x >= region_min.x && p.x <= region_max.x &&
                      p.y >= region_min.y && p.y <= region_max.y;
        }
#elif defined(MODE_RINGS)
        float4 rotation_scale = gs.rotation_scale;
        float4 scale_opacity = unpack_half4(rotation_scale.zw);
        float4 q = unpack_half4(rotation_scale.xy);
        float3 s = scale_opacity.xyz * gs_splat_size;

        float3x3 cov3D;
        computeCov3D(s, 1.0, q, cov3D);
        const float tan_fovy = 1.0 / abs(proj[1][1]);
        const float tan_fovx = 1.0 / abs(proj[0][0]);
        const float focal_y = surface_height / (2.0f * tan_fovy);
        const float focal_x = surface_width / (2.0f * tan_fovx);
        const bool is_ortho = proj[2][3] == 0.0f;
        float3 covariance = is_ortho ? computeCov2D_ortho(p_view.xyz, abs(proj[0][0]) * surface_width / 2.0, abs(proj[1][1]) * surface_height / 2.0, cov3D, view, 1) :
                                computeCov2D(p_view.xyz, focal_x, focal_y, tan_fovx, tan_fovy, cov3D, view, 1);

        // Gaussian region
        float det = (covariance.x * covariance.y - covariance.z * covariance.z);
        if (det > 0)
        {
            // Compute opacity-aware bounding box.
            // https://arxiv.org/pdf/2402.00525 Section B.2
            float extend = min(3.5, sqrt(2.0f * log(scale_opacity.w / minAlpha)));
            float det_inv = 1.f / det;
            float mid = 0.5f * (covariance.x + covariance.y);
            float term2 = sqrt(max(0.01f, mid * mid - det));
            float lambda1 = mid + term2;
            // float lambda2 = mid - term2;
            float lambda2 = extend * sqrt(lambda1);
            float radius_x = ceil(min(extend * sqrt(covariance.x), lambda2));
            float radius_y = ceil(min(extend * sqrt(covariance.y), lambda2));
            if (radius_x <= 0.0 || radius_y < 0.0) return;
            float region_size = max(radius_x, radius_y);
            float2 region_min = max(point_image - region_size,float2(0,0));
            float2 region_max = min(point_image + region_size,float2(surface_width, surface_height));
            if (edit_tool_type == RECT_EDIT) { 
                // whether the region_rect is in the crop rect region, where the rect is edit_rect
                // in_crop = region_min.x >= edit_rect.x && region_min.y >= edit_rect.y &&
                //           region_max.x <= edit_rect.z && region_max.y <= edit_rect.w;
                for(float i = region_min.x; i <= region_max.x; i++) {
                    for(float j = region_min.y; j <= region_max.y; j++) {
                        float2 p = float2(i, j);
                        uint vertId = pick_rect_image[int2(p.x, p.y)].x;
                        in_crop |= p.x >= edit_rect.x && p.x <= edit_rect.z &&
                                   p.y >= edit_rect.y && p.y <= edit_rect.w && global_id == vertId;
                    }
                }
            }
            else if (edit_tool_type >= BRUSH_EDIT && edit_tool_type <= PAINT_EDIT) {
                for (float i = region_min.x; i <= region_max.x; i++) {
                    for (float j = region_min.y; j <= region_max.y; j++) {
                        float2 p = float2(i, j);
                        uint vertId = pick_rect_image[int2(p.x, p.y)].x;
                        in_crop |= (image_mask[int(p.y) * surface_width + int(p.x)] > 0
                                    &&  global_id == vertId);
                    }
                }
            }
            else if (edit_tool_type == PICKER_EDIT) {
                // in_crop = image_mask[int(point_image.y) * surface_width + int(point_image.x)] == 0xffffffffu;
                float2 p = mouse_pos;
                uint vertId = pick_rect_image[int2(p.x,p.y)].x;
                in_crop |= global_id == vertId;
            }
        }
#endif
        gs_state = setOpFlag(gs_state, in_crop);
        bindless_splat_state[buf_id].Store<uint>(global_id * sizeof(uint), gs_state);
    }
}