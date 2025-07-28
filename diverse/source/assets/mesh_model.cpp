#include "mesh_model.h"
#include "utility/string_utils.h"
#include "engine/file_system.h"
#include "core/profiler.h"
#include "assets/asset_manager.h"
#include <utility/triangle_utils.h>
#include "backend/drs_rhi/gpu_device.h"
namespace diverse
{
    MeshModel::MeshModel(const std::string& filePath)
        : file_path(filePath)
        , primitive_type(PrimitiveType::File)
    {
        load_model(file_path);
    }

    MeshModel::MeshModel(const SharedPtr<Mesh>& mesh, PrimitiveType type)
        : primitive_type(type)
    {
        meshes.push_back(mesh);
        meshes.back()->create_gpu_buffer();
        set_flag(AssetFlag::Loaded);
        auto mat = ResourceManager<Material>::get().get_default_resource("default");
        meshes.back()->set_material(mat);
    }

    MeshModel::MeshModel(PrimitiveType type)
        : primitive_type(type)
    {
        meshes.push_back(SharedPtr<Mesh>(CreatePrimative(type)));
        meshes.back()->create_gpu_buffer();
        set_flag(AssetFlag::Loaded);
        auto mat = ResourceManager<Material>::get().get_default_resource("default");
        meshes.back()->set_material(mat);
    }

    MeshModel::~MeshModel()
    {
    }

    size_t MeshModel::get_vertex_count() const
    {
        if( meshes.size() == 0) return 0;
        size_t s = 0;
        for (const auto& m : meshes)
            s += m->get_vertex_count();
        return s;
    }

    maths::BoundingBox& MeshModel::get_local_bounding_box()
    {
        if(local_bounding_box.defined())
        {
            return local_bounding_box;
        }
        for(auto& mesh : meshes)
            local_bounding_box.merge(mesh->get_bounding_box());
        //reset_center();
        return local_bounding_box;
    }

    auto MeshModel::get_world_bounding_box(const glm::mat4& t) -> maths::BoundingBox
    {
        return local_bounding_box.transformed(t);
    }

    auto  MeshModel::reset_center() -> void
    {
        local_bounding_box = get_local_bounding_box();
        for (auto& mesh : meshes)
            mesh->translate(local_bounding_box.center());
        glm::mat4 extr_transform = glm::translate(glm::mat4(1.0), -local_bounding_box.center());
        local_bounding_box.transform(extr_transform);
    }

    void MeshModel::load_model(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        file_path = path;
        std::string physicalPath;
        if (!diverse::FileSystem::get().resolve_physical_path(path, physicalPath))
        {
            DS_LOG_INFO("Failed to load MeshModel - {0}", path);
            return;
        }

        std::string resolvedPath = physicalPath;

        const std::string fileExtension = stringutility::get_file_extension(path);
        bool ret = false;
        if (fileExtension == "obj")
            ret = load_obj(resolvedPath);
        else if (fileExtension == "gltf" || fileExtension == "glb")
            ret = load_gltf(resolvedPath);
        else if (fileExtension == "fbx" || fileExtension == "FBX")
            ret = load_fbx(resolvedPath);
        else if(fileExtension == "ply")
            ret = load_ply(resolvedPath);
        else
            DS_LOG_ERROR("Unsupported File Type : {0}", fileExtension);
        if (!ret)
        {
            set_flag(AssetFlag::Invalid);
            return;
        }
        reset_center();
        for (auto& mesh : meshes)
            mesh->create_gpu_buffer();
        set_flag(AssetFlag::Loaded);
        DS_LOG_INFO("Loaded MeshModel - {0}", path);
    }

    auto    MeshModel::uv_to_surface_postion(const glm::vec2& uv, glm::vec3& surface_pos)->bool
    {
        for(const auto& mesh : meshes)
        {
            const auto face_count = mesh->indices.size() / 3;
            for (size_t i = 0; i < face_count; i++)
            {
                const auto& uv0 = mesh->vertices[mesh->indices[i * 3 + 0]].TexCoords;
                const auto& uv1 = mesh->vertices[mesh->indices[i * 3 + 1]].TexCoords;
                const auto& uv2 = mesh->vertices[mesh->indices[i * 3 + 2]].TexCoords;

                if(point_in_triangle_barycentric(uv,uv0,uv1,uv2))
                {
                    glm::vec3 bary;
                    calculate_barycentric(uv, uv0, uv1, uv2,bary);
                    auto pos0 = mesh->vertices[mesh->indices[i * 3]].Position;
                    auto pos1 = mesh->vertices[mesh->indices[i * 3 + 1]].Position;
                    auto pos2 = mesh->vertices[mesh->indices[i * 3 + 2]].Position;
                    
                    surface_pos = bary.x * pos0 + bary.y * pos1 + bary.z * pos2;

                    return true;
                }
            }
        }
        return false;
    }

    auto    MeshModel::get_uv2pos_map(const std::shared_ptr<rhi::GpuTexture>& tex)->std::shared_ptr<asset::Texture>
    {
        if (!tex) return {};
        static std::unordered_map<rhi::GpuTexture*,std::shared_ptr<asset::Texture>> uv2pos_map;
        if( uv2pos_map.find(tex.get()) == uv2pos_map.end())
        {
            auto width = tex->desc.extent[0];
            auto height = tex->desc.extent[1];
            std::vector<u8> data(width * height * 3 * sizeof(float));
            parallel_for<size_t>(0, width * height, [&](size_t i) {
                auto x = i % width;
                auto y = i / width;
                auto uv = glm::vec2(x,y)/glm::vec2(width,height);
                glm::vec3 surface_pos;
                if (uv_to_surface_postion(uv, surface_pos)) 
                {
                    memcpy(&data[i * sizeof(float) * 3],&surface_pos, sizeof(float) * 3);
                }
            });
            auto uv2pos = std::make_shared<asset::Texture>(width,height,data,PixelFormat::R32G32B32_Float);
            uv2pos_map[tex.get()] = uv2pos;
        }
        return uv2pos_map[tex.get()];
    }

}