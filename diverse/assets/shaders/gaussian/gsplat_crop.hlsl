#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"

#include "gaussian_common.hlsl"

// #define MODE_CENTERS 0
// #define MODE_RINGS   1
#define BOX_CROP 0
#define SPHERE_CROP 1
// [[vk::push_constant]]
// struct {
//     uint instance_id;
//     uint mesh_index;
// } push_constants;

[[vk::binding(0)]] cbuffer _ {
    int surface_width;
    int surface_height;
    uint num_gaussians;
    uint max_gaussians;
    float4x4 model_matrix;

    uint    buf_id;
    int     crop_num;
    uint2   padding;

    float4 crop_min[8];
    float4 crop_max[8];
    uint   crop_type[8];
};
[[vk::binding(1)]] ByteAddressBuffer splat_pos_buf;

float2 clip_to_uv(float2 cs) {
    return cs * float2(0.5, -0.5) + float2(0.5, 0.5);
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint2 px: SV_DispatchThreadID) {

    uint global_id = px.x;
    if (global_id < num_gaussians) {
#if SCAFFOLD_GS
        float4 gs_position = splat_pos_buf.Load<float4>(global_id * sizeof(float4));
#else
        Gaussian gs = bindless_gaussians(buf_id).Load<Gaussian>(global_id * sizeof(Gaussian));
        uint state = bindless_splat_state[buf_id].Load<uint>(global_id * sizeof(uint));
        float4 gs_position = float4(gs.position.xyz, asfloat(state));
#endif
        uint gs_state = asuint(gs_position.w);
        uint op_state = getOpState(gs_state);
        float4x4 view = frame_constants.view_constants.world_to_view;
        float4x4 proj = frame_constants.view_constants.view_to_clip;
        float4 world_pos = float4(gs_position.xyz, 1);
#if SPLAT_EDIT
        uint transform_index = getTransformIndex(gs_state);
        float4x4 transform = get_transform(bindless_splat_transform(buf_id), transform_index);
        transform = mul(transform, model_matrix);
#else
        float4x4 transform = model_matrix;
#endif
        world_pos.xyz = mul(world_pos, transform).xyz;
        float3 p_view = mul(world_pos, view).xyz;
        // perspective projection
        float4 p_hom = mul(float4(p_view, 1.0), proj);
        float p_w = 1.0f / (p_hom.w + 0.0000001f);
        float3 p_proj = { p_hom.x * p_w, p_hom.y * p_w, p_hom.z * p_w };
        if (p_proj.z <= 0.0 || p_proj.z >= 1.0) return;
        float2 point_image = clip_to_uv(p_proj.xy) * float2(surface_width, surface_height);

        // point_image.y = surface_height - point_image.y - 1;
        // whether the point is in the crop bouding box region, where min is crop_min, max is crop_max
        bool in_crop = false;
        for (uint i = 0; i < crop_num; i++) {
            int croptype = (crop_type[i] >> 16) & 0xFF;
            if (croptype == BOX_CROP) {
                in_crop = !(world_pos.x > crop_min[i].x && world_pos.x < crop_max[i].x &&
                            world_pos.y > crop_min[i].y && world_pos.y < crop_max[i].y &&
                            world_pos.z > crop_min[i].z && world_pos.z < crop_max[i].z);
            } else if (croptype == SPHERE_CROP) { // whether the point is in the crop bouding sphere region, where center is crop_sphere_center, radius is crop_sphere_radius
                in_crop = length(world_pos.xyz - crop_min[i].xyz) >= crop_max[i].x;
            }
        }
        if (in_crop) {
            op_state |= DELETE_STATE;
        } else {
            op_state = NORMAL_STATE;
        }
        gs_state = setOpState(gs_state, op_state);
        gs_position.w = asfloat(gs_state);
#if SCAFFOLD_GS
        splat_pos_buf.Store<float4>(global_id * sizeof(float4), gs_position);
#else
        bindless_splat_state[buf_id].Store<uint>(global_id * sizeof(uint), gs_state);
#endif
    }
}