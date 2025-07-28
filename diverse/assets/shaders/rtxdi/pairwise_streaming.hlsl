
#ifndef RTXDI_DI_PAIRWISE_STREAMING_HLSLI
#define RTXDI_DI_PAIRWISE_STREAMING_HLSLI

#include "reservoir.hlsl"

// A helper used for pairwise MIS computations.  This might be able to simplify code elsewhere, too.
float RTXDI_TargetPdfHelper(const RTXDI_DIReservoir lightReservoir, const RAB_Surface surface, bool priorFrame RTXDI_DEFAULT(false))
{
    RAB_LightSample lightSample = RAB_SamplePolymorphicLight(
        RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(lightReservoir), priorFrame),
        surface, RTXDI_GetDIReservoirSampleUV(lightReservoir));

    return RAB_GetLightSampleTargetPdfForSurface(lightSample, surface);
}

// "Pairwise MIS" is a MIS approach that is O(N) instead of O(N^2) for N estimators.  The idea is you know
// a canonical sample which is a known (pretty-)good estimator, but you'd still like to improve the result
// given multiple other candidate estimators.  You can do this in a pairwise fashion, MIS'ing between each
// candidate and the canonical sample.  RTXDI_StreamNeighborWithPairwiseMIS() is executed once for each 
// candidate, after which the MIS is completed by calling RTXDI_StreamCanonicalWithPairwiseStep() once for
// the canonical sample.
// See Chapter 9.1 of https://digitalcommons.dartmouth.edu/dissertations/77/, especially Eq 9.10 & Algo 8
bool RTXDI_StreamNeighborWithPairwiseMIS(inout RTXDI_DIReservoir reservoir,
    float random,
    const RTXDI_DIReservoir neighborReservoir,
    const RAB_Surface neighborSurface,
    const RTXDI_DIReservoir canonicalReservor,
    const RAB_Surface canonicalSurface,
    const uint numberOfNeighborsInStream)    // # neighbors streamed via pairwise MIS before streaming the canonical sample
{
    // Compute PDFs of the neighbor and cannonical light samples and surfaces in all permutations.
    // Note: First two must be computed this way.  Last two *should* be replacable by neighborReservoir.targetPdf
    // and canonicalReservor.targetPdf to reduce redundant computations, but there's a bug in that naive reuse.
    float neighborWeightAtCanonical = max(0.0f, RTXDI_TargetPdfHelper(neighborReservoir, canonicalSurface, false));
    float canonicalWeightAtNeighbor = max(0.0f, RTXDI_TargetPdfHelper(canonicalReservor, neighborSurface, false));
    float neighborWeightAtNeighbor = max(0.0f, RTXDI_TargetPdfHelper(neighborReservoir, neighborSurface, false));
    float canonicalWeightAtCanonical = max(0.0f, RTXDI_TargetPdfHelper(canonicalReservor, canonicalSurface, false));

    // Compute two pairwise MIS weights
    float w0 = RTXDI_PairwiseMisWeight(neighborWeightAtNeighbor, neighborWeightAtCanonical,
        neighborReservoir.M * numberOfNeighborsInStream, canonicalReservor.M);
    float w1 = RTXDI_PairwiseMisWeight(canonicalWeightAtNeighbor, canonicalWeightAtCanonical,
        neighborReservoir.M * numberOfNeighborsInStream, canonicalReservor.M);

    // Determine the effective M value when using pairwise MIS
    float M = neighborReservoir.M * min(
        RTXDI_MFactor(neighborWeightAtNeighbor, neighborWeightAtCanonical),
        RTXDI_MFactor(canonicalWeightAtNeighbor, canonicalWeightAtCanonical));

    // With pairwise MIS, we touch the canonical sample multiple times (but every other sample only once).  This 
    // with overweight the canonical sample; we track how much it is overweighted so we can renormalize to account
    // for this in the function RTXDI_StreamCanonicalWithPairwiseStep()
    reservoir.canonicalWeight += (1.0f - w1);

    // Go ahead and stream the neighbor sample through via RIS, appropriately weighted
    return RTXDI_InternalSimpleResample(reservoir, neighborReservoir, random,
        neighborWeightAtCanonical,
        neighborReservoir.weightSum * w0,
        M);
}

// Called to finish the process of doing pairwise MIS.  This function must be called after all required calls to
// RTXDI_StreamNeighborWithPairwiseMIS(), since pairwise MIS overweighs the canonical sample.  This function 
// compensates for this overweighting, but it can only happen after all neighbors have been processed.
bool RTXDI_StreamCanonicalWithPairwiseStep(inout RTXDI_DIReservoir reservoir,
    float random,
    const RTXDI_DIReservoir canonicalReservoir,
    const RAB_Surface canonicalSurface)
{
    return RTXDI_InternalSimpleResample(reservoir, canonicalReservoir, random,
        canonicalReservoir.targetPdf,
        canonicalReservoir.weightSum * reservoir.canonicalWeight,
        canonicalReservoir.M);
}

#endif // RTXDI_DI_PAIRWISE_STREAMING_HLSLI