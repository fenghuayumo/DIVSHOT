
#include "restir_di.h"

#include <cassert>
#include <vector>
#include <memory>
#include <numeric>
#include <math.h>

using namespace rtxdi;

namespace rtxdi
{

const uint32_t ReSTIRDIContext::NumReservoirBuffers = 3;

ReSTIRDI_BufferIndices GetDefaultReSTIRDIBufferIndices()
{
    ReSTIRDI_BufferIndices bufferIndices = {};
    bufferIndices.initialSamplingOutputBufferIndex = 0;
    bufferIndices.temporalResamplingInputBufferIndex = 0;
    bufferIndices.temporalResamplingOutputBufferIndex = 0;
    bufferIndices.spatialResamplingInputBufferIndex = 0;
    bufferIndices.spatialResamplingOutputBufferIndex = 0;
    bufferIndices.shadingInputBufferIndex = 0;
    return bufferIndices;
}

ReSTIRDI_InitialSamplingParameters GetDefaultReSTIRDIInitialSamplingParams()
{
    ReSTIRDI_InitialSamplingParameters params = {};
    params.brdfCutoff = 0.0001f;
    params.enableInitialVisibility = true;
    params.environmentMapImportanceSampling = 1;
    params.localLightSamplingMode = ReSTIRDI_LocalLightSamplingMode::Uniform;
    params.numPrimaryBrdfSamples = 1;
    params.numPrimaryEnvironmentSamples = 1;
    params.numPrimaryInfiniteLightSamples = 1;
    params.numPrimaryLocalLightSamples = 8;
    return params;
}

ReSTIRDI_TemporalResamplingParameters GetDefaultReSTIRDITemporalResamplingParams()
{
    ReSTIRDI_TemporalResamplingParameters params = {};
    params.boilingFilterStrength = 0.2f;
    params.discardInvisibleSamples = false;
    params.enableBoilingFilter = true;
    params.enablePermutationSampling = true;
    params.maxHistoryLength = 20;
    params.permutationSamplingThreshold = 0.9f;
    params.temporalBiasCorrection = ReSTIRDI_TemporalBiasCorrectionMode::Basic;
    params.temporalDepthThreshold = 0.1f;
    params.temporalNormalThreshold = 0.5f;
    return params;
}

ReSTIRDI_SpatialResamplingParameters GetDefaultReSTIRDISpatialResamplingParams()
{
    ReSTIRDI_SpatialResamplingParameters params = {};
    params.numDisocclusionBoostSamples = 8;
    params.numSpatialSamples = 1;
    params.spatialBiasCorrection = ReSTIRDI_SpatialBiasCorrectionMode::Basic;
    params.spatialDepthThreshold = 0.1f;
    params.spatialNormalThreshold = 0.5f;
    params.spatialSamplingRadius = 32.0f;
    return params;
}

ReSTIRDI_ShadingParameters GetDefaultReSTIRDIShadingParams()
{
    ReSTIRDI_ShadingParameters params = {};
    params.enableDenoiserInputPacking = false;
    params.enableFinalVisibility = true;
    params.finalVisibilityMaxAge = 4;
    params.finalVisibilityMaxDistance = 16.f;
    params.reuseFinalVisibility = true;
    return params;
}

ReSTIRDIContext::ReSTIRDIContext(const ReSTIRDIStaticParameters& params) :
    m_lastFrameOutputReservoir(0),
    m_currentFrameOutputReservoir(0),
    m_frameIndex(0),
    m_staticParams(params),
    m_resamplingMode(ReSTIRDI_ResamplingMode::TemporalAndSpatial),
    m_bufferIndices(GetDefaultReSTIRDIBufferIndices()),
    m_initialSamplingParams(GetDefaultReSTIRDIInitialSamplingParams()),
    m_temporalResamplingParams(GetDefaultReSTIRDITemporalResamplingParams()),
    m_spatialResamplingParams(GetDefaultReSTIRDISpatialResamplingParams()),
    m_shadingParams(GetDefaultReSTIRDIShadingParams())
{
    UpdateCheckerboardField();
    m_runtimeParams.neighborOffsetMask = m_staticParams.NeighborOffsetCount - 1;
    UpdateBufferIndices();
}

ReSTIRDI_ResamplingMode ReSTIRDIContext::GetResamplingMode() const
{
    return m_resamplingMode;
}

RTXDI_RuntimeParameters ReSTIRDIContext::GetRuntimeParams() const
{
    return m_runtimeParams;
}

RTXDI_ReservoirBufferParameters ReSTIRDIContext::GetReservoirBufferParameters() const
{
    return m_reservoirBufferParams;
}

ReSTIRDI_BufferIndices ReSTIRDIContext::GetBufferIndices() const
{
    return m_bufferIndices;
}

ReSTIRDI_InitialSamplingParameters ReSTIRDIContext::GetInitialSamplingParameters() const
{
    return m_initialSamplingParams;
}

ReSTIRDI_TemporalResamplingParameters ReSTIRDIContext::GetTemporalResamplingParameters() const
{
    return m_temporalResamplingParams;
}

ReSTIRDI_SpatialResamplingParameters ReSTIRDIContext::GetSpatialResamplingParameters() const
{
    return m_spatialResamplingParams;
}

ReSTIRDI_ShadingParameters ReSTIRDIContext::GetShadingParameters() const
{
    return m_shadingParams;
}

const ReSTIRDIStaticParameters& ReSTIRDIContext::GetStaticParameters() const
{
    return m_staticParams;
}

void ReSTIRDIContext::SetFrameIndex(uint32_t frameIndex)
{
    m_frameIndex = frameIndex;
    m_temporalResamplingParams.uniformRandomNumber = JenkinsHash(m_frameIndex);
    m_lastFrameOutputReservoir = m_currentFrameOutputReservoir;
    UpdateBufferIndices();
    UpdateCheckerboardField();
}

uint32_t ReSTIRDIContext::GetFrameIndex() const
{
    return m_frameIndex;
}

void ReSTIRDIContext::SetResamplingMode(ReSTIRDI_ResamplingMode resamplingMode)
{
    m_resamplingMode = resamplingMode;
    UpdateBufferIndices();
}

void ReSTIRDIContext::SetInitialSamplingParameters(const ReSTIRDI_InitialSamplingParameters& initialSamplingParams)
{
    m_initialSamplingParams = initialSamplingParams;
}

void ReSTIRDIContext::SetTemporalResamplingParameters(const ReSTIRDI_TemporalResamplingParameters& temporalResamplingParams)
{
    m_temporalResamplingParams = temporalResamplingParams;
    m_temporalResamplingParams.uniformRandomNumber = JenkinsHash(m_frameIndex);
}

void ReSTIRDIContext::SetSpatialResamplingParameters(const ReSTIRDI_SpatialResamplingParameters& spatialResamplingParams)
{
    ReSTIRDI_SpatialResamplingParameters srp = spatialResamplingParams;
    srp.neighborOffsetMask = m_spatialResamplingParams.neighborOffsetMask;
    m_spatialResamplingParams = srp;
}

void ReSTIRDIContext::SetShadingParameters(const ReSTIRDI_ShadingParameters& shadingParams)
{
    m_shadingParams = shadingParams;
}

void ReSTIRDIContext::SetReservoirBufferParameters(const RTXDI_ReservoirBufferParameters& reservoirBufferParams)
{
    m_reservoirBufferParams = reservoirBufferParams;
}

void ReSTIRDIContext::UpdateBufferIndices()
{
    const bool useTemporalResampling =
        m_resamplingMode == ReSTIRDI_ResamplingMode::Temporal ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::TemporalAndSpatial ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::FusedSpatiotemporal;

    const bool useSpatialResampling =
        m_resamplingMode == ReSTIRDI_ResamplingMode::Spatial ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::TemporalAndSpatial ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::FusedSpatiotemporal;


    if (m_resamplingMode == ReSTIRDI_ResamplingMode::FusedSpatiotemporal)
    {
        m_bufferIndices.initialSamplingOutputBufferIndex = (m_lastFrameOutputReservoir + 1) % ReSTIRDIContext::NumReservoirBuffers;
        m_bufferIndices.temporalResamplingInputBufferIndex = m_lastFrameOutputReservoir;
        m_bufferIndices.shadingInputBufferIndex = m_bufferIndices.initialSamplingOutputBufferIndex;
    }
    else
    {
        m_bufferIndices.initialSamplingOutputBufferIndex = (m_lastFrameOutputReservoir + 1) % ReSTIRDIContext::NumReservoirBuffers;
        m_bufferIndices.temporalResamplingInputBufferIndex = m_lastFrameOutputReservoir;
        m_bufferIndices.temporalResamplingOutputBufferIndex = (m_bufferIndices.temporalResamplingInputBufferIndex + 1) % ReSTIRDIContext::NumReservoirBuffers;
        m_bufferIndices.spatialResamplingInputBufferIndex = useTemporalResampling
            ? m_bufferIndices.temporalResamplingOutputBufferIndex
            : m_bufferIndices.initialSamplingOutputBufferIndex;
        m_bufferIndices.spatialResamplingOutputBufferIndex = (m_bufferIndices.spatialResamplingInputBufferIndex + 1) % ReSTIRDIContext::NumReservoirBuffers;
        m_bufferIndices.shadingInputBufferIndex = useSpatialResampling
            ? m_bufferIndices.spatialResamplingOutputBufferIndex
            : m_bufferIndices.temporalResamplingOutputBufferIndex;
    }
    m_currentFrameOutputReservoir = m_bufferIndices.shadingInputBufferIndex;
}

void ReSTIRDIContext::UpdateCheckerboardField()
{
    switch (m_staticParams.CheckerboardSamplingMode)
    {
    case CheckerboardMode::Black:
        m_runtimeParams.activeCheckerboardField = (m_frameIndex & 1u) ? 1u : 2u;
        break;
    case CheckerboardMode::White:
        m_runtimeParams.activeCheckerboardField = (m_frameIndex & 1u) ? 2u : 1u;
        break;
    case CheckerboardMode::Off:
    default:
        m_runtimeParams.activeCheckerboardField = 0;
    }
}

}
