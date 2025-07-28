#define NUM_BITS_PER_PASS   4
#define NUM_BINS            (1 << NUM_BITS_PER_PASS)
#define GROUP_SIZE          256
#define KEYS_PER_THREAD     4
#define KEYS_PER_GROUP      (GROUP_SIZE * KEYS_PER_THREAD)

#define USE_ARGS 1

[[vk::binding(0)]] RWStructuredBuffer<uint4> g_ArgsBuffer;
[[vk::binding(1)]] RWStructuredBuffer<uint> g_ScanCountBuffer;
[[vk::binding(2)]] RWStructuredBuffer<uint4> g_CountBuffer;

[[vk::binding(3)]] cbuffer _ {
    uint g_pass;
};


uint GetCount()
{
    return g_CountBuffer[g_pass].x;
}

[numthreads(1, 1, 1)]
void main()
{
    uint num_groups = (GetCount() + KEYS_PER_GROUP - 1) / KEYS_PER_GROUP;
    g_ArgsBuffer[0] = uint4(num_groups, 1, 1, 0);
    g_ScanCountBuffer[0] = NUM_BINS * num_groups;
};