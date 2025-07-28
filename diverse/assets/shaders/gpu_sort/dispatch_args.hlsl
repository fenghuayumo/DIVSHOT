#define PART_SIZE       3840U       //size of a partition tile

[[vk::binding(0)]] RWStructuredBuffer<uint4> g_ArgsBuffer;
[[vk::binding(1)]] StructuredBuffer<uint4> g_CountBuffer;

uint GetCount()
{
    return g_CountBuffer[0].x;
}
#define GROUP_SIZE 64
[numthreads(1, 1, 1)]
void main()
{
    uint num_groups = (GetCount() + GROUP_SIZE - 1) / GROUP_SIZE;

    const uint cnt = g_CountBuffer[0].x;
    uint threadBlock = (cnt + PART_SIZE - 1) / PART_SIZE;
    g_ArgsBuffer[0] = uint4(threadBlock * 128, 1, 1, 0);
    g_ArgsBuffer[1] = uint4(threadBlock * 256, 1, 1, 0);
}