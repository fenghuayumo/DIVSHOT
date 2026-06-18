#ifndef SAMPLERS_HLSL
#define SAMPLERS_HLSL

#include "binding.hlsl"

DS_RESOURCE(32) SamplerState sampler_lnc;
DS_RESOURCE(33) SamplerState sampler_llr;
DS_RESOURCE(34) SamplerState sampler_nnc;
DS_RESOURCE(35) SamplerState sampler_llc;
#ifndef ISE_SAMPLER
#define IES_SAMPLER sampler_llc
#endif
#endif
