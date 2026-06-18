#include "../inc/binding.hlsl"

DS_RESOURCE(0) StructuredBuffer<uint> src_buffer;
DS_RESOURCE(1) RWStructuredBuffer<uint> dst_buffer;

[numthreads(64, 1, 1)]
void main(uint bin: SV_DispatchThreadID) {
    dst_buffer[bin] = src_buffer[bin];
}
