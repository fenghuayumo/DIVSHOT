#pragma once

#include "asset_id.h"
#include "model_asset.h"
#include "mesh_importer.h"
#include <filesystem>
#include <memory>

namespace diverse
{
    // Synchronous CPU model import via Registry + staged MeshImporter
    std::shared_ptr<ModelAsset> load_model_asset(
        const std::filesystem::path& logical_path,
        bool preserve_origin = false);

    std::shared_ptr<ModelAsset> load_primitive_model(PrimitiveType type);

    // Hot reload: re-import model CPU data in place (same AssetId)
    bool reload_model_asset(const AssetId& model_id);

} // namespace diverse
