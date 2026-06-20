#include "model_asset.h"
#include "model_asset_loader.h"
#include "asset_system.h"
#include "mesh_factory.h"
#include "core/profiler.h"
#include <utility/triangle_utils.h>
#include <glm/gtc/matrix_transform.hpp>

namespace diverse
{
    void ModelAsset::assign_from(const ModelAsset& src)
    {
        id = src.id;
        source_path = src.source_path;
        primitive_type = src.primitive_type;
        slots = src.slots;
        local_bounding_box = src.local_bounding_box;
        skeleton = src.skeleton;
        bind_poses = src.bind_poses;
        animation = src.animation;
        loaded = src.loaded;
        invalid = src.invalid;
    }

    ModelAsset::ModelAsset(const std::string& file_path)
        : source_path(file_path)
        , primitive_type(PrimitiveType::File)
    {
        load_model(file_path);
    }

    ModelAsset::ModelAsset(PrimitiveType type)
        : primitive_type(type)
    {
        if (auto model = AssetSystem::get_instance().load_primitive(type))
            assign_from(*model);
        else
            load_primitive(type);
    }

    void ModelAsset::add_slot(std::shared_ptr<MeshAsset> mesh, std::shared_ptr<MaterialAsset> material)
    {
        ModelMeshSlot slot;
        if (mesh)
            slot.mesh = AssetRegistry::get_instance().get_mesh_handle(mesh->id);
        if (material)
            slot.material = AssetRegistry::get_instance().get_material_handle(material->id);
        slots.push_back(slot);
    }

    size_t ModelAsset::calculate_memory_size() const
    {
        size_t total = source_path.capacity();
        for (const auto& slot : slots)
        {
            auto mesh = slot.get_mesh();
            if (mesh)
                total += mesh->calculate_memory_size();
            auto material = slot.get_material();
            if (material)
                total += material->calculate_memory_size();
        }
        return total;
    }

    void ModelAsset::load_primitive(PrimitiveType type)
    {
        primitive_type = type;
        id = GenerateAssetId();
        slots.clear();
        ModelMeshSlot slot;
        auto mesh = create_primitive_mesh(type);
        auto material = ::diverse::create_default_material();
        if (mesh)
            slot.mesh = AssetRegistry::get_instance().get_mesh_handle(mesh->id);
        if (material)
            slot.material = AssetRegistry::get_instance().get_material_handle(material->id);
        if (slot.mesh.is_valid() || slot.material.is_valid())
            slots.push_back(std::move(slot));
        loaded = !slots.empty();
        invalid = slots.empty();
        if (loaded)
            get_local_bounding_box();
    }

    size_t ModelAsset::get_vertex_count() const
    {
        size_t count = 0;
        for (const auto& slot : slots)
        {
            auto mesh = slot.get_mesh();
            if (mesh)
                count += mesh->get_vertex_count();
        }
        return count;
    }

    maths::BoundingBox& ModelAsset::get_local_bounding_box()
    {
        if (local_bounding_box.defined())
            return local_bounding_box;
        for (const auto& slot : slots)
        {
            auto mesh = slot.get_mesh();
            if (mesh)
                mesh->calculate_bounding_box();
        }
        for (const auto& slot : slots)
        {
            auto mesh = slot.get_mesh();
            if (mesh)
                local_bounding_box.merge(mesh->bounding_box);
        }
        return local_bounding_box;
    }

    auto ModelAsset::get_world_bounding_box(const glm::mat4& t) -> maths::BoundingBox
    {
        return get_local_bounding_box().transformed(t);
    }

    void ModelAsset::load_model(const std::string& path, bool preserve_origin)
    {
        DS_PROFILE_FUNCTION();
        loaded = false;
        invalid = false;
        slots.clear();
        local_bounding_box.clear();
        source_path = path;

        auto model = AssetSystem::get_instance().load_model(path, preserve_origin);
        if (!model)
        {
            invalid = true;
            DS_LOG_INFO("Failed to load ModelAsset - {0}", path);
            return;
        }

        assign_from(*model);
        DS_LOG_INFO("Loaded ModelAsset - {0}", path);
    }

    void ModelAsset::reset_center()
    {
        local_bounding_box = get_local_bounding_box();
        for (auto& slot : slots)
        {
            auto mesh = slot.get_mesh();
            if (!mesh)
                continue;
            for (auto& vertex : mesh->vertices)
                vertex.Position -= local_bounding_box.center();
            mesh->calculate_bounding_box();
        }
        local_bounding_box.transform(glm::translate(glm::mat4(1.0f), -local_bounding_box.center()));
    }

    auto ModelAsset::uv_to_surface_position(const glm::vec2& uv, glm::vec3& surface_pos) -> bool
    {
        for (const auto& slot : slots)
        {
            auto mesh = slot.get_mesh();
            if (!mesh)
                continue;
            const auto face_count = mesh->indices.size() / 3;
            for (size_t i = 0; i < face_count; ++i)
            {
                const auto& uv0 = mesh->vertices[mesh->indices[i * 3 + 0]].TexCoords;
                const auto& uv1 = mesh->vertices[mesh->indices[i * 3 + 1]].TexCoords;
                const auto& uv2 = mesh->vertices[mesh->indices[i * 3 + 2]].TexCoords;
                if (point_in_triangle_barycentric(uv, uv0, uv1, uv2))
                {
                    glm::vec3 bary;
                    calculate_barycentric(uv, uv0, uv1, uv2, bary);
                    const auto& pos0 = mesh->vertices[mesh->indices[i * 3]].Position;
                    const auto& pos1 = mesh->vertices[mesh->indices[i * 3 + 1]].Position;
                    const auto& pos2 = mesh->vertices[mesh->indices[i * 3 + 2]].Position;
                    surface_pos = bary.x * pos0 + bary.y * pos1 + bary.z * pos2;
                    return true;
                }
            }
        }
        return false;
    }

    MeshAsset* ModelMeshSlot::get_mesh() const
    {
        if (!mesh.is_valid())
            return nullptr;
        return AssetSystem::get_instance().get_asset<MeshAsset>(mesh.get_id()).get();
    }

    MaterialAsset* ModelMeshSlot::get_material() const
    {
        if (!material.is_valid())
            return nullptr;
        return AssetSystem::get_instance().get_asset<MaterialAsset>(material.get_id()).get();
    }
}
