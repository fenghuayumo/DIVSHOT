#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "gaussian_common.hlsl"
[[vk::binding(0)]] RWStructuredBuffer<uint> point_list_key_buffer;
[[vk::binding(1)]] RWStructuredBuffer<uint> point_list_value_buffer;
[[vk::binding(2)]] cbuffer _ {
    uint max_gaussians;
};

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint2 px: SV_DispatchThreadID) {

    if (px.x < max_gaussians) {
        point_list_key_buffer[px.x] = 0xffffffffu;
        point_list_value_buffer[px.x] = 0u;
    }
}

