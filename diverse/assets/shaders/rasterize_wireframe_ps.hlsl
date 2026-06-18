#include "inc/math.hlsl"
#include "inc/samplers.hlsl"
#include "inc/frame_constants.hlsl"
#include "inc/mesh.hlsl"
#include "inc/pack_unpack.hlsl"
#include "inc/bindless.hlsl"
#include "inc/gbuffer.hlsl"

struct PsOut {
    float4 color: SV_TARGET0;
};

PsOut main() {
    PsOut ps_out;
    ps_out.color =  float4(0.43,0.45,0.77,1.0);
    return ps_out;
}
