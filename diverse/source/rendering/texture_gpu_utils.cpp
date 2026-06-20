#include "rendering/texture_gpu_utils.h"

namespace diverse
{
    namespace
    {
        u32 mip_row_pitch(const MipData& mip, PixelFormat format, u32 mip_width)
        {
            if (mip.height > 0 && !mip.data.empty())
                return static_cast<u32>(mip.data.size() / mip.height);
            return mip_width * 4;
        }
    }
    std::shared_ptr<rhi::GpuTexture> upload_texture_asset(TextureAsset& texture, rhi::GpuDevice* device)
    {
        if (!device || !texture.is_valid())
            return nullptr;

        auto desc = rhi::GpuTextureDesc::new_2d(texture.format, { texture.extent[0], texture.extent[1] })
            .with_usage(rhi::TextureUsageFlags::SAMPLED |
                rhi::TextureUsageFlags::TRANSFER_DST |
                rhi::TextureUsageFlags::TRANSFER_SRC)
            .with_mip_levels(texture.mips.size());

        std::vector<rhi::ImageSubData> initial_data;
        for (size_t mip_level = 0; mip_level < texture.mips.size(); ++mip_level)
        {
            const auto& mip = texture.mips[mip_level];
            const u32 mip_width = std::max<u32>(1u, desc.extent[0] >> mip_level);
            const auto row_pitch = mip_row_pitch(mip, texture.format, mip_width);
            initial_data.emplace_back(rhi::ImageSubData{
                const_cast<u8*>(mip.data.data()), (u32)mip.data.size(), row_pitch, 0 });
        }

        return device->create_texture(desc, initial_data);
    }

} // namespace diverse
