#ifndef SAMPLERS_HLSL
#define SAMPLERS_HLSL

[[vk::binding(32)]] SamplerState sampler_lnc;
[[vk::binding(33)]] SamplerState sampler_llr;
[[vk::binding(34)]] SamplerState sampler_nnc;
[[vk::binding(35)]] SamplerState sampler_llc;
#ifndef ISE_SAMPLER
#define IES_SAMPLER sampler_llc
#endif
#endif