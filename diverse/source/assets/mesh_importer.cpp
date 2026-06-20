#include "mesh_importer.h"
#include "asset_registry.h"
#include "asset_system.h"
#include "mesh_factory.h"
#include "core/ds_log.h"
#include <ModelLoaders/meshoptimizer/src/meshoptimizer.h>
#include <algorithm>

namespace diverse
{
    MeshImporter& MeshImporter::get_instance()
    {
        static MeshImporter instance;
        return instance;
    }

    std::shared_ptr<ModelAsset> MeshImporter::io_stage(
        const AssetId& id,
        const std::filesystem::path& path,
        const MeshImportSettings& settings)
    {
        DS_UNUSED(settings);

        auto model = std::make_shared<ModelAsset>();
        model->id = id;
        model->source_path = path.string();
        model->primitive_type = PrimitiveType::File;

        if (path.empty())
            return model;

        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        bool ok = false;
        if (ext == ".gltf" || ext == ".glb")
            ok = load_gltf(path, model->get_slots());
        else if (ext == ".obj")
            ok = load_obj(path, model->get_slots());
        else if (ext == ".fbx")
            ok = load_fbx(path, model->get_slots());
        else if (ext == ".ply")
            ok = load_ply(path, model->get_slots());
        else
            DS_LOG_ERROR("Unsupported mesh format: {}", ext);

        if (!ok || model->get_slots().empty())
            return nullptr;

        return model;
    }

    void MeshImporter::assign_sub_asset_ids(ModelAsset& model)
    {
        for (auto& slot : model.get_slots())
        {
            if (slot.mesh)
            {
                if (!slot.mesh->id.is_valid())
                    slot.mesh->id = GenerateAssetId();
                slot.mesh->source_path = model.source_path;
            }
            if (slot.material && !slot.material->id.is_valid())
                slot.material->id = GenerateAssetId();
        }
    }

    void MeshImporter::register_sub_assets(ModelAsset& model, const AssetId& model_id)
    {
        auto& registry = AssetRegistry::get_instance();
        auto& sys = AssetSystem::get_instance();

        auto register_child = [&](const AssetId& child_id, AssetType type) {
            if (!child_id.is_valid())
                return;
            if (!registry.get_metadata(child_id))
            {
                AssetMetadata child_meta;
                child_meta.id = child_id;
                child_meta.type = type;
                child_meta.state = AssetState::ReadyCpu;
                registry.register_asset(child_id, child_meta);
            }
            registry.add_dependency(model_id, child_id);
        };

        for (auto& slot : model.get_slots())
        {
            if (slot.mesh)
            {
                register_child(slot.mesh->id, AssetType::MeshModel);
                sys.register_cpu_mesh(slot.mesh);
            }
            if (slot.material)
            {
                register_child(slot.material->id, AssetType::Material);
                sys.register_cpu_material(slot.material);

                const AssetHandle<TextureAsset>* handles[] = {
                    &slot.material->albedo,
                    &slot.material->normal,
                    &slot.material->metallic,
                    &slot.material->roughness,
                    &slot.material->ao,
                    &slot.material->emissive,
                    &slot.material->transmission,
                    &slot.material->normal_detail,
                };
                for (const auto* handle : handles)
                {
                    if (handle && handle->is_valid())
                    {
                        registry.add_dependency(slot.material->id, handle->get_id());
                        registry.add_dependency(model_id, handle->get_id());
                    }
                }
            }
        }
    }

    bool MeshImporter::decode_and_register(ModelAsset& model, const AssetId& model_id)
    {
        if (model.get_slots().empty())
            return false;

        assign_sub_asset_ids(model);
        register_sub_assets(model, model_id);
        return true;
    }

    std::vector<std::shared_ptr<MeshAsset>> MeshImporter::collect_meshes(const ModelAsset& model) const
    {
        std::vector<std::shared_ptr<MeshAsset>> meshes;
        meshes.reserve(model.get_slots().size());
        for (const auto& slot : model.get_slots())
        {
            if (slot.mesh && slot.mesh->is_valid())
                meshes.push_back(slot.mesh);
        }
        return meshes;
    }

    void MeshImporter::finalize_model_cpu(
        ModelAsset& model,
        const MeshImportSettings& settings,
        bool preserve_origin)
    {
        DS_UNUSED(settings);
        if (!preserve_origin)
            model.reset_center();
        else
            model.get_local_bounding_box();
    }

    bool MeshImporter::cpu_optimize_stage(
        const AssetId& id,
        std::vector<std::shared_ptr<MeshAsset>>& meshes,
        const MeshImportSettings& settings)
    {
        DS_UNUSED(id);

        if (meshes.empty())
            return false;

        for (auto& mesh : meshes)
        {
            if (!mesh || !mesh->is_valid())
                continue;

            if (settings.calculate_normals)
                mesh->generate_normals();

            if (settings.calculate_tangents)
                mesh->generate_tangents_bitangents();

            mesh->calculate_bounding_box();

            if (settings.optimize_vertices)
                optimize_mesh(*mesh, settings);

            if (settings.generate_lod)
                generate_lods(*mesh, settings.max_lod_levels);
        }

        return true;
    }

    void MeshImporter::upload_stage(const ModelAsset& model)
    {
        auto& gpu_sys = AssetSystem::get_instance().gpu_system();
        for (const auto& slot : model.get_slots())
        {
            if (slot.mesh && slot.mesh->id.is_valid())
                gpu_sys.queue_upload(slot.mesh->id, AssetType::MeshModel, UploadPriority::Normal);
        }
    }

    MeshImportResult MeshImporter::import_mesh(
        const std::filesystem::path& path,
        const MeshImportSettings& settings)
    {
        DS_UNUSED(path);
        DS_UNUSED(settings);
        MeshImportResult result;
        result.error_message = "Use AssetSystem::load_model()";
        return result;
    }

    bool MeshImporter::load_gltf(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots)
    {
        ModelAsset temp_model;
        temp_model.source_path = path.string();
        if (!temp_model.load_gltf(path.string()))
            return false;
        slots = temp_model.get_slots();
        return true;
    }

    bool MeshImporter::load_obj(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots)
    {
        ModelAsset temp_model;
        temp_model.source_path = path.string();
        if (!temp_model.load_obj(path.string()))
            return false;
        slots = temp_model.get_slots();
        return true;
    }

    bool MeshImporter::load_fbx(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots)
    {
        ModelAsset temp_model;
        temp_model.source_path = path.string();
        if (!temp_model.load_fbx(path.string()))
            return false;
        slots = temp_model.get_slots();
        return true;
    }

    bool MeshImporter::load_ply(const std::filesystem::path& path, std::vector<ModelMeshSlot>& slots)
    {
        ModelAsset temp_model;
        temp_model.source_path = path.string();
        if (!temp_model.load_ply(path.string()))
            return false;
        slots = temp_model.get_slots();
        return true;
    }

    void MeshImporter::optimize_mesh(MeshAsset& mesh, const MeshImportSettings& settings)
    {
        DS_UNUSED(settings);
        meshopt_optimize(mesh);
    }

    void MeshImporter::generate_lods(MeshAsset& mesh, uint32_t lod_levels)
    {
        DS_UNUSED(mesh);
        DS_UNUSED(lod_levels);
    }

    void MeshImporter::meshopt_optimize(MeshAsset& mesh)
    {
        if (mesh.vertices.empty() || mesh.indices.empty())
            return;

        std::vector<glm::vec3> positions(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i)
            positions[i] = mesh.vertices[i].Position;

        std::vector<uint32_t> optimized_indices(mesh.indices.size());
        meshopt_optimizeVertexCacheStrip(
            optimized_indices.data(),
            mesh.indices.data(),
            mesh.indices.size(),
            mesh.vertices.size());

        meshopt_optimizeOverdraw(
            optimized_indices.data(),
            optimized_indices.data(),
            mesh.indices.size(),
            reinterpret_cast<const float*>(positions.data()),
            positions.size(),
            sizeof(glm::vec3),
            1.05f);

        mesh.indices = optimized_indices;
    }

} // namespace diverse
