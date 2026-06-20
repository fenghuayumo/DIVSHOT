#pragma once

#include "asset_id.h"
#include "cpu_assets.h"
#include "material_asset.h"
#include "core/reference.h"
#include "maths/bounding_box.h"
#include "primitive_type.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace diverse
{
    class Skeleton;
    class Animation;    struct ModelMeshSlot
    {
        std::shared_ptr<MeshAsset> mesh;
        std::shared_ptr<MaterialAsset> material;
    };

    class ModelAsset
    {
    public:
        ModelAsset() = default;
        explicit ModelAsset(const std::string& file_path);
        explicit ModelAsset(PrimitiveType type);

        AssetId id;
        std::string source_path;
        PrimitiveType primitive_type = PrimitiveType::None;
        std::vector<ModelMeshSlot> slots;
        maths::BoundingBox local_bounding_box;

        SharedPtr<Skeleton> skeleton;
        std::vector<glm::mat4> bind_poses;
        std::vector<SharedPtr<Animation>> animation;
        bool is_loaded() const { return loaded; }
        bool is_invalid() const { return invalid; }

        const std::vector<ModelMeshSlot>& get_slots() const { return slots; }
        std::vector<ModelMeshSlot>& get_slots() { return slots; }

        size_t get_vertex_count() const;
        maths::BoundingBox& get_local_bounding_box();
        auto get_world_bounding_box(const glm::mat4& t) -> maths::BoundingBox;

        void load_model(const std::string& path, bool preserve_origin = false);
        void reset_center();
        auto uv_to_surface_position(const glm::vec2& uv, glm::vec3& surface_pos) -> bool;

        static bool is_mesh_model_file(const std::string& filepath);
        static bool is_gaussian_file(const std::string& filepath);

        void load_primitive(PrimitiveType type);
        void add_slot(std::shared_ptr<MeshAsset> mesh, std::shared_ptr<MaterialAsset> material);

        size_t calculate_memory_size() const;

        bool load_obj(const std::string& path);
        bool load_gltf(const std::string& path);
        bool load_fbx(const std::string& path);
        bool load_ply(const std::string& path);

        void assign_from(const ModelAsset& other);

        void mark_loaded(bool value = true) { loaded = value; }
        void mark_invalid(bool value = true) { invalid = value; }

    private:
        bool loaded = false;
        bool invalid = false;
        u32 num_select_verts = 0;
        u32 num_select_faces = 0;
    };
}
