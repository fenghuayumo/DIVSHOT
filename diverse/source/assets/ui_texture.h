#pragma once

#include "cpu_assets.h"
#include "core/reference.h"
#include "image_io.h"
#include <filesystem>
#include <memory>

namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
    }

    // Editor/UI helpers — CPU TextureAsset + on-demand GPU upload via GpuResourceSystem.
    std::shared_ptr<TextureAsset> load_ui_texture(const std::filesystem::path& path);
    std::shared_ptr<TextureAsset> load_ui_texture_from_raw(const image_io::RawImage& image);

    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const TextureAsset& asset);
    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const TextureAsset* asset);
    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const std::shared_ptr<TextureAsset>& asset);
    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const SharedPtr<TextureAsset>& asset);
}
