#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

[[vk::binding(0)]] cbuffer _ {
    uint num_gaussians;
    uint max_gaussians;
    uint buf_id;
    uint padding;
};
[numthreads(GROUP_SIZE, 1, 1)]
void main(uint2 px: SV_DispatchThreadID) {

    uint global_id = px.x;
    if (global_id < num_gaussians) {
        Gaussian gs = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
        uint state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
        float4 gs_position = float4(gs.position.xyz,asfloat(state));
        uint gs_state = asuint(gs_position.w);
        uint op_state = NORMAL_STATE;
        gs_state = setOpState(gs_state, op_state);
        gs_position.w = asfloat(gs_state);
        bindless_splat_state[buf_id].Store<uint>(global_id * sizeof(uint), gs_state);
    }
}