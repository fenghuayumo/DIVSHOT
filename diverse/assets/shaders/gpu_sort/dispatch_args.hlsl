#define PART_SIZE       3840U       //size of a partition tile

[[vk::binding(0)]] RWByteAddressBuffer g_ArgsBuffer;
[[vk::binding(1)]] ByteAddressBuffer g_CountBuffer;

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
    g_ArgsBuffer.Store4(0, uint4(threadBlock * 128, 1, 1, 0));
    g_ArgsBuffer.Store4(16, uint4(threadBlock * 256, 1, 1, 0));
}