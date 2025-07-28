
#define NUM_BITS_PER_PASS   4
#define NUM_BINS            (1 << NUM_BITS_PER_PASS)
#define GROUP_SIZE          256
#define KEYS_PER_THREAD     4
#define KEYS_PER_GROUP (GROUP_SIZE * KEYS_PER_THREAD)
#define USE_ARGS 1
[[vk::binding(0)]] RWStructuredBuffer<uint> g_InputKeys;
[[vk::binding(1)]] RWStructuredBuffer<uint> g_OutputKeys;
[[vk::binding(2)]] RWStructuredBuffer<uint> g_GroupHistograms;
#if USE_ARGS
[[vk::binding(3)]] RWStructuredBuffer<uint4> g_CountBuffer;
[[vk::binding(4)]] cbuffer _ {
    uint g_pass;
    uint g_Bitshift;
};
#else
[[vk::binding(3)]] cbuffer _ {
    uint g_Count;
    uint g_Bitshift;
};
#endif

groupshared uint lds_keys[GROUP_SIZE];
groupshared uint lds_scratch[GROUP_SIZE];
groupshared uint lds_histogram[NUM_BINS];
groupshared uint lds_scanned_histogram[NUM_BINS];

uint GetCount()
{
#if USE_ARGS
    return g_CountBuffer[g_pass].x;
#else
    return g_Count;
#endif
}

uint GetOffset()
{
#if USE_ARGS
    return g_CountBuffer[g_pass].y;
#else
    return 0;
#endif
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(in uint gidx: SV_DispatchThreadID,
                  in uint lidx: SV_GroupThreadID,
                  in uint bidx: SV_GroupID)
{
    if(lidx < NUM_BINS)
        lds_histogram[lidx] = 0;
    GroupMemoryBarrierWithGroupSync();

    for(uint i = 0; i < KEYS_PER_THREAD; ++i)
    {
        uint key_index = gidx * KEYS_PER_THREAD + i;
        if (key_index >= GetCount()) break; // out of bounds
        uint bin_index = (g_InputKeys[key_index + GetOffset()] >> g_Bitshift) & 0xFu;
        InterlockedAdd(lds_histogram[bin_index], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    if(lidx < NUM_BINS)
    {
        uint num_blocks = (GetCount() + KEYS_PER_GROUP - 1) / KEYS_PER_GROUP;
        g_GroupHistograms[num_blocks * lidx + bidx] = lds_histogram[lidx];
    }
}