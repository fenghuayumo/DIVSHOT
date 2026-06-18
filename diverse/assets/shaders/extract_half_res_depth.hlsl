#include "inc/binding.hlsl"
#include "inc/frame_constants.hlsl"
#include "inc/pack_unpack.hlsl"

DS_RESOURCE(0) Texture2D<float> input_tex;
DS_RESOURCE(1) RWTexture2D<float> output_tex;
DS_CBUFFER(2) cbuffer _ {
    float4 input_tex_size;
    float4 output_tex_size;
};

[numthreads(8, 8, 1)]
void main(in int2 px : SV_DispatchThreadID) {
    const int2 src_px = px * 2 + HALFRES_SUBSAMPLE_OFFSET;

	output_tex[px] = input_tex[src_px];
}
