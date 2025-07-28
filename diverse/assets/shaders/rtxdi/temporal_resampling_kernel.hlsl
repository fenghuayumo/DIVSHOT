#include "rtxdi_bridege.hlsl"
#include "boiling_filter.hlsl"
#include "temporal_resampling.hlsl"

[[vk::binding(12)]] Texture2D<float4> reprojection_tex;

bool IsComplexSurface(int2 pixelPosition, RAB_Surface surface)
{
    // Use a simple classification of surfaces into simple and complex based on the roughness.
    // The PostprocessGBuffer pass modifies the surface roughness and writes the DenoiserNormalRoughness
    // channel where the roughness is preserved. The roughness stored in the regular G-buffer is modified
    // based on the surface curvature around the current pixel. If the surface is curved, roughness increases.
    // Detect that increase here and disable permutation sampling based on a threshold.
    // Other classification methods can be employed for better quality.
    // float originalRoughness = t_DenoiserNormalRoughness[pixelPosition].a;
    // return originalRoughness < (surface.material.specular_brdf.roughness * g_Const.restirDI.temporalResamplingParams.permutationSamplingThreshold);
    return false;
}

#if USE_RAY_QUERY
[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)] 
void main(uint2 GlobalIndex : SV_DispatchThreadID, uint2 LocalIndex : SV_GroupThreadID, uint2 GroupIdx : SV_GroupID)
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

    RAB_RandomSamplerState rng = RAB_InitRandomSampler(pixelPosition, 2);

    RAB_Surface surface = RAB_GetGBufferSurface(pixelPosition, false);

    bool usePermutationSampling = false;
    if (g_Const.restirDI.temporalResamplingParams.enablePermutationSampling)
    {
        // Permutation sampling makes more noise on thin, high-detail objects.
        usePermutationSampling = !IsComplexSurface(pixelPosition, surface);
    }

    RTXDI_DIReservoir temporalResult = RTXDI_EmptyDIReservoir();
    int2 temporalSamplePixelPos = -1;
    
    if (RAB_IsSurfaceValid(surface))
    {
        RTXDI_DIReservoir curSample = RTXDI_LoadDIReservoir(g_Const.restirDI.reservoirBufferParams,
            GlobalIndex, g_Const.restirDI.bufferIndices.initialSamplingOutputBufferIndex);

        float2 reproject_uv = reprojection_tex[pixelPosition].xy;
        float3 motionVector = float3(reproject_uv * frame_constants.view_constants.viewport_size.xy,0);

        RTXDI_DITemporalResamplingParameters tparams;
        tparams.screenSpaceMotion = motionVector;
        tparams.sourceBufferIndex = g_Const.restirDI.bufferIndices.temporalResamplingInputBufferIndex;
        tparams.maxHistoryLength = g_Const.restirDI.temporalResamplingParams.maxHistoryLength;
        tparams.biasCorrectionMode = g_Const.restirDI.temporalResamplingParams.temporalBiasCorrection;
        tparams.depthThreshold = g_Const.restirDI.temporalResamplingParams.temporalDepthThreshold;
        tparams.normalThreshold = g_Const.restirDI.temporalResamplingParams.temporalNormalThreshold;
        tparams.enableVisibilityShortcut = g_Const.restirDI.temporalResamplingParams.discardInvisibleSamples;
        tparams.enablePermutationSampling = usePermutationSampling;
        tparams.uniformRandomNumber = g_Const.restirDI.temporalResamplingParams.uniformRandomNumber;

        RAB_LightSample selectedLightSample = (RAB_LightSample)0;
        
        temporalResult = RTXDI_DITemporalResampling(pixelPosition, surface, curSample,
            rng, params, g_Const.restirDI.reservoirBufferParams, tparams, temporalSamplePixelPos, selectedLightSample);
    }

#ifdef RTXDI_ENABLE_BOILING_FILTER
    if (g_Const.restirDI.temporalResamplingParams.enableBoilingFilter)
    {
        RTXDI_BoilingFilter(LocalIndex, g_Const.restirDI.temporalResamplingParams.boilingFilterStrength, temporalResult);
    }
#endif

    // u_TemporalSamplePositions[GlobalIndex] = temporalSamplePixelPos;
    
    RTXDI_StoreDIReservoir(temporalResult, g_Const.restirDI.reservoirBufferParams, GlobalIndex, g_Const.restirDI.bufferIndices.temporalResamplingOutputBufferIndex);
}