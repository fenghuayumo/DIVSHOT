#pragma once
#include "components.h"
#include "assets/gaussian_model.h"

namespace diverse
{
	enum class GaussianRenderType
	{
		Splat,
		Point,
		Depth,
		Normal,
		Rings,
		Ellipsoids,
		Centers,
	};
	void set_gaussian_render_type(GaussianRenderType ty);

	struct GaussianComponent
	{
		
		GaussianComponent(const SharedPtr<GaussianModel>& modelRef)
			: ModelRef(modelRef)
		{
		}

		GaussianComponent(const std::string& path);
		GaussianComponent();
		GaussianComponent(int max_splats);
		GaussianRenderType get_render_type() { return gs_render_type;}
		void			   set_render_type(GaussianRenderType ty) { gs_render_type = ty;}

		SharedPtr<GaussianModel>	ModelRef;

		GaussianRenderType gs_render_type = GaussianRenderType::Splat;
		bool 			   participate_render = true;
		bool 			   skip_render = false;
		i32 			   sh_degree = 3;
		f32 			   transparency = 1.0f;
		f32 			   brightness = 0.0f;
		f32				   white_point = 1.0f;
		f32				   black_point = 0.0f;
		glm::vec3		   albedo_color = glm::vec3(1.0f);
		u32 			   max_splats = 2000000;
		void 			  apply_color_adjustment();
		template <typename Archive>
		void save(Archive& archive) const
		{
			if (!ModelRef )
				return;
			std::string newPath;
			FileSystem::get().absolute_path_2_fileSystem(ModelRef->get_file_path(), newPath);
			archive(cereal::make_nvp("gsvistype", gs_render_type));
			archive(cereal::make_nvp("FilePath", newPath));
			archive(cereal::make_nvp("splat_size", ModelRef->splat_size));
			archive(cereal::make_nvp("max_splats", max_splats));
		}

		template <typename Archive>
		void load(Archive& archive)
		{
			std::string filePath;
			float splat_size = 1.0;
			archive(cereal::make_nvp("gsvistype", gs_render_type));
			archive(cereal::make_nvp("FilePath", filePath));
			archive(cereal::make_nvp("splat_size", splat_size));
			archive(cereal::make_nvp("max_splats", max_splats));
			if( !filePath.empty())
				ModelRef = createSharedPtr<GaussianModel>(filePath);
			else
				ModelRef = createSharedPtr<GaussianModel>(max_splats); //from trainning image
			ModelRef->splat_size = splat_size;
		}
	};
}