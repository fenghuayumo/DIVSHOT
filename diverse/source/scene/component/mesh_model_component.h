#pragma once
#include "assets/model_asset.h"
#include "engine/file_system.h"
#include <cereal/cereal.hpp>
#include <memory>

namespace diverse
{
    struct MeshModelComponent
    {
        MeshModelComponent(const std::shared_ptr<ModelAsset>& model_ref);
        MeshModelComponent(const std::string& path);
        MeshModelComponent(PrimitiveType primitive);
        MeshModelComponent();

        void load_primitive(PrimitiveType primitive)
        {
            ModelRef = std::make_shared<ModelAsset>(primitive);
        }

        void load_from_library(const std::string& path);
        std::shared_ptr<ModelAsset> ModelRef;

        template <typename Archive>
        void save(Archive& archive) const
        {
            if (!ModelRef || ModelRef->get_slots().empty() || !ModelRef->is_loaded())
                return;

            std::string new_path;
            if (ModelRef->primitive_type == PrimitiveType::File)
                FileSystem::get().absolute_path_2_fileSystem(ModelRef->source_path, new_path);
            else
                new_path = "Primitive";

            archive(cereal::make_nvp("PrimitiveType", ModelRef->primitive_type),
                    cereal::make_nvp("FilePath", new_path));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            std::string file_path;
            PrimitiveType primitive_type;
            archive(cereal::make_nvp("PrimitiveType", primitive_type), cereal::make_nvp("FilePath", file_path));

            if (primitive_type != PrimitiveType::File)
                ModelRef = std::make_shared<ModelAsset>(primitive_type);
            else
                load_from_library(file_path);
        }
    };
}
