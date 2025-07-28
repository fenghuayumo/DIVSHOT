#include "mesh_model_component.h"
#include "assets/asset_manager.h"
namespace diverse
{
    MeshModelComponent::MeshModelComponent(const SharedPtr<MeshModel>& modelRef)
        : ModelRef(modelRef)
    {
    }
    MeshModelComponent::MeshModelComponent()
        : ModelRef(createSharedPtr<MeshModel>(PrimitiveType::Cube))
    {
    }

    MeshModelComponent::MeshModelComponent(PrimitiveType primitive)
        : ModelRef(createSharedPtr<MeshModel>(primitive))
    {
    }

    MeshModelComponent::MeshModelComponent(const std::string& path)
    {
        load_from_library(path);
    }

    void MeshModelComponent::load_from_library(const std::string& path)
    {
        ModelRef = ResourceManager<MeshModel>::get().get_resource(path);
    }
}