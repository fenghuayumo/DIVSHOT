#pragma once
#include "core/reference.h"
#include "maths/bounding_box.h"
#include "assets/asset.h"
#include "splat_transform_palette.h"
#include <glm/gtx/quaternion.hpp>
#include <array>

#define NORMAL_STATE    0
#define SELECT_STATE    1
#define HIDE_STATE      2   // locked
#define DELETE_STATE    4
#define PAINT_STATE     8
namespace diverse
{
    inline uint setOpState(uint value, uint op_state) {
		return (value & 0xFFFFFF00) | (op_state & 0x000000FF);
	}

	inline uint getOpState(uint value) {
		 return value & 0xFF;
	}

    //get op_flag 
    inline uint getOpFlag(uint value) {
        return (value >> 8) & 0xFF;
    }
    //set op_flag 
    inline uint setOpFlag(uint value, uint op_flag) {
        return (value & 0xFFFF00FF) | ((op_flag << 8) & 0x0000FF00);
    }

    inline uint getTransformIndex(uint value) {
        return (value >> 16) & 0xFFFF;
    }
    //set transform index
    inline uint setTransformIndex(uint value, uint index) {
        return (value & 0x0000FFFF) | ((index << 16) & 0xFFFF0000);
    }

    namespace rhi
    {
       struct GpuBuffer;
    }

    struct Gaussian
    {
        glm::vec4 position;				// Gaussian position
        glm::vec4 rotation_scale;		// rotation, scale, and opacity
    };

    struct PackedVertexSH
    {
        glm::uvec4 sh1to3;
        glm::uvec4 sh4to7;
        glm::uvec4 sh8to11;
        glm::uvec4 sh12to15;
    };

    // struct PackedVertexColor
    // {
    //     glm::vec2 sh0;
    // };
    using PackedVertexColor = glm::vec2;
	struct GaussianModel : public Asset
	{
	public:
        GaussianModel() = default;
		GaussianModel(int max_splats);
		GaussianModel(const std::string& filePath);

		maths::BoundingBox& get_local_bounding_box()  { return local_bounding_box; }

        void    update_from_gpu(float* pos, float* shs,float* opacities,float* scales,float* rots);
        void    update_from_cpu(float* pos, 
                                float* shs, 
                                float* opacities, 
                                float* scales, 
                                float* rots,
                                int num_gaussians);
        void    update_from_pos_color(u8* pos_color,int num_gaussians);
        void    update_feature_dc_data(const std::vector<u32>& indices);
        void    update_state();
        void    update_transform_index();
        void    save_to_file(const std::string& filepath, bool apply_transfom = false);
        auto    load_model(const std::string& filepath)->void;
        void    export_to_cpu();
        void    download_state_buffer();
        u64     get_num_gaussians() const;
        std::string get_file_path() const {return file_path;}
        auto    position()->std::vector<glm::vec3>& {return pos;}
        auto    sh()->std::vector<std::array<float, 48>>& {return shs;}
        auto    opacity()->std::vector<float>& {return opacities;}
        auto    scale()->std::vector<glm::vec3>& {return scales;}
        auto    rotation()->std::vector<glm::vec4>& {return rot;}
        auto    get_feature_dc_rest(const std::vector<std::array<float,48>>& colors)->std::pair<std::vector<glm::vec3>, std::vector<std::array<glm::vec3,15>>>;
        auto    state()->std::vector<u8>& {return splat_state;}
        auto    flags()->std::vector<u8>& {return splat_select_flag;}
        auto    transform_index()->std::vector<u16>& {return splat_transform_index;}
        auto    merge(GaussianModel* model,bool apply_transform = false)->void;
        auto    merge(GaussianModel* model,const std::vector<u32>& indices,bool apply_transform = false)-> std::vector<u32>;
        auto    remove(const std::vector<u32>& indices)->void;
        auto    get_compressed_data(bool apply_transform = false)->std::vector<u8>;

        auto    get_world_bounding_box(const glm::mat4& t) -> maths::BoundingBox;
        auto    get_selection_bounding_box(const glm::mat4& t)->maths::BoundingBox;
        auto    make_world_bound_dirty() ->void { world_bound_dirty = true;}
        auto    make_selection_bound_dirty() -> void;
        auto    has_select_gaussians() ->bool {return num_select > 0;} 
        auto    num_selected_gaussians() ->u32 {return num_select;}
        auto    num_hidden_gaussians() ->u32 {return num_hidden;}
        auto    num_delete_gaussians() -> u32 { return num_delete; }
        auto    antialiased() -> bool& { return mip_antialiased; }
        SET_ASSET_TYPE(AssetType::Splat);
    protected:
        void    update_data();
        void    create_gpu_buffer(bool compact = false);
    public:
        std::shared_ptr<rhi::GpuBuffer>	gaussians_buf;
        std::shared_ptr<rhi::GpuBuffer>	gaussians_color_buf; //sh0 data, f16 store float data
        std::shared_ptr<rhi::GpuBuffer>	gaussians_sh_buf; //sh1to3, sh4to7, sh8to11, sh12to15
        std::shared_ptr<rhi::GpuBuffer>	points_key_buf;
        std::shared_ptr<rhi::GpuBuffer>	points_value_buf;
        std::shared_ptr<rhi::GpuBuffer>	gaussian_state_buf;

        SplatTransformPalette                 splat_transforms;
        maths::BoundingBox	                  local_bounding_box;
        maths::BoundingBox	                  world_bounding_box;
        maths::BoundingBox                    selection_bounding_box;
        float				                  splat_size = 1.0;
	protected:
		std::vector<glm::vec3>                pos;
		std::vector<std::array<float,48>>     shs;
		std::vector<float>                    opacities;
		std::vector<glm::vec3>                scales;
		std::vector<glm::vec4>                rot;
        std::vector<u8>                       splat_state;
        std::vector<u8>                       splat_select_flag;
        std::vector<u16>                      splat_transform_index;
        std::string                           file_path;
        u32                                   num_select = 0;
        u32                                   num_hidden = 0;
        u32                                   num_delete = 0;
        bool                                  world_bound_dirty = true;
        bool                                  selection_bound_dirty = true;
        int                                   max_splats = 10000;
        bool                                  mip_antialiased = false;     
	};
}
