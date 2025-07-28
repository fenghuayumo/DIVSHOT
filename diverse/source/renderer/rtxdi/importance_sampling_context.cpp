
#include "importance_sampling_context.h"

#include <cassert>

#include "restir_di.h"
// #include "restir_gi.h"
#include "ris_buffer_segment_allocator.h"
#include "regir.h"

namespace
{

bool IsNonzeroPowerOf2(uint32_t i)
{
    return ((i & (i - 1)) == 0) && (i > 0);
}

void debugCheckParameters(const rtxdi::RISBufferSegmentParameters& localLightRISBufferParams,
                          const rtxdi::RISBufferSegmentParameters& environmentLightRISBufferParams)
{
    assert(IsNonzeroPowerOf2(localLightRISBufferParams.tileSize));
    assert(IsNonzeroPowerOf2(localLightRISBufferParams.tileCount));
    assert(IsNonzeroPowerOf2(environmentLightRISBufferParams.tileSize));
    assert(IsNonzeroPowerOf2(environmentLightRISBufferParams.tileCount));
}

}

namespace rtxdi
{

ImportanceSamplingContext::ImportanceSamplingContext(const ImportanceSamplingContext_StaticParameters& isParams) : 
    m_lightBufferParams({})
{
    debugCheckParameters(isParams.localLightRISBufferParams, isParams.environmentLightRISBufferParams);

    m_risBufferSegmentAllocator = std::make_unique<rtxdi::RISBufferSegmentAllocator>();
    m_localLightRISBufferSegmentParams.bufferOffset = m_risBufferSegmentAllocator->allocateSegment(isParams.localLightRISBufferParams.tileCount * isParams.localLightRISBufferParams.tileSize);
    m_localLightRISBufferSegmentParams.tileCount = isParams.localLightRISBufferParams.tileCount;
    m_localLightRISBufferSegmentParams.tileSize = isParams.localLightRISBufferParams.tileSize;
    m_environmentLightRISBufferSegmentParams.bufferOffset = m_risBufferSegmentAllocator->allocateSegment(isParams.environmentLightRISBufferParams.tileCount * isParams.environmentLightRISBufferParams.tileSize);
    m_environmentLightRISBufferSegmentParams.tileCount = isParams.environmentLightRISBufferParams.tileCount;
    m_environmentLightRISBufferSegmentParams.tileSize = isParams.environmentLightRISBufferParams.tileSize;
    
    ReSTIRDIStaticParameters restirDIStaticParams;
    restirDIStaticParams.CheckerboardSamplingMode = isParams.CheckerboardSamplingMode;
    restirDIStaticParams.NeighborOffsetCount = isParams.NeighborOffsetCount;
    m_restirDIContext = std::make_unique<rtxdi::ReSTIRDIContext>(restirDIStaticParams);

    m_regirContext = std::make_unique<rtxdi::ReGIRContext>(isParams.regirStaticParams, *m_risBufferSegmentAllocator);

    //ReSTIRGIStaticParameters restirGIStaticParams;
    //restirGIStaticParams.CheckerboardSamplingMode = isParams.CheckerboardSamplingMode;
    //restirGIStaticParams.RenderWidth = isParams.renderWidth;
    //restirGIStaticParams.RenderHeight = isParams.renderHeight;
    // m_restirGIContext = std::make_unique<rtxdi::ReSTIRGIContext>(restirGIStaticParams);
}

ImportanceSamplingContext::~ImportanceSamplingContext()
{

}

ReSTIRDIContext& ImportanceSamplingContext::GetReSTIRDIContext()
{
    return *m_restirDIContext;
}

const ReSTIRDIContext& ImportanceSamplingContext::GetReSTIRDIContext() const
{
    return *m_restirDIContext;
}

ReGIRContext& ImportanceSamplingContext::GetReGIRContext()
{
    return *m_regirContext;
}

const ReGIRContext& ImportanceSamplingContext::GetReGIRContext() const
{
    return *m_regirContext;
}

// ReSTIRGIContext& ImportanceSamplingContext::GetReSTIRGIContext()
// {
//     return *m_restirGIContext;
// }

// const ReSTIRGIContext& ImportanceSamplingContext::GetReSTIRGIContext() const
// {
//     return *m_restirGIContext;
// }

const RISBufferSegmentAllocator& ImportanceSamplingContext::GetRISBufferSegmentAllocator() const
{
    return *m_risBufferSegmentAllocator;
}

const RTXDI_LightBufferParameters& ImportanceSamplingContext::GetLightBufferParameters() const
{
    return m_lightBufferParams;
}

const RTXDI_RISBufferSegmentParameters& ImportanceSamplingContext::GetLocalLightRISBufferSegmentParams() const
{
    return m_localLightRISBufferSegmentParams;
}

const RTXDI_RISBufferSegmentParameters& ImportanceSamplingContext::GetEnvironmentLightRISBufferSegmentParams() const
{
    return m_environmentLightRISBufferSegmentParams;
}

uint32_t ImportanceSamplingContext::GetNeighborOffsetCount() const
{
    return m_restirDIContext->GetStaticParameters().NeighborOffsetCount;
}

bool ImportanceSamplingContext::IsLocalLightPowerRISEnabled() const
{
    ReSTIRDI_InitialSamplingParameters iss = m_restirDIContext->GetInitialSamplingParameters();
    if (iss.localLightSamplingMode == ReSTIRDI_LocalLightSamplingMode::Power_RIS)
        return true;
    if (iss.localLightSamplingMode == ReSTIRDI_LocalLightSamplingMode::ReGIR_RIS)
    {
        if( (m_regirContext->GetReGIRDynamicParameters().presamplingMode == LocalLightReGIRPresamplingMode::Power_RIS) ||
            (m_regirContext->GetReGIRDynamicParameters().fallbackSamplingMode == LocalLightReGIRFallbackSamplingMode::Power_RIS))
            return true;
    }
    return false;
}

bool ImportanceSamplingContext::IsReGIREnabled() const
{
    return (m_restirDIContext->GetInitialSamplingParameters().localLightSamplingMode == ReSTIRDI_LocalLightSamplingMode::ReGIR_RIS);
}

void ImportanceSamplingContext::SetLightBufferParams(const RTXDI_LightBufferParameters& lightBufferParams)
{
    m_lightBufferParams = lightBufferParams;
}

}
