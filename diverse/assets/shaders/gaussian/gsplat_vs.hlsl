#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"
#include "gsplat_sh.hlsl"
#include "threedgs_covariance.hlsl"
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] nointerpolation float4 colour : COLOR0;
    [[vk::location(1)]] float2 fragPos : TEXCOORD1;
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

static const float alphaCullThreshold = 1.0 / 255.0;

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
        output.position = float4(0, 0, 2.0f, 1.0f);
        return output;
    }
    Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
    uint state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
    float4 gs_position = float4(gaussian.position.xyz,asfloat(state));
    uint gs_state = asuint(gs_position.w);
    uint op_state = getOpState(gs_state);
    if (op_state & DELETE_STATE) 
    {
        output.position = float4(0, 0, 2.0f, 1.0f);
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
        output.position = float4(0, 0, 2.0f, 1.0f);
        return output;
    }
    // perspective projection
    float4 p_hom = mul(p_view, proj);
    float p_w = 1.0f / (p_hom.w + 0.0000001f);
    float4 p_proj = p_hom * p_w;
    
    // Frustum culling with dilation to avoid flickering at boundaries
    const float frustumDilation = 0.15f;  // 15% margin
    const float clip = 1.0f + frustumDilation;
    if(abs(p_proj.x) > clip || abs(p_proj.y) > clip || 
       p_proj.z < (0.0f - frustumDilation) || p_proj.z > 1.0f)
    {
        output.position = float4(0, 0, 2.0f, 1.0f);
        return output;
    }
    const float2 fragPos = vPosition[vertexID].xy;
    output.fragPos = fragPos * sqrt8;
    float3 center_view = p_view.xyz / p_view.w;
#if OUTLINE_PASS
    if (op_state != SELECT_STATE)
    {
       output.position = float4(0, 0, 2.0f, 1.0f);
        return output;
    }
#endif

    // Gaussian rotation, scale, and opacity
    uint4 rotation_scale = gaussian.rotation_scale;
    float4 scale_opacity = unpack_uint2(rotation_scale.zw);
    float opac = scale_opacity.w;
    
    // Alpha culling threshold like vk_gaussian_splatting
    // Cull splats with very low opacity to reduce overdraw and fogginess
    if(opac < alphaCullThreshold) 
    {
        output.position = float4(0, 0, 2.0f, 1.0f);
        return output;
    }
    float4 q = unpack_uint2(rotation_scale.xy);
    float quat_norm_sqr = dot(q, q);
    if (quat_norm_sqr < 1e-6) 
    {
        output.position = float4(0, 0, 2.0f, 1.0f);
        return output;
    }
    float3 s = scale_opacity.xyz * saturate(color_offset.w);

    float3 covA, covB;
#if VISUALIZE_NORMAL
    float3 normal;
    threedgs_compute_covariance_3d(s, q, covA, covB, normal);
#else
    threedgs_compute_covariance_3d(s, q, covA, covB);
#endif
    
    // Reconstruct full 3x3 covariance matrix from compact representation
    float3x3 cov3D = float3x3(
        covA.x, covA.y, covA.z,
        covA.y, covB.x, covB.y,
        covA.z, covB.y, covB.z
    );
    
    // Calculate both focal lengths (important for non-square viewports!)
    const float tan_fovx = 1.0 / proj[0][0];
    const float tan_fovy = 1.0 / abs(proj[1][1]);
    const float focal_x = surface_width / (2.0f * tan_fovx);
    const float focal_y = surface_height / (2.0f * tan_fovy);
    float2 focal = float2(focal_x, focal_y);
    const bool is_ortho = proj[2][3] == 0.0f;
    // Use VK-style covariance projection
    float4 viewCenter = p_view;
    float3 cov2Dv = threedgs_covariance_projection(cov3D, viewCenter, focal, model_view, is_ortho);
    
    float2 basisVector1, basisVector2;
    if(!threedgs_projected_extent_basis(cov2Dv, sqrt8, 1.0f, opac, basisVector1, basisVector2))
    {
        // emit same vertex to get degenerate triangle
        output.position = float4(0.0, 0.0, 2.0, 1.0);
        return output;
    }
 
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
    float4 ndcCenter = p_proj;
    const float2 basisViewport = 1.0 / float2(surface_width, surface_height);
    const float2 ndcOffset = float2(fragPos.x * basisVector1 + fragPos.y * basisVector2) * basisViewport * 2.0;

    const float4 quadPos = float4(ndcCenter.xy + ndcOffset, ndcCenter.z, 1.0);

    // Emit the vertex position
    output.position = quadPos;
    // output.colour = output.colour;
    return output;
}