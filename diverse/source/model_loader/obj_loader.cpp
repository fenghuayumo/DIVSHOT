#include "assets/model_asset.h"
#include "assets/material_asset.h"
#include "assets/asset_system.h"
#include "assets/asset_registry.h"
#include "model_loader/model_loader_utils.h"
#include "maths/transform.h"
#include "utility/string_utils.h"
#include "core/profiler.h"
#include "core/ds_log.h"
#include "utility/thread_pool.h"
#include <algorithm>
#define TINYOBJLOADER_IMPLEMENTATION
#include <ModelLoaders/tinyobjloader/tiny_obj_loader.h>

namespace diverse
{
    namespace
    {
        AssetHandle<TextureAsset> load_texture_handle(const std::string& name, const std::string& directory)
        {
            std::string file_path = directory + name;
            stringutility::back_slashes_2_slashes(file_path);
            return import_and_register_texture(file_path);
        }
    }

    bool ModelAsset::load_obj(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        std::string resolved_path = path;
        stringutility::back_slashes_2_slashes(resolved_path);
        const auto directory = stringutility::get_file_location(resolved_path);

        tinyobj::attrib_t attrib;
        std::string error;
        std::string warn;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, resolved_path.c_str(), directory.c_str(), true);
        if (!ok)
        {
            DS_LOG_CRITICAL("{}", error);
            return false;
        }
        if (!warn.empty())
            DS_LOG_WARN("{}", warn);

        slots.resize(shapes.size());
        parallel_for<size_t>(0, shapes.size(), [&](uint32_t shape_idx) {
            auto& shape = shapes[shape_idx];
            uint32_t unique_vertex_count = 0;
            const uint32_t num_indices = static_cast<uint32_t>(shape.mesh.indices.size());
            std::vector<Vertex> vertices(num_indices);
            std::vector<uint32_t> indices;
            indices.reserve(num_indices);
            std::unordered_map<Vertex, uint32_t> unique_vertices;

            const int material_id = !shape.mesh.material_ids.empty() ? shape.mesh.material_ids[0] : -1;
            const tinyobj::material_t* material = (material_id >= 0 && material_id < static_cast<int>(materials.size()))
                ? &materials[material_id]
                : nullptr;

            for (uint32_t i = 0; i < shape.mesh.indices.size(); i++)
            {
                auto& index = shape.mesh.indices[i];
                Vertex vertex;

                if (index.vertex_index < 0 || (3 * index.vertex_index + 2) >= static_cast<int>(attrib.vertices.size()))
                {
                    i += 2 - (i % 3);
                    continue;
                }

                if (index.texcoord_index >= 0 && (2 * index.texcoord_index + 1) < static_cast<int>(attrib.texcoords.size()))
                {
                    vertex.TexCoords = glm::vec2(
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);
                }

                vertex.Position = glm::vec3(
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]);

                if (index.normal_index >= 0 && (3 * index.normal_index + 2) < static_cast<int>(attrib.normals.size()))
                {
                    vertex.Normal = glm::vec3(
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]);
                }

                if (material)
                    vertex.Colours = glm::vec4(material->diffuse[0], material->diffuse[1], material->diffuse[2], 1.0f);

                if (unique_vertices.count(vertex) == 0)
                {
                    unique_vertices[vertex] = unique_vertex_count;
                    vertices[unique_vertex_count] = vertex;
                    unique_vertex_count++;
                }
                indices.push_back(unique_vertices[vertex]);
            }

            vertices.resize(unique_vertex_count);
            if (vertices.empty() || indices.empty())
                return;

            auto mesh = std::make_shared<MeshAsset>();
            mesh->id = GenerateAssetId();
            mesh->name = shape.name;
            mesh->vertices = std::move(vertices);
            mesh->indices = std::move(indices);
            if (attrib.normals.empty())
                mesh->generate_normals();
            mesh->generate_tangents_bitangents();
            mesh->calculate_bounding_box();

            auto material_asset = std::make_shared<MaterialAsset>();
            material_asset->id = GenerateAssetId();
            material_asset->name = shape.name.empty() ? std::format("mat_{}", shape_idx) : shape.name;
            material_asset->is_valid = true;

            if (material)
            {
                const tinyobj::material_t* mp = material;
                if (!mp->diffuse_texname.empty())
                    material_asset->albedo = load_texture_handle(mp->diffuse_texname, directory);
                if (!mp->bump_texname.empty())
                    material_asset->normal = load_texture_handle(mp->bump_texname, directory);
                if (!mp->roughness_texname.empty())
                    material_asset->roughness = load_texture_handle(mp->roughness_texname, directory);
                if (!mp->metallic_texname.empty())
                    material_asset->metallic = load_texture_handle(mp->metallic_texname, directory);
                if (!mp->specular_highlight_texname.empty())
                    material_asset->metallic = load_texture_handle(mp->specular_highlight_texname, directory);

                material_asset->properties.base_color_mult = glm::vec4(mp->diffuse[0], mp->diffuse[1], mp->diffuse[2], 1.0f);
                material_asset->properties.emissive = glm::vec3(mp->emission[0], mp->emission[1], mp->emission[2]);
                material_asset->properties.metalness_factor = mp->metallic;
                material_asset->properties.roughness_mult = mp->roughness;
            }

            // Register assets and create handles
            AssetSystem::get_instance().register_cpu_mesh(mesh);
            AssetSystem::get_instance().register_cpu_material(material_asset);

            ModelMeshSlot slot;
            slot.mesh = AssetRegistry::get_instance().get_mesh_handle(mesh->id);
            slot.material = AssetRegistry::get_instance().get_material_handle(material_asset->id);
            slots[shape_idx] = std::move(slot);
        });

        slots.erase(std::remove_if(slots.begin(), slots.end(),
            [](const ModelMeshSlot& slot) { return !slot.mesh.is_valid(); }), slots.end());
        return !slots.empty();
    }
}
