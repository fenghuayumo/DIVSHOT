#include "mesh.h"
#include <ModelLoaders/meshoptimizer/src/meshoptimizer.h>
#include <glm/gtx/norm.hpp>
#include "backend/drs_rhi/gpu_device.h"
#include "backend/drs_rhi/buffer_builder.h"
#include "utility/pack_utils.h"
#include "utility/thread_pool.h"

namespace diverse
{
    Mesh::Mesh()
        : vertex_buffer(nullptr)
        , index_buffer(nullptr)
        , indices()
        , vertices()
    {
    }

    Mesh::Mesh(const Mesh& mesh)
        : vertex_buffer(mesh.vertex_buffer)
        , index_buffer(mesh.index_buffer)
        , bounding_box(mesh.bounding_box)
        , name(mesh.name)
        , indices(mesh.indices)
        , vertices(mesh.vertices)
    {
    }
    Mesh::Mesh(const std::vector<uint32_t>& indices_data, const std::vector<Vertex>& vertices_data, bool optimise, float optimiseThreshold)
    {
        // int lod = 2;
        // float threshold = powf(0.7f, float(lod));
        indices = indices_data;
        vertices = vertices_data;

        if (optimise)
        {
            size_t indexCount = indices.size();
            size_t target_index_count = size_t(indices.size() * optimiseThreshold);

            float target_error = 1e-3f;
            float* resultError = nullptr;

            auto newIndexCount = meshopt_simplify(indices.data(), indices.data(), indices.size(), (const float*)(&vertices[0]), vertices.size(), sizeof(diverse::Vertex), target_index_count, target_error, resultError);

            auto newVertexCount = meshopt_optimizeVertexFetch( // return vertices (not vertex attribute values)
                (vertices.data()),
                (unsigned int*)(indices.data()),
                newIndexCount, // total new indices (not faces)
                (vertices.data()),
                (size_t)vertices.size(), // total vertices (not vertex attribute values)
                sizeof(diverse::Vertex)   // vertex stride
            );

            vertices.resize(newVertexCount);
            indices.resize(newIndexCount);

            //DS_LOG_INFO("Mesh Optimizer - Before : {0} indices {1} vertices , After : {2} indices , {3} vertices", indexCount, m_Vertices.size(), newIndexCount, newVertexCount);
        }
        
        for (auto& vertex : vertices)
        {
            bounding_box.merge(vertex.Position);
        }

        stats.VertexCount = (uint32_t)vertices.size();
        stats.TriangleCount = stats.VertexCount / 3;
        stats.IndexCount = (uint32_t)indices.size();
        stats.OptimiseThreshold = optimiseThreshold;

        // const bool storeData = true;//false;
        // if (!storeData)
        // {
        //     indices.clear();
        //     indices.shrink_to_fit();

        //     vertices.clear();
        //     vertices.shrink_to_fit();
        // }
    }

    Mesh::Mesh(const std::vector<uint32_t>& indices, std::vector<AnimVertex>& vert_data)
    {
        this->indices = indices;
         //this->vertices = vertices;
         this->vertices.resize(vert_data.size());
        for( auto i = 0;i<vert_data.size();i++)
        {
            const auto& vertex = vert_data[i];
            bounding_box.merge(vertex.Position);
            vertices[i].Colours = vertex.Colours;
            vertices[i].Position = vertex.Position;
            vertices[i].Normal = vertex.Normal;
            vertices[i].Tangent = vertex.Tangent;
            vertices[i].TexCoords = vertex.TexCoords;
            vertices[i].Bitangent = vertex.Bitangent;
        }
        stats.VertexCount = (uint32_t)vertices.size();
        stats.TriangleCount = stats.VertexCount / 3;
        stats.IndexCount = (uint32_t)indices.size();
    }
    Mesh::~Mesh()
    {
    }

    void Mesh::calculate_bounding_box()
    {
        for (auto& vertex : vertices)
        {
            bounding_box.merge(vertex.Position);
        }
    }
    void Mesh::create_gpu_buffer()
    {
        PackedVertices packed_vertices; 
        packed_vertices.resize(vertices.size());
        // for (auto v_id = 0; v_id < vertices.size(); v_id++)
        parallel_for<size_t>(0, vertices.size(), [&](size_t v_id){
            auto pos = vertices[v_id].Position;
            auto nor = vertices[v_id].Normal;

            auto v_pos_nor = PackedPosNormal{ pos, pack_unit_direction_11_10_11(nor) };
            packed_vertices.pos_normals[v_id] = v_pos_nor;
            packed_vertices.uvs[v_id] = vertices[v_id].TexCoords;
            packed_vertices.tangents[v_id] = pack_unit_direction_11_10_11(vertices[v_id].Tangent);
            packed_vertices.colors[v_id] = pack_color_8888(vertices[v_id].Colours);
        });
        vertex_pos_nor_offset = 0;
        vertex_uv_offset = sizeof(PackedPosNormal) * vertices.size();
        vertex_tangent_offset = vertex_uv_offset + sizeof(glm::vec2) * vertices.size();
        vertex_color_offset = vertex_tangent_offset + sizeof(u32) * vertices.size();
        auto total_size = vertex_color_offset + sizeof(u32) * vertices.size();
        auto index_des = rhi::GpuBufferDesc::new_gpu_only(indices.size() * sizeof(u32),
            rhi::BufferUsageFlags::STORAGE_BUFFER
            | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
            | rhi::BufferUsageFlags::INDEX_BUFFER
            | rhi::BufferUsageFlags::TRANSFER_DST);
        auto vertex_desc = rhi::GpuBufferDesc::new_gpu_only(total_size,
            rhi::BufferUsageFlags::STORAGE_BUFFER
            | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
            | rhi::BufferUsageFlags::VERTEX_BUFFER
            | rhi::BufferUsageFlags::TRANSFER_DST);
        if (g_device->gpu_limits.ray_tracing_enabled)
        {
            index_des.usage |= rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY;
            vertex_desc.usage |= rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY;
        }
        std::vector<u8> packed_data(total_size);
        memcpy(packed_data.data(), packed_vertices.pos_normals.data(), vertex_uv_offset);
        memcpy(packed_data.data() + vertex_uv_offset, packed_vertices.uvs.data(), sizeof(glm::vec2) * vertices.size());
        memcpy(packed_data.data() + vertex_tangent_offset, packed_vertices.tangents.data(), sizeof(u32) * vertices.size());
        memcpy(packed_data.data() + vertex_color_offset, packed_vertices.colors.data(), sizeof(u32) * vertices.size());

        index_buffer = g_device->create_buffer(index_des, "mesh_index_buf", (u8*)indices.data());
        vertex_buffer = g_device->create_buffer(vertex_desc, "mesh_vert_buf", (u8*)packed_data.data());
    }

    void Mesh::translate(const glm::vec3& center)
    {
         parallel_for<size_t>(0, vertices.size(), [&](size_t v_id) {
            auto& v = vertices[v_id];
             v.Position -= center;
         });
         glm::mat4 extr_transform = glm::translate(glm::mat4(1.0), -center);
         bounding_box.transform(extr_transform);
    }

    void Mesh::generate_normals(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t indexCount)
    {
        std::vector<glm::vec3> normals(vertexCount);
        if (indices)
        {
            const auto numTriangles = indexCount / 3;
            parallel_for<size_t>(0, numTriangles, [&](size_t i){
                const int a = indices[i * 3];
                const int b = indices[i * 3 + 1];
                const int c = indices[i * 3 + 2];

                const glm::vec3 _normal = glm::cross((vertices[b].Position - vertices[a].Position), (vertices[c].Position - vertices[a].Position));

                normals[a] = _normal;
                normals[b] = _normal;
                normals[c] = _normal;
            });
        }
        else
        {
            // It's just a list of triangles, so generate face normals
            const auto numFaces = vertexCount / 3;
            parallel_for<size_t>(0, numFaces, [&](size_t i){
                glm::vec3& a = vertices[i * 3 + 0].Position;
                glm::vec3& b = vertices[i * 3 + 1].Position;
                glm::vec3& c = vertices[i * 3 + 2].Position;

                const glm::vec3 _normal = glm::cross(b - a, c - a);

                normals[i * 3 + 0] = _normal;
                normals[i * 3 + 1] = _normal;
                normals[i * 3 + 2] = _normal;
            });
        }

        parallel_for<size_t>(0, vertexCount, [&](size_t i){
            vertices[i].Normal =  glm::normalize(normals[i]);
        });
    }

#define CHECK_VEC3(vec3) maths::IsInf(vec3.x) || maths::IsInf(vec3.y) || maths::IsInf(vec3.z) || maths::IsNaN(vec3.x) || maths::IsNaN(vec3.y) || maths::IsNaN(vec3.z)

    void Mesh::generate_tangents_bitangents(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t numIndices)
    {
        for (uint32_t i = 0; i < vertexCount; i++)
        {
            vertices[i].Tangent = glm::vec3(0.0f);
            vertices[i].Bitangent = glm::vec3(0.0f);
        }

        for (uint32_t i = 0; i < numIndices; i += 3)
        {
            glm::vec3 v0 = vertices[indices[i]].Position;
            glm::vec3 v1 = vertices[indices[i + 1]].Position;
            glm::vec3 v2 = vertices[indices[i + 2]].Position;

            glm::vec2 uv0 = vertices[indices[i]].TexCoords;
            glm::vec2 uv1 = vertices[indices[i + 1]].TexCoords;
            glm::vec2 uv2 = vertices[indices[i + 2]].TexCoords;

            glm::vec3 n0 = vertices[indices[i]].Normal;
            glm::vec3 n1 = vertices[indices[i + 1]].Normal;
            glm::vec3 n2 = vertices[indices[i + 2]].Normal;

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;

            glm::vec2 deltaUV1 = uv1 - uv0;
            glm::vec2 deltaUV2 = uv2 - uv0;

            float den = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
            if (den < maths::M_EPSILON)
                den = 1.0f;

            float f = 1.0f / den;

            glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
            glm::vec3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

            // Store tangent and bitangent for each vertex of the triangle
            vertices[indices[i]].Tangent += tangent;
            vertices[indices[i + 1]].Tangent += tangent;
            vertices[indices[i + 2]].Tangent += tangent;

            vertices[indices[i]].Bitangent += bitangent;
            vertices[indices[i + 1]].Bitangent += bitangent;
            vertices[indices[i + 2]].Bitangent += bitangent;
        }

        // Normalize the tangent and bitangent vectors
        for (uint32_t i = 0; i < vertexCount; i++)
        {
            if (glm::length2(vertices[i].Tangent) > maths::M_EPSILON)
                vertices[i].Tangent = glm::normalize(vertices[i].Tangent);
            else
                vertices[i].Tangent = glm::vec3(0.0f);

            if (glm::length2(vertices[i].Tangent) > maths::M_EPSILON)
                vertices[i].Bitangent = glm::normalize(vertices[i].Bitangent);
            else
                vertices[i].Bitangent = glm::vec3(0.0f);

            DS_ASSERT(!CHECK_VEC3(vertices[i].Tangent));
            DS_ASSERT(!CHECK_VEC3(vertices[i].Bitangent));
        }
    }

    glm::vec3 Mesh::generate_tangent(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec2& ta, const glm::vec2& tb, const glm::vec2& tc)
    {
        const glm::vec2 coord1 = tb - ta;
        const glm::vec2 coord2 = tc - ta;

        const glm::vec3 vertex1 = b - a;
        const glm::vec3 vertex2 = c - a;

        const glm::vec3 axis = glm::vec3(vertex1 * coord2.y - vertex2 * coord1.y);

        const float factor = 1.0f / (coord1.x * coord2.y - coord2.x * coord1.y);

        return axis * factor;
    }

    void Mesh::calculate_triangles()
    {
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            triangles.emplace_back(vertices[indices[i + 0]], vertices[indices[i + 1]], vertices[indices[i + 2]]);
        }
    }
}