#pragma once

#include "assets/cpu_assets.h"
#include "assets/material_asset.h"
#include "assets/texture_importer.h"
#include "assets/asset_handle.h"
#include "assets/image_io.h"
#include "assets/asset_system.h"
#include <memory>
#include <string>

namespace diverse
{
    inline std::shared_ptr<MeshAsset> make_mesh_asset(const std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices)
    {
        auto mesh = std::make_shared<MeshAsset>();
        mesh->id = GenerateAssetId();
        mesh->indices = indices;
        mesh->vertices = vertices;
        mesh->calculate_bounding_box();
        return mesh;
    }

    inline std::shared_ptr<MeshAsset> make_mesh_asset(const std::vector<uint32_t>& indices, const std::vector<AnimVertex>& anim_vertices)
    {
        auto mesh = std::make_shared<MeshAsset>();
        mesh->id = GenerateAssetId();
        mesh->indices = indices;
        mesh->anim_vertices = anim_vertices;
        mesh->has_skeleton = true;
        mesh->calculate_bounding_box();
        return mesh;
    }

    AssetHandle<TextureAsset> register_texture_handle(const std::shared_ptr<TextureAsset>& texture);
    AssetHandle<TextureAsset> import_and_register_texture(const std::filesystem::path& path);
    AssetHandle<TextureAsset> import_and_register_texture(const image_io::RawImage& image, const TextureImportSettings& settings = {});
    void register_texture_asset(const std::shared_ptr<TextureAsset>& texture);
}
