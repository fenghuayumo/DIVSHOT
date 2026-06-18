#include "../inc/binding.hlsl"
#define PART_SIZE       3840U       //size of a partition tile

DS_RESOURCE(0) RWByteAddressBuffer g_ArgsBuffer;
DS_RESOURCE(1) ByteAddressBuffer g_CountBuffer;

uint GetCount()
{
    return g_CountBuffer.Load(0);
}
#define GROUP_SIZE 64
[numthreads(1, 1, 1)]
void main()
{
    const uint cnt = g_CountBuffer.Load(0);
    uint threadBlock = (cnt + PART_SIZE - 1) / PART_SIZE;
    // VkDispatchIndirectCommand is workgroup count (not thread count)
    // upsweep has 128 threads per workgroup, downsweep has 256
    g_ArgsBuffer.Store3(0, uint3(threadBlock, 1, 1));   // workgroup count for upsweep
    g_ArgsBuffer.Store3(12, uint3(threadBlock, 1, 1));  // workgroup count for downsweep
}