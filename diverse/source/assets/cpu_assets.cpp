#include "cpu_assets.h"
#include "backend/drs_rhi/pixel_format.h"
#include <cmath>

namespace diverse
{
    // MipData implementation
    size_t MipData::get_size_in_bytes(PixelFormat format) const
    {
        uint32_t block_size = get_pixel_bytes(format);
        return width * height * depth * block_size;
    }

    // TextureAsset implementation
    size_t TextureAsset::calculate_memory_size() const
    {
        size_t total = 0;
        for (const auto& mip : mips)
        {
            total += mip.get_size_in_bytes(format);
        }
        return total;
    }

    // AnimVertex implementation
    void AnimVertex::add_bone_data(uint32_t bone_info_index, float weight)
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
                    BoneInfoIndices[i] = bone_info_index;
                    Weights[i] = weight;
                    return;
                }
            }
        }
    }

    void AnimVertex::normalize_weights()
    {
        float sum_weights = 0.0f;
        for (size_t i = 0; i < 4; i++)
        {
            sum_weights += Weights[i];
        }
        if (sum_weights > 0.0f)
        {
            for (size_t i = 0; i < 4; i++)
            {
                Weights[i] /= sum_weights;
            }
        }
    }

    // MeshAsset implementation
    size_t MeshAsset::calculate_memory_size() const
    {
        size_t total = 0;
        total += indices.size() * sizeof(uint32_t);
        total += vertices.size() * sizeof(Vertex);
        total += anim_vertices.size() * sizeof(AnimVertex);
        return total;
    }

    void MeshAsset::generate_normals()
    {
        // Initialize normals to zero
        for (auto& vertex : vertices)
        {
            vertex.Normal = glm::vec3(0.0f);
        }

        // Accumulate face normals
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            const auto& v0 = vertices[indices[i]];
            const auto& v1 = vertices[indices[i + 1]];
            const auto& v2 = vertices[indices[i + 2]];

            glm::vec3 edge1 = v1.Position - v0.Position;
            glm::vec3 edge2 = v2.Position - v0.Position;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

            // Add to all three vertices (will normalize later)
            const_cast<Vertex&>(vertices[indices[i]]).Normal += normal;
            const_cast<Vertex&>(vertices[indices[i + 1]]).Normal += normal;
            const_cast<Vertex&>(vertices[indices[i + 2]]).Normal += normal;
        }

        // Normalize all normals
        for (auto& vertex : vertices)
        {
            vertex.Normal = glm::normalize(vertex.Normal);
        }
    }

    void MeshAsset::generate_tangents_bitangents()
    {
        // This is a simplified implementation
        // A full implementation would use the UV-based approach
        for (auto& vertex : vertices)
        {
            vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        }
    }

    void MeshAsset::calculate_bounding_box()
    {
        if (vertices.empty())
        {
            bounding_box = maths::BoundingBox();
            return;
        }

        glm::vec3 min = vertices[0].Position;
        glm::vec3 max = vertices[0].Position;

        for (const auto& vertex : vertices)
        {
            min = glm::min(min, vertex.Position);
            max = glm::max(max, vertex.Position);
        }

        bounding_box = maths::BoundingBox(min, max);
    }
}
