#include "point_cloud.h"
#include "asset_system.h"
#include "asset_registry.h"
#include "utility/string_utils.h"
#include "engine/file_system.h"
#include "core/profiler.h"
#include <mutex>
#include <unordered_map>

namespace diverse
{
    namespace
    {
        std::mutex s_point_cloud_cache_mutex;
        std::unordered_map<std::string, SharedPtr<PointCloud>> s_point_cloud_cache;
    }

    SharedPtr<PointCloud> PointCloud::acquire(const std::string& path)
    {
        if (path.empty())
            return nullptr;

        std::lock_guard lock(s_point_cloud_cache_mutex);
        auto it = s_point_cloud_cache.find(path);
        if (it != s_point_cloud_cache.end())
            return it->second;

        auto cloud = createSharedPtr<PointCloud>(path);
        s_point_cloud_cache[path] = cloud;
        return cloud;
    }

    PointCloud::PointCloud(const std::string& filePath)
        : file_path(filePath)
    {
        std::thread t([this, filePath]() {
            load(filePath);
        });
        t.detach();
    }

    PointCloud::PointCloud()
    {
    }

    bool PointCloud::is_gpu_uploaded() const
    {
        if (!id.is_valid())
            return false;
        return AssetRegistry::get_instance().get_state(id) == AssetState::ResidentGpu;
    }

    void PointCloud::sync_cpu_asset()
    {
        if (!loaded || pcd_vertex.empty())
            return;

        if (!id.is_valid())
            id = GenerateAssetId();

        auto asset = std::make_shared<PointCloudAsset>();
        asset->id = id;
        asset->source_path = file_path;
        asset->vertices = pcd_vertex;
        asset->bounding_box = local_bounding_box;
        asset->version = 0;
        asset->cpu_memory_size = asset->calculate_memory_size();

        AssetSystem::get_instance().register_cpu_point_cloud(asset);
    }

    void PointCloud::reset_center()
    {
        local_bounding_box = get_local_bounding_box();
        glm::mat4 extr_transform = glm::translate(glm::mat4(1.0), -local_bounding_box.center());
        local_bounding_box.transform(extr_transform);
    }

    maths::BoundingBox& PointCloud::get_local_bounding_box()
    {
        if (local_bounding_box.defined())
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
        if (fileExtension == "ply")
            ret = load_ply(resolvedPath);
        else
            DS_LOG_ERROR("Unsupported File Type : {0}", fileExtension);
        if (!ret)
        {
            invalid = true;
            loaded = false;
            return;
        }
        reset_center();
        loaded = true;
        invalid = false;
        sync_cpu_asset();
        DS_LOG_INFO("Loaded PointCloud - {0}", path);
    }
}
