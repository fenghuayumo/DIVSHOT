[[vk::binding(0)]] RWTexture2D<float4> main_tex;
// [[vk::push_constant]]
// struct {
//     uint alpha;
// } push_constants;
[[vk::binding(1)]] cbuffer _ {
    uint alpha;
    uint3 padding;
};

[numthreads(8, 8, 1)]
void main(in uint2 px: SV_DispatchThreadID) {
    main_tex[px].a = alpha;
}