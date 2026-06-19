#pragma once
#include <string>
#include <vector>
#include <assets/texture.h>
#include <core/reference.h>
namespace diverse
{
    void embed_texture(const std::string& texFilePath, const std::string& outPath, const std::string& arrayName);

    std::unordered_map<std::string, SharedPtr<asset::Texture>>& get_embeded_asset_textures();
    SharedPtr<asset::Texture> get_embed_texture(const std::string& texture_path);
}