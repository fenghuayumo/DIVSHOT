#include "model_loader_utils.h"
#include "assets/asset_system.h"

namespace diverse
{
    void register_texture_asset(const std::shared_ptr<TextureAsset>& texture)
    {
        if (!texture)
            return;
        auto& sys = AssetSystem::get_instance();
        sys.register_cpu_texture(texture);
    }

    AssetHandle<TextureAsset> register_texture_handle(const std::shared_ptr<TextureAsset>& texture)
    {
        if (!texture)
            return {};
        register_texture_asset(texture);
        return AssetHandle<TextureAsset>(texture->id, 0);
    }

    AssetHandle<TextureAsset> import_and_register_texture(const std::filesystem::path& path)
    {
        return register_texture_handle(import_texture_from_path(path));
    }

    AssetHandle<TextureAsset> import_and_register_texture(const image_io::RawImage& image, const TextureImportSettings& settings)
    {
        return register_texture_handle(import_texture_from_raw(image, settings));
    }
}
