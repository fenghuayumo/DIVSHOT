#include "point_cloud_component.h"
#include "assets/asset_manager.h"
namespace diverse
{
    PointCloudComponent::PointCloudComponent(const std::string& path)
    {
        load_from_library(path);
    }

    PointCloudComponent::PointCloudComponent()
    {}

    void PointCloudComponent::load_from_library(const std::string& path)
    {
        ModelRef = ResourceManager<PointCloud>::get().get_resource(path);
    }
}