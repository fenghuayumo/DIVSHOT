#pragma once

#include "asset_id.h"
#include "maths/bounding_box.h"
#include <glm/glm.hpp>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace diverse
{
    // Point cloud vertex structure
    struct PointCloudVertex
    {
        glm::vec3 position;
        uint32_t color;
    };

    // CPU-side point cloud asset — no GPU state
    class PointCloudAsset
    {
    public:
        AssetId id;
        std::filesystem::path source_path;
        std::vector<PointCloudVertex> vertices;
        maths::BoundingBox bounding_box;

        uint32_t version = 0;
        size_t cpu_memory_size = 0;

        bool is_valid() const { return !vertices.empty(); }

        size_t calculate_memory_size() const
        {
            return vertices.size() * sizeof(PointCloudVertex) + source_path.native().capacity();
        }

        size_t get_vertex_count() const { return vertices.size(); }
    };
}
