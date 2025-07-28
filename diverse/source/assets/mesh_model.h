#pragma once

#include "mesh_factory.h"
#include "mesh.h"
#include "assets/material.h"
#include "engine/file_system.h"
#include "assets/asset.h"
#include "core/reference.h"
#include "maths/transform.h"
#include <cereal/cereal.hpp>
namespace diverse
{
	class Skeleton;
	class Animation;
	class MeshModel : public Asset
	{
	public:
    public:
        MeshModel() = default;
        MeshModel(const std::string& filePath);
        MeshModel(const SharedPtr<Mesh>& mesh, PrimitiveType type);
        MeshModel(PrimitiveType type);

        ~MeshModel();

        auto    reset_center()->void;
        auto    get_world_bounding_box(const glm::mat4& t)->maths::BoundingBox;
        maths::BoundingBox& get_local_bounding_box();
        std::vector<SharedPtr<Mesh>>& get_meshes_ref() { return meshes; }
        const std::vector<SharedPtr<Mesh>>& get_meshes() const { return meshes; }
        void    add_mesh(SharedPtr<Mesh> mesh) { meshes.push_back(mesh); }

        auto    has_select_vertex() ->bool {return num_select_verts > 0;} 
        auto    num_selected_vertex() ->u32 {return num_select_verts;}
        auto    num_selected_faces() ->u32 {return num_select_faces;}
        auto    uv_to_surface_postion(const glm::vec2& uv, glm::vec3& surface_pos)->bool;
        auto    get_uv2pos_map(const std::shared_ptr<rhi::GpuTexture>& tex)-> std::shared_ptr<asset::Texture>;
        size_t  get_vertex_count() const;
        template <typename Archive>
        void save(Archive& archive) const
        {
            if (meshes.size() > 0)
            {
                std::string newPath;
                FileSystem::get().absolute_path_2_fileSystem(file_path, newPath);

                auto material = std::unique_ptr<Material>(meshes.front()->get_material().get());
                archive(cereal::make_nvp("PrimitiveType", primitive_type), cereal::make_nvp("FilePath", newPath), cereal::make_nvp("Material", material));
                material.release();
            }
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            auto material = std::unique_ptr<Material>();

            archive(cereal::make_nvp("PrimitiveType", primitive_type), cereal::make_nvp("FilePath", file_path), cereal::make_nvp("Material", material));

            meshes.clear();

            if (primitive_type != PrimitiveType::File)
            {
                meshes.push_back(SharedPtr<Mesh>(CreatePrimative(primitive_type)));
                meshes.back()->set_material(SharedPtr<Material>(material.get()));
                material.release();
            }
            else
            {
                load_model(file_path);
            }
        }

        const std::string& get_file_path() const { return file_path; }
        PrimitiveType get_primitive_type() { return primitive_type; }
        void set_primitive_type(PrimitiveType type) { primitive_type = type; }
        SET_ASSET_TYPE(AssetType::MeshModel);
        
        maths::BoundingBox	local_bounding_box;

    public:
        static bool is_mesh_model_file(const std::string& filepath);
        static bool is_gaussian_file(const std::string& filepath);
    private:
        PrimitiveType primitive_type = PrimitiveType::None;
        std::vector<SharedPtr<Mesh>> meshes;
        std::string file_path;

        bool load_obj(const std::string& path);
        bool load_gltf(const std::string& path);
        bool load_fbx(const std::string& path);
        bool load_ply(const std::string& path);
        SharedPtr<Skeleton> skeleton;
        std::vector<SharedPtr<Animation>> animation;
        std::vector<glm::mat4> bind_poses;
        u32     num_select_verts = 0;
        u32     num_select_faces = 0;
    public:
        void load_model(const std::string& path);
    };
}