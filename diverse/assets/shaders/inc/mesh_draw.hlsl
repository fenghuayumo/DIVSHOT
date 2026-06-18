#ifndef MESH_DRAW_HLSL
#define MESH_DRAW_HLSL

struct InstanceTransform {
    row_major float3x4 current;
    row_major float3x4 previous;
};

struct MeshDrawGpuData {
    float4 bounds_min;
    float4 bounds_max;
    uint mesh_instance_id;
    uint material_id;
    uint mesh_id;
    uint index_count;
};

#endif
