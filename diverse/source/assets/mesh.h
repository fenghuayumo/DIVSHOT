#pragma once
#include "core/core.h"
#include "maths/maths_utils.h"
#include "maths/bounding_box.h"
#include <glm/gtx/hash.hpp>
#include <array>
#include "assets/material.h"
namespace diverse
{
    namespace rhi
    {
        struct GpuBuffer;
    }
    struct BasicVertex
    {
        glm::vec3 Position;
        glm::vec2 TexCoords;
    };

    struct Vertex
    {
        Vertex()
            : Position(glm::vec3(0.0f))
            , Colours(glm::vec4(1.0f))
            , TexCoords(glm::vec2(0.0f))
            , Normal(glm::vec3(0.0f))
            , Tangent(glm::vec3(0.0f))
            , Bitangent(glm::vec3(0.0f))
        {
        }

        glm::vec3 Position;
        glm::vec4 Colours;
        glm::vec2 TexCoords;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;

        bool operator==(const Vertex& other) const
        {
            return Position == other.Position && TexCoords == other.TexCoords && Colours == other.Colours && Normal == other.Normal && Tangent == other.Tangent && Bitangent == other.Bitangent;
        }
    };

    struct AnimVertex
    {
        AnimVertex()
            : Position(glm::vec3(0.0f))
            , Colours(glm::vec4(1.0f))
            , TexCoords(glm::vec2(0.0f))
            , Normal(glm::vec3(0.0f))
            , Tangent(glm::vec3(0.0f))
            , Bitangent(glm::vec3(0.0f))
        {
        }

        glm::vec3 Position;
        glm::vec4 Colours;
        glm::vec2 TexCoords;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
        uint32_t BoneInfoIndices[4] = { 0, 0, 0, 0 };
        float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        bool operator==(const AnimVertex& other) const
        {
            return Position == other.Position && TexCoords == other.TexCoords && Colours == other.Colours && Normal == other.Normal && Tangent == other.Tangent && Bitangent == other.Bitangent;
        }

        void add_bone_data(uint32_t boneInfoIndex, float weight)
        {
            if (weight < 0.0f || weight > 1.0f)
            {
                weight = std::clamp(weight, 0.0f, 1.0f);
            }
            if (weight > 0.0f)
            {
                for (size_t i = 0; i < 4; i++)
                {
                    if (Weights[i] == 0.0f)
                    {
                        BoneInfoIndices[i] = boneInfoIndex;
                        Weights[i] = weight;
                        return;
                    }
                }
            }
        }

        void normalize_weights()
        {
            float sumWeights = 0.0f;
            for (size_t i = 0; i < 4; i++)
            {
                sumWeights += Weights[i];
            }
            if (sumWeights > 0.0f)
            {
                for (size_t i = 0; i < 4; i++)
                {
                    Weights[i] /= sumWeights;
                }
            }
        }
    };
    struct Triangle
    {
        Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
        {
            p0 = v0;
            p1 = v1;
            p2 = v2;
        }

        Vertex p0;
        Vertex p1;
        Vertex p2;
    };

    struct MeshStats
    {
        uint32_t TriangleCount;
        uint32_t VertexCount;
        uint32_t IndexCount;
        float OptimiseThreshold;
    };

    struct PackedPosNormal
    {
        glm::vec3 pos;
        u32     normal;
    };

    struct PackedVertices
    {
        std::vector<PackedPosNormal> pos_normals;
        std::vector<glm::vec2> uvs;
        std::vector<u32> tangents;
        std::vector<u32> colors;
        void resize(size_t vert_size)
        {
            pos_normals.resize(vert_size);
			uvs.resize(vert_size);
			colors.resize(vert_size);
			tangents.resize(vert_size);
        }
    };

    class Mesh
    {
    public:
        Mesh();
        Mesh(const Mesh& mesh);
        Mesh(const std::vector<uint32_t>& indices, const std::vector<Vertex>& vertices, bool optimise = false, float optimiseThreshold = 0.95f);
        Mesh(const std::vector<uint32_t>& indices, std::vector<AnimVertex>& vertices);
        virtual ~Mesh();

        const std::shared_ptr<rhi::GpuBuffer>& get_vertex_buffer() const { return vertex_buffer; }
        const std::shared_ptr<rhi::GpuBuffer>& get_index_buffer() const { return index_buffer; }

        void set_name(const std::string& name) {     this->name = name; }
        const std::string& get_name() const { return name; }

        const maths::BoundingBox& get_bounding_box() const { return bounding_box; }
        const SharedPtr<Material>& get_material() const { return material; }
        void set_material(const SharedPtr<Material>& material) { this->material = material; }
        static void generate_normals(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t indexCount);
        static void generate_tangents_bitangents(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t indexCount);

        void calculate_triangles();
        void create_gpu_buffer();
        void translate(const glm::vec3& center);
        const std::vector<Triangle>& get_triangles()
        {
            if(triangles.empty())
                calculate_triangles();

            return triangles;
        }

        size_t get_vertex_count() const { return vertices.size(); }
        const MeshStats& get_stats() const
        {
            return stats;
        }
        const u32 get_vertex_pos_nor_offset() const {return vertex_pos_nor_offset;}
        const u32 get_vertex_uv_offset() const { return vertex_uv_offset; }
        const u32 get_vertex_tangent_offset() const { return vertex_tangent_offset; }
        const u32 get_vertex_color_offset() const {return vertex_color_offset;}
        const size_t get_index_count() const {return indices.size(); }


        std::vector<uint32_t>   indices;
        std::vector<Vertex>     vertices;
    protected:
        static glm::vec3 generate_tangent(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec2& ta, const glm::vec2& tb, const glm::vec2& tc);
        
        void   calculate_bounding_box();
    protected:
        maths::BoundingBox bounding_box;

        std::shared_ptr<rhi::GpuBuffer> vertex_buffer;
        std::shared_ptr<rhi::GpuBuffer> index_buffer;
        SharedPtr<Material> material;
        std::string name;

        // Only calculated on request
        std::vector<Triangle> triangles;
        MeshStats stats;
        u32 vertex_pos_nor_offset = 0;
        u32 vertex_uv_offset = 0;
        u32 vertex_tangent_offset = 0;
        u32 vertex_color_offset = 0;
    };

}

namespace std
{
    template <>
    struct hash<diverse::Vertex>
    {
        size_t operator()(diverse::Vertex const& vertex) const
        {
            return ((hash<glm::vec3>()(vertex.Position) ^ (hash<glm::vec2>()(vertex.TexCoords) << 1) ^ (hash<glm::vec4>()(vertex.Colours) << 1) ^ (hash<glm::vec3>()(vertex.Normal) << 1) ^ (hash<glm::vec3>()(vertex.Tangent) << 1)));
        }
    };
}
