#include "asset_pipeline_handlers.h"

#include "asset_pipeline.h"
#include "asset_registry.h"
#include "asset_system.h"
#include "material_importer.h"
#include "mesh_importer.h"
#include "model_asset_loader.h"
#include "texture_importer.h"
#include "core/ds_log.h"
#include "engine/file_system.h"

namespace diverse
{
    namespace
    {
        PipelineStageResult handle_io_stage(const AssetId& id, PipelineStage& next_stage)
        {
            auto& registry = AssetRegistry::get_instance();
            auto metadata = registry.get_metadata(id);
            if (!metadata)
                return PipelineStageResult::Failed;

            if (metadata->source_path.empty())
            {
                if (registry.get_state(id) == AssetState::ReadyCpu)
                {
                    next_stage = PipelineStage::CpuOptimize;
                    return PipelineStageResult::Completed;
                }

                return PipelineStageResult::Failed;
            }

            if (!FileSystem::file_exists(metadata->source_path.string()))
            {
                DS_LOG_WARN("AssetPipeline IO: file not found {}", metadata->source_path.string());
                return PipelineStageResult::Failed;
            }

            auto& sys = AssetSystem::get_instance();
            if (registry.get_state(id) == AssetState::Failed)
                return PipelineStageResult::Failed;

            if (registry.get_state(id) == AssetState::ReadyCpu)
            {
                next_stage = PipelineStage::CpuOptimize;
                return PipelineStageResult::Completed;
            }

            if (sys.is_async_loading(id))
                return PipelineStageResult::Pending;

            if (metadata->type == AssetType::Texture)
            {
                sys.start_async_load(id);
                return PipelineStageResult::Pending;
            }

            next_stage = PipelineStage::Decode;
            return PipelineStageResult::Completed;
        }

        PipelineStageResult handle_decode_stage(const AssetId& id, PipelineStage& next_stage)
        {
            auto& registry = AssetRegistry::get_instance();
            auto metadata = registry.get_metadata(id);
            if (!metadata)
                return PipelineStageResult::Failed;

            if (registry.get_state(id) == AssetState::ReadyCpu)
            {
                next_stage = PipelineStage::CpuOptimize;
                return PipelineStageResult::Completed;
            }

            auto& sys = AssetSystem::get_instance();
            switch (metadata->type)
            {
                case AssetType::Texture:
                {
                    auto texture = sys.get_asset<TextureAsset>(id);
                    if (!texture)
                    {
                        texture = import_texture_from_path(metadata->source_path);
                        if (!texture)
                            return PipelineStageResult::Failed;

                        texture->id = id;
                        texture->source_path = metadata->source_path;
                        sys.register_cpu_texture(texture);
                    }

                    registry.set_state(id, AssetState::ReadyCpu);
                    break;
                }
                case AssetType::MeshModel:
                {
                    if (!reload_model_asset(id))
                        return PipelineStageResult::Failed;

                    registry.set_state(id, AssetState::ReadyCpu);
                    break;
                }
                case AssetType::Material:
                {
                    auto result = MaterialImporter::get_instance().import_material(metadata->source_path);
                    if (!result.success)
                        return PipelineStageResult::Failed;

                    sys.register_cpu_material(result.material);
                    registry.set_state(id, AssetState::ReadyCpu);
                    break;
                }
                default:
                    DS_LOG_WARN("AssetPipeline Decode: unsupported asset type for {}", metadata->source_path.string());
                    return PipelineStageResult::Failed;
            }

            next_stage = PipelineStage::CpuOptimize;
            return PipelineStageResult::Completed;
        }

        PipelineStageResult handle_cpu_optimize_stage(const AssetId& id, PipelineStage& next_stage)
        {
            auto metadata = AssetRegistry::get_instance().get_metadata(id);
            if (!metadata)
                return PipelineStageResult::Failed;

            auto& sys = AssetSystem::get_instance();
            auto& importer = MeshImporter::get_instance();

            switch (metadata->type)
            {
                case AssetType::MeshModel:
                {
                    auto model = sys.get_model(id);
                    if (!model)
                        return PipelineStageResult::Failed;

                    auto settings = metadata->get_mesh_settings();
                    auto meshes = importer.collect_meshes(*model);
                    if (!importer.cpu_optimize_stage(id, meshes, settings))
                        return PipelineStageResult::Failed;

                    const bool preserve_origin = metadata->import_params.get<bool>("preserve_origin", false);
                    importer.finalize_model_cpu(*model, settings, preserve_origin);
                    model->mark_loaded(true);
                    break;
                }
                case AssetType::Material:
                case AssetType::Texture:
                    break;
                default:
                    break;
            }

            next_stage = PipelineStage::Upload;
            return PipelineStageResult::Completed;
        }

        PipelineStageResult handle_upload_stage(const AssetId& id, PipelineStage& next_stage)
        {
            auto& registry = AssetRegistry::get_instance();
            auto metadata = registry.get_metadata(id);
            if (!metadata)
                return PipelineStageResult::Failed;

            auto& sys = AssetSystem::get_instance();
            auto& gpu = sys.gpu_system();

            switch (metadata->type)
            {
                case AssetType::Texture:
                {
                    auto texture_gpu = gpu.request_texture(id, UploadPriority::Normal);
                    if (!texture_gpu.texture)
                        return PipelineStageResult::Failed;
                    break;
                }
                case AssetType::MeshModel:
                {
                    auto model = sys.get_model(id);
                    if (!model || model->get_slots().empty())
                        return PipelineStageResult::Failed;

                    bool all_meshes_resident = true;
                    for (const auto& slot : model->get_slots())
                    {
                        if (!slot.mesh.is_valid())
                            continue;

                        auto mesh_gpu = gpu.request_mesh(slot.mesh.get_id(), UploadPriority::Normal);
                        if (!mesh_gpu.is_valid())
                            all_meshes_resident = false;
                    }

                    if (!all_meshes_resident)
                    {
                        registry.set_state(id, AssetState::UploadQueued);
                        return PipelineStageResult::Pending;
                    }

                    break;
                }
                case AssetType::Material:
                {
                    auto material_gpu = gpu.request_material(id, UploadPriority::Normal);
                    if (!material_gpu.is_valid())
                        return PipelineStageResult::Failed;
                    break;
                }
                default:
                    DS_LOG_WARN("AssetPipeline Upload: unsupported asset type {}", metadata->source_path.string());
                    return PipelineStageResult::Failed;
            }

            registry.set_state(id, AssetState::ResidentGpu);
            next_stage = PipelineStage::Resident;
            return PipelineStageResult::Completed;
        }

        PipelineStageResult handle_resident_stage(const AssetId& id, PipelineStage& next_stage)
        {
            DS_UNUSED(id);
            next_stage = PipelineStage::Resident;
            return PipelineStageResult::Completed;
        }
    }

    void register_asset_pipeline_handlers()
    {
        auto& pipeline = AssetPipeline::get_instance();
        pipeline.set_stage_handler(PipelineStage::IO, handle_io_stage);
        pipeline.set_stage_handler(PipelineStage::Decode, handle_decode_stage);
        pipeline.set_stage_handler(PipelineStage::CpuOptimize, handle_cpu_optimize_stage);
        pipeline.set_stage_handler(PipelineStage::Upload, handle_upload_stage);
        pipeline.set_stage_handler(PipelineStage::Resident, handle_resident_stage);
    }
}
