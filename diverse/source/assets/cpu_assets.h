#pragma once

#include "asset_id.h"
#include "backend/drs_rhi/pixel_format.h"
#include "maths/bounding_box.h"
#include "core/base_type.h"
#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <glm/glm.hpp>
#include <array>

namespace diverse
{
    // Import settings for textures
    struct TextureImportSettings
    {
        bool generate_mips = true;
        bool srgb = false;
        bool compression = false;
        std::optional<uint32_t> max_mip_levels;
    };

    // GPU vertex packing layout (shared by MeshAsset upload and ray tracing)
    struct PackedPosNormal
    {
        glm::vec3 pos;
        u32 normal;
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

    // Mip level data storage
    struct MipData
    {
        std::vector<u8> data;
        uint32_t width;
        uint32_t height;
        uint32_t depth;

        MipData() : width(0), height(0), depth(0) {}
        MipData(uint32_t w, uint32_t h, uint32_t d = 1)
            : width(w), height(h), depth(d) {}

        size_t get_size_in_bytes(PixelFormat format) const;
    };

    // CPU-side texture asset
    // Contains only CPU data - no GPU state
    class TextureAsset
    {
    public:
        AssetId id;
        std::filesystem::path source_path;
        TextureImportSettings settings;

        // Image data
        std::vector<MipData> mips;
        PixelFormat format;
        std::array<u32, 3> extent;  // width, height, depth

        // Metadata
        uint32_t version;  // Incremented on reload
        size_t cpu_memory_size;

        TextureAsset()
            : format(PixelFormat::R8G8B8A8_UNorm)
            , extent{0, 0, 0}
            , version(0)
            , cpu_memory_size(0)
        {}

        bool is_valid() const { return !mips.empty() && extent[0] > 0; }

        // Calculate total CPU memory usage
        size_t calculate_memory_size() const;

        // Get mipmap count
        uint32_t get_mip_count() const { return static_cast<uint32_t>(mips.size()); }

        // Check if fully loaded
        bool is_fully_loaded() const { return !mips.empty(); }
    };

    // Vertex structure (same as existing Mesh::Vertex for compatibility)
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec4 Colours;
        glm::vec2 TexCoords;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;

        Vertex()
            : Position(glm::vec3(0.0f))
            , Colours(glm::vec4(1.0f))
            , TexCoords(glm::vec2(0.0f))
            , Normal(glm::vec3(0.0f))
            , Tangent(glm::vec3(0.0f))
            , Bitangent(glm::vec3(0.0f))
        {}

        bool operator==(const Vertex& other) const
        {
            return Position == other.Position &&
                   TexCoords == other.TexCoords &&
                   Colours == other.Colours &&
                   Normal == other.Normal &&
                   Tangent == other.Tangent &&
                   Bitangent == other.Bitangent;
        }
    };

    // Animated vertex with skeletal animation support
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
            BoneInfoIndices[0] = BoneInfoIndices[1] = BoneInfoIndices[2] = BoneInfoIndices[3] = 0;
            Weights[0] = Weights[1] = Weights[2] = Weights[3] = 0.0f;
        }

        glm::vec3 Position;
        glm::vec4 Colours;
        glm::vec2 TexCoords;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
        uint32_t BoneInfoIndices[4];
        float Weights[4];

        void add_bone_data(uint32_t bone_info_index, float weight);
        void normalize_weights();
    };

    // CPU-side mesh asset
    // Contains only CPU data - no GPU buffers
    class MeshAsset
    {
    public:
        AssetId id;
        std::string name;
        std::filesystem::path source_path;

        // Geometry data
        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        // Skeletal animation support
        std::vector<AnimVertex> anim_vertices;
        bool has_skeleton;

        // Bounding volume
        maths::BoundingBox bounding_box;

        // Metadata
        uint32_t version;  // Incremented on reload
        size_t cpu_memory_size;

        // Vertex offset information for GPU upload
        u32 vertex_pos_nor_offset = 0;
        u32 vertex_uv_offset = 0;
        u32 vertex_tangent_offset = 0;
        u32 vertex_color_offset = 0;

        MeshAsset()
            : has_skeleton(false)
            , version(0)
            , cpu_memory_size(0)
        {}

        bool is_valid() const { return !vertices.empty() && !indices.empty(); }

        // Calculate total CPU memory usage
        size_t calculate_memory_size() const;

        // Get vertex/index counts
        size_t get_vertex_count() const { return vertices.size(); }
        size_t get_index_count() const { return indices.size(); }
        size_t get_anim_vertex_count() const { return anim_vertices.size(); }

        // Check if mesh has animation data
        bool has_animation() const { return has_skeleton && !anim_vertices.empty(); }

        // Generate normals from geometry
        void generate_normals();

        // Generate tangents/bitangents from normals and UVs
        void generate_tangents_bitangents();

        // Calculate bounding box from vertices
        void calculate_bounding_box();
    };

    // Mesh statistics
    struct MeshStats
    {
        uint32_t triangle_count;
        uint32_t vertex_count;
        uint32_t index_count;
        float optimization_threshold;

        MeshStats()
            : triangle_count(0)
            , vertex_count(0)
            , index_count(0)
            , optimization_threshold(0.95f)
        {}
    };

    // Import settings for meshes
    struct MeshImportSettings
    {
        bool calculate_normals = false;
        bool calculate_tangents = true;
        bool optimize_vertices = false;
        bool generate_lod = false;
        float optimization_threshold = 0.95f;
        uint32_t max_lod_levels = 3;
        bool import_skeleton = false;
        bool import_animations = false;
    };
}

// Hash function for Vertex (for vertex deduplication)
namespace std
{
    template <>
    struct hash<diverse::Vertex>
    {
        size_t operator()(const diverse::Vertex& vertex) const
        {
            size_t h = 0;
            hash_combine(h, vertex.Position);
            hash_combine(h, vertex.TexCoords);
            hash_combine(h, vertex.Colours);
            hash_combine(h, vertex.Normal);
            hash_combine(h, vertex.Tangent);
            hash_combine(h, vertex.Bitangent);
            return h;
        }

    private:
        template<typename T>
        static void hash_combine(size_t& seed, const T& value)
        {
            seed ^= std::hash<T>()(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        static void hash_combine(size_t& seed, const glm::vec2& v)
        {
            hash_combine(seed, v.x);
            hash_combine(seed, v.y);
        }

        static void hash_combine(size_t& seed, const glm::vec3& v)
        {
            hash_combine(seed, v.x);
            hash_combine(seed, v.y);
            hash_combine(seed, v.z);
        }

        static void hash_combine(size_t& seed, const glm::vec4& v)
        {
            hash_combine(seed, v.x);
            hash_combine(seed, v.y);
            hash_combine(seed, v.z);
            hash_combine(seed, v.w);
        }
    };
}
