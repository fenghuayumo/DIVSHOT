#pragma once

#include "asset_id.h"
#include "maths/bounding_box.h"
#include <filesystem>
#include <vector>
#include <cstdint>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace diverse
{
    namespace rhi
    {
        struct GpuBuffer;
    }

    // Gaussian splat data structures
    struct Gaussian
    {
        glm::vec4 position;              // Gaussian position
        glm::uvec4 rotation_scale;       // rotation, scale, and opacity
    };

    struct PackedVertexSH
    {
        glm::uvec4 sh1to3;
        glm::uvec4 sh4to7;
        glm::uvec4 sh8to11;
        glm::uvec4 sh12to15;
    };

    using PackedVertexColor = glm::uvec2;

    // CPU-side Gaussian splat asset
    // Contains only CPU data - no GPU state
    class GaussianAsset
    {
    public:
        AssetId id;
        std::filesystem::path source_path;

        // Gaussian splat data
        std::vector<glm::vec3> pos;
        std::vector<std::array<float, 3>> shs_0;
        std::vector<std::array<float, 45>> shs_n;
        std::vector<float> opacities;
        std::vector<glm::vec3> scales;
        std::vector<glm::vec4> rot;

        // State data
        std::vector<uint8_t> splat_state;
        std::vector<uint8_t> splat_select_flag;
        std::vector<uint16_t> splat_transform_index;

        maths::BoundingBox bounding_box;

        // Metadata
        uint32_t version;
        size_t cpu_memory_size;

        GaussianAsset()
            : version(0)
            , cpu_memory_size(0)
        {}

        bool is_valid() const { return !pos.empty(); }

        // Calculate total CPU memory usage
        size_t calculate_memory_size() const
        {
            return pos.size() * (sizeof(glm::vec3) + sizeof(std::array<float, 3>) + sizeof(std::array<float, 45>)
                + sizeof(float) + sizeof(glm::vec3) + sizeof(glm::vec4))
                + splat_state.size() + splat_select_flag.size() + splat_transform_index.size() * sizeof(uint16_t)
                + source_path.native().capacity();
        }

        // Get splat count
        size_t get_num_gaussians() const { return pos.size(); }

        // Load from file
        bool load(const std::string& path);

        // Calculate bounding box
        void calculate_bounding_box();
    };
}
