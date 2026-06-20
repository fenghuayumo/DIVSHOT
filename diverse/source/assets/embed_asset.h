#pragma once
#include "cpu_assets.h"
#include <string>
#include <vector>
#include <memory>

namespace diverse
{
    void embed_texture(const std::string& texFilePath, const std::string& outPath, const std::string& arrayName);

    std::unordered_map<std::string, std::shared_ptr<TextureAsset>>& get_embeded_asset_textures();
    std::shared_ptr<TextureAsset> get_embed_texture(const std::string& texture_path);
}
