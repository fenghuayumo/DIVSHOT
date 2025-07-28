#include "point_cloud.h"
#include "utility/string_utils.h"
#include "engine/file_system.h"
#include "core/profiler.h"
#include "assets/asset_manager.h"
#include "backend/drs_rhi/gpu_device.h"

namespace diverse
{
    PointCloud::PointCloud(const std::string& filePath)
        :file_path(filePath)
    {
        std::thread t([this, filePath]() {
            load(filePath);
        });
        t.detach();
    }
    PointCloud::PointCloud()
    {
    }
    void PointCloud::create_gpu_buffer()
    {
        DS_PROFILE_FUNCTION();
        if (pcd_vertex.size() == 0)
            return;
        auto vertex_desc = rhi::GpuBufferDesc::new_gpu_only(pcd_vertex.size() * sizeof(PointCloudVertex),
            rhi::BufferUsageFlags::STORAGE_BUFFER
            | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
            | rhi::BufferUsageFlags::VERTEX_BUFFER
            | rhi::BufferUsageFlags::TRANSFER_DST);
        vertex_buffer = g_device->create_buffer(vertex_desc, "point_vert_buf", (u8*)pcd_vertex.data());
        set_flag(AssetFlag::UploadedGpu);
    }

    void PointCloud::reset_center()
    {
        local_bounding_box = get_local_bounding_box();
        glm::mat4 extr_transform = glm::translate(glm::mat4(1.0), -local_bounding_box.center());
        local_bounding_box.transform(extr_transform);
    }

    maths::BoundingBox& PointCloud::get_local_bounding_box()
    {
        if(local_bounding_box.defined())
        {
            return local_bounding_box;
        }
        for (auto& vertex : pcd_vertex)
        {
            local_bounding_box.merge(vertex.position);
        }
        return local_bounding_box;
    }

    auto PointCloud::get_world_bounding_box(const glm::mat4& t) -> maths::BoundingBox
    {
        return local_bounding_box.transformed(t);
    }

    void PointCloud::load(const std::string& path)
    {
        DS_PROFILE_FUNCTION();
        file_path = path;
        std::string physicalPath;
        if (!diverse::FileSystem::get().resolve_physical_path(path, physicalPath))
        {
            DS_LOG_INFO("Failed to load PointCloud - {0}", path);
            return;
        }
        std::string resolvedPath = physicalPath;

        const std::string fileExtension = stringutility::get_file_extension(path);
        bool ret = false;
        if(fileExtension == "ply")
            ret = load_ply(resolvedPath);
        else
            DS_LOG_ERROR("Unsupported File Type : {0}", fileExtension);
        if (!ret)
        {
            set_flag(AssetFlag::Invalid);
            return;
        }
        reset_center();
        create_gpu_buffer();
        set_flag(AssetFlag::Loaded);
        DS_LOG_INFO("Loaded PointCloud - {0}", path);
    }
}