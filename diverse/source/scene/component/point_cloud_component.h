#pragma once
#include "assets/point_cloud.h"
#include "engine/file_system.h"
#include <cereal/cereal.hpp>

namespace diverse
{
    class PointCloudComponent
    {
    public:
        PointCloudComponent(const std::string& path);
        PointCloudComponent();

        void load_from_library(const std::string& path);
        SharedPtr<PointCloud> ModelRef;
        float point_size = 1.0f;
        template <typename Archive>
		void save(Archive& archive) const
		{
			if (!ModelRef )
				return;
			std::string newPath;
			FileSystem::get().absolute_path_2_fileSystem(ModelRef->get_file_path(), newPath);
			archive(cereal::make_nvp("FilePath", newPath), cereal::make_nvp("point_size",point_size));
		}

		template <typename Archive>
		void load(Archive& archive)
		{
			std::string filePath;
			archive(cereal::make_nvp("FilePath", filePath), cereal::make_nvp("point_size", point_size));
			if( !filePath.empty())
				ModelRef = createSharedPtr<PointCloud>(filePath);
		}
    };
}