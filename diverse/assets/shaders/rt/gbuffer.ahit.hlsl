#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/mesh.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/rt.hlsl"

#define GBUFFERPAYLOAD_ANYHIT 1
#include "anyhit_common.hlsl"

[shader("anyhit")]
void main(inout GbufferRayPayload payload: SV_RayPayload, in RayHitAttrib attrib: SV_IntersectionAttributes) {
    uint geometry_offset = InstanceID();
    float opacity = eval_non_opacity_material(payload, attrib.bary, geometry_offset + GeometryIndex());

    if( opacity < 0.5) IgnoreHit();
}
