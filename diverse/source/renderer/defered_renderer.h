#pragma once
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
#include "frame_snapshot.h"
#include "ui_renderer.h"
#include "scene/component/gaussian_crop.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

namespace diverse
{
	class  Scene;
	namespace asset
	{
		struct Texture;
	}
	
	struct GaussianModel;
	class PointCloud;
	class MeshModel;
	class Material;
	struct MaterialProperties;
	struct PBRMataterialTextures;
	class  Mesh;
	struct PostProcessRenderer;
	struct TaaRenderer;
	struct ImageLut;
	class GridRenderer;
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
		SharedPtr<GaussianModel> model;
		u32 		   sh_degree;
		glm::vec4 	   select_color;
		glm::vec4 	   locked_color;
		glm::vec4 	   tintColor;
		glm::vec3	   color_offset;
		bool		   mip_antialiased;
		std::vector<GaussianCrop::GaussianCropData> crop_data;
	};
	struct RenderMeshCommand
	{
		u32				mesh_instance_id;
		u32				material_id;
		u32				mesh_id;
	};

	struct MeshDrawGpuData
	{
		glm::vec4 bounds_min;
		glm::vec4 bounds_max;
		u32 mesh_instance_id = 0;
		u32 material_id = 0;
		u32 mesh_id = 0;
		u32 index_count = 0;
	};

	struct RenderPointCommand
	{
		maths::Transform transform;
		SharedPtr<PointCloud> model;
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

	struct CameraFrameParams
	{
		f32 fov_rad = 1.0471975512f;
		f32 dof_enabled = 0.0f;
		f32 focus_distance = 10.0f;
		f32 aperture = 0.0f;
		u32 camera_type = 0;
	};

	struct MeshFrameState
	{
		u32 entity_id = 0;
		glm::mat4x4 world_transform = glm::mat4x4(1.0f);
	};

	struct MeshDrawRequest
	{
		u32 entity_id = 0;
		maths::Transform transform;
		SharedPtr<MeshModel> model;
		bool active = false;
	};

	struct EnvironmentFrameParams
	{
		bool has_environment = false;
		bool dirty = false;
		f32 mode = 0.0f;
		glm::vec3 color = glm::vec3(0.0f);
		glm::vec3 sun_color = glm::vec3(1.0f);
		f32 sun_size_multiplier = 1.0f;
		glm::vec4 sky_ambient = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		glm::vec3 sun_direction = glm::vec3(0.0f);
		IblRenderParameter ibl_params;
		int cube_resolution = 256;
		std::shared_ptr<rhi::GpuTexture> hdr_img;
		std::string hdr_path;
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

	struct FrameSnapshot
	{
		FrameParamDesc frame_desc;
		CameraFrameParams camera_params;
		EnvironmentFrameParams environment;
		RenderSettings render_settings;
		std::array<u32, 2> swapchain_extent = { 0, 0 };
		f32 delta_t = 0.0f;
		bool skip_gs_render = false;

		std::vector<RenderGSCommand> gs_commands;
		std::vector<MeshDrawRequest> mesh_requests;
		std::vector<RenderPointCommand> point_commands;
		std::vector<FrameLight> primitive_lights;
		std::vector<MeshFrameState> mesh_frame_states;
		std::optional<UiFrame> ui_frame;
		DebugDrawFrame debug_draw_frame;
		GridFrameParams grid_params;
	};

	struct RenderFramePacket : FrameSnapshot
	{
		std::vector<RenderMeshCommand> mesh_commands;
		std::vector<MeshDrawGpuData> mesh_draw_data;
		std::vector<TriangleLight> triangle_lights;
		std::vector<u8> rt_instance_masks;
		GpuSceneDirtyState gpu_scene_dirty;
	};

	struct BindlessImageHandle
	{
		u32 handle;
	};

	struct GpuSceneUploadState
	{
		std::unordered_map<u32, glm::mat4x4> previous_transforms;
		std::vector<InstanceTransform> instance_transforms;
		std::vector<InstanceDynamicConstants> instance_dynamic_constants;
		std::unordered_map<u32, Mesh*> mesh_buf_id_2_mesh;
		std::vector<u32> ent_2_model_id;
		std::unordered_map<MeshModel*, u32> model_2_blas_id;
		std::unordered_map<MeshModel*, u32> model_2_mesh_buf_id;
		std::unordered_map<GaussianModel*, u32> model_2_gs_buf_id;
		std::unordered_map<PointCloud*, u32> model_2_point_buf_id;
		std::vector<u8> rt_instance_masks;
		std::unordered_map<Material*, u32> mat_2_mat_buf_id;
		std::unordered_map<Mesh*, u32> mesh_2_mesh_buf_id;
		std::unordered_map<rhi::GpuTexture*, u32> bindless_image_ids;
		std::vector<MaterialProperties*> material_datas;
		std::vector<std::shared_ptr<rhi::GpuRayTracingAcceleration>> mesh_blas;
		u32 next_bindless_image_id = 0;
		u32 mesh_buf_id = 0;
	};

	class DeferedRenderer
	{
	public:
		DeferedRenderer(rhi::GpuDevice* dev,rhi::Swapchain* swapchain);
		~DeferedRenderer();

		void	init(std::array<u32, 2> swapchain_extent);
		void	render(std::array<u32, 2> swapchain_extent);
		auto	build_render_frame_packet(f32 delta_t, std::array<u32, 2> swapchain_extent)->std::optional<RenderFramePacket>;
		auto	submit_render_frame_packet(RenderFramePacket&& packet)->void;
		auto	start_render_thread()->void;
		auto	stop_render_thread()->void;
		auto	enqueue_render_frame_packet(RenderFramePacket&& packet)->void;
		auto	is_render_thread_running() const -> bool { return render_thread_running.load(std::memory_order_acquire); }
		auto	prepare_render_graph(rg::TemporalGraph& rg, const FrameParamDesc& frame_desc, const EnvironmentFrameParams& environment) -> rg::Handle<rhi::GpuTexture>;
		auto	retire_frame(const std::vector<MeshFrameState>& mesh_frame_states) -> void;
		auto	prepare_frame_constants(rhi::DynamicConstants& dynamic_constants, const FrameParamDesc& frame_desc, const CameraFrameParams& camera_params, f32 delta_t) -> rg::FrameConstantsLayout;
		auto	release()->void;

		auto 	debug_pass(rg::TemporalGraph& rg, rg::Handle<rhi::GpuTexture>& color_img, rg::Handle<rhi::GpuTexture>& depth_img)->void;

		auto	handle_resize(uint32_t width, uint32_t height)->void;
		auto 	handle_window_resize(uint32_t width, uint32_t height)->void;
		auto	enqueue_render_command(std::function<void()>&& command)->void;
		auto	flush_render_commands()->void;
		auto	wait_for_render_idle()->void;
		struct UiRenderer*	get_ui_renderer() {return ui_renderer
		.get(); }
		auto	get_debug_draw_frame() const -> const DebugDrawFrame& { return debug_draw_frame; }

		auto	set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform)->void;

		auto	set_current_scene(Scene* scene) ->void{ current_scene = scene;}

		auto	handle_new_scene(Scene* scene)->void;
		auto	get_main_render_image()->std::shared_ptr<rhi::GpuTexture> {return main_render_tex; }
		auto	set_main_render_image(std::shared_ptr<rhi::GpuTexture>& tex)->void {main_render_tex = tex; }
		auto	get_render_depth()->std::shared_ptr<rhi::GpuTexture> {return depth_render_tex;}
		auto	get_device() const -> rhi::GpuDevice* { return rg_renderer->device; }
		auto	get_camera()->Camera* {return camera;}
		auto	get_camera_transform()-> maths::Transform* {return camera_transform;}
		auto	get_frame_render_settings() const -> const RenderSettings& { return frame_render_settings; }
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
		auto	render_frame_packet(RenderFramePacket&& packet)->void;
		auto	apply_environment_frame(const EnvironmentFrameParams& environment)->void;
		auto	prepare_environment_resources(RenderFramePacket& packet)->void;
		auto	upload_gpu_buffers(RenderFramePacket& packet)->void;
		auto	upload_gaussian_gpu_buffers(const std::vector<RenderGSCommand>& gs_commands, GpuSceneDirtyState& dirty_state)->void;
		auto	upload_point_cloud_gpu_buffers(const std::vector<RenderPointCommand>& point_commands, GpuSceneDirtyState& dirty_state)->void;
		auto	upload_mesh_gpu_buffers(RenderFramePacket& packet)->void;
		auto	upload_mesh_materials(MeshModel* model, GpuSceneDirtyState& dirty_state)->int;
		auto	is_material_texture_bound(const SharedPtr<asset::Texture>& texture)->bool;
		auto	are_material_textures_bound(const PBRMataterialTextures& textures)->bool;
		auto	update_material_texture_bindings(MaterialProperties& material, const PBRMataterialTextures& textures)->void;
		auto	record_mesh_instance_gpu_state(MeshModel* model, u32 entity_id, const maths::Transform& transform)->void;
		auto	upload_mesh_model(class MeshModel* model)->void;
		auto	upload_material(const struct MaterialProperties* material)->void;
		auto	upload_image(const std::shared_ptr<rhi::GpuTexture>& image)-> BindlessImageHandle;
		auto	defer_release(std::function<void()>&& release)->void;
		auto	retire_deferred_releases(bool release_all = false)->void;
		auto	prepare_render_graph_hybrid(
				rg::TemporalGraph& rg, 
				const FrameParamDesc& frame_desc,
				const EnvironmentFrameParams& environment,
				rg::Handle<rhi::GpuTexture>& accum_img,
				rg::Handle<rhi::GpuTexture>& depth_img) -> rg::Handle<rhi::GpuTexture>;
		auto	prepare_render_graph_pt(
				rg::TemporalGraph& rg, 
				const FrameParamDesc& frame_desc,
				const EnvironmentFrameParams& environment,
				rg::Handle<rhi::GpuTexture>& accum_img,
				rg::Handle<rhi::GpuTexture>& depth_img) -> rg::Handle<rhi::GpuTexture>;
		auto	render_thread_main()->void;
	public:
		std::vector<RenderGSCommand> gs_command_queue;
		std::vector<RenderMeshCommand> mesh_command_queue;
		std::vector<MeshDrawGpuData> mesh_draw_data;
		std::vector<RenderPointCommand>	 point_command_queue;
		GpuSceneUploadState gpu_scene;
		GpuSceneDirtyState gpu_scene_dirty;
		u32 frame_idx = 0;
		std::optional<CameraMatrices> prev_camera_matrix;
		Scene* current_scene = nullptr;
		Camera* camera = nullptr;
		maths::Transform* camera_transform = nullptr;
	public:
	  	std::shared_ptr<rhi::GpuBuffer>     mesh_buffer;
        std::shared_ptr<rhi::GpuBuffer>     material_buffer;
        std::shared_ptr<rhi::GpuBuffer>     bindless_texture_sizes;
		std::shared_ptr<rhi::GpuRayTracingAcceleration> tlas;
	protected:
		std::shared_ptr<rg::Renderer>  rg_renderer;
		std::array<u32, 2>	render_extent;

		std::shared_ptr<struct UiRenderer>	ui_renderer;
		DebugDrawFrame debug_draw_frame;
		std::shared_ptr<GridRenderer> grid_renderer;
		std::shared_ptr<rhi::GpuTexture>	main_render_tex;
		std::shared_ptr<rhi::GpuTexture>	depth_render_tex;

		Camera* override_camera = nullptr;
		maths::Transform* override_camera_transform = nullptr;
		auto add_image_lut(const std::shared_ptr<ImageLut>& computer, u64 id)->void;

	protected:
		std::unique_ptr<GaussianRenderPass> gaussian;
		std::unique_ptr<DebugRenderPass> debug_render_pass;
		std::unique_ptr<IblRenderer>	ibl;
		std::vector<std::shared_ptr<ImageLut>>   image_luts;
		std::unordered_map<std::string, std::shared_ptr<rhi::GpuTexture>> environment_texture_cache;
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
		std::mutex render_command_mutex;
		std::vector<std::function<void()>> pending_render_commands;
		std::thread render_thread;
		mutable std::mutex render_thread_mutex;
		std::condition_variable render_thread_cv;
		std::deque<RenderFramePacket> pending_render_frames;
		std::atomic_bool render_thread_running = false;
		bool render_thread_stop_requested = false;
		bool render_thread_busy = false;
		u64 queued_render_frame_serial = 0;
		u64 completed_render_frame_serial = 0;
		static constexpr size_t MaxPendingRenderFrames = 1;
		struct DeferredRelease
		{
			u64 retire_frame = 0;
			std::function<void()> release;
		};
		std::vector<DeferredRelease> deferred_releases;
		u64 render_frame_serial = 0;
	protected:
		float       	sun_size_multiplier;
		glm::vec3    	sun_color;
		glm::vec4    	sky_ambient;
		glm::vec3    	sun_direction;
		std::vector<TriangleLight> triangle_lights;
		std::vector<FrameLight> primitive_lights;
		RenderSettings frame_render_settings;
	private:
		std::shared_ptr<rhi::DescriptorSet> bindless_descriptor_set;
		std::array<u32, 2>					temporal_upscale_extent;
		std::vector<glm::vec2>   			supersample_offsets;

		bool		reset_pt = false;
		bool 		skip_gs_render = false;
		bool		released = false;
	};
}
