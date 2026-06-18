#include "inc/frame_constants.hlsl"
#include "inc/mesh_draw.hlsl"

[[vk::binding(0)]] StructuredBuffer<MeshDrawGpuData> mesh_draws;
[[vk::binding(1)]] StructuredBuffer<InstanceTransform> instance_transforms_dyn;

struct IndirectDrawArgsInstanced {
    uint vertex_count_per_instance;
    uint instance_count;
    uint start_vertex_location;
    uint start_instance_location;
};

[[vk::binding(2)]] RWStructuredBuffer<IndirectDrawArgsInstanced> indirect_args;
[[vk::binding(3)]] RWStructuredBuffer<uint> indirect_count;

[[vk::binding(4)]] cbuffer _ {
    uint draw_count;
    uint3 _padding;
};

float3 transform_point(row_major float3x4 transform, float3 position)
{
    return mul(transform, float4(position, 1.0)).xyz;
}

float4 world_to_clip(float3 world_position)
{
    float4 view_position = mul(float4(world_position, 1.0), frame_constants.view_constants.world_to_view);
    return mul(view_position, frame_constants.view_constants.view_to_sample);
}

bool aabb_visible(float3 bounds_min, float3 bounds_max, row_major float3x4 transform)
{
    bool outside_left = true;
    bool outside_right = true;
    bool outside_bottom = true;
    bool outside_top = true;
    bool outside_near = true;
    bool outside_far = true;

    [unroll]
    for (uint corner = 0; corner < 8; ++corner)
    {
        float3 local_position = float3(
            (corner & 1) ? bounds_max.x : bounds_min.x,
            (corner & 2) ? bounds_max.y : bounds_min.y,
            (corner & 4) ? bounds_max.z : bounds_min.z);
        float4 clip_position = world_to_clip(transform_point(transform, local_position));

        outside_left = outside_left && (clip_position.x < -clip_position.w);
        outside_right = outside_right && (clip_position.x > clip_position.w);
        outside_bottom = outside_bottom && (clip_position.y < -clip_position.w);
        outside_top = outside_top && (clip_position.y > clip_position.w);
        outside_near = outside_near && (clip_position.z < 0.0);
        outside_far = outside_far && (clip_position.z > clip_position.w);
    }

    return !(outside_left || outside_right || outside_bottom || outside_top || outside_near || outside_far);
}

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    const uint draw_id = dispatch_id.x;
    if (draw_id == 0)
    {
        indirect_count[0] = draw_count;
    }

    if (draw_id >= draw_count)
    {
        return;
    }

    MeshDrawGpuData draw = mesh_draws[draw_id];
    InstanceTransform instance_transform = instance_transforms_dyn[draw.mesh_instance_id];
    const bool visible = aabb_visible(draw.bounds_min.xyz, draw.bounds_max.xyz, instance_transform.current);

    IndirectDrawArgsInstanced args;
    args.vertex_count_per_instance = draw.index_count;
    args.instance_count = visible ? 1 : 0;
    args.start_vertex_location = 0;
    args.start_instance_location = draw_id;
    indirect_args[draw_id] = args;
}
