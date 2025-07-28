#include "../inc/bindless_textures.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/uv.hlsl"
#include "../inc/blue_noise.hlsl"
#include "../inc/math.hlsl"

[[vk::binding(0)]] StructuredBuffer<uint> image_mask;
[[vk::binding(1)]] Texture2D<float3> uv2pos_map;
[[vk::binding(2)]] RWStructuredBuffer<int> output_buffer;
[[vk::binding(3)]] cbuffer _ {
    int surface_width;
    int surface_height;
    int paint_tex_width;
    int paint_tex_height;
    float4x4 transform;
};

[numthreads(8, 8, 1)]
void main(in uint3 px: SV_DispatchThreadID) {
    if(px.x >= paint_tex_width || px.y >= paint_tex_height) {
        return;
    }
    float2 uv = float2(px.xy) / float2(paint_tex_width, paint_tex_height);

    float3 pos = uv2pos_map[px.xy];
    float4 world_pos = mul(float4(pos, 1.0), transform);
    // float4 position = mul(world_pos, frame_constants.view_constants.world_to_clip);
    float3 clip_pos = position_world_to_clip(world_pos.xyz);
    float2 screen_uv = cs_to_uv(clip_pos.xy);
    int2 screen_pos = screen_uv * float2(surface_width, surface_height);
    int pixel_id = screen_pos.y * surface_width + screen_pos.x;
    if (pixel_id < 0 || pixel_id >= surface_width * surface_height) {
        return;
    }
    int mask = image_mask[pixel_id];
    if (mask <= 0) {
        return;
    }
    int2 paint_tex_size = int2(paint_tex_width, paint_tex_height);
    int paint_tex_index = px.y * paint_tex_size.x + px.x;
    output_buffer[paint_tex_index] = 1;
}