#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
// #include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"


[[vk::binding(0)]] cbuffer _ {
    float4 color;
    uint width;
    uint height;
    float alphaCutoff;
    uint pad;
};
[[vk::binding(1)]] Texture2D<float4> outlineTexture;
[[vk::binding(2)]] RWTexture2D<float4> color_image;

[numthreads(8, 8, 1)]
void main(in uint2 px: SV_DispatchThreadID) {
    if (px.x >= width || px.y >= height) {
        return;
    }
    // skip solid pixels
    float4 outlineColor = outlineTexture[px];
    if (outlineColor.a > alphaCutoff) {
        return;
    }

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            if ((x != 0) && (y != 0) && outlineTexture[px + int2(x, y)].a > alphaCutoff) {
                color_image[px] = float4(color.xyz, outlineColor.a);
                return ;
            }
        }
    }

    return;
}