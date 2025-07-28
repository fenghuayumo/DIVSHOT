[[vk::binding(0)]] RWStructuredBuffer<uint> b_globalHist; // buffer holding global device level offsets

// Clear the global histogram, as we will be adding to it atomically
[numthreads(1024, 1, 1)]
void main(int3 id: SV_DispatchThreadID)
{
    b_globalHist[id.x] = 0;
}
