#pragma once

#include "asset_id.h"
#include "material_asset.h"
#include "cpu_assets.h"
#include <filesystem>
#include <memory>
#include <string>

namespace diverse
{
    // Import result
    struct MaterialImportResult
    {
        bool success = false;
        std::string error_message;
        std::shared_ptr<MaterialAsset> material;
    };

    // MaterialImporter - Multi-stage material loading pipeline
    class MaterialImporter
    {
    public:
        static MaterialImporter& get_instance();

        // Stage 1: IO - Read material file
        std::shared_ptr<MaterialAsset> io_stage(const AssetId& id, const std::filesystem::path& path, const MaterialImportSettings& settings);

        // Stage 2: Decode - Parse material data and resolve texture references
        bool decode_stage(const AssetId& id, std::shared_ptr<MaterialAsset>& material, const MaterialImportSettings& settings);

        // Stage 3: CpuOptimize - Validate and optimize material
        bool cpu_optimize_stage(const AssetId& id, std::shared_ptr<MaterialAsset>& material);

        // Stage 4: Upload - Upload to GPU (handled by GpuResourceSystem)
        void upload_stage(const AssetId& id, const std::shared_ptr<MaterialAsset>& material);

        // Complete import pipeline
        MaterialImportResult import_material(const std::filesystem::path& path, const MaterialImportSettings& settings = {});

    private:
        MaterialImporter() = default;

        // Format-specific loaders
        bool load_material_file(const std::filesystem::path& path, std::shared_ptr<MaterialAsset>& material);

        // Parse material properties from file
        bool parse_pbr_material(const std::filesystem::path& path, MaterialAsset& material);

        // Resolve texture paths to AssetHandle<TextureAsset>
        void resolve_texture_references(std::shared_ptr<MaterialAsset>& material, const std::filesystem::path& base_dir);

        // Validate material data
        bool validate_material(const MaterialAsset& material);
    };

    // Convenience functions
    inline std::shared_ptr<MaterialAsset> import_material_from_path(const std::filesystem::path& path, const MaterialImportSettings& settings = {})
    {
        auto importer = MaterialImporter::get_instance();
        auto result = importer.import_material(path, settings);
        return result.material;
    }

    inline std::shared_ptr<MaterialAsset> create_default_material()
    {
        auto material = std::make_shared<MaterialAsset>();
        material->id = GenerateAssetId();
        material->name = "default";
        material->is_valid = true;
        material->version = 0;

        // Set default properties
        material->properties.base_color_mult = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        material->properties.roughness_mult = 0.7f;
        material->properties.metalness_factor = 0.0f;
        material->properties.emissive = glm::vec3(0.0f, 0.0f, 0.0f);

        // Textures will use fallback defaults
        return material;
    }

} // namespace diverse
