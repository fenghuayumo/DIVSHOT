#include "sort_common.hlsl"

[numthreads(US_DIM, 1, 1)]
void main(uint3 gtid: SV_GroupThreadID, uint3 gid: SV_GroupID)
{
    // clear shared memory
    const uint histsEnd = RADIX * 2;
    for (uint i = gtid.x; i < histsEnd; i += US_DIM)
        g_us[i] = 0;
    GroupMemoryBarrierWithGroupSync();

    // histogram, 64 threads to a histogram
    const uint histOffset = gtid.x / 64 * RADIX;
    const uint partitionEnd = gid.x == getThreadBlocks() - 1 ?
        getNumKeys() : (gid.x + 1) * PART_SIZE;
    for (uint i = gtid.x + gid.x * PART_SIZE; i < partitionEnd; i += US_DIM)
        InterlockedAdd(g_us[ExtractDigit(b_sort[i]) + histOffset], 1);
    GroupMemoryBarrierWithGroupSync();

    // reduce and pass to tile histogram
    for (uint i = gtid.x; i < RADIX; i += US_DIM)
    {
        g_us[i] += g_us[i + RADIX];
        b_passHist[i * getThreadBlocks() + gid.x] = g_us[i];
    }

    // Larger 16 or greater can perform a more elegant scan because 16 * 16 = 256
    if (WaveGetLaneCount() >= 16)
    {
        for (uint i = gtid.x; i < RADIX; i += US_DIM)
            g_us[i] += WavePrefixSum(g_us[i]);
        GroupMemoryBarrierWithGroupSync();

        if (gtid.x < (RADIX / WaveGetLaneCount()))
        {
            g_us[(gtid.x + 1) * WaveGetLaneCount() - 1] +=
                WavePrefixSum(g_us[(gtid.x + 1) * WaveGetLaneCount() - 1]);
        }
        GroupMemoryBarrierWithGroupSync();

        // atomically add to global histogram
        const uint globalHistOffset = GlobalHistOffset();
        const uint laneMask = WaveGetLaneCount() - 1;
        const uint circularLaneShift = WaveGetLaneIndex() + 1 & laneMask;
        for (uint i = gtid.x; i < RADIX; i += US_DIM)
        {
            const uint index = circularLaneShift + (i & ~laneMask);
            InterlockedAdd(b_globalHist[index + globalHistOffset],
                           (WaveGetLaneIndex() != laneMask ? g_us[i] : 0) +
                               (i >= WaveGetLaneCount() ? WaveReadLaneAt(g_us[i - 1], 0) : 0));
        }
    }

    // Exclusive Brent-Kung with fused upsweep downsweep
    if (WaveGetLaneCount() < 16)
    {
        const uint globalHistOffset = GlobalHistOffset();
        for (uint i = gtid.x; i < RADIX; i += US_DIM)
            g_us[i] += WavePrefixSum(g_us[i]);

        if (gtid.x < WaveGetLaneCount())
        {
            const uint circularLaneShift = WaveGetLaneIndex() + 1 &
                                           WaveGetLaneCount() - 1;
            InterlockedAdd(b_globalHist[circularLaneShift + globalHistOffset],
                           circularLaneShift ? g_us[gtid.x] : 0);
        }
        GroupMemoryBarrierWithGroupSync();

        const uint laneLog = countbits(WaveGetLaneCount() - 1);
        uint offset = laneLog;
        uint j = WaveGetLaneCount();
        for (; j < (RADIX >> 1); j <<= laneLog)
        {
            for (uint i = gtid.x; i < (RADIX >> offset); i += US_DIM)
            {
                g_us[((i + 1) << offset) - 1] +=
                    WavePrefixSum(g_us[((i + 1) << offset) - 1]);
            }
            GroupMemoryBarrierWithGroupSync();

            for (uint i = gtid.x + j; i < RADIX; i += US_DIM)
            {
                if ((i & ((j << laneLog) - 1)) >= j)
                {
                    if (i < (j << laneLog))
                    {
                        InterlockedAdd(b_globalHist[i + globalHistOffset],
                                       WaveReadLaneAt(g_us[((i >> offset) << offset) - 1], 0) +
                                           ((i & (j - 1)) ? g_us[i - 1] : 0));
                    }
                    else
                    {
                        if ((i + 1) & (j - 1))
                        {
                            g_us[i] +=
                                WaveReadLaneAt(g_us[((i >> offset) << offset) - 1], 0);
                        }
                    }
                }
            }
            offset += laneLog;
        }
        GroupMemoryBarrierWithGroupSync();

        for (uint i = gtid.x + j; i < RADIX; i += US_DIM)
        {
            InterlockedAdd(b_globalHist[i + globalHistOffset],
                           WaveReadLaneAt(g_us[((i >> offset) << offset) - 1], 0) +
                               ((i & (j - 1)) ? g_us[i - 1] : 0));
        }
    }
}