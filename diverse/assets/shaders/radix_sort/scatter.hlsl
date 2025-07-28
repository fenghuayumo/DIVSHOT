#define NUM_BITS_PER_PASS   4
#define NUM_BINS            (1 << NUM_BITS_PER_PASS)
#define GROUP_SIZE          256
#define KEYS_PER_THREAD     4
#define KEYS_PER_GROUP (GROUP_SIZE * KEYS_PER_THREAD)
#define SORT_VALUES 1

[[vk::binding(0)]] RWStructuredBuffer<uint> g_InputKeys;
[[vk::binding(1)]] RWStructuredBuffer<uint> g_OutputKeys;
[[vk::binding(2)]] RWStructuredBuffer<uint> g_InputValues;
[[vk::binding(3)]] RWStructuredBuffer<uint> g_OutputValues;
[[vk::binding(4)]] RWStructuredBuffer<uint> g_GroupHistograms;
#if USE_ARGS
[[vk::binding(5)]] RWStructuredBuffer<uint4> g_CountBuffer;
uint g_Bitshift;
#else
[[vk::binding(5)]] cbuffer _ {
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
    return g_CountBuffer[0];
#else
    return g_Count;
#endif
}


uint GroupScan(in uint key, in uint lidx)
{
    lds_keys[lidx] = key;
    GroupMemoryBarrierWithGroupSync();

    uint stride;
    for(stride = 1; stride < GROUP_SIZE; stride <<= 1)
    {
        if(lidx < GROUP_SIZE / (2 * stride))
            lds_keys[2 * (lidx + 1) * stride - 1] += lds_keys[(2 * lidx + 1) * stride - 1];
        GroupMemoryBarrierWithGroupSync();
    }

    if(lidx == 0)
        lds_keys[GROUP_SIZE - 1] = 0;
    GroupMemoryBarrierWithGroupSync();

    for(stride = (GROUP_SIZE >> 1); stride > 0; stride >>= 1)
    {
        if(lidx < GROUP_SIZE / (2 * stride))
        {
            uint tmp = lds_keys[(2 * lidx + 1) * stride - 1];
            lds_keys[(2 * lidx + 1) * stride - 1] = lds_keys[2 * (lidx + 1) * stride - 1];
            lds_keys[2 * (lidx + 1) * stride - 1] += tmp;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    return lds_keys[lidx];
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(in uint lidx : SV_GroupThreadID,
             in uint bidx : SV_GroupID)
{
    uint block_start_index = bidx * GROUP_SIZE * KEYS_PER_THREAD;
    uint num_blocks = (GetCount() + KEYS_PER_GROUP - 1) / KEYS_PER_GROUP;
    if(lidx < NUM_BINS)
        lds_scanned_histogram[lidx] = g_GroupHistograms[num_blocks * lidx + bidx];

    for(uint i = 0; i < KEYS_PER_THREAD; ++i)
    {
        if(lidx < NUM_BINS)
            lds_histogram[lidx] = 0;

        uint key_index = block_start_index + i * GROUP_SIZE + lidx;
        uint key = (key_index < GetCount() ? g_InputKeys[key_index] : 0xFFFFFFFFu);
#ifdef SORT_VALUES
        uint value = (key_index < GetCount() ? g_InputValues[key_index] : 0);
#endif

        for(uint shift = 0; shift < NUM_BITS_PER_PASS; shift += 2)
        {
            uint bin_index = ((key >> g_Bitshift) >> shift) & 0x3;

            uint local_histogram = 1 << (bin_index * 8);
            uint local_histogram_scanned = GroupScan(local_histogram, lidx);
            if(lidx == (GROUP_SIZE - 1))
                lds_scratch[0] = local_histogram_scanned + local_histogram;
            GroupMemoryBarrierWithGroupSync();

            local_histogram = lds_scratch[0];
            local_histogram = (local_histogram << 8) +
                              (local_histogram << 16) +
                              (local_histogram << 24);
            local_histogram_scanned += local_histogram;

            uint offset = (local_histogram_scanned >> (bin_index * 8)) & 0xFFu;
            lds_keys[offset] = key;
            GroupMemoryBarrierWithGroupSync();
            key = lds_keys[lidx];

#ifdef SORT_VALUES
            GroupMemoryBarrierWithGroupSync();
            lds_keys[offset] = value;
            GroupMemoryBarrierWithGroupSync();
            value = lds_keys[lidx];
            GroupMemoryBarrierWithGroupSync();
#endif
        }

        uint bin_index = (key >> g_Bitshift) & 0xFu;
        InterlockedAdd(lds_histogram[bin_index], 1);
        GroupMemoryBarrierWithGroupSync();

        uint histogram_value = GroupScan(lidx < NUM_BINS ? lds_histogram[lidx] : 0, lidx);
        if(lidx < NUM_BINS)
            lds_scratch[lidx] = histogram_value;

        uint global_offset = lds_scanned_histogram[bin_index];
        GroupMemoryBarrierWithGroupSync();
        uint local_offset = lidx - lds_scratch[bin_index];

        if(global_offset + local_offset < GetCount())
        {
            g_OutputKeys[global_offset + local_offset] = key;
#ifdef SORT_VALUES
            g_OutputValues[global_offset + local_offset] = value;
#endif
        }
        GroupMemoryBarrierWithGroupSync();

        if(lidx < NUM_BINS)
            lds_scanned_histogram[lidx] += lds_histogram[lidx];
    }
}