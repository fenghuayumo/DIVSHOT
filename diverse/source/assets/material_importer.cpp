#include "material_importer.h"
#include "asset_registry.h"
#include "asset_system.h"
#include "texture_importer.h"
#include "core/ds_log.h"
#include <algorithm>
#include <sstream>

namespace diverse
{
    MaterialImporter& MaterialImporter::get_instance()
    {
        static MaterialImporter instance;
        return instance;
    }

    // Stage 1: IO - Read material file
    std::shared_ptr<MaterialAsset> MaterialImporter::io_stage(const AssetId& id, const std::filesystem::path& path, const MaterialImportSettings& settings)
    {
        DS_UNUSED(settings);

        auto material = std::make_shared<MaterialAsset>();
        material->id = id;
        material->source_path = path;
        material->name = path.stem().string();

        // Load material data from file
        if (!load_material_file(path, material))
        {
            return nullptr;
        }

        material->is_valid = true;
        material->version = 0;

        return material;
    }

    // Stage 2: Decode - Parse material data and resolve texture references
    bool MaterialImporter::decode_stage(const AssetId& id, std::shared_ptr<MaterialAsset>& material, const MaterialImportSettings& settings)
    {
        if (!material)
        {
            return false;
        }

        // Resolve texture references
        std::filesystem::path base_dir = material->source_path.parent_path();
        resolve_texture_references(material, base_dir);

        // Register in asset cache
        AssetSystem::get_instance().register_cpu_material(material);

        return true;
    }

    // Stage 3: CpuOptimize - Validate and optimize material
    bool MaterialImporter::cpu_optimize_stage(const AssetId& id, std::shared_ptr<MaterialAsset>& material)
    {
        DS_UNUSED(id);
        return validate_material(*material);
    }

    // Stage 4: Upload - Upload to GPU
    void MaterialImporter::upload_stage(const AssetId& id, const std::shared_ptr<MaterialAsset>& material)
    {
        // Materials are uploaded to a uniform buffer, handled by renderer
        // Mark as ready for GPU upload
        auto& registry = AssetRegistry::get_instance();
        registry.set_state(id, AssetState::UploadQueued);
    }

    // Complete import pipeline
    MaterialImportResult MaterialImporter::import_material(const std::filesystem::path& path, const MaterialImportSettings& settings)
    {
        MaterialImportResult result;

        if (path.empty())
        {
            result.error_message = "Empty path";
            return result;
        }

        AssetId id = GenerateAssetId();

        // Stage 1: IO
        auto material = io_stage(id, path, settings);
        if (!material || !material->is_valid)
        {
            result.error_message = "Failed to load material file";
            return result;
        }

        // Stage 2: Decode
        if (!decode_stage(id, material, settings))
        {
            result.error_message = "Failed to decode material";
            return result;
        }

        // Stage 3: CpuOptimize
        if (!cpu_optimize_stage(id, material))
        {
            result.error_message = "Material validation failed";
            return result;
        }

        result.success = true;
        result.material = material;

        DS_LOG_INFO("Successfully imported material: {}", path.string());

        return result;
    }

    // Format-specific loaders
    bool MaterialImporter::load_material_file(const std::filesystem::path& path, std::shared_ptr<MaterialAsset>& material)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Support various material formats
        if (ext == ".mat" || ext == ".mtl" || ext == ".material")
        {
            return parse_pbr_material(path, *material);
        }
        else
        {
            DS_LOG_ERROR("Unsupported material format: {}", ext);
            return false;
        }
    }

    bool MaterialImporter::parse_pbr_material(const std::filesystem::path& path, MaterialAsset& material)
    {
        material.properties = MaterialProperties{};

        std::filesystem::path base_dir = path.parent_path();
        std::string base_name = path.stem().string();

        auto try_load_texture = [&](const std::string& suffix) -> AssetHandle<TextureAsset> {
            std::vector<std::string> variants = {
                base_name + "_" + suffix + ".png",
                base_name + "_" + suffix + ".jpg",
                base_name + "_" + suffix + ".dds",
                base_name + "_" + suffix + ".hdr"
            };

            for (const auto& variant : variants)
            {
                std::filesystem::path tex_path = base_dir / variant;
                if (std::filesystem::exists(tex_path))
                {
                    auto texture = import_texture_from_path(tex_path);
                    if (!texture)
                        continue;
                    AssetSystem::get_instance().register_cpu_texture(texture);
                    return AssetRegistry::get_instance().get_handle<TextureAsset>(texture->id);
                }
            }
            return AssetHandle<TextureAsset>();
        };

        material.albedo = try_load_texture("albedo");
        if (!material.albedo.is_valid())
            material.albedo = try_load_texture("diffuse");
        material.normal = try_load_texture("normal");
        material.roughness = try_load_texture("roughness");
        material.metallic = try_load_texture("metallic");
        material.ao = try_load_texture("ao");
        material.emissive = try_load_texture("emissive");

        return true;
    }

    // Resolve texture paths to AssetHandle<TextureAsset>
    void MaterialImporter::resolve_texture_references(std::shared_ptr<MaterialAsset>& material, const std::filesystem::path& base_dir)
    {
        auto& registry = AssetRegistry::get_instance();

        // Helper to resolve a single texture
        auto resolve_texture = [&](const std::string& tex_name) -> AssetHandle<TextureAsset> {
            if (tex_name.empty())
            {
                return AssetHandle<TextureAsset>();
            }

            std::filesystem::path tex_path = base_dir / tex_name;
            AssetId tex_id = registry.find_by_path(tex_path);

            if (!tex_id.is_valid())
            {
                // Try to find in common texture directories
                // For now, return invalid handle
                return AssetHandle<TextureAsset>();
            }

            return registry.get_texture_handle(tex_id);
        };

        // Resolve all texture references
        // In a real implementation, this would parse the material file
        // and resolve each texture path to an AssetHandle

        // For now, leave handles as invalid (will use fallback)
    }

    // Validate material data
    bool MaterialImporter::validate_material(const MaterialAsset& material)
    {
        // Validate properties
        if (material.properties.roughness_mult < 0.0f || material.properties.roughness_mult > 1.0f)
        {
            DS_LOG_WARN("Material {}: Invalid roughness value {}", material.name, material.properties.roughness_mult);
        }

        if (material.properties.metalness_factor < 0.0f || material.properties.metalness_factor > 1.0f)
        {
            DS_LOG_WARN("Material {}: Invalid metalness value {}", material.name, material.properties.metalness_factor);
        }

        // Validate alpha cutoff
        if (material.properties.alpha_cutoff < 0.0f || material.properties.alpha_cutoff > 1.0f)
        {
            DS_LOG_WARN("Material {}: Invalid alpha cutoff value {}", material.name, material.properties.alpha_cutoff);
        }

        return true;
    }

} // namespace diverse
