#include "model_asset_loader.h"
#include "asset_registry.h"
#include "asset_system.h"
#include "engine/file_system.h"
#include "core/ds_log.h"
#include "core/profiler.h"

namespace diverse
{
    namespace
    {
        std::filesystem::path primitive_registry_path(PrimitiveType type)
        {
            return std::filesystem::path(std::string("primitive://") + std::to_string(static_cast<int>(type)));
        }
    }

    std::shared_ptr<ModelAsset> load_model_asset(
        const std::filesystem::path& logical_path,
        bool preserve_origin)
    {
        DS_PROFILE_FUNCTION();

        auto& registry = AssetRegistry::get_instance();

        const AssetId existing_id = registry.find_by_path(logical_path);
        if (existing_id.is_valid())
        {
            if (auto cached = AssetSystem::get_instance().get_model(existing_id))
                return cached;
        }

        std::string physical_path;
        if (!FileSystem::get().resolve_physical_path(logical_path.string(), physical_path))
        {
            DS_LOG_INFO("Failed to resolve model path: {}", logical_path.string());
            return nullptr;
        }

        const AssetId model_id = existing_id.is_valid() ? existing_id : GenerateAssetId();

        AssetMetadata metadata;
        metadata.id = model_id;
        metadata.source_path = logical_path;
        metadata.type = AssetType::MeshModel;
        metadata.state = AssetState::LoadingCpu;
        metadata.import_params.set("preserve_origin", preserve_origin ? "true" : "false");
        registry.register_asset(model_id, metadata);

        MeshImportSettings settings = metadata.get_mesh_settings();
        auto& importer = MeshImporter::get_instance();

        auto model = importer.io_stage(model_id, std::filesystem::path(physical_path), settings);
        if (!model || model->get_slots().empty())
        {
            registry.set_state(model_id, AssetState::Failed);
            return nullptr;
        }

        if (!importer.decode_and_register(*model, model_id))
        {
            registry.set_state(model_id, AssetState::Failed);
            return nullptr;
        }

        auto meshes = importer.collect_meshes(*model);
        if (!importer.cpu_optimize_stage(model_id, meshes, settings))
        {
            registry.set_state(model_id, AssetState::Failed);
            return nullptr;
        }

        importer.finalize_model_cpu(*model, settings, preserve_origin);

        model->mark_loaded(true);
        model->mark_invalid(false);
        AssetSystem::get_instance().register_cpu_model(model);
        registry.set_state(model_id, AssetState::ReadyCpu);

        DS_LOG_INFO("Loaded model asset via Registry/Pipeline: {}", logical_path.string());
        return model;
    }

    bool reload_model_asset(const AssetId& model_id)
    {
        DS_PROFILE_FUNCTION();

        auto& sys = AssetSystem::get_instance();
        auto& registry = AssetRegistry::get_instance();
        auto metadata = registry.get_metadata(model_id);
        if (!metadata || metadata->type != AssetType::MeshModel)
            return false;

        if (metadata->source_path.string().starts_with("primitive://"))
            return false;

        std::string physical_path;
        if (!FileSystem::get().resolve_physical_path(metadata->source_path.string(), physical_path))
        {
            DS_LOG_WARN("Hot reload: failed to resolve model path {}", metadata->source_path.string());
            return false;
        }

        auto existing = sys.get_model(model_id);
        if (!existing)
        {
            existing = std::make_shared<ModelAsset>();
            existing->id = model_id;
        }

        const bool preserve_origin = metadata->import_params.get<bool>("preserve_origin", false);
        MeshImportSettings settings = metadata->get_mesh_settings();
        auto& importer = MeshImporter::get_instance();

        auto temp = importer.io_stage(model_id, std::filesystem::path(physical_path), settings);
        if (!temp || temp->get_slots().empty())
        {
            registry.set_state(model_id, AssetState::Failed);
            return false;
        }

        temp->source_path = metadata->source_path.string();

        if (!importer.decode_and_register(*temp, model_id))
        {
            registry.set_state(model_id, AssetState::Failed);
            return false;
        }

        auto meshes = importer.collect_meshes(*temp);
        if (!importer.cpu_optimize_stage(model_id, meshes, settings))
        {
            registry.set_state(model_id, AssetState::Failed);
            return false;
        }

        importer.finalize_model_cpu(*temp, settings, preserve_origin);

        temp->mark_loaded(true);
        temp->mark_invalid(false);
        existing->assign_from(*temp);
        AssetSystem::get_instance().register_cpu_model(existing);
        return true;
    }

    std::shared_ptr<ModelAsset> load_primitive_model(PrimitiveType type)
    {
        auto& registry = AssetRegistry::get_instance();

        const auto registry_path = primitive_registry_path(type);
        const AssetId existing_id = registry.find_by_path(registry_path);
        if (existing_id.is_valid())
        {
            if (auto cached = AssetSystem::get_instance().get_model(existing_id))
                return cached;
        }

        auto model = std::make_shared<ModelAsset>();
        model->load_primitive(type);

        if (!model->is_loaded())
            return nullptr;

        const AssetId model_id = model->id.is_valid() ? model->id : GenerateAssetId();
        model->id = model_id;

        AssetMetadata metadata;
        metadata.id = model_id;
        metadata.source_path = registry_path;
        metadata.type = AssetType::MeshModel;
        metadata.state = AssetState::ReadyCpu;
        registry.register_asset(model_id, metadata);

        auto& importer = MeshImporter::get_instance();
        importer.decode_and_register(*model, model_id);

        AssetSystem::get_instance().register_cpu_model(model);
        return model;
    }

} // namespace diverse
