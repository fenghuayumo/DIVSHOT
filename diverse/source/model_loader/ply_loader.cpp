#include "assets/mesh_model.h"
#include "assets/point_cloud.h"
#include "assets/material.h"
#include "maths/transform.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "utility/string_utils.h"
#include "engine/application.h"
#include "assets/asset_manager.h"
#include "core/profiler.h"
#include "utility/thread_pool.h"
#include <future>
#include <tinyply/tinyply.h>

namespace diverse
{
    bool MeshModel::is_mesh_model_file(const std::string& filepath)
    {
        const std::string fileExtension = stringutility::get_file_extension(filepath);
        if(fileExtension == "obj" || fileExtension == "gltf" || 
            fileExtension == "glb" || fileExtension == "fbx" || fileExtension == "FBX")
            return true;
        if (fileExtension == "ply")
        {
            std::ifstream file_stream(filepath, std::ios::binary);
            if (!file_stream.is_open())
            {
                return false;
            }
            tinyply::PlyFile ply_file;
            ply_file.parse_header(file_stream);
            try
            {
                ply_file.request_properties_from_element("face", { "vertex_indices" }, 3);
            }
            catch (const std::exception& e)
            {
                return false;
            }
            return true;
        }
        return false;
    }

    bool MeshModel::is_gaussian_file(const std::string& filepath)
    {
        std::string extension = stringutility::get_file_extension(filepath);

        if ( extension == "splat" || extension == "dsplat" || extension == "dvsplat" || extension == "spz")
            return true;
        if (extension == "ply")
        {
            if(filepath.find(".compressed") != std::string::npos)   return true;
            std::ifstream file_stream(filepath, std::ios::binary);
            if (!file_stream.is_open())
            {
                return false;
            }
            tinyply::PlyFile ply_file;
            ply_file.parse_header(file_stream);
            try
            {
                ply_file.request_properties_from_element("vertex", { "f_dc_0","f_dc_1","f_dc_2" });
            }
            catch (const std::exception& e)
            {
                return false;
            }
            return true;
        }
        return false;
    }

    bool PointCloud::is_point_cloud_file(const std::string& filepath)
    {
        std::string extension = stringutility::get_file_extension(filepath);
        if (extension == "ply")
        {
            std::ifstream file_stream(filepath, std::ios::binary);
            if (!file_stream.is_open())
            {
                return false;
            }
            tinyply::PlyFile ply_file;
            ply_file.parse_header(file_stream);
            try
            {
                ply_file.request_properties_from_element("vertex", { "x","y","z" });
                ply_file.request_properties_from_element("vertex", { "red", "green", "blue" });
            }
            catch (const std::exception& e)
            {
                return false;
            }
            try{
                ply_file.request_properties_from_element("face", { "vertex_indices" }, 3);
            }
            catch (const std::exception& e)
            {
                return true;
            }
            return false;
        }
        return false;
    }
    bool MeshModel::load_ply(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        //read mesh data 
        std::ifstream file_stream(path, std::ios::binary);
        if (!file_stream.is_open())
        {
            std::cerr << "Failed to open PLY file: " << path << std::endl;
            return false;
        }

        tinyply::PlyFile ply_file;
        ply_file.parse_header(file_stream);
        std::shared_ptr<tinyply::PlyData> vertices, normals, colors, texcoords, faces;
        try
        {
            vertices = ply_file.request_properties_from_element("vertex", { "x", "y", "z" });
            faces = ply_file.request_properties_from_element("face", { "vertex_indices" }, 3);
        }
        catch (const std::exception & e)
        {
            DS_LOG_ERROR("Error reading PLY properties: {}", e.what() );
            return false;
        }

        ply_file.read(file_stream);
        try
        {
            normals = ply_file.request_properties_from_element("vertex", { "nx", "ny", "nz" });
            colors = ply_file.request_properties_from_element("vertex", { "red", "green", "blue", "alpha" });
            texcoords = ply_file.request_properties_from_element("vertex", { "u", "v" });
        }
        catch (const std::exception& e)
        {
            DS_LOG_WARN("Error reading PLY properties: {}", e.what());
        }
        std::vector<float> normal_data, texcoord_data;
        std::vector<uint32_t> index_data;
        std::vector<uint8_t> color_data;
        if (faces)
        {
            index_data.resize(faces->count * 3);
            std::memcpy(index_data.data(), faces->buffer.get(), faces->buffer.size_bytes());
        }
        if (normals)
        {
            normal_data.resize(normals->count * 3);
            std::memcpy(normal_data.data(), normals->buffer.get(), normals->buffer.size_bytes());
        }

        if (colors)
        {
            auto cnt = colors->buffer.size_bytes() / colors->count;
            color_data.resize(colors->count * cnt);
            std::memcpy(color_data.data(), colors->buffer.get(), colors->buffer.size_bytes());
        }
        if (texcoords)
        {
            texcoord_data.resize(texcoords->count * 2);
            std::memcpy(texcoord_data.data(), texcoords->buffer.get(), texcoords->buffer.size_bytes());
        }
        if( vertices)
        {
            std::vector<Vertex> vertices_data(vertices->count);
            // meshes.resize(1);
            float* vertex_data = reinterpret_cast<float*>(vertices->buffer.get());
            double* vertex_data_double = reinterpret_cast<double*>(vertices->buffer.get());
            parallel_for<size_t>(0, vertices_data.size(),[&](size_t i){
                vertices_data[i].Position = vertices->t == tinyply::Type::FLOAT32 ? glm::vec3(vertex_data[i * 3 + 0], vertex_data[i * 3 + 1],vertex_data[i * 3 + 2]) : glm::vec3(vertex_data_double[i * 3 + 0], vertex_data_double[i * 3 + 1], vertex_data_double[i * 3 + 2]);

                if( texcoord_data.size() > 0 )
                    vertices_data[i].TexCoords = glm::vec2(texcoord_data[i * 2 + 0], texcoord_data[i * 2 + 1]);

                if(normal_data.size() > 0)
                    vertices_data[i].Normal = glm::vec3(normal_data[i*3+0],normal_data[i*3+1],normal_data[i*3+2]);
                if(color_data.size() > 0){
                    auto cnt = colors->buffer.size_bytes() / colors->count;
                    vertices_data[i].Colours = glm::vec4(color_data[i* cnt +0] / 255.0f,color_data[i* cnt +1] / 255.0f,color_data[i* cnt +2] / 255.0f, 1.0f);
                }
            });
            if (normal_data.empty())
                Mesh::generate_normals(vertices_data.data(), vertices_data.size(), index_data.data(), index_data.size());

            SharedPtr<Material> pbrMaterial = createSharedPtr<Material>();

            PBRMataterialTextures textures;
            pbrMaterial->set_textures(textures);
            pbrMaterial->set_name("defalut");
            auto mesh = createSharedPtr<Mesh>(index_data, vertices_data);
            mesh->set_name(stringutility::get_file_name(path));
            mesh->set_material(pbrMaterial);
            mesh->generate_tangents_bitangents(vertices_data.data(), uint32_t(vertices_data.size()), index_data.data(), uint32_t(index_data.size()));

            meshes.push_back(mesh);
        }
        else
            return false;
        return true;
    }

    
    bool PointCloud::load_ply(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        //read mesh data 
        std::ifstream file_stream(path, std::ios::binary);
        if (!file_stream.is_open())
        {
            std::cerr << "Failed to open PLY file: " << path << std::endl;
            return false;
        }
        tinyply::PlyFile ply_file;
        ply_file.parse_header(file_stream);
        std::shared_ptr<tinyply::PlyData> vertices, colors;
        try
        {
            vertices = ply_file.request_properties_from_element("vertex", { "x", "y", "z" });
            colors = ply_file.request_properties_from_element("vertex", { "red", "green", "blue" });
        }
        catch (const std::exception & e)
        {
            DS_LOG_ERROR("Error reading PLY properties: {}", e.what() );
            return false;
        }
        ply_file.read(file_stream);
        std::vector<uint8_t> color_data;
        if (colors)
        {
            auto cnt = colors->buffer.size_bytes() / colors->count;
            color_data.resize(colors->count * cnt);
            std::memcpy(color_data.data(), colors->buffer.get(), colors->buffer.size_bytes());
        }
        if( vertices)
        {
            pcd_vertex.resize(vertices->count);
            // meshes.resize(1);
            float* vertex_data = reinterpret_cast<float*>(vertices->buffer.get());
            double* vertex_data_double = reinterpret_cast<double*>(vertices->buffer.get());
            parallel_for<size_t>(0, pcd_vertex.size(),[&](size_t i){
                pcd_vertex[i].position = vertices->t == tinyply::Type::FLOAT32 ? glm::vec3(vertex_data[i * 3 + 0], vertex_data[i * 3 + 1], vertex_data[i * 3 + 2]) : 
                                                                                    glm::vec3(vertex_data_double[i * 3 + 0], vertex_data_double[i * 3 + 1], vertex_data_double[i * 3 + 2]);

                if(color_data.size() > 0){
                    auto cnt = colors->buffer.size_bytes() / colors->count;
                    pcd_vertex[i].color = color_data[i* cnt +0] << 16 | color_data[i* cnt +1] << 8 | color_data[i* cnt +2];
                }
            });
        }
        else
            return false;
        return true;
    }

}