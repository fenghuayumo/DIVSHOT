#include "sort_common.hlsl"

[numthreads(DS_DIM, 1, 1)]
void main(uint3 gtid: SV_GroupThreadID, uint3 gid: SV_GroupID)
{
    if (gid.x < getThreadBlocks() - 1)
    {
        uint keys[DS_KEYS_PER_THREAD];
        uint offsets[DS_KEYS_PER_THREAD];

        if (WaveGetLaneCount() >= 16)
        {
            // Load keys into registers
            [unroll]
            for (uint i = 0, t = DeviceOffsetWGE16(gtid.x, gid.x);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount())
            {
                keys[i] = b_sort[t];
            }

            // Clear histogram memory
            for (uint i = gtid.x; i < WaveHistsSizeWGE16(); i += DS_DIM)
                g_ds[i] = 0;
            GroupMemoryBarrierWithGroupSync();

            // Warp Level Multisplit
            const uint waveParts = (WaveGetLaneCount() + 31) / 32;
            [unroll]
            for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
            {
                uint4 waveFlags = (WaveGetLaneCount() & 31) ?
                    (1U << WaveGetLaneCount()) - 1 : 0xffffffff;

                [unroll]
                for (uint k = 0; k < RADIX_LOG; ++k)
                {
                    const bool t = keys[i] >> (k + e_radixShift) & 1;
                    const uint4 ballot = WaveActiveBallot(t);
                    for (uint wavePart = 0; wavePart < waveParts; ++wavePart)
                        waveFlags[wavePart] &= (t ? 0 : 0xffffffff) ^ ballot[wavePart];
                }

                uint bits = 0;
                for (uint wavePart = 0; wavePart < waveParts; ++wavePart)
                {
                    if (WaveGetLaneIndex() >= wavePart * 32)
                    {
                        const uint ltMask = WaveGetLaneIndex() >= (wavePart + 1) * 32 ?
                            0xffffffff : (1U << (WaveGetLaneIndex() & 31)) - 1;
                        bits += countbits(waveFlags[wavePart] & ltMask);
                    }
                }

                const uint index = ExtractDigit(keys[i]) + (getWaveIndex(gtid.x) * RADIX);
                offsets[i] = g_ds[index] + bits;

                GroupMemoryBarrierWithGroupSync();
                if (bits == 0)
                {
                    for (uint wavePart = 0; wavePart < waveParts; ++wavePart)
                        g_ds[index] += countbits(waveFlags[wavePart]);
                }
                GroupMemoryBarrierWithGroupSync();
            }

            // inclusive/exclusive prefix sum up the histograms
            // followed by exclusive prefix sum across the reductions
            uint reduction = g_ds[gtid.x];
            for (uint i = gtid.x + RADIX; i < WaveHistsSizeWGE16(); i += RADIX)
            {
                reduction += g_ds[i];
                g_ds[i] = reduction - g_ds[i];
            }

            reduction += WavePrefixSum(reduction);
            GroupMemoryBarrierWithGroupSync();

            const uint laneMask = WaveGetLaneCount() - 1;
            g_ds[((WaveGetLaneIndex() + 1) & laneMask) + (gtid.x & ~laneMask)] = reduction;
            GroupMemoryBarrierWithGroupSync();

            if (gtid.x < RADIX / WaveGetLaneCount())
            {
                g_ds[gtid.x * WaveGetLaneCount()] =
                    WavePrefixSum(g_ds[gtid.x * WaveGetLaneCount()]);
            }
            GroupMemoryBarrierWithGroupSync();

            if (WaveGetLaneIndex())
                g_ds[gtid.x] += WaveReadLaneAt(g_ds[gtid.x - 1], 1);
            GroupMemoryBarrierWithGroupSync();

            // Update offsets
            if (gtid.x >= WaveGetLaneCount())
            {
                const uint t = getWaveIndex(gtid.x) * RADIX;
                [unroll]
                for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
                {
                    const uint t2 = ExtractDigit(keys[i]);
                    offsets[i] += g_ds[t2 + t] + g_ds[t2];
                }
            }
            else
            {
                [unroll]
                for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
                    offsets[i] += g_ds[ExtractDigit(keys[i])];
            }

            // take advantage of barrier
            const uint exclusiveWaveReduction = g_ds[gtid.x];
            GroupMemoryBarrierWithGroupSync();

            // scatter keys into shared memory
            for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
                g_ds[offsets[i]] = keys[i];

            g_ds[gtid.x + PART_SIZE] = b_globalHist[gtid.x + GlobalHistOffset()] +
                                       b_passHist[gtid.x * getThreadBlocks() + gid.x] - exclusiveWaveReduction;
            GroupMemoryBarrierWithGroupSync();

            [unroll]
            for (uint i = 0, t = SharedOffsetWGE16(gtid.x);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount())
            {
                keys[i] = g_ds[ExtractDigit(g_ds[t]) + PART_SIZE] + t;
                b_alt[keys[i]] = g_ds[t];
            }
            GroupMemoryBarrierWithGroupSync();

            [unroll]
            for (uint i = 0, t = DeviceOffsetWGE16(gtid.x, gid.x);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount())
            {
                g_ds[offsets[i]] = b_sortPayload[t];
            }
            GroupMemoryBarrierWithGroupSync();

            [unroll]
            for (uint i = 0, t = SharedOffsetWGE16(gtid.x);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount())
            {
                b_altPayload[keys[i]] = g_ds[t];
            }
        }

        if (WaveGetLaneCount() < 16)
        {
            const uint serialIterations = (DS_DIM / WaveGetLaneCount() + 31) / 32;

            // Load keys into registers
            [unroll]
            for (uint i = 0, t = DeviceOffsetWLT16(gtid.x, gid.x, serialIterations);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount() * serialIterations)
            {
                keys[i] = b_sort[t];
            }

            // clear shared memory
            for (uint i = gtid.x; i < WaveHistsSizeWLT16(); i += DS_DIM)
                g_ds[i] = 0;
            GroupMemoryBarrierWithGroupSync();

            const uint ltMask = (1U << WaveGetLaneIndex()) - 1;
            [unroll]
            for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
            {
                uint waveFlag = (1U << WaveGetLaneCount()) - 1; // for full agnostic add ternary and uint4

                [unroll]
                for (uint k = 0; k < RADIX_LOG; ++k)
                {
                    const bool t = keys[i] >> (k + e_radixShift) & 1;
                    waveFlag &= (t ? 0 : 0xffffffff) ^ (uint)WaveActiveBallot(t);
                }

                uint bits = countbits(waveFlag & ltMask);
                const uint index = ExtractPackedIndex(keys[i]) +
                                   (getWaveIndex(gtid.x) / serialIterations * HALF_RADIX);

                for (uint k = 0; k < serialIterations; ++k)
                {
                    if (getWaveIndex(gtid.x) % serialIterations == k)
                        offsets[i] = ExtractPackedValue(g_ds[index], keys[i]) + bits;

                    GroupMemoryBarrierWithGroupSync();
                    if (getWaveIndex(gtid.x) % serialIterations == k && bits == 0)
                    {
                        InterlockedAdd(g_ds[index],
                                       countbits(waveFlag) << ExtractPackedShift(keys[i]));
                    }
                    GroupMemoryBarrierWithGroupSync();
                }
            }

            // inclusive/exclusive prefix sum up the histograms,
            // use a blelloch scan for in place exclusive
            uint reduction;
            if (gtid.x < HALF_RADIX)
            {
                reduction = g_ds[gtid.x];
                for (uint i = gtid.x + HALF_RADIX; i < WaveHistsSizeWLT16(); i += HALF_RADIX)
                {
                    reduction += g_ds[i];
                    g_ds[i] = reduction - g_ds[i];
                }
                g_ds[gtid.x] = reduction + (reduction << 16);
            }

            uint shift = 1;
            for (uint j = RADIX >> 2; j > 0; j >>= 1)
            {
                GroupMemoryBarrierWithGroupSync();
                for (uint i = gtid.x; i < j; i += DS_DIM)
                {
                    g_ds[((((i << 1) + 2) << shift) - 1) >> 1] +=
                        g_ds[((((i << 1) + 1) << shift) - 1) >> 1] & 0xffff0000;
                }
                shift++;
            }
            GroupMemoryBarrierWithGroupSync();

            if (gtid.x == 0)
                g_ds[HALF_RADIX - 1] &= 0xffff;

            for (uint j = 1; j < RADIX >> 1; j <<= 1)
            {
                --shift;
                GroupMemoryBarrierWithGroupSync();
                for (uint i = gtid.x; i < j; i += DS_DIM)
                {
                    const uint t = ((((i << 1) + 1) << shift) - 1) >> 1;
                    const uint t2 = ((((i << 1) + 2) << shift) - 1) >> 1;
                    const uint t3 = g_ds[t];
                    g_ds[t] = (g_ds[t] & 0xffff) | (g_ds[t2] & 0xffff0000);
                    g_ds[t2] += t3 & 0xffff0000;
                }
            }

            GroupMemoryBarrierWithGroupSync();
            if (gtid.x < HALF_RADIX)
            {
                const uint t = g_ds[gtid.x];
                g_ds[gtid.x] = (t >> 16) + (t << 16) + (t & 0xffff0000);
            }
            GroupMemoryBarrierWithGroupSync();

            // Update offsets
            if (gtid.x >= WaveGetLaneCount() * serialIterations)
            {
                const uint t = getWaveIndex(gtid.x) / serialIterations * HALF_RADIX;
                [unroll]
                for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
                {
                    const uint t2 = ExtractPackedIndex(keys[i]);
                    offsets[i] += ExtractPackedValue(g_ds[t2 + t] + g_ds[t2], keys[i]);
                }
            }
            else
            {
                [unroll]
                for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
                    offsets[i] += ExtractPackedValue(g_ds[ExtractPackedIndex(keys[i])], keys[i]);
            }

            const uint exclusiveWaveReduction = g_ds[gtid.x >> 1] >> ((gtid.x & 1) ? 16 : 0) & 0xffff;
            GroupMemoryBarrierWithGroupSync();

            // scatter keys into shared memory
            for (uint i = 0; i < DS_KEYS_PER_THREAD; ++i)
                g_ds[offsets[i]] = keys[i];

            g_ds[gtid.x + PART_SIZE] = b_globalHist[gtid.x + GlobalHistOffset()] +
                                       b_passHist[gtid.x * getThreadBlocks() + gid.x] - exclusiveWaveReduction;
            GroupMemoryBarrierWithGroupSync();

            // scatter runs of keys into device memory,
            // store the scatter location in the key register to reuse for the payload
            [unroll]
            for (uint i = 0, t = SharedOffsetWLT16(gtid.x, serialIterations);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount() * serialIterations)
            {
                keys[i] = g_ds[ExtractDigit(g_ds[t]) + PART_SIZE] + t;
                b_alt[keys[i]] = g_ds[t];
            }
            GroupMemoryBarrierWithGroupSync();

            [unroll]
            for (uint i = 0, t = DeviceOffsetWLT16(gtid.x, gid.x, serialIterations);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount() * serialIterations)
            {
                g_ds[offsets[i]] = b_sortPayload[t];
            }
            GroupMemoryBarrierWithGroupSync();

            [unroll]
            for (uint i = 0, t = SharedOffsetWLT16(gtid.x, serialIterations);
                 i < DS_KEYS_PER_THREAD;
                 ++i, t += WaveGetLaneCount() * serialIterations)
            {
                b_altPayload[keys[i]] = g_ds[t];
            }
        }
    }

    // perform the sort on the final partition slightly differently
    // to handle input sizes not perfect multiples of the partition
    if (gid.x == getThreadBlocks() - 1)
    {
        // load the global and pass histogram values into shared memory
        if (gtid.x < RADIX)
        {
            g_ds[gtid.x] = b_globalHist[gtid.x + GlobalHistOffset()] +
                           b_passHist[gtid.x * getThreadBlocks() + gid.x];
        }
        GroupMemoryBarrierWithGroupSync();

        const uint waveParts = (WaveGetLaneCount() + 31) / 32;
        const uint partEnd = (getNumKeys() + DS_DIM - 1) / DS_DIM * DS_DIM;
        for (uint i = gtid.x + gid.x * PART_SIZE; i < partEnd; i += DS_DIM)
        {
            uint key;
            if (i < getNumKeys())
            {
                key = b_sort[i];
            }

            uint4 waveFlags = (WaveGetLaneCount() & 31) ?
                (1U << WaveGetLaneCount()) - 1 : 0xffffffff;
            uint offset;
            uint bits = 0;
            if (i < getNumKeys())
            {
                [unroll]
                for (uint k = 0; k < RADIX_LOG; ++k)
                {
                    const bool t = key >> (k + e_radixShift) & 1;
                    const uint4 ballot = WaveActiveBallot(t);
                    for (uint wavePart = 0; wavePart < waveParts; ++wavePart)
                        waveFlags[wavePart] &= (t ? 0 : 0xffffffff) ^ ballot[wavePart];
                }

                for (uint wavePart = 0; wavePart < waveParts; ++wavePart)
                {
                    if (WaveGetLaneIndex() >= wavePart * 32)
                    {
                        const uint ltMask = WaveGetLaneIndex() >= (wavePart + 1) * 32 ?
                            0xffffffff : (1U << (WaveGetLaneIndex() & 31)) - 1;
                        bits += countbits(waveFlags[wavePart] & ltMask);
                    }
                }
            }

            for (uint k = 0; k < DS_DIM / WaveGetLaneCount(); ++k)
            {
                if (getWaveIndex(gtid.x) == k && i < getNumKeys())
                    offset = g_ds[ExtractDigit(key)] + bits;
                GroupMemoryBarrierWithGroupSync();

                if (getWaveIndex(gtid.x) == k && i < getNumKeys() && bits == 0)
                {
                    for (uint wavePart = 0; wavePart < waveParts; ++wavePart)
                        g_ds[ExtractDigit(key)] += countbits(waveFlags[wavePart]);
                }
                GroupMemoryBarrierWithGroupSync();
            }

            if (i < getNumKeys())
            {
                b_alt[offset] = key;
                b_altPayload[offset] = b_sortPayload[i];
            }
        }
    }
}