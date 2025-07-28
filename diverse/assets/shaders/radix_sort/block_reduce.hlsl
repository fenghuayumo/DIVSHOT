
#define GROUP_SIZE      256
#define KEYS_PER_THREAD 4
#define KEYS_PER_GROUP (GROUP_SIZE * KEYS_PER_THREAD)

#define OpType_Min 0
#define OpType_Max 1
#define OpType_Sum 2
#define op_type OpType_Sum
#define data_type_str uint
#define identity_str (op_type == OpType_Min ? 4294967295u : 0)

[[vk::binding(0)]] RWStructuredBuffer<data_type_str> g_InputKeys;
[[vk::binding(1)]] RWStructuredBuffer<data_type_str> g_PartialResults;

#if USE_ARGS
[[vk::binding(2)]] RWStructuredBuffer<uint> g_CountBuffer;
#else
[[vk::binding(2)]] cbuffer _ {
    uint g_Count;
};
#endif


groupshared data_type_str lds_keys[GROUP_SIZE];
groupshared data_type_str lds_loads[KEYS_PER_THREAD][GROUP_SIZE];
uint GetCount()
{
#if USE_ARGS
    return g_CountBuffer[0];
#else
    return g_Count;
#endif
}

data_type_str Op(in data_type_str lhs, in data_type_str rhs)
{
    return (lhs + rhs);
}

data_type_str GroupScan(in data_type_str key, in uint lidx)
{
    lds_keys[lidx] = key;
    GroupMemoryBarrierWithGroupSync();

    uint stride;
    for(stride = 1; stride < GROUP_SIZE; stride <<= 1)
    {
        if(lidx < GROUP_SIZE / (2 * stride))
        {
            lds_keys[2 * (lidx + 1) * stride - 1] = Op(lds_keys[2 * (lidx + 1) * stride - 1],
                                                       lds_keys[(2 * lidx + 1) * stride - 1]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (lidx == 0)
        lds_keys[GROUP_SIZE - 1] = identity_str;
    GroupMemoryBarrierWithGroupSync();
    for(stride = (GROUP_SIZE >> 1); stride > 0; stride >>= 1)
    {
        if(lidx < GROUP_SIZE / (2 * stride))
        {
            data_type_str tmp = lds_keys[(2 * lidx + 1) * stride - 1];
            lds_keys[(2 * lidx + 1) * stride - 1] = lds_keys[2 * (lidx + 1) * stride - 1];
            lds_keys[2 * (lidx + 1) * stride - 1] = Op(lds_keys[2 * (lidx + 1) * stride - 1], tmp);
        }
        GroupMemoryBarrierWithGroupSync();
    }
    return lds_keys[lidx];
}

data_type_str GroupReduce(in data_type_str key, in uint lidx)
{
    lds_keys[lidx] = key;
    GroupMemoryBarrierWithGroupSync();

    for(uint stride = (GROUP_SIZE >> 1); stride > 0; stride >>= 1)
    {
        if(lidx < stride)
        {
            lds_keys[lidx] = Op(lds_keys[lidx], lds_keys[lidx + stride]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    return lds_keys[0];
}



[numthreads(GROUP_SIZE, 1, 1)]
void main(in uint gidx : SV_DispatchThreadID, in uint lidx : SV_GroupThreadID, in uint bidx : SV_GroupID)
{
    data_type_str thread_sum = identity_str;
    uint range_begin = bidx * GROUP_SIZE * KEYS_PER_THREAD, count = GetCount(); 
    for(uint i = 0; i < KEYS_PER_THREAD; ++i)
    {
        uint load_index = range_begin + i * GROUP_SIZE + lidx;
        thread_sum = Op(load_index < count ? g_InputKeys[load_index] : identity_str, thread_sum);
    }
    thread_sum = GroupReduce(thread_sum, lidx);
    if (lidx == 0) 
    {
        g_PartialResults[bidx] = thread_sum;
    }
}