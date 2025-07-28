

#define GROUP_SIZE 256
#define KEYS_PER_THREAD 4
#define KEYS_PER_GROUP (GROUP_SIZE * KEYS_PER_THREAD)

[[vk::binding(0)]] RWStructuredBuffer<uint> g_CountBuffer;
[[vk::binding(1)]] RWStructuredBuffer<uint4> g_Args1Buffer;
[[vk::binding(2)]] RWStructuredBuffer<uint4> g_Args2Buffer;
[[vk::binding(3)]] RWStructuredBuffer<uint> g_Count1Buffer;
[[vk::binding(4)]] RWStructuredBuffer<uint> g_Count2Buffer;

[numthreads(1, 1, 1)]
void main()
{
    uint num_keys = g_CountBuffer[0].x;
    uint num_groups_level_1 = (num_keys + KEYS_PER_GROUP - 1) / KEYS_PER_GROUP;
    uint num_groups_level_2 = (num_groups_level_1 + KEYS_PER_GROUP - 1) / KEYS_PER_GROUP;
    g_Args1Buffer[0] = uint4(num_groups_level_1, 1, 1, 0);
    g_Args2Buffer[0] = uint4(num_groups_level_2, 1, 1, 0);
    g_Count1Buffer[0] = num_groups_level_1;
    g_Count2Buffer[0] = num_groups_level_2;
}