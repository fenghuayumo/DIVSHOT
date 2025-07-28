#pragma once
#include "assets/mesh_model.h"
#include "engine/file_system.h"
#include <cereal/cereal.hpp>

namespace diverse
{
    struct MeshModelComponent
    {
        MeshModelComponent(const SharedPtr<MeshModel>& modelRef);
        MeshModelComponent(const std::string& path);
        MeshModelComponent(PrimitiveType primitive);
        MeshModelComponent();

        void load_primitive(PrimitiveType primitive)
        {
            ModelRef = createSharedPtr<MeshModel>(primitive);
        }
        void load_from_library(const std::string& path);
        SharedPtr<MeshModel> ModelRef;

        template <typename Archive>
        void save(Archive& archive) const
        {
            if(!ModelRef || ModelRef->get_meshes().size() == 0 || !ModelRef->is_flag_set(AssetFlag::Loaded))
                return;
            {
                std::string newPath;

                if(ModelRef->get_primitive_type() == PrimitiveType::File)
                    FileSystem::get().absolute_path_2_fileSystem(ModelRef->get_file_path(), newPath);
                else
                    newPath = "Primitive";

                // For now this saved material will be overriden by materials in the model file
                auto material = std::unique_ptr<Material>(ModelRef->get_meshes().front()->get_material().get());
                archive(cereal::make_nvp("PrimitiveType", ModelRef->get_primitive_type()), cereal::make_nvp("FilePath", newPath), cereal::make_nvp("Material", material));
                material.release();
            }
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            auto material = std::unique_ptr<Material>();

            std::string filePath;
            PrimitiveType primitiveType;

            archive(cereal::make_nvp("PrimitiveType", primitiveType), cereal::make_nvp("FilePath", filePath), cereal::make_nvp("Material", material));

            if(primitiveType != PrimitiveType::File)
            {
                ModelRef = createSharedPtr<MeshModel>(primitiveType);
                ModelRef->get_meshes().back()->set_material(SharedPtr<Material>(material.get()));
                material.release();
            }
            else
            {
                load_from_library(filePath);
            }
        }
    };
}
