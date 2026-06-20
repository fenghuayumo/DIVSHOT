#include "asset_pipeline_handlers.h"

#include "asset_pipeline.h"

#include "asset_registry.h"

#include "asset_system.h"

#include "model_asset_loader.h"

#include "texture_importer.h"

#include "mesh_importer.h"

#include "material_importer.h"

#include "engine/file_system.h"

#include "core/ds_log.h"



namespace diverse

{

    namespace

    {

        bool handle_io_stage(const AssetId& id, PipelineStage& next_stage)

        {

            auto metadata = AssetRegistry::get_instance().get_metadata(id);

            if (!metadata)

                return false;



            if (metadata->source_path.empty())

            {

                if (AssetRegistry::get_instance().get_state(id) == AssetState::ReadyCpu)

                {

                    next_stage = PipelineStage::CpuOptimize;

                    return true;

                }

                return false;

            }



            if (!FileSystem::file_exists(metadata->source_path.string()))

            {

                DS_LOG_WARN("AssetPipeline IO: file not found {}", metadata->source_path.string());

                return false;

            }



            next_stage = PipelineStage::Decode;

            return true;

        }



        bool handle_decode_stage(const AssetId& id, PipelineStage& next_stage)

        {

            auto& registry = AssetRegistry::get_instance();

            auto metadata = registry.get_metadata(id);

            if (!metadata)

                return false;



            if (registry.get_state(id) == AssetState::ReadyCpu)

            {

                next_stage = PipelineStage::CpuOptimize;

                return true;

            }



            auto& sys = AssetSystem::get_instance();

            switch (metadata->type)

            {

                case AssetType::Texture:

                {

                    auto texture = import_texture_from_path(metadata->source_path);

                    if (!texture)

                        return false;

                    texture->id = id;

                    texture->source_path = metadata->source_path;

                    sys.register_cpu_texture(texture);

                    registry.set_state(id, AssetState::ReadyCpu);

                    break;

                }

                case AssetType::MeshModel:

                {

                    if (!reload_model_asset(id))

                        return false;



                    registry.set_state(id, AssetState::ReadyCpu);

                    break;

                }

                case AssetType::Material:

                {

                    auto mat_importer = MaterialImporter::get_instance();

                    auto result = mat_importer.import_material(metadata->source_path);

                    if (!result.success)

                        return false;



                    sys.register_cpu_material(result.material);

                    registry.set_state(id, AssetState::ReadyCpu);

                    break;

                }

                default:

                    DS_LOG_WARN("AssetPipeline Decode: unsupported asset type for {}", metadata->source_path.string());

                    return false;

            }



            next_stage = PipelineStage::CpuOptimize;

            return true;

        }



        bool handle_cpu_optimize_stage(const AssetId& id, PipelineStage& next_stage)

        {

            auto metadata = AssetRegistry::get_instance().get_metadata(id);

            if (!metadata)

                return false;



            auto& sys = AssetSystem::get_instance();

            auto& importer = MeshImporter::get_instance();



            switch (metadata->type)

            {

                case AssetType::MeshModel:

                {

                    auto model = sys.get_model(id);

                    if (!model)

                        return false;



                    auto settings = metadata->get_mesh_settings();

                    auto meshes = importer.collect_meshes(*model);

                    if (!importer.cpu_optimize_stage(id, meshes, settings))

                        return false;



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

            return true;

        }



        bool handle_upload_stage(const AssetId& id, PipelineStage& next_stage)

        {

            auto& registry = AssetRegistry::get_instance();

            auto metadata = registry.get_metadata(id);

            if (!metadata)

                return false;



            auto& gpu = AssetSystem::get_instance().gpu_system();

            auto& sys = AssetSystem::get_instance();



            switch (metadata->type)

            {

                case AssetType::Texture:

                {

                    auto texture_gpu = gpu.request_texture(id, UploadPriority::Normal);

                    if (!texture_gpu.texture)

                        return false;

                    registry.set_state(id, AssetState::ResidentGpu);

                    break;

                }

                case AssetType::MeshModel:

                {

                    auto model = sys.get_model(id);

                    if (!model || model->get_slots().empty())

                        return false;



                    MeshImporter::get_instance().upload_stage(*model);

                    registry.set_state(id, AssetState::UploadQueued);

                    registry.set_state(id, AssetState::ResidentGpu);

                    break;

                }

                case AssetType::Material:

                {

                    registry.set_state(id, AssetState::ResidentGpu);

                    break;

                }

                default:

                    DS_LOG_WARN("AssetPipeline Upload: unsupported asset type {}", metadata->source_path.string());

                    return false;

            }



            next_stage = PipelineStage::Resident;

            return true;

        }



        bool handle_resident_stage(const AssetId& id, PipelineStage& next_stage)

        {

            DS_UNUSED(id);

            next_stage = PipelineStage::Resident;

            return true;

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

