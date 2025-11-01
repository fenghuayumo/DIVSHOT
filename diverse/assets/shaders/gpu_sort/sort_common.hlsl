// General macros
#define PART_SIZE       3840U       //size of a partition tile

#define US_DIM          128U        //The number of threads in a Upsweep threadblock
#define SCAN_DIM        128U        //The number of threads in a Scan threadblock
#define DS_DIM          256U        //The number of threads in a Downsweep threadblock

#define RADIX           256U        //Number of digit bins
#define RADIX_MASK      255U        //Mask of digit bins
#define RADIX_LOG       8U          //log2(RADIX)

#define HALF_RADIX      128U        //For smaller waves where bit packing is necessary
#define HALF_MASK       127U        // '' 

//For the downsweep kernels
#define DS_KEYS_PER_THREAD  15U     //The number of keys per thread in a Downsweep Threadblock
#define MAX_DS_SMEM         4096U   //shared memory for downsweep kernel

[[vk::binding(0)]] cbuffer cbParallelSort {
    uint e_numKeys;
    uint e_radixShift;
    uint e_threadBlocks;
    uint padding0;
};

[[vk::binding(1)]] RWStructuredBuffer<uint> b_sort; // buffer to be sorted
[[vk::binding(2)]] RWStructuredBuffer<uint> b_alt; // payload buffer
[[vk::binding(3)]] RWStructuredBuffer<uint> b_sortPayload; // double buffer
[[vk::binding(4)]] RWStructuredBuffer<uint> b_altPayload; // double buffer payload
[[vk::binding(5)]] RWStructuredBuffer<uint> b_globalHist; // buffer holding global device level offsets
                                       // for each digit during a binning pass
[[vk::binding(6)]] RWStructuredBuffer<uint> b_passHist;        //buffer used to store device level offsets for 
                                            //each partition tile for each digit during a binning pass
#ifdef GPU_SORT_INDIRECT
[[vk::binding(7)]] ByteAddressBuffer b_count;; // buffer to be sorted
#endif
groupshared uint g_us[RADIX * 2];           //Shared memory for upsweep kernel
groupshared uint g_scan[SCAN_DIM];          //Shared memory for the scan kernel
groupshared uint g_ds[MAX_DS_SMEM];         //Shared memory for the downsweep kernel

inline uint getNumKeys()
{
#ifdef GPU_SORT_INDIRECT
    const uint cnt = b_count.Load(0);
    return cnt;
#else
    return e_numKeys;
#endif
}

inline uint getThreadBlocks()
{
#ifdef GPU_SORT_INDIRECT
    const uint cnt = b_count.Load(0);
    return (cnt + PART_SIZE - 1) / PART_SIZE;
#else
    return e_threadBlocks;
#endif
}

inline uint getWaveIndex(uint gtid)
{
    return gtid / WaveGetLaneCount();
}

inline uint ExtractDigit(uint key)
{
    return key >> e_radixShift & RADIX_MASK;
}

inline uint ExtractPackedIndex(uint key)
{
    return key >> (e_radixShift + 1) & HALF_MASK;
}

inline uint ExtractPackedShift(uint key)
{
    return (key >> e_radixShift & 1) ? 16 : 0;
}

inline uint ExtractPackedValue(uint packed, uint key)
{
    return packed >> ExtractPackedShift(key) & 0xffff;
}

inline uint SubPartSizeWGE16()
{
    return DS_KEYS_PER_THREAD * WaveGetLaneCount();
}

inline uint SharedOffsetWGE16(uint gtid)
{
    return WaveGetLaneIndex() + getWaveIndex(gtid) * SubPartSizeWGE16();
}

inline uint DeviceOffsetWGE16(uint gtid, uint gid)
{
    return SharedOffsetWGE16(gtid) + gid * PART_SIZE;
}

inline uint SubPartSizeWLT16(uint _serialIterations)
{
    return DS_KEYS_PER_THREAD * WaveGetLaneCount() * _serialIterations;
}

inline uint SharedOffsetWLT16(uint gtid, uint _serialIterations)
{
    return WaveGetLaneIndex() +
        (getWaveIndex(gtid) / _serialIterations * SubPartSizeWLT16(_serialIterations)) +
        (getWaveIndex(gtid) % _serialIterations * WaveGetLaneCount());
}

inline uint DeviceOffsetWLT16(uint gtid, uint gid, uint _serialIterations)
{
    return SharedOffsetWLT16(gtid, _serialIterations) + gid * PART_SIZE;
}

inline uint GlobalHistOffset()
{
    return e_radixShift << 5;
}

inline uint WaveHistsSizeWGE16()
{
    return DS_DIM / WaveGetLaneCount() * RADIX;
}

inline uint WaveHistsSizeWLT16()
{
    return MAX_DS_SMEM;
}
