#pragma once

#include "cpu_assets.h"
#include "image_io.h"
#include <filesystem>
#include <memory>

namespace diverse
{
    std::shared_ptr<TextureAsset> import_texture_from_raw(const image_io::RawImage& image, const TextureImportSettings& settings = {});
    std::shared_ptr<TextureAsset> import_texture_from_path(const std::filesystem::path& path, const TextureImportSettings& settings = {});

    std::shared_ptr<TextureAsset> create_white_texture_asset();
    std::shared_ptr<TextureAsset> create_black_texture_asset();
    std::shared_ptr<TextureAsset> create_normal_texture_asset();
    std::shared_ptr<TextureAsset> create_rgb_texture_asset(const std::array<u8, 4>& rgba);
}
