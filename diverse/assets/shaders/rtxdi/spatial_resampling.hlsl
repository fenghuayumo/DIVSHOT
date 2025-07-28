
#ifndef RTXDI_DI_SPATIAL_RESAMPLING_HLSLI
#define RTXDI_DI_SPATIAL_RESAMPLING_HLSLI

#include "rtxdi_parameters.hlsl"
#include "pairwise_streaming.hlsl"
#include "checker_board.hlsl"

#ifndef RTXDI_NEIGHBOR_OFFSETS_BUFFER
#error "RTXDI_NEIGHBOR_OFFSETS_BUFFER must be defined to point to a Buffer<float2> type resource"
#endif

// This macro can be defined in the including shader file to reduce code bloat
// and/or remove ray tracing calls from temporal and spatial resampling shaders
// if bias correction is not necessary.
#ifndef RTXDI_ALLOWED_BIAS_CORRECTION
#define RTXDI_ALLOWED_BIAS_CORRECTION RTXDI_BIAS_CORRECTION_RAY_TRACED
#endif

// A structure that groups the application-provided settings for spatial resampling.
struct RTXDI_DISpatialResamplingParameters
{
    // The index of the reservoir buffer to pull the spatial samples from.
    uint sourceBufferIndex;
    
    // Number of neighbor pixels considered for resampling (1-32)
    // Some of the may be skipped if they fail the surface similarity test.
    uint numSamples;

    // Number of neighbor pixels considered when there is not enough history data (1-32)
    // Setting this parameter equal or lower than `numSpatialSamples` effectively
    // disables the disocclusion boost.
    uint numDisocclusionBoostSamples;

    // Disocclusion boost is activated when the current reservoir's M value
    // is less than targetHistoryLength.
    uint targetHistoryLength;

    // Controls the bias correction math for spatial reuse. Depending on the setting, it can add
    // some shader cost and one approximate shadow ray *per every spatial sample* per pixel 
    // (or per two pixels if checkerboard sampling is enabled).
    uint biasCorrectionMode;

    // Screen-space radius for spatial resampling, measured in pixels.
    float samplingRadius;

    // Surface depth similarity threshold for spatial reuse.
    // See 'RTXDI_TemporalResamplingParameters::depthThreshold' for more information.
    float depthThreshold;

    // Surface normal similarity threshold for spatial reuse.
    // See 'RTXDI_TemporalResamplingParameters::normalThreshold' for more information.
    float normalThreshold;

    // Enables the comparison of surface materials before taking a surface into resampling.
    bool enableMaterialSimilarityTest;

    // Prevents samples which are from the current frame or have no reasonable temporal history merged being spread to neighbors
    bool discountNaiveSamples;
};

// Spatial resampling pass, using pairwise MIS.  
// Inputs and outputs equivalent to RTXDI_SpatialResampling(), but only uses pairwise MIS.
// Can call this directly, or call RTXDI_SpatialResampling() with sparams.biasCorrectionMode 
// set to RTXDI_BIAS_CORRECTION_PAIRWISE, which simply calls this function.
RTXDI_DIReservoir RTXDI_DISpatialResamplingWithPairwiseMIS(
    uint2 pixelPosition,
    RAB_Surface centerSurface,
    RTXDI_DIReservoir centerSample,
    inout RAB_RandomSamplerState rng,
    RTXDI_RuntimeParameters params,
    RTXDI_ReservoirBufferParameters reservoirParams,
    RTXDI_DISpatialResamplingParameters sparams,
    inout RAB_LightSample selectedLightSample)
{
    // Initialize the output reservoir
    RTXDI_DIReservoir state = RTXDI_EmptyDIReservoir();
    state.canonicalWeight = 0.0f;

    // How many spatial samples to use?  
    uint numSpatialSamples = (centerSample.M < sparams.targetHistoryLength)
        ? max(sparams.numDisocclusionBoostSamples, sparams.numSamples)
        : sparams.numSamples;

    // Walk the specified number of neighbors, resampling using RIS
    uint startIdx = uint(RAB_GetNextRandom(rng) * params.neighborOffsetMask);
    uint validSpatialSamples = 0;
    uint i;
    for (i = 0; i < numSpatialSamples; ++i)
    {
        // Get screen-space location of neighbor
        uint sampleIdx = (startIdx + i) & params.neighborOffsetMask;
        int2 spatialOffset = int2(float2(RTXDI_NEIGHBOR_OFFSETS_BUFFER[sampleIdx].xy) * sparams.samplingRadius);
        int2 idx = int2(pixelPosition)+spatialOffset;
        idx = RAB_ClampSamplePositionIntoView(idx, false);

        RTXDI_ActivateCheckerboardPixel(idx, false, params.activeCheckerboardField);

        RAB_Surface neighborSurface = RAB_GetGBufferSurface(idx, false);

        // Check for surface / G-buffer matches between the canonical sample and this neighbor
        if (!RAB_IsSurfaceValid(neighborSurface))
            continue;

        if (!RTXDI_IsValidNeighbor(RAB_GetSurfaceNormal(centerSurface), RAB_GetSurfaceNormal(neighborSurface),
            RAB_GetSurfaceLinearDepth(centerSurface), RAB_GetSurfaceLinearDepth(neighborSurface),
            sparams.normalThreshold, sparams.depthThreshold))
            continue;

        if (sparams.enableMaterialSimilarityTest && !RAB_AreMaterialsSimilar(RAB_GetMaterial(centerSurface), RAB_GetMaterial(neighborSurface)))
            continue;

        // The surfaces are similar enough so we *can* reuse a neighbor from this pixel, so load it.
        RTXDI_DIReservoir neighborSample = RTXDI_LoadDIReservoir(reservoirParams,
            RTXDI_PixelPosToReservoirPos(idx, params.activeCheckerboardField), sparams.sourceBufferIndex);
        neighborSample.spatialDistance += spatialOffset;

        if (RTXDI_IsValidDIReservoir(neighborSample))
        {
            if (sparams.discountNaiveSamples && neighborSample.M <= RTXDI_NAIVE_SAMPLING_M_THRESHOLD)
                continue;
        }

        validSpatialSamples++;

        // If sample has weight 0 due to visibility (or etc), skip the expensive-ish MIS computations
        if (neighborSample.M <= 0) continue;

        // Stream this light through the reservoir using pairwise MIS
        RTXDI_StreamNeighborWithPairwiseMIS(state, RAB_GetNextRandom(rng),
            neighborSample, neighborSurface,   // The spatial neighbor
            centerSample, centerSurface,       // The canonical (center) sample
            numSpatialSamples);
    }

    // If we've seen no usable neighbor samples, set the weight of the central one to 1
    state.canonicalWeight = (validSpatialSamples <= 0) ? 1.0f : state.canonicalWeight;

    // Stream the canonical sample (i.e., from prior computations at this pixel in this frame) using pairwise MIS.
    RTXDI_StreamCanonicalWithPairwiseStep(state, RAB_GetNextRandom(rng), centerSample, centerSurface);

    RTXDI_FinalizeResampling(state, 1.0, float(max(1, validSpatialSamples)));

    // Return the selected light sample.  This is a redundant lookup and could be optimized away by storing
        // the selected sample from the stream steps above.
    selectedLightSample = RAB_SamplePolymorphicLight(
        RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(state), false),
        centerSurface, RTXDI_GetDIReservoirSampleUV(state));

    return state;
}


// Spatial resampling pass.
// Operates on the current frame G-buffer and its reservoirs.
// For each pixel, considers a number of its neighbors and, if their surfaces are 
// similar enough to the current pixel, combines their light reservoirs.
// Optionally, one visibility ray is traced for each neighbor being considered, to reduce bias.
// The selectedLightSample parameter is used to update and return the selected sample; it's optional,
// and it's safe to pass a null structure there and ignore the result.
RTXDI_DIReservoir RTXDI_DISpatialResampling(
    uint2 pixelPosition,
    RAB_Surface centerSurface,
    RTXDI_DIReservoir centerSample,
    inout RAB_RandomSamplerState rng,
    RTXDI_RuntimeParameters params,
    RTXDI_ReservoirBufferParameters reservoirParams,
    RTXDI_DISpatialResamplingParameters sparams,
    inout RAB_LightSample selectedLightSample)
{
    if (sparams.biasCorrectionMode == RTXDI_BIAS_CORRECTION_PAIRWISE)
    {
        return RTXDI_DISpatialResamplingWithPairwiseMIS(pixelPosition, centerSurface, 
            centerSample, rng, params, reservoirParams, sparams, selectedLightSample);
    }

    RTXDI_DIReservoir state = RTXDI_EmptyDIReservoir();

    // This is the weight we'll use (instead of 1/M) to make our estimate unbaised (see paper).
    float normalizationWeight = 1.0f;

    // Since we're using our bias correction scheme, we need to remember which light selection we made
    int selected = -1;

    RAB_LightInfo selectedLight = RAB_EmptyLightInfo();

    if (RTXDI_IsValidDIReservoir(centerSample))
    {
        selectedLight = RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(centerSample), false);
    }

    RTXDI_CombineDIReservoirs(state, centerSample, /* random = */ 0.5f, centerSample.targetPdf);

    uint startIdx = uint(RAB_GetNextRandom(rng) * params.neighborOffsetMask);
    
    uint i;
    uint numSpatialSamples = sparams.numSamples;
    if(centerSample.M < sparams.targetHistoryLength)
        numSpatialSamples = max(sparams.numDisocclusionBoostSamples, numSpatialSamples);

    // Clamp the sample count at 32 to make sure we can keep the neighbor mask in an uint (cachedResult)
    numSpatialSamples = min(numSpatialSamples, 32);

    // We loop through neighbors twice.  Cache the validity / edge-stopping function
    //   results for the 2nd time through.
    uint cachedResult = 0;

    // Walk the specified number of neighbors, resampling using RIS
    for (i = 0; i < numSpatialSamples; ++i)
    {
        // Get screen-space location of neighbor
        uint sampleIdx = (startIdx + i) & params.neighborOffsetMask;
        int2 spatialOffset = int2(float2(RTXDI_NEIGHBOR_OFFSETS_BUFFER[sampleIdx].xy) * sparams.samplingRadius);
        int2 idx = int2(pixelPosition) + spatialOffset;

        idx = RAB_ClampSamplePositionIntoView(idx, false);

        RTXDI_ActivateCheckerboardPixel(idx, false, params.activeCheckerboardField);

        RAB_Surface neighborSurface = RAB_GetGBufferSurface(idx, false);

        if (!RAB_IsSurfaceValid(neighborSurface))
            continue;

        if (!RTXDI_IsValidNeighbor(RAB_GetSurfaceNormal(centerSurface), RAB_GetSurfaceNormal(neighborSurface), 
            RAB_GetSurfaceLinearDepth(centerSurface), RAB_GetSurfaceLinearDepth(neighborSurface), 
            sparams.normalThreshold, sparams.depthThreshold))
            continue;

        if (sparams.enableMaterialSimilarityTest && !RAB_AreMaterialsSimilar(RAB_GetMaterial(centerSurface), RAB_GetMaterial(neighborSurface)))
            continue;

        uint2 neighborReservoirPos = RTXDI_PixelPosToReservoirPos(idx, params.activeCheckerboardField);

        RTXDI_DIReservoir neighborSample = RTXDI_LoadDIReservoir(reservoirParams,
            neighborReservoirPos, sparams.sourceBufferIndex);
        neighborSample.spatialDistance += spatialOffset;

        cachedResult |= (1u << uint(i));

        RAB_LightInfo candidateLight = RAB_EmptyLightInfo();

        // Load that neighbor's RIS state, do resampling
        float neighborWeight = 0;
        RAB_LightSample candidateLightSample = RAB_EmptyLightSample();
        if (RTXDI_IsValidDIReservoir(neighborSample))
        {   
            if (sparams.discountNaiveSamples && neighborSample.M <= RTXDI_NAIVE_SAMPLING_M_THRESHOLD)
                continue;

            candidateLight = RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(neighborSample), false);
            
            candidateLightSample = RAB_SamplePolymorphicLight(
                candidateLight, centerSurface, RTXDI_GetDIReservoirSampleUV(neighborSample));
            
            neighborWeight = RAB_GetLightSampleTargetPdfForSurface(candidateLightSample, centerSurface);
        }
        
        if (RTXDI_CombineDIReservoirs(state, neighborSample, RAB_GetNextRandom(rng), neighborWeight))
        {
            selected = int(i);
            selectedLight = candidateLight;
            selectedLightSample = candidateLightSample;
        }
    }

    if (RTXDI_IsValidDIReservoir(state))
    {
#if RTXDI_ALLOWED_BIAS_CORRECTION >= RTXDI_BIAS_CORRECTION_BASIC
        if (sparams.biasCorrectionMode >= RTXDI_BIAS_CORRECTION_BASIC)
        {
            // Compute the unbiased normalization term (instead of using 1/M)
            float pi = state.targetPdf;
            float piSum = state.targetPdf * centerSample.M;

            // To do this, we need to walk our neighbors again
            for (i = 0; i < numSpatialSamples; ++i)
            {
                // If we skipped this neighbor above, do so again.
                if ((cachedResult & (1u << uint(i))) == 0) continue;

                uint sampleIdx = (startIdx + i) & params.neighborOffsetMask;

                // Get the screen-space location of our neighbor
                int2 idx = int2(pixelPosition) + int2(float2(RTXDI_NEIGHBOR_OFFSETS_BUFFER[sampleIdx].xy) * sparams.samplingRadius);

                idx = RAB_ClampSamplePositionIntoView(idx, false);

                RTXDI_ActivateCheckerboardPixel(idx, false, params.activeCheckerboardField);

                // Load our neighbor's G-buffer
                RAB_Surface neighborSurface = RAB_GetGBufferSurface(idx, false);
                
                // Get the PDF of the sample RIS selected in the first loop, above, *at this neighbor* 
                const RAB_LightSample selectedSampleAtNeighbor = RAB_SamplePolymorphicLight(
                    selectedLight, neighborSurface, RTXDI_GetDIReservoirSampleUV(state));

                float ps = RAB_GetLightSampleTargetPdfForSurface(selectedSampleAtNeighbor, neighborSurface);

#if RTXDI_ALLOWED_BIAS_CORRECTION >= RTXDI_BIAS_CORRECTION_RAY_TRACED
                if (sparams.biasCorrectionMode == RTXDI_BIAS_CORRECTION_RAY_TRACED && ps > 0)
                {
                    if (!RAB_GetConservativeVisibility(neighborSurface, selectedSampleAtNeighbor))
                    {
                        ps = 0;
                    }
                }
#endif

                uint2 neighborReservoirPos = RTXDI_PixelPosToReservoirPos(idx, params.activeCheckerboardField);

                RTXDI_DIReservoir neighborSample = RTXDI_LoadDIReservoir(reservoirParams,
                    neighborReservoirPos, sparams.sourceBufferIndex);

                // Select this sample for the (normalization) numerator if this particular neighbor pixel
                //     was the one we selected via RIS in the first loop, above.
                pi = selected == i ? ps : pi;

                // Add to the sums of weights for the (normalization) denominator
                piSum += ps * neighborSample.M;
            }

            // Use "MIS-like" normalization
            RTXDI_FinalizeResampling(state, pi, piSum);
        }
        else
#endif
        {
            RTXDI_FinalizeResampling(state, 1.0, state.M);
        }
    }

    return state;
}

#endif // RTXDI_DI_SPATIAL_RESAMPLING_HLSLI
