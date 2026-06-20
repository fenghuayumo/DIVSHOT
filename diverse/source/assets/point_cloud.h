#pragma once
#include "point_cloud_asset.h"
#include "asset_id.h"
#include "core/core.h"
#include "maths/maths_utils.h"
#include "maths/bounding_box.h"
#include "engine/file_system.h"
#include "core/reference.h"
#include "maths/transform.h"
#include <cereal/cereal.hpp>

namespace diverse
{
    namespace rhi
    {
        struct GpuDevice;
    }

    class PointCloud
    {
    public:
        PointCloud(const std::string& file_path);
        PointCloud();

        bool is_loaded() const { return loaded; }
        bool is_invalid() const { return invalid; }
        bool is_gpu_uploaded() const;

        void load(const std::string& path);
        bool load_ply(const std::string& path);
        void reset_center();
        auto get_world_bounding_box(const glm::mat4& t) -> maths::BoundingBox;
        auto get_local_bounding_box() -> maths::BoundingBox&;
        auto get_num_points() -> u64 { return pcd_vertex.size(); }

        AssetId get_asset_id() const { return id; }

        maths::BoundingBox local_bounding_box;
        std::string get_file_path() { return file_path; }

        static bool is_point_cloud_file(const std::string& file_path);
        static SharedPtr<PointCloud> acquire(const std::string& path);

    protected:
        void sync_cpu_asset();

        AssetId id;
        std::vector<PointCloudVertex> pcd_vertex;
        std::string file_path;
        bool loaded = false;
        bool invalid = false;
    };
}
