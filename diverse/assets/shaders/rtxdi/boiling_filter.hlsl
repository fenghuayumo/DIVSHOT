#ifndef RTXDI_DI_BOILING_FILTER_HLSLI
#define RTXDI_DI_BOILING_FILTER_HLSLI

#include "reservoir.hlsl"

#ifdef RTXDI_ENABLE_BOILING_FILTER
// RTXDI_BOILING_FILTER_GROUP_SIZE must be defined - 16 is a reasonable value
#define RTXDI_BOILING_FILTER_MIN_LANE_COUNT 32

groupshared float s_weights[(RTXDI_BOILING_FILTER_GROUP_SIZE * RTXDI_BOILING_FILTER_GROUP_SIZE + RTXDI_BOILING_FILTER_MIN_LANE_COUNT - 1) / RTXDI_BOILING_FILTER_MIN_LANE_COUNT];
groupshared uint s_count[(RTXDI_BOILING_FILTER_GROUP_SIZE * RTXDI_BOILING_FILTER_GROUP_SIZE + RTXDI_BOILING_FILTER_MIN_LANE_COUNT - 1) / RTXDI_BOILING_FILTER_MIN_LANE_COUNT];

bool RTXDI_BoilingFilterInternal(
    uint2 LocalIndex,
    float filterStrength, // (0..1]
    float reservoirWeight)
{
    // Boiling happens when some highly unlikely light is discovered and it is relevant
    // for a large surface area around the pixel that discovered it. Then this light sample
    // starts to propagate to the neighborhood through spatiotemporal reuse, which looks like
    // a flash. We can detect such lights because their weight is significantly higher than
    // the weight of their neighbors. So, compute the average group weight and apply a threshold.

    float boilingFilterMultiplier = 10.f / clamp(filterStrength, 1e-6, 1.0) - 9.f;

    // Start with average nonzero weight within the wavefront
    float waveWeight = WaveActiveSum(reservoirWeight);
    uint waveCount = WaveActiveCountBits(reservoirWeight > 0);

    // Store the results of each wavefront into shared memory
    uint linearThreadIndex = LocalIndex.x + LocalIndex.y * RTXDI_BOILING_FILTER_GROUP_SIZE;
    uint waveIndex = linearThreadIndex / WaveGetLaneCount();

    if (WaveIsFirstLane())
    {
        s_weights[waveIndex] = waveWeight;
        s_count[waveIndex] = waveCount;
    }

    GroupMemoryBarrierWithGroupSync();

    // Reduce the per-wavefront averages into a global average using one wavefront
    if (linearThreadIndex < (RTXDI_BOILING_FILTER_GROUP_SIZE * RTXDI_BOILING_FILTER_GROUP_SIZE + WaveGetLaneCount() - 1) / WaveGetLaneCount())
    {
        waveWeight = s_weights[linearThreadIndex];
        waveCount = s_count[linearThreadIndex];

        waveWeight = WaveActiveSum(waveWeight);
        waveCount = WaveActiveSum(waveCount);

        if (linearThreadIndex == 0)
        {
            s_weights[0] = (waveCount > 0) ? (waveWeight / float(waveCount)) : 0.0;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Read the per-group average and apply the threshold
    float averageNonzeroWeight = s_weights[0];
    if (reservoirWeight > averageNonzeroWeight * boilingFilterMultiplier)
    {
        return true;
    }

    return false;
}

#endif // RTXDI_ENABLE_BOILING_FILTER

#ifdef RTXDI_ENABLE_BOILING_FILTER
// Boiling filter that should be applied at the end of the temporal resampling pass.
// Can be used inside the same shader that does temporal resampling if it's a compute shader,
// or in a separate pass if temporal resampling is a raygen shader.
// The filter analyzes the weights of all reservoirs in a thread group, and discards
// the reservoirs whose weights are very high, i.e. above a certain threshold.
void RTXDI_BoilingFilter(
    uint2 LocalIndex,
    float filterStrength, // (0..1]
    inout RTXDI_DIReservoir reservoir)
{
    if (RTXDI_BoilingFilterInternal(LocalIndex, filterStrength, reservoir.weightSum))
        reservoir = RTXDI_EmptyDIReservoir();
}
#endif // RTXDI_ENABLE_BOILING_FILTER

#endif
