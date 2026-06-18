#include "../inc/binding.hlsl"
// Prepare indirect dispatch arguments for GUT mesh shader
// Converts splat count to mesh shader workgroup count

DS_RESOURCE(0) StructuredBuffer<uint> count_buffer;
DS_RESOURCE(1) RWStructuredBuffer<uint> indirect_args;

DS_CBUFFER(2) cbuffer Constants {
    uint workgroup_size;
    uint3 padding;
};

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint splat_count = count_buffer[0];
    
    // Calculate number of workgroups needed
    // Each workgroup processes workgroup_size splats
    uint workgroup_count = (splat_count + workgroup_size - 1) / workgroup_size;
    
    // IndirectDrawMeshTasksArgs layout:
    // uint32_t group_count_x
    // uint32_t group_count_y  
    // uint32_t group_count_z
    indirect_args[0] = workgroup_count;  // group_count_x
    indirect_args[1] = 1;                 // group_count_y
    indirect_args[2] = 1;                 // group_count_z
    
    // Store splat count for mesh shader to read
    indirect_args[3] = splat_count;       // instanceCount (used by mesh shader)
}

