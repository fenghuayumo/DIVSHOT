
#include "rtxdi_bridege.hlsl"
#include "spatial_resampling.hlsl"

#if USE_RAY_QUERY
[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)]
void main(uint2 GlobalIndex : SV_DispatchThreadID)
#else
[shader("raygeneration")]
void main()
#endif
{
#if !USE_RAY_QUERY
    uint2 GlobalIndex = DispatchRaysIndex().xy;
#endif

    const RTXDI_RuntimeParameters params = g_Const.runtimeParams;

    uint2 pixelPosition = RTXDI_ReservoirPosToPixelPos(GlobalIndex, params.activeCheckerboardField);

    RAB_RandomSamplerState rng = RAB_InitRandomSampler(pixelPosition, 3);

    RAB_Surface surface = RAB_GetGBufferSurface(pixelPosition, false);

    RTXDI_DIReservoir spatialResult = RTXDI_EmptyDIReservoir();
    
    if (RAB_IsSurfaceValid(surface))
    {
        RTXDI_DIReservoir centerSample = RTXDI_LoadDIReservoir(g_Const.restirDI.reservoirBufferParams,
            GlobalIndex, g_Const.restirDI.bufferIndices.spatialResamplingInputBufferIndex);

        RTXDI_DISpatialResamplingParameters sparams;
        sparams.sourceBufferIndex = g_Const.restirDI.bufferIndices.spatialResamplingInputBufferIndex;
        sparams.numSamples = g_Const.restirDI.spatialResamplingParams.numSpatialSamples;
        sparams.numDisocclusionBoostSamples = g_Const.restirDI.spatialResamplingParams.numDisocclusionBoostSamples;
        sparams.targetHistoryLength = g_Const.restirDI.temporalResamplingParams.maxHistoryLength;
        sparams.biasCorrectionMode = g_Const.restirDI.spatialResamplingParams.spatialBiasCorrection;
        sparams.samplingRadius = g_Const.restirDI.spatialResamplingParams.spatialSamplingRadius;
        sparams.depthThreshold = g_Const.restirDI.spatialResamplingParams.spatialDepthThreshold;
        sparams.normalThreshold = g_Const.restirDI.spatialResamplingParams.spatialNormalThreshold;
        sparams.enableMaterialSimilarityTest = true;
        sparams.discountNaiveSamples = g_Const.restirDI.spatialResamplingParams.discountNaiveSamples;

        RAB_LightSample lightSample = (RAB_LightSample)0;
        spatialResult = RTXDI_DISpatialResampling(pixelPosition, surface, centerSample, 
             rng, params, g_Const.restirDI.reservoirBufferParams, sparams, lightSample);
    }

    RTXDI_StoreDIReservoir(spatialResult, g_Const.restirDI.reservoirBufferParams, GlobalIndex, g_Const.restirDI.bufferIndices.spatialResamplingOutputBufferIndex);
}