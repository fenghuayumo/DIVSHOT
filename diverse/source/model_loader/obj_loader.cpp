#include "assets/mesh_model.h"
#include "assets/material.h"
#include "maths/transform.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "utility/string_utils.h"
#include "engine/application.h"
#include "assets/asset_manager.h"
#include "core/profiler.h"
#include "utility/thread_pool.h"
#include <future>
#define TINYOBJLOADER_IMPLEMENTATION
#include <ModelLoaders/tinyobjloader/tiny_obj_loader.h>

namespace diverse
{
    std::string m_Directory;

    SharedPtr<asset::Texture> load_material_textures(const std::string& typeName, const std::string& name, const std::string& directory)
    {
        std::string filePath = directory + name;
        filePath             = stringutility::back_slashes_2_slashes(filePath);
        auto texture = ResourceManager<asset::Texture>::get().get_resource(filePath);
        return texture;
    }

    bool MeshModel::load_obj(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        std::string resolvedPath = path;
        tinyobj::attrib_t attrib;
        std::string error;
        std::string warn;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        resolvedPath = stringutility::back_slashes_2_slashes(resolvedPath);
        m_Directory  = stringutility::get_file_location(resolvedPath);

        std::string name = stringutility::get_file_name(resolvedPath);

        bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials,&warn, &error, resolvedPath.c_str(), m_Directory.c_str(),true);

        if(!ok)
        {
            DS_LOG_CRITICAL(error);
            return false;
        }

        meshes.resize(shapes.size());
        parallel_for<size_t>(0, shapes.size(), [&](uint32_t shape_idx){
        //for(auto shape_idx = 0; shape_idx < shapes.size(); shape_idx++) {
            auto& shape = shapes[shape_idx];
            uint32_t vertexCount       = 0;
            const uint32_t numIndices  = static_cast<uint32_t>(shape.mesh.indices.size());
            const uint32_t numVertices = numIndices; // attrib.vertices.size();// numIndices / 3.0f;
            std::vector<Vertex> vertices(numVertices);
            std::vector<uint32_t> indices(numIndices);

            std::unordered_map<Vertex, uint32_t> uniqueVertices;

            maths::BoundingBox boundingBox;
            for(uint32_t i = 0; i < shape.mesh.indices.size(); i++)
            {
                auto& index = shape.mesh.indices[i];
                Vertex vertex;

                if(!attrib.texcoords.empty())
                {
                    vertex.TexCoords = (glm::vec2(
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]));
                }
                else
                {
                    vertex.TexCoords = glm::vec2(0.0f, 0.0f);
                }
                vertex.Position = (glm::vec3(
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]));

                boundingBox.merge(vertex.Position);

                if(!attrib.normals.empty())
                {
                    vertex.Normal = (glm::vec3(
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]));
                }

                glm::vec4 colour = glm::vec4(0.0f);

                if(shape.mesh.material_ids[0] >= 0)
                {
                    tinyobj::material_t* mp = &materials[shape.mesh.material_ids[0]];
                    colour                  = glm::vec4(mp->diffuse[0], mp->diffuse[1], mp->diffuse[2], 1.0f);
                }

                vertex.Colours = colour;

                if(uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertexCount);
                    vertices[vertexCount]  = vertex;
                }

                indices[vertexCount] = uniqueVertices[vertex];

                vertexCount++;
            }

            if(attrib.normals.empty())
               Mesh::generate_normals(vertices.data(), vertexCount, indices.data(), numIndices);
           
            SharedPtr<Material> pbrMaterial = createSharedPtr<Material>();

            PBRMataterialTextures textures;

            if(shape.mesh.material_ids[0] >= 0)
            {
                tinyobj::material_t* mp = &materials[shape.mesh.material_ids[0]];

                if(mp->diffuse_texname.length() > 0)
                {
                    SharedPtr<asset::Texture> texture = load_material_textures("Albedo", mp->diffuse_texname, m_Directory);
                    if(texture)
                        textures.albedo = texture;
                }

                if(mp->bump_texname.length() > 0)
                {
                    SharedPtr<asset::Texture> texture = load_material_textures("Normal", mp->bump_texname, m_Directory);
                    if(texture)
                        textures.normal = texture; // pbrMaterial->SetNormalMap(texture);
                }

                if(mp->roughness_texname.length() > 0)
                {
                    SharedPtr<asset::Texture> texture = load_material_textures("Roughness", mp->roughness_texname.c_str(), m_Directory);
                    if(texture)
                        textures.roughness = texture;
                }

                if(mp->metallic_texname.length() > 0)
                {
                    SharedPtr<asset::Texture> texture = load_material_textures("Metallic", mp->metallic_texname, m_Directory);
                    if(texture)
                        textures.metallic = texture;
                }

                if(mp->specular_highlight_texname.length() > 0)
                {
                    SharedPtr<asset::Texture> texture = load_material_textures("Metallic", mp->specular_highlight_texname, m_Directory);
                    if(texture)
                        textures.metallic = texture;
                }
                auto& material_props = pbrMaterial->get_properties();
                material_props.albedoColour = glm::vec4(mp->diffuse[0], mp->diffuse[1], mp->diffuse[2],1);
                material_props.emissive = glm::vec3(mp->emission[0], mp->emission[1], mp->emission[2]);
                material_props.metalness = mp->metallic;
                material_props.roughness = mp->roughness;
            }

            pbrMaterial->set_textures(textures);
            auto matname = shape.name;
            pbrMaterial->set_name(matname.empty() ? std::format("mat_{}", shape_idx) : matname);
            auto mesh = createSharedPtr<Mesh>(indices, vertices);
            mesh->set_name(shape.name);
            mesh->set_material(pbrMaterial);
            mesh->generate_tangents_bitangents(vertices.data(), uint32_t(numVertices), indices.data(), uint32_t(numIndices));

            meshes[shape_idx] = mesh;
            // meshes.push_back(mesh);
        });
        return true;
    }

}
