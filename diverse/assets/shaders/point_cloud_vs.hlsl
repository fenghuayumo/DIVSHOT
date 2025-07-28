#include "inc/frame_constants.hlsl"
#include "inc/bindless.hlsl"


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

struct PointCloudVertex
{
    float3 position;
    uint color;
};

VS_OUTPUT main(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VS_OUTPUT output;
    float4x4 transform = model_matrix;

    PointCloudVertex pcd = bindless_point_buf[buf_id].Load<PointCloudVertex>(instanceID * sizeof(PointCloudVertex));
    float4 world_pos = mul(float4(pcd.position.xyz, 1.0), transform);
    float4 position = mul(world_pos, frame_constants.view_constants.world_to_clip);

    const uint vi = vertexID % 4;
    float2 bb = get_bounding_box(vPosition[vi], point_size);
    float p_w = 1.0f / (position.w + 0.0000001f);
    float4 p_proj = position * p_w;
    float2 quad = p_proj.xy + bb.xy;
    output.position = float4(quad, p_proj.zw);
    // Decode color from packed uint
    float3 color = float3((pcd.color & 0x00FF0000) >> 16, (pcd.color & 0x0000FF00) >> 8, (pcd.color & 0x000000FF)) / 255.0f;
    output.colour = float4(color,1.0);
    return output;
}