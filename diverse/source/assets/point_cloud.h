#pragma once
#include "core/core.h"
#include "maths/maths_utils.h"
#include "maths/bounding_box.h"
#include "engine/file_system.h"
#include "assets/asset.h"
#include "core/reference.h"
#include "maths/transform.h"
#include <cereal/cereal.hpp>

namespace diverse
{
    namespace rhi
    {
        struct GpuBuffer;
    }
    struct PointCloudVertex
    {
        glm::vec3 position;
        uint32_t color;
    };
    class PointCloud : public Asset
    {
    public:
        PointCloud(const std::string& file_path);
        PointCloud();
        SET_ASSET_TYPE(AssetType::PointCloud);
    public:
        void    load(const std::string& path);
        bool    load_ply(const std::string& path);
        void    reset_center();
        void    create_gpu_buffer();
        auto    get_world_bounding_box(const glm::mat4& t)->maths::BoundingBox;
        auto    get_local_bounding_box()->maths::BoundingBox&;
        auto    get_num_points()->u64 { return pcd_vertex.size(); }
        maths::BoundingBox	local_bounding_box;
        std::shared_ptr<rhi::GpuBuffer> vertex_buffer;
    public:
        std::string get_file_path() {return file_path;}
    public:
        static bool is_point_cloud_file(const std::string& file_path);
    protected:
        std::vector<PointCloudVertex> pcd_vertex;
        std::string file_path;
    };
}