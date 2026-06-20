#include "mesh_model_component.h"
#include "assets/asset_system.h"

namespace diverse
{
    MeshModelComponent::MeshModelComponent(const std::shared_ptr<ModelAsset>& model_ref)
        : ModelRef(model_ref)
    {
    }

    MeshModelComponent::MeshModelComponent()
        : ModelRef(AssetSystem::get_instance().load_primitive(PrimitiveType::Cube))
    {
    }

    MeshModelComponent::MeshModelComponent(PrimitiveType primitive)
        : ModelRef(AssetSystem::get_instance().load_primitive(primitive))
    {
    }

    MeshModelComponent::MeshModelComponent(const std::string& path)
    {
        load_from_library(path);
    }

    void MeshModelComponent::load_from_library(const std::string& path)
    {
        ModelRef = AssetSystem::get_instance().load_model(path);
        if (!ModelRef)
            ModelRef = std::make_shared<ModelAsset>();
    }
}
