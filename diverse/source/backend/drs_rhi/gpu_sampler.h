#pragma once
#include "utility/hash_utils.h"

namespace diverse
{
    namespace rhi
    {
        enum class TexelFilter
        {
            NEAREST = 1,
            LINEAR = 2
        };

        enum class SamplerMipmapMode
        {
            NEAREST = 1,
            LINEAR = 2
        };

        enum class SamplerAddressMode
        {
            REPEAT,
            MIRRORED_REPEAT,
            CLAMP_TO_EDGE,
            CLAMP_TO_BORDER,
            MIRROR_CLAMP_TO_EDGE
        };

        struct GpuSamplerDesc
        {
            TexelFilter texel_filter;
            SamplerMipmapMode mipmap_mode;
            SamplerAddressMode address_modes;
        };

        inline bool operator==(const GpuSamplerDesc& s1, const GpuSamplerDesc& s2)
        {
            return s1.texel_filter == s2.texel_filter
                && s1.mipmap_mode == s2.mipmap_mode
                && s1.address_modes == s2.address_modes;
        }
    }
}

template<>
struct std::hash<diverse::rhi::GpuSamplerDesc>
{
	std::size_t operator()(diverse::rhi::GpuSamplerDesc const& s) const noexcept
	{
		auto hash_code = diverse::hash<u32>()((uint32)s.texel_filter);
        diverse::hash_combine(hash_code, uint32(s.mipmap_mode));
        diverse::hash_combine(hash_code, uint32(s.address_modes));
		return hash_code;
	}
};
