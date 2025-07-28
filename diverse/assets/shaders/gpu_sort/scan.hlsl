#include "sort_common.hlsl"

// Scan along the spine of the upsweep
[numthreads(SCAN_DIM, 1, 1)]
void main(uint3 gtid: SV_GroupThreadID, uint3 gid: SV_GroupID)
{
    if (WaveGetLaneCount() >= 16)
    {
        uint aggregate = 0;
        const uint laneMask = WaveGetLaneCount() - 1;
        const uint circularLaneShift = WaveGetLaneIndex() + 1 & laneMask;
        const uint partionsEnd = getThreadBlocks() / SCAN_DIM * SCAN_DIM;
        const uint offset = gid.x * getThreadBlocks();
        uint i = gtid.x;
        for (; i < partionsEnd; i += SCAN_DIM)
        {
            g_scan[gtid.x] = b_passHist[i + offset];
            g_scan[gtid.x] += WavePrefixSum(g_scan[gtid.x]);
            GroupMemoryBarrierWithGroupSync();

            if (gtid.x < SCAN_DIM / WaveGetLaneCount())
            {
                g_scan[(gtid.x + 1) * WaveGetLaneCount() - 1] +=
                    WavePrefixSum(g_scan[(gtid.x + 1) * WaveGetLaneCount() - 1]);
            }
            GroupMemoryBarrierWithGroupSync();

            b_passHist[circularLaneShift + (i & ~laneMask) + offset] =
                (WaveGetLaneIndex() != laneMask ? g_scan[gtid.x] : 0) +
                (gtid.x >= WaveGetLaneCount() ?
                WaveReadLaneAt(g_scan[gtid.x - 1], 0) : 0) +
                aggregate;

            aggregate += g_scan[SCAN_DIM - 1];
            GroupMemoryBarrierWithGroupSync();
        }

        // partial
        if (i < getThreadBlocks())
            g_scan[gtid.x] = b_passHist[offset + i];
        g_scan[gtid.x] += WavePrefixSum(g_scan[gtid.x]);
        GroupMemoryBarrierWithGroupSync();

        if (gtid.x < SCAN_DIM / WaveGetLaneCount())
        {
            g_scan[(gtid.x + 1) * WaveGetLaneCount() - 1] +=
                WavePrefixSum(g_scan[(gtid.x + 1) * WaveGetLaneCount() - 1]);
        }
        GroupMemoryBarrierWithGroupSync();

        const uint index = circularLaneShift + (i & ~laneMask);
        if (index < getThreadBlocks())
        {
            b_passHist[index + offset] = (WaveGetLaneIndex() != laneMask ? g_scan[gtid.x] : 0) +
                                         (gtid.x >= WaveGetLaneCount() ? g_scan[(gtid.x & ~laneMask) - 1] : 0) + aggregate;
        }
    }

    if (WaveGetLaneCount() < 16)
    {
        uint aggregate = 0;
        const uint partitions = getThreadBlocks() / SCAN_DIM;
        const uint deviceOffset = gid.x * getThreadBlocks();
        const uint laneLog = countbits(WaveGetLaneCount() - 1);
        const uint circularLaneShift = WaveGetLaneIndex() + 1 &
                                       WaveGetLaneCount() - 1;

        uint k = 0;
        for (; k < partitions; ++k)
        {
            g_scan[gtid.x] = b_passHist[gtid.x + k * SCAN_DIM + deviceOffset];
            g_scan[gtid.x] += WavePrefixSum(g_scan[gtid.x]);

            if (gtid.x < WaveGetLaneCount())
            {
                b_passHist[circularLaneShift + k * SCAN_DIM + deviceOffset] =
                    (circularLaneShift ? g_scan[gtid.x] : 0) + aggregate;
            }
            GroupMemoryBarrierWithGroupSync();

            uint offset = laneLog;
            uint j = WaveGetLaneCount();
            for (; j < (SCAN_DIM >> 1); j <<= laneLog)
            {
                for (uint i = gtid.x; i < (SCAN_DIM >> offset); i += SCAN_DIM)
                {
                    g_scan[((i + 1) << offset) - 1] +=
                        WavePrefixSum(g_scan[((i + 1) << offset) - 1]);
                }
                GroupMemoryBarrierWithGroupSync();

                if ((gtid.x & ((j << laneLog) - 1)) >= j)
                {
                    if (gtid.x < (j << laneLog))
                    {
                        b_passHist[gtid.x + k * SCAN_DIM + deviceOffset] =
                            WaveReadLaneAt(g_scan[((gtid.x >> offset) << offset) - 1], 0) +
                            ((gtid.x & (j - 1)) ? g_scan[gtid.x - 1] : 0) + aggregate;
                    }
                    else
                    {
                        if ((gtid.x + 1) & (j - 1))
                        {
                            g_scan[gtid.x] +=
                                WaveReadLaneAt(g_scan[((gtid.x >> offset) << offset) - 1], 0);
                        }
                    }
                }
                offset += laneLog;
            }
            GroupMemoryBarrierWithGroupSync();

            for (uint i = gtid.x + j; i < SCAN_DIM; i += SCAN_DIM)
            {
                b_passHist[i + k * SCAN_DIM + deviceOffset] =
                    WaveReadLaneAt(g_scan[((i >> offset) << offset) - 1], 0) +
                    ((i & (j - 1)) ? g_scan[i - 1] : 0) + aggregate;
            }

            aggregate += WaveReadLaneAt(g_scan[SCAN_DIM - 1], 0) +
                         WaveReadLaneAt(g_scan[(((SCAN_DIM - 1) >> offset) << offset) - 1], 0);
            GroupMemoryBarrierWithGroupSync();
        }

        // partial
        const uint finalPartSize = getThreadBlocks() - k * SCAN_DIM;
        if (gtid.x < finalPartSize)
        {
            g_scan[gtid.x] = b_passHist[gtid.x + k * SCAN_DIM + deviceOffset];
            g_scan[gtid.x] += WavePrefixSum(g_scan[gtid.x]);
        }

        if (gtid.x < WaveGetLaneCount() && circularLaneShift < finalPartSize)
        {
            b_passHist[circularLaneShift + k * SCAN_DIM + deviceOffset] =
                (circularLaneShift ? g_scan[gtid.x] : 0) + aggregate;
        }
        GroupMemoryBarrierWithGroupSync();

        uint offset = laneLog;
        for (uint j = WaveGetLaneCount(); j < finalPartSize; j <<= laneLog)
        {
            for (uint i = gtid.x; i < (finalPartSize >> offset); i += SCAN_DIM)
            {
                g_scan[((i + 1) << offset) - 1] +=
                    WavePrefixSum(g_scan[((i + 1) << offset) - 1]);
            }
            GroupMemoryBarrierWithGroupSync();

            if ((gtid.x & ((j << laneLog) - 1)) >= j && gtid.x < finalPartSize)
            {
                if (gtid.x < (j << laneLog))
                {
                    b_passHist[gtid.x + k * SCAN_DIM + deviceOffset] =
                        WaveReadLaneAt(g_scan[((gtid.x >> offset) << offset) - 1], 0) +
                        ((gtid.x & (j - 1)) ? g_scan[gtid.x - 1] : 0) + aggregate;
                }
                else
                {
                    if ((gtid.x + 1) & (j - 1))
                    {
                        g_scan[gtid.x] +=
                            WaveReadLaneAt(g_scan[((gtid.x >> offset) << offset) - 1], 0);
                    }
                }
            }
            offset += laneLog;
        }
    }
}
