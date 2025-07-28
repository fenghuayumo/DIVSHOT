
#ifndef RTXDI_RESTIRDI_PARAMETERS_H
#define RTXDI_RESTIRDI_PARAMETERS_H


// Flag that is used in the RIS buffer to identify that a light is 
// stored in a compact form.
#define RTXDI_LIGHT_COMPACT_BIT 0x80000000u

// Light index mask for the RIS buffer.
#define RTXDI_LIGHT_INDEX_MASK 0x7fffffff

// Reservoirs are stored in a structured buffer in a block-linear layout.
// This constant defines the size of that block, measured in pixels.
#define RTXDI_RESERVOIR_BLOCK_SIZE 16

// Bias correction modes for temporal and spatial resampling:
// Use (1/M) normalization, which is very biased but also very fast.
#define RTXDI_BIAS_CORRECTION_OFF 0
// Use MIS-like normalization but assume that every sample is visible.
#define RTXDI_BIAS_CORRECTION_BASIC 1
// Use pairwise MIS normalization (assuming every sample is visible).  Better perf & specular quality
#define RTXDI_BIAS_CORRECTION_PAIRWISE 2
// Use MIS-like normalization with visibility rays. Unbiased.
#define RTXDI_BIAS_CORRECTION_RAY_TRACED 3

// Select local lights with equal probability from the light buffer during initial sampling
#define ReSTIRDI_LocalLightSamplingMode_UNIFORM 0
// Use power based RIS to select local lights during initial sampling
#define ReSTIRDI_LocalLightSamplingMode_POWER_RIS 1
// Use ReGIR based RIS to select local lights during initial sampling.
#define ReSTIRDI_LocalLightSamplingMode_REGIR_RIS 2

// When neighboring samples have less than the naive sampling M threshold, they are ignored during spatial resampling
#define RTXDI_NAIVE_SAMPLING_M_THRESHOLD 2

// This macro enables the functions that deal with the RIS buffer and presampling.
#ifndef RTXDI_ENABLE_PRESAMPLING
#define RTXDI_ENABLE_PRESAMPLING 1
#endif

#define RTXDI_INVALID_LIGHT_INDEX (0xffffffffu)

static const uint RTXDI_InvalidLightIndex = RTXDI_INVALID_LIGHT_INDEX;

#include "light_sampling/ris_buffer_segment_parameters.hlsl"
#include "regir/regir_parameters.hlsl"

struct RTXDI_LightBufferRegion
{
    uint firstLightIndex;
    uint numLights;
    uint pad1;
    uint pad2;
};

struct RTXDI_EnvironmentLightBufferParameters
{
    uint lightPresent;
    uint lightIndex;
    uint pad1;
    uint pad2;
};

struct RTXDI_RuntimeParameters
{
    uint neighborOffsetMask; // Spatial
    uint activeCheckerboardField; // 0 - no checkerboard, 1 - odd pixels, 2 - even pixels
    uint pad1;
    uint pad2;
};

struct RTXDI_LightBufferParameters
{
    RTXDI_LightBufferRegion localLightBufferRegion;
    RTXDI_LightBufferRegion infiniteLightBufferRegion;
    RTXDI_EnvironmentLightBufferParameters environmentLightParams;
};

struct RTXDI_ReservoirBufferParameters
{
    uint reservoirBlockRowPitch;
    uint reservoirArrayPitch;
    uint pad1;
    uint pad2;
};

struct RTXDI_PackedDIReservoir
{
    uint lightData;
    uint uvData;
    uint mVisibility;
    uint distanceAge;
    float targetPdf;
    float weight;
};

#define ReSTIRDI_LocalLightSamplingMode uint
#define ReSTIRDI_TemporalBiasCorrectionMode uint
#define ReSTIRDI_SpatialBiasCorrectionMode uint

struct ReSTIRDI_BufferIndices
{
    uint initialSamplingOutputBufferIndex;
    uint temporalResamplingInputBufferIndex;
    uint temporalResamplingOutputBufferIndex;
    uint spatialResamplingInputBufferIndex;

    uint spatialResamplingOutputBufferIndex;
    uint shadingInputBufferIndex;
    uint pad1;
    uint pad2;
};

struct ReSTIRDI_InitialSamplingParameters
{
    uint numPrimaryLocalLightSamples;
    uint numPrimaryInfiniteLightSamples;
    uint numPrimaryEnvironmentSamples;
    uint numPrimaryBrdfSamples;

    float brdfCutoff;
    uint enableInitialVisibility;
    uint environmentMapImportanceSampling; // Only used in InitialSamplingFunctions.hlsli via RAB_EvaluateEnvironmentMapSamplingPdf
    ReSTIRDI_LocalLightSamplingMode localLightSamplingMode;
};

struct ReSTIRDI_TemporalResamplingParameters
{
    float temporalDepthThreshold;
    float temporalNormalThreshold;
    uint maxHistoryLength;
    ReSTIRDI_TemporalBiasCorrectionMode temporalBiasCorrection;

    uint enablePermutationSampling;
    float permutationSamplingThreshold;
    uint enableBoilingFilter;
    float boilingFilterStrength;

    uint discardInvisibleSamples;
    uint uniformRandomNumber;
    uint pad2;
    uint pad3;
};

struct ReSTIRDI_SpatialResamplingParameters
{
    float spatialDepthThreshold;
    float spatialNormalThreshold;
    ReSTIRDI_SpatialBiasCorrectionMode spatialBiasCorrection;
    uint numSpatialSamples;

    uint numDisocclusionBoostSamples;
    float spatialSamplingRadius;
    uint neighborOffsetMask;
    uint discountNaiveSamples;
};

struct ReSTIRDI_ShadingParameters
{
    uint enableFinalVisibility;
    uint reuseFinalVisibility;
    uint finalVisibilityMaxAge;
    float finalVisibilityMaxDistance;

    uint enableDenoiserInputPacking;
    uint pad1;
    uint pad2;
    uint pad3;
};

struct ReSTIRDI_Parameters
{
    RTXDI_ReservoirBufferParameters reservoirBufferParams;
    ReSTIRDI_BufferIndices bufferIndices;
    ReSTIRDI_InitialSamplingParameters initialSamplingParams;
    ReSTIRDI_TemporalResamplingParameters temporalResamplingParams;
    ReSTIRDI_SpatialResamplingParameters spatialResamplingParams;
    ReSTIRDI_ShadingParameters shadingParams;
};

#define RTXDI_TEX2D Texture2D
#define RTXDI_TEX2D_LOAD(t, pos, lod) t.Load(int3(pos, lod))
#define RTXDI_DEFAULT(value) = value

#endif // RTXDI_RESTIRDI_PARAMETERS_H