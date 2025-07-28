
#include "rtxdi_bridege.hlsl"
#include "pre_sampling_functions.hlsl"

[numthreads(256, 1, 1)]
void main(uint GlobalIndex: SV_DispatchThreadID)
{
    RAB_RandomSamplerState rng = RAB_InitRandomSampler(uint2(GlobalIndex & 0xfff, GlobalIndex >> 12), 1);
    RAB_RandomSamplerState coherentRng = RAB_InitRandomSampler(uint2(GlobalIndex >> 8, 0), 1);

    RTXDI_PresampleLocalLightsForReGIR(rng, coherentRng, GlobalIndex, g_Const.lightBufferParams.localLightBufferRegion, g_Const.localLightsRISBufferSegmentParams, g_Const.regir);
}