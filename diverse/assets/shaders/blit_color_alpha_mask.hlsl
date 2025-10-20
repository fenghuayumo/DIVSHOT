[[vk::binding(0)]] Texture2D<float4> src_tex;
[[vk::binding(1)]] RWTexture2D<float4> dst_tex;

[[vk::binding(2)]] cbuffer _ {
    uint alpha;
    uint3 padding;
};

[numthreads(8, 8, 1)]
void main(in uint2 px: SV_DispatchThreadID) {
    float4 src_color = src_tex[px];
    // if(src_color.a > alpha)
    {
        dst_tex[px] = src_color;
        dst_tex[px].a = 1.0f;
    }
}