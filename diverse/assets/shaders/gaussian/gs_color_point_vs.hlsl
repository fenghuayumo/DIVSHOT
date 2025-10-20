#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "gaussian_common.hlsl"
#include "gsplat_sh.hlsl"
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 colour : COLOR0;
};

[[vk::binding(0)]] cbuffer _ {
    float4x4 model_matrix;
    float point_size;
    uint surface_width;
    uint surface_height;
    uint buf_id;
};
static const float2 vPosition[4] = {
    float2(-1, -1),
    float2(-1, 1),
    float2(1, -1),
    float2(1, 1)
};


float2 get_bounding_box(float2 direction, float radius_px)
{
    float2 screen_pixel_size = 1.0 / float2(surface_width, surface_height);
    float2 radius_ndc = screen_pixel_size * radius_px * 2;
    return float2(radius_ndc * direction);
}

VS_OUTPUT main(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VS_OUTPUT output;

    Gaussian gaussian = bindless_gaussians(buf_id).Load<Gaussian>(instanceID * sizeof(Gaussian));
    uint state = bindless_splat_state[buf_id].Load<uint>(instanceID * sizeof(uint));
    float4 gs_pos = float4(gaussian.position.xyz,asfloat(state));
    uint gs_state = asuint(gs_pos.w);
    uint op_state = getOpState(gs_state);
    if (op_state & DELETE_STATE) {
        output.colour = float4(0, 0, 0, 0);
        return output;
    }
#if SPLAT_EDIT
    uint transform_index = getTransformIndex(gs_state);
    float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
    transform = mul(transform, model_matrix);
#else
    float4x4 transform = model_matrix;
#endif
    float4 world_pos = mul(float4(gs_pos.xyz, 1.0), transform);
    // float4 position = mul(world_pos, frame_constants.view_constants.world_to_clip);
    const uint vi = vertexID % 4;
    float2 bb = get_bounding_box(vPosition[vi], point_size);
    float4x4 view = frame_constants.view_constants.world_to_view;
    float4x4 proj = frame_constants.view_constants.view_to_clip;
    float3 p_view = mul(world_pos, view).xyz;
    if(p_view.z >= 0.001f)
    {
        output.colour = float4(0, 0, 0, 0);
        return output;
    }
    float4 p_hom = mul(float4(p_view, 1.0), proj);
    float p_w = 1.0f / (p_hom.w + 0.0000001f);
    float4 p_proj = p_hom * p_w;
    if (p_proj.z <= 0.0 || p_proj.z >= 1.0 ) 
    {
        output.colour = float4(0, 0, 0, 0);
        return output;
    }
    float2 quad = p_proj.xy + bb.xy;
    output.position = float4(quad, p_proj.zw);
    output.colour = float4(read_splat_color(buf_id, instanceID), 1.0f);
    return output;
}