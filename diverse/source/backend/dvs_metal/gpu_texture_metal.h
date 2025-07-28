#pragma once
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_sampler.h"
#include "dvs_metal_utils.h"

namespace diverse
{
	namespace rhi
	{

		struct GpuTextureViewMetal : public GpuTextureView
		{
			GpuTextureViewMetal(const GpuTextureViewDesc& view_desc, const GpuTextureDesc& desc, id<MTLTexture> tex);
			~GpuTextureViewMetal();
			id<MTLTexture> id_tex;
		};

		struct GpuTextureMetal : public GpuTexture
		{
			auto view(const struct GpuDevice* device, const GpuTextureViewDesc& desc) -> std::shared_ptr<GpuTextureView>;
            std::unordered_map<GpuTextureViewDesc, std::shared_ptr<GpuTextureView>>	views;
            id<MTLTexture>  id_tex;
        };

        auto texture_usage_2_metal(TextureUsageFlags flags) -> MTLTextureUsage;
        auto pixel_format_2_metal(PixelFormat pxiel_format) -> MTLPixelFormat;
        auto texture_type_to_metal(const rhi::TextureType& tex_type)->MTLTextureType;
		auto texel_filter_mode_2_metal(const TexelFilter& mode) -> MTLSamplerMinMagFilter;
		auto texel_address_mode_2_metal(const rhi::SamplerAddressMode& mode) -> MTLSamplerAddressMode;
    }
}
