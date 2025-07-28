#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "gaussian_common.hlsl"

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

    float4 locked_color;
    float4 select_color;
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
    uint gs_state = bindless_splat_state[buf_id].Load<uint>(instanceID * sizeof(uint));
    uint op_state = getOpState(gs_state);
#if SPLAT_EDIT
    uint transform_index = getTransformIndex(gs_state);
    float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
    transform = mul(transform, model_matrix);
#else
    float4x4 transform = model_matrix;
#endif
    
    float4 world_pos = mul(float4(gaussian.position.xyz, 1.0), transform);
    float4 position = mul(world_pos, frame_constants.view_constants.world_to_clip);
    // output.positionVS = input.position;

    // if (op_state == NORMAL_STATE)
    //     output.colour = float4(0, 0, 1, 1);
    // else if (op_state == SELECT_STATE)
    //     output.colour = float4(1, 0, 0, 1);
    // else if (op_state == DELETE_STATE)
    //     output.colour = float4(0, 0, 0, 0);
    if (op_state & DELETE_STATE)
        output.colour = float4(0, 0, 0, 0);
    else if (op_state & HIDE_STATE)
        output.colour = float4(locked_color.xyz, 1);
    else if (op_state & SELECT_STATE)
        output.colour = float4(select_color.xyz, 1);
    else
        output.colour = float4(0, 0, 1, 1);
    const uint vi = vertexID % 4;
    float2 bb = get_bounding_box(vPosition[vi], point_size);
    float p_w = 1.0f / (position.w + 0.0000001f);
    float4 p_proj = position * p_w;
    float2 quad = p_proj.xy + bb.xy;
    output.position = float4(quad, p_proj.zw);
    return output;
}