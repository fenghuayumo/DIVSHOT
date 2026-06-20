#include "ui_texture.h"
#include "asset_system.h"
#include "texture_importer.h"
#include "rendering/gpu_resource_system.h"

namespace diverse
{
    std::shared_ptr<TextureAsset> load_ui_texture(const std::filesystem::path& path)
    {
        if (path.empty())
            return nullptr;
        return AssetSystem::get_instance().load_asset<TextureAsset>(path);
    }

    std::shared_ptr<TextureAsset> load_ui_texture_from_raw(const image_io::RawImage& image)
    {
        auto texture = import_texture_from_raw(image);
        if (!texture)
            return nullptr;
        AssetSystem::get_instance().register_cpu_texture(texture);
        return texture;
    }

    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const TextureAsset& asset)
    {
        if (!asset.is_valid())
            return nullptr;

        auto& gpu_sys = AssetSystem::get_instance().gpu_system();
        auto tex_gpu = gpu_sys.request_texture(asset.id, UploadPriority::Critical);
        return tex_gpu.texture;
    }

    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const TextureAsset* asset)
    {
        return asset ? get_ui_gpu_texture(*asset) : nullptr;
    }

    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const std::shared_ptr<TextureAsset>& asset)
    {
        return asset ? get_ui_gpu_texture(*asset) : nullptr;
    }

    std::shared_ptr<rhi::GpuTexture> get_ui_gpu_texture(const SharedPtr<TextureAsset>& asset)
    {
        return asset ? get_ui_gpu_texture(*asset.get()) : nullptr;
    }
}
