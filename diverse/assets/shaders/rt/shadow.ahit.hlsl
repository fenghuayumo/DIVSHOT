#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/mesh.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/rt.hlsl"

struct ShadowPayload {
    bool is_shadowed;
};
#include "anyhit_common.hlsl"

[shader("anyhit")]
void main(inout ShadowPayload payload : SV_RayPayload,in RayHitAttrib attrib: SV_IntersectionAttributes) {
    float opacity = eval_non_opacity_material(payload, attrib.bary, InstanceID());
    if( opacity > 0.5) AcceptHitAndEndSearch();
    else IgnoreHit();
    // if (opacity < 1 ) IgnoreHit();
}
