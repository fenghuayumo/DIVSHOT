#pragma once
#include "backend/drs_rhi/drs_rhi.h"
#include "backend/drs_rhi/gpu_device.h"
#include "render_settings.h"
#include "drs_rg/renderer.h"
#include "scene/mesh_light.h"
#include "maths/transform.h"
#include "scene/camera/camera.h"
#include "frame_constants.h"
#include "gaussian.h"
#include "debug_render_pass.h"
#include "sky_pass.h"
#include "light.h"

namespace diverse
{
	class  Scene;
	
	struct GaussianModel;
	class PointCloud;
	class MeshModel;
	struct Material;
	class  Mesh;
	struct PostProcessRenderer;
	struct TaaRenderer;
	struct ImageLut;
	struct ShadowDenoiser;
	struct IracheRender;
	struct SsgiRenderer;
	struct RtrRenderer;
	struct RestirGiRenderer;
	struct PointRenderPass;
	class LightingPass;
	class RasterizeMesh;
	struct RenderGSCommand
	{
		maths::Transform transform;
		GaussianModel* model;
		u32 		   sh_degree;
		glm::vec4 	   select_color;
		glm::vec4 	   locked_color;
		glm::vec4 	   tintColor;
		glm::vec3	   color_offset;
		std::vector<void*>	crop_data;
	};
	struct RenderMeshCommand
	{
		u32				mesh_instance_id;
		u32				material_id;
		u32				mesh_id;
	};

	struct RenderPointCommand
	{
		maths::Transform transform;
		PointCloud* model;
	};
	struct InstanceTransform
	{
		glm::mat3x4 transform;
		glm::mat3x4 previous_transform;
	};

	struct InstanceDynamicConstants
	{
		u32 gemoetry_offset; // model gpumesh index offset
		f32 emissive_multiplier;
	};

	struct FrameParamDesc
	{
		CameraMatrices  camera_matrices;
		std::array<u32, 2> render_extent;
	};

	struct FrameContext
	{
		std::array<u32, 2> render_extent;
		f32                delta_t;
		class DeferedRenderer* world_renderer;
		auto aspect_ratio() const ->f32
		{
			return (f32)render_extent[0] / (f32)render_extent[1];
		}
	};

	struct BindlessImageHandle
	{
		u32 handle;
	};
	class DeferedRenderer
	{
	public:
		DeferedRenderer(rhi::GpuDevice* dev,rhi::Swapchain* swapchain);
		~DeferedRenderer();

		void	init();
		void	render();
		auto	upload_gpu_buffers()->void;
		auto	prepare_render_graph(rg::TemporalGraph& rg, const FrameParamDesc& frame_desc) -> rg::Handle<rhi::GpuTexture>;
		auto	retire_frame() -> void;
		auto	prepare_frame_constants(rhi::DynamicConstants& dynamic_constants, const FrameParamDesc& frame_desc, f32 delta_t) -> rg::FrameConstantsLayout;
		auto	release()->void;

		auto 	debug_pass(rg::TemporalGraph& rg, rg::Handle<rhi::GpuTexture>& color_img, rg::Handle<rhi::GpuTexture>& depth_img)->void;

		auto	handle_resize(uint32_t width, uint32_t height)->void;
		auto 	handle_window_resize(uint32_t width, uint32_t height)->void;
		struct UiRenderer*	get_ui_renderer() {return ui_renderer
		.get(); }

		auto	set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform)->void;

		auto	set_current_scene(Scene* scene) ->void{ current_scene = scene;}

		auto	handle_new_scene(Scene* scene)->void;
		auto	get_main_render_image()->std::shared_ptr<rhi::GpuTexture> {return main_render_tex; }
		auto	set_main_render_image(std::shared_ptr<rhi::GpuTexture>& tex)->void {main_render_tex = tex; }
		auto	get_render_depth()->std::shared_ptr<rhi::GpuTexture> {return depth_render_tex;}
		auto	get_camera()->Camera* {return camera;}
		auto	get_camera_transform()-> maths::Transform* {return camera_transform;}
		auto	binldess_descriptorset()-> rhi::DescriptorSet* {return bindless_descriptor_set.get();}
		auto 	invalidate_pt_state()->void { reset_pt = true;}	
		auto    has_reset_pt_state()->bool {return reset_pt;}
		auto 	refresh_shaders()->void {return rg_renderer->refresh_shaders();}
		auto	register_event_render_graph(rg::RenderGraph& rg) -> void;
		u32 	get_buf_id(GaussianModel* model);
		u32 	get_buf_id(PointCloud* model);
    public:
		auto 	prepare_top_level_acceleration(rg::RenderGraph& rg) -> std::optional<rg::Handle<rhi::GpuRayTracingAcceleration>>;
		auto 	build_ray_tracing_top_level_acceleration() -> void;
		auto 	build_ray_tracing_buttom_level_acceleration(MeshModel* mesh)->void;
	protected:
		auto	upload_mesh_model(class MeshModel* model)->void;
		auto	upload_material(const struct MaterialProperties* material)->void;
		auto	upload_image(const std::shared_ptr<rhi::GpuTexture>& image)-> BindlessImageHandle;
		auto	prepare_render_graph_hybrid(
				rg::TemporalGraph& rg, 
				const FrameParamDesc& frame_desc,
				rg::Handle<rhi::GpuTexture>& accum_img,
				rg::Handle<rhi::GpuTexture>& depth_img) -> rg::Handle<rhi::GpuTexture>;
		auto	prepare_render_graph_pt(
				rg::TemporalGraph& rg, 
				const FrameParamDesc& frame_desc,
				rg::Handle<rhi::GpuTexture>& accum_img,
				rg::Handle<rhi::GpuTexture>& depth_img) -> rg::Handle<rhi::GpuTexture>;
	public:
		std::vector<RenderGSCommand> gs_command_queue;
		std::vector<RenderMeshCommand> mesh_command_queue;
		std::vector<RenderPointCommand>	 point_command_queue;
		std::unordered_map<u32,glm::mat4x4>	  previous_transforms;
		u32 frame_idx = 0;
		std::optional<CameraMatrices> prev_camera_matrix;
		Scene* current_scene = nullptr;
		Camera* camera = nullptr;
		maths::Transform* camera_transform = nullptr;
		std::vector<InstanceTransform>			instantce_transforms;
		std::unordered_map<u32,Mesh*>			mesh_buf_id_2_mesh;
		std::vector<InstanceDynamicConstants>	instance_dynamic_constants;
	public:
	  	std::shared_ptr<rhi::GpuBuffer>     mesh_buffer;
        std::shared_ptr<rhi::GpuBuffer>     material_buffer;
        std::shared_ptr<rhi::GpuBuffer>     bindless_texture_sizes;
		std::shared_ptr<rhi::GpuRayTracingAcceleration> tlas;
		std::vector<std::shared_ptr<rhi::GpuRayTracingAcceleration>> mesh_blas;
	protected:
		std::shared_ptr<rg::Renderer>  rg_renderer;
		std::array<u32, 2>	render_extent;

		std::shared_ptr<struct UiRenderer>	ui_renderer;
		std::shared_ptr<struct GridRenderer> grid_renderer;
		std::shared_ptr<rhi::GpuTexture>	main_render_tex;
		std::shared_ptr<rhi::GpuTexture>	depth_render_tex;

		Camera* override_camera = nullptr;
		maths::Transform* override_camera_transform = nullptr;
		std::vector<u32>							ent_2_model_id;
		std::unordered_map<MeshModel*,u32>			model_2_blas_id;
		std::unordered_map<MeshModel*,u32>			model_2_mesh_buf_id;
		std::unordered_map<GaussianModel*,u32>		model_2_gs_buf_id;
		std::unordered_map<PointCloud*,u32>			model_2_point_buf_id;
		std::unordered_map<struct Material*, u32>	mat_2_mat_buf_id;
		std::unordered_map<Mesh*, u32>				mesh_2_mesh_buf_id;
		std::unordered_map<rhi::GpuTexture*,u32> 	bindless_image_ids;
		std::vector<struct MaterialProperties*>		material_datas;
		u32 next_bindless_image_id = 0;
		auto add_image_lut(const std::shared_ptr<ImageLut>& computer, u64 id)->void;

	protected:
		std::unique_ptr<GaussianRenderPass> gaussian;
		std::unique_ptr<DebugRenderPass> debug_render_pass;
		std::unique_ptr<IblRenderer>	ibl;
		std::vector<std::shared_ptr<ImageLut>>   image_luts;
		std::unique_ptr<PostProcessRenderer> post;
        std::unique_ptr<TaaRenderer>    taa;
		std::unique_ptr<RasterizeMesh>  rasterizer;
		std::unique_ptr<ShadowDenoiser> shadow_denoise;
		std::unique_ptr<SsgiRenderer>	ssgi;
		std::unique_ptr<IracheRender>	irache;
		std::unique_ptr<RestirGiRenderer>       restir_gi;
		std::unique_ptr<RtrRenderer>            rtr;
		std::unique_ptr<PointRenderPass>	point_pass;
		std::unique_ptr<LightingPass>		lighting_pass;
	protected:
		//only excute once
		std::vector<std::function<void(rg::RenderGraph&,rg::Handle<rhi::GpuTexture>&)>> gpu_events;
	protected:
		float       	sun_size_multiplier;
		glm::vec3    	sun_color;
		glm::vec4    	sky_ambient;
		glm::vec3    	sun_direction;
		std::vector<TriangleLight> triangle_lights;
	private:
		std::shared_ptr<rhi::DescriptorSet> bindless_descriptor_set;
		std::array<u32, 2>					temporal_upscale_extent;
		std::vector<glm::vec2>   			supersample_offsets;

		bool		reset_pt = false;
		bool 		skip_gs_render = false;
		u32 		mesh_buf_id = 0;
	};
}
