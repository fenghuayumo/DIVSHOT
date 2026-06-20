#include "point_cloud_component.h"
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
        ModelRef = PointCloud::acquire(path);
    }
}