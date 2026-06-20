#pragma once

#include "asset_id.h"
#include "cpu_assets.h"
#include "model_asset.h"
#include <filesystem>
#include <memory>
#include <vector>

namespace diverse
{
    struct MeshImportResult
    {
        bool success = false;
        std::string error_message;
        std::shared_ptr<ModelAsset> model;
        std::vector<std::shared_ptr<MeshAsset>> meshes;
    };

    class MeshImporter
    {
    public:
        static MeshImporter& get_instance();

        std::shared_ptr<ModelAsset> io_stage(const AssetId& id, const std::filesystem::path& path, const MeshImportSettings& settings);

        bool decode_and_register(ModelAsset& model, const AssetId& model_id);
        std::vector<std::shared_ptr<MeshAsset>> collect_meshes(const ModelAsset& model) const;
        void finalize_model_cpu(ModelAsset& model, const MeshImportSettings& settings, bool preserve_origin);

        bool cpu_optimize_stage(const AssetId& id, std::vector<std::shared_ptr<MeshAsset>>& meshes, const MeshImportSettings& settings);
        void upload_stage(const ModelAsset& model);

        MeshImportResult import_mesh(const std::filesystem::path& path, const MeshImportSettings& settings = {});

    private:
        MeshImporter() = default;

        void assign_sub_asset_ids(ModelAsset& model);
        void register_sub_assets(ModelAsset& model, const AssetId& model_id);

        bool load_gltf(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots);
        bool load_obj(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots);
        bool load_fbx(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots);
        bool load_ply(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots);

        void optimize_mesh(MeshAsset& mesh, const MeshImportSettings& settings);
        void generate_lods(MeshAsset& mesh, uint32_t lod_levels);
        void meshopt_optimize(MeshAsset& mesh);
    };

} // namespace diverse
