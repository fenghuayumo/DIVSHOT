#include "core/string.h"
#include "defered_renderer.h"
#include "drs_rg/simple_pass.h"
#include "backend/drs_rhi/buffer_builder.h"
#include "engine/engine.h"
#include "engine/window.h"
#include "engine/input.h"
#include "engine/thread_affinity.h"
#include "drs_rg/image_op.h"
#include "ui_renderer.h"
#include "debug_renderer.h"
#include "grid_renderer.h"
#include "lut_renderer.h"
#include "post_process.h"
#include "rasterize_mesh.h"
#include "shadow_denoiser.h"
#include "irache.h"
#include "restir_gi.h"
#include "ssgi.h"
#include "rtr.h"
#include "taa.h"
#include "radiance_cache.h"
#include "point_render.h"
#include "lighting.h"
#include "scene/entity.h"
#include "scene/component/gaussian_component.h"
#include "scene/component/mesh_model_component.h"
#include "scene/component/components.h"
#include "scene/component/point_cloud_component.h"
#include "scene/component/gaussian_crop.h"
#include "scene/component/environment.h"
#include "assets/asset_manager.h"
#include "scene/entity_manager.h"
#include "scene/sun_controller.h"
#include "utility/pack_utils.h"
#include "utility/cmd_variable.h"

#include <execution>
#include <mutex>
#define MATERIAL_BUFFER_CAPACITY 1024 * 512
#define MAX_GPU_MESHES (1024 * 256)
#define VERTEX_BUFFER_CAPACITY (1024 * 1024 * 1024 * 1)
#define TLAS_PREALLOCATE_BYTES (1024 * 1024 * 64)
#define MAX_MESH_INSTANCE (1024 * 1024)

#define GS_BINDING_ID						0
#define SPLAT_STATE_BINDING_ID				1
#define MESH_BUF_BINDING_ID					2
#define MAT_BUF_BINDING_ID					3
#define VERTEX_BUF_BINDING_ID					4
#define INDEX_BUF_BINDING_ID					5
#define TEX_BUF_BINDING_ID						6
#define TEX_SIZE_BUF_BINDING_ID					7
#define POINT_BUF_BINDING_ID					8

#define BrdfFgLut_TEX_ID						0
#define Bluenoise_TEX_ID						1
#define BezoldBruckeLut_TEX_ID					2
#define WHITE_TEX_ID							3
#define NORMAL_TEX_ID							4

#define INSTANCE_MASK_OPAQUE 0x01
#define INSTANCE_MASK_ALPHA_TESTED 0x02
#define INSTANCE_MASK_TRANSPARENT 0x04
#define INSTANCE_MASK_ALL 0xFF

namespace diverse
{
	CmdVariable	accumulate_spp("r.accumulate.spp", 1, "accumulate spp");
	CmdVariable take_photon("r.takePhoton", false, "enable take phton");
	CmdVariable video_export_cmd("r.video_export", false, "enable export video");
}

namespace diverse
{
	namespace
	{
		std::mutex g_render_settings_mutex;
	}

	struct GpuMesh
    {
        u32 vertex_pos_nor_offset;
        u32 vertex_uv_offset;
        u32 vertex_tangent_offset;
		u32 vertex_color_offset = 0;
        u32 vertex_buf_id;
        u32 index_buf_id;
        u32 material_id;
    };

	RenderSettings	g_render_settings;
	auto snapshot_render_settings() -> RenderSettings
	{
		std::lock_guard<std::mutex> lock(g_render_settings_mutex);
		return g_render_settings;
	}

	auto set_render_settings(const RenderSettings& settings) -> void
	{
		std::lock_guard<std::mutex> lock(g_render_settings_mutex);
		g_render_settings = settings;
	}

	void set_gaussian_render_type(GaussianRenderType ty)
	{
		std::lock_guard<std::mutex> lock(g_render_settings_mutex);
		g_render_settings.gs_vis_type = (int)ty;
	}
	GaussianRenderType get_gaussian_render_type() 
	{
		return (GaussianRenderType)snapshot_render_settings().gs_vis_type;
	}

	std::unordered_map<u32, rhi::DescriptorInfo> BINDLESS_DESCRIPTOR_SET_LAYOUT = {
        std::pair{GS_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::RuntimeArray}},
        std::pair{SPLAT_STATE_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::RuntimeArray}},
		std::pair{MESH_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::Single}},
		std::pair{MAT_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::Single}},
		std::pair{VERTEX_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::RuntimeArray}},
		std::pair{INDEX_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::RuntimeArray}},
		std::pair{TEX_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::SAMPLED_IMAGE, rhi::DescriptorDimensionality::RuntimeArray}},
		std::pair{TEX_SIZE_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::Single}},
		std::pair{POINT_BUF_BINDING_ID, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER, rhi::DescriptorDimensionality::RuntimeArray}},
    
	};

	auto radical_inverse(u32 n, u32 base) -> f32
	{
		auto val = 0.0f;
		auto inv_base = 1.0f / base;
		auto inv_bi = inv_base;
		while (n > 0)
		{
			auto d_i = n % base;
			val += d_i * inv_bi;
			n = n * inv_base;
			inv_bi *= inv_base;
		}
		return val;
	}

	extern auto calculate_reprojection_map(rg::TemporalGraph& rg,
		const GbufferDepth& gbuffer_depth,
		const rg::Handle<rhi::GpuTexture>& velocity_img) -> rg::Handle<rhi::GpuTexture>;

	extern auto trace_sun_shadow_mask(
		rg::RenderGraph& rg,
		const GbufferDepth& gbuffer_depth,
		const rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
		rhi::DescriptorSet* bindless_descriptor_set
	) -> rg::Handle<rhi::GpuTexture>;

	extern auto motion_blur(rg::RenderGraph& rg,
		const rg::Handle<rhi::GpuTexture>& input,
		const rg::Handle<rhi::GpuTexture>& depth,
		const rg::Handle<rhi::GpuTexture>& reprojection_map) -> rg::Handle<rhi::GpuTexture>;

	DeferedRenderer::DeferedRenderer(rhi::GpuDevice* device,rhi::Swapchain* swapchain)
	{
		threading::assert_render_thread();
		rg_renderer.reset(new rg::Renderer(device, swapchain));
		ui_renderer.reset(new UiRenderer());

        bindless_descriptor_set = device->create_descriptor_set(BINDLESS_DESCRIPTOR_SET_LAYOUT, "bindless_render_set");

		auto mat_buffer_desc = rhi::GpuBufferDesc::new_cpu_to_gpu(MATERIAL_BUFFER_CAPACITY,
            rhi::BufferUsageFlags::STORAGE_BUFFER
            | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
            | rhi::BufferUsageFlags::TRANSFER_DST);
        material_buffer = device->create_buffer(mat_buffer_desc, "material buffer", nullptr);

        auto buffer_desc = rhi::GpuBufferDesc::new_cpu_to_gpu(MAX_GPU_MESHES * sizeof(GpuMesh), rhi::BufferUsageFlags::STORAGE_BUFFER);
        mesh_buffer = device->create_buffer(buffer_desc, "mesh buffer", nullptr);
  		auto max_bindless_descriptor_count = device->max_bindless_storage_images();
        auto bindless_buffer_desc = rhi::GpuBufferDesc::new_cpu_to_gpu(max_bindless_descriptor_count * sizeof(glm::vec4),rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST);
        bindless_texture_sizes = device->create_buffer(bindless_buffer_desc,
            "bindless_textures_size",
            nullptr);
		device->write_descriptor_set(bindless_descriptor_set.get(), MESH_BUF_BINDING_ID, mesh_buffer.get());
        device->write_descriptor_set(bindless_descriptor_set.get(), MAT_BUF_BINDING_ID, material_buffer.get());
        device->write_descriptor_set(bindless_descriptor_set.get(), TEX_SIZE_BUF_BINDING_ID, bindless_texture_sizes.get());

		auto supersample_count = 128;
		for (auto i = 1; i <= supersample_count; i++)
		{
			supersample_offsets.push_back(glm::vec2(radical_inverse(i, 2) - 0.5, radical_inverse(i, 3) - 0.5));
		}
	}
	DeferedRenderer::~DeferedRenderer()
	{
		stop_render_thread();
		release();
	}
	void DeferedRenderer::init(std::array<u32, 2> swapchain_extent)
	{
		threading::assert_render_thread();
		auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, swapchain_extent).with_usage(
			rhi::TextureUsageFlags::SAMPLED |
			rhi::TextureUsageFlags::STORAGE |
			rhi::TextureUsageFlags::TRANSFER_DST |
			rhi::TextureUsageFlags::TRANSFER_SRC | 
			rhi::TextureUsageFlags::COLOR_ATTACHMENT);
		main_render_tex = rg_renderer->device->create_texture(desc, {}, "main_tex");
		depth_render_tex = rg_renderer->device->create_texture(
							rhi::GpuTextureDesc::new_2d(PixelFormat::D32_Float,swapchain_extent)
							.with_usage(rhi::TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT |
							rhi::TextureUsageFlags::TRANSFER_DST |
							rhi::TextureUsageFlags::TRANSFER_SRC |
							rhi::TextureUsageFlags::SAMPLED
							), 
							{}, "depth_tex");
		temporal_upscale_extent = main_render_tex->desc.extent_2d();
		DebugRenderer::Init();
		
		grid_renderer.reset(new GridRenderer(rg_renderer->device));
		gaussian.reset(new GaussianRenderPass(this));
		debug_render_pass.reset(new DebugRenderPass(this));
		ibl.reset(new IblRenderer());
		post.reset(new PostProcessRenderer(rg_renderer->device));
 		taa.reset(new TaaRenderer());
		rasterizer.reset(new RasterizeMesh(this));
		shadow_denoise.reset(new ShadowDenoiser());
		restir_gi.reset(new RestirGiRenderer());
		irache.reset(new IracheRender());
		ssgi.reset(new SsgiRenderer());
		rtr.reset(new RtrRenderer(rg_renderer->device));
		point_pass.reset(new PointRenderPass(this));
		lighting_pass.reset(new LightingPass(this));

		sun_size_multiplier = 1.0;
		sun_color = glm::vec4(1, 1, 1, 1);
		sky_ambient = glm::vec4(0, 0, 0,1);

		add_image_lut(std::make_shared<BrdfFgLutComputer>(rg_renderer->device), BrdfFgLut_TEX_ID);
		add_image_lut(std::make_shared<BlueNoiseLutComputer>(rg_renderer->device), Bluenoise_TEX_ID);
		add_image_lut(std::make_shared<BezoldBruckeLutComputer>(rg_renderer->device), BezoldBruckeLut_TEX_ID);

		auto white_texture = ResourceManager<asset::Texture>::get().get_default_resource("white");
		auto normal_texture = ResourceManager<asset::Texture>::get().get_default_resource("normal");
		if (white_texture)
			white_texture->upload_to_gpu(rg_renderer->device);
		if (normal_texture)
			normal_texture->upload_to_gpu(rg_renderer->device);
		upload_image(white_texture ? white_texture->gpu_texture : nullptr);
		upload_image(normal_texture ? normal_texture->gpu_texture : nullptr);
		build_ray_tracing_top_level_acceleration();
	}

	void DeferedRenderer::render(std::array<u32, 2> swapchain_extent)
	{
		threading::assert_game_thread();
		const auto delta_dt = static_cast<f32>(Engine::get_time_step().get_seconds());
		auto packet = build_render_frame_packet(delta_dt, swapchain_extent);
		if (!packet)
			return;

		if (is_render_thread_running())
			enqueue_render_frame_packet(std::move(*packet));
		else
			submit_render_frame_packet(std::move(*packet));
	}

	auto DeferedRenderer::start_render_thread() -> void
	{
		threading::assert_game_thread();
		if (render_thread_running.load(std::memory_order_acquire))
			return;

		{
			std::lock_guard lock(render_thread_mutex);
			render_thread_stop_requested = false;
			render_thread_busy = false;
			pending_render_frames.clear();
			queued_render_frame_serial = 0;
			completed_render_frame_serial = 0;
		}

		render_thread_running.store(true, std::memory_order_release);
		render_thread = std::thread([this]() {
			render_thread_main();
		});
	}

	auto DeferedRenderer::stop_render_thread() -> void
	{
		if (!render_thread_running.load(std::memory_order_acquire))
			return;

		if (threading::is_render_thread())
			return;

		wait_for_render_idle();
		{
			std::lock_guard lock(render_thread_mutex);
			render_thread_stop_requested = true;
		}
		render_thread_cv.notify_all();

		if (render_thread.joinable())
			render_thread.join();
	}

	auto DeferedRenderer::enqueue_render_frame_packet(RenderFramePacket&& packet) -> void
	{
		if (!render_thread_running.load(std::memory_order_acquire))
		{
			submit_render_frame_packet(std::move(packet));
			return;
		}

		{
			std::lock_guard lock(render_thread_mutex);
			while (pending_render_frames.size() >= MaxPendingRenderFrames)
			{
				pending_render_frames.pop_front();
				completed_render_frame_serial++;
			}
			pending_render_frames.push_back(std::move(packet));
			queued_render_frame_serial++;
		}
		render_thread_cv.notify_one();
	}

	auto DeferedRenderer::render_thread_main() -> void
	{
		threading::mark_render_thread();

		for (;;)
		{
			std::optional<RenderFramePacket> packet;
			{
				std::unique_lock lock(render_thread_mutex);
				render_thread_cv.wait(lock, [this]() {
					std::lock_guard command_lock(render_command_mutex);
					return render_thread_stop_requested || !pending_render_frames.empty() || !pending_render_commands.empty();
				});

				if (render_thread_stop_requested && pending_render_frames.empty())
				{
					std::lock_guard command_lock(render_command_mutex);
					if (pending_render_commands.empty())
						break;
				}

				if (!pending_render_frames.empty())
				{
					packet = std::move(pending_render_frames.front());
					pending_render_frames.pop_front();
				}
				render_thread_busy = true;
			}

			flush_render_commands();
			if (packet)
			{
				submit_render_frame_packet(std::move(*packet));
				{
					std::lock_guard lock(render_thread_mutex);
					completed_render_frame_serial++;
				}
			}

			{
				std::lock_guard lock(render_thread_mutex);
				render_thread_busy = false;
			}
			render_thread_cv.notify_all();
		}

		flush_render_commands();
		release();
		render_thread_running.store(false, std::memory_order_release);
		render_thread_cv.notify_all();
	}

	auto DeferedRenderer::build_render_frame_packet(f32 delta_dt, std::array<u32, 2> swapchain_extent)->std::optional<RenderFramePacket>
	{
		threading::assert_game_thread();
		if (!current_scene)
			return std::nullopt;

		auto& registry = current_scene->get_registry();
		if (override_camera)
		{
			camera = override_camera;
			camera_transform = override_camera_transform;
		}
		else
		{
			auto cameraView = registry.view<Camera>();
			if (!cameraView.empty())
			{
				camera = &cameraView.get<Camera>(cameraView.front());
				camera_transform = registry.try_get<maths::Transform>(cameraView.front());
			}
		}

		if(!camera || !camera_transform)
			return std::nullopt;

		RenderFramePacket packet;
		packet.render_settings = snapshot_render_settings();
		packet.delta_t = delta_dt;
		packet.swapchain_extent = swapchain_extent;
		packet.frame_desc.render_extent = main_render_tex->desc.extent_2d();//swapchain_extent;
		auto projection = camera->get_projection_matrix();
		auto invProj = glm::inverse(projection);
		auto invView = camera_transform->get_world_matrix();
		auto view = glm::inverse(invView);

		packet.frame_desc.camera_matrices = CameraMatrices{
			projection,
			invProj,
			view,
			invView
		};
		packet.camera_params.fov_rad = glm::radians(camera->get_fov());
		packet.camera_params.dof_enabled = camera->is_dof_enabled() ? 1.0f : 0.0f;
		packet.camera_params.focus_distance = camera->get_focus_distance();
		packet.camera_params.aperture = camera->get_aperture() * 0.001f;
		packet.camera_params.camera_type = camera->get_camera_type() == Camera::CameraType::Fisheye ? 1u : 0u;

		auto enviroment_view = current_scene->get_entity_manager()->get_entities_with_type<diverse::Environment>();
		if (!enviroment_view.empty())
		{
			auto ent = enviroment_view.front().get_component<Environment>();
			packet.environment.has_environment = true;
			packet.environment.dirty = ent.dirty_flag;
			packet.environment.mode = (f32)ent.mode;
			packet.environment.color = ent.get_color();
			packet.environment.sun_color = ent.mode == Environment::Mode::Pure ? glm::vec3(0.0f) : ent.sun_color;
			packet.environment.sun_size_multiplier = ent.sun_size_multiplier;
			packet.environment.sky_ambient = glm::vec4(ent.sky_ambient, ent.intensity);
			packet.environment.hdr_img = ent.mode == Environment::Mode::HDR ? ent.get_enviroment_map() : nullptr;
			packet.environment.hdr_path = ent.mode == Environment::Mode::HDR ? ent.get_file_path() : std::string{};
			packet.environment.ibl_params = { ent.theta, ent.phi };
			packet.environment.cube_resolution = ent.cubeResolution;
			packet.environment.sun_direction = sun_direction;
			if (ent.mode == Environment::Mode::SunSky)
			{
				auto* sun_controller = enviroment_view.front().try_get_component<SunController>();
				if (sun_controller)
				{
					packet.environment.sun_direction = sun_controller->towards_sun();
				}
			}
		}

		if( prev_camera_matrix && prev_camera_matrix->world_to_view != packet.frame_desc.camera_matrices.world_to_view )
			reset_pt = true;

		auto group = registry.group<GaussianComponent>(entt::get<maths::Transform>);
		bool skip_render = false;
		for (auto gs_ent : group)
		{
			if (!Entity(gs_ent, current_scene).active())
				continue;

			const auto& [gs_com, trans] = group.get<GaussianComponent, maths::Transform>(gs_ent);
			if(!gs_com.ModelRef->is_flag_set(AssetFlag::Loaded) || !gs_com.participate_render) continue;
			skip_render |= gs_com.skip_render;
			auto offset = -gs_com.black_point + gs_com.brightness;
			auto scale = 1.0f / (gs_com.white_point - gs_com.black_point);
			packet.gs_commands.push_back(RenderGSCommand{ trans,
										gs_com.ModelRef,
										(u32)gs_com.sh_degree,
										packet.render_settings.select_color,
										packet.render_settings.locked_color,
										glm::vec4(gs_com.albedo_color.xyz * scale, gs_com.transparency),
										glm::vec3(offset,offset,offset),	
										gs_com.mip_antialiased});
			
			const auto crop = Entity(gs_ent, current_scene).try_get_component<GaussianCrop>();
			if (crop)
			{
				for (auto& crop_data : crop->get_crop_data())
				{
					packet.gs_commands.back().crop_data.push_back(crop_data);
				}
			}
		}
		packet.skip_gs_render = skip_render;
		//pointcloud
		auto pointcloud_group = registry.group<PointCloudComponent>(entt::get<maths::Transform>);
		for(auto pcd : pointcloud_group)
		{
			if (!Entity(pcd, current_scene).active())
				continue;

			const auto& [pcd_com, trans] = pointcloud_group.get<PointCloudComponent, maths::Transform>(pcd);
			if(!pcd_com.ModelRef->is_flag_set(AssetFlag::Loaded) ) continue;
			packet.point_commands.push_back(RenderPointCommand{ trans,
										pcd_com.ModelRef});
		}
		auto meshgroup = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
		// if (!tlas && gpu_scene.instance_transforms.size() > 0 && gpu_scene.ent_2_model_id.size() == meshgroup.size())
		// 	build_ray_tracing_top_level_acceleration();
		for (auto mesh_ent : meshgroup)
		{
			const auto& [model, trans] = meshgroup.get<MeshModelComponent, maths::Transform>(mesh_ent);
			packet.mesh_frame_states.push_back(MeshFrameState{ (u32)mesh_ent, trans.get_world_matrix() });
			if (!model.ModelRef) continue;
			auto mesh_active = Entity(mesh_ent, current_scene).active();
			packet.mesh_requests.push_back(MeshDrawRequest{ (u32)mesh_ent, trans, model.ModelRef, mesh_active });
		}
		return packet;
	}

	auto DeferedRenderer::submit_render_frame_packet(RenderFramePacket&& packet)->void
	{
		threading::assert_render_thread();
		flush_render_commands();
		prepare_environment_resources(packet);
		upload_gpu_buffers(packet);

		auto mesh_frame_states = std::move(packet.mesh_frame_states);
		render_frame_packet(std::move(packet));
		retire_frame(mesh_frame_states);
		retire_deferred_releases();
	}

	auto DeferedRenderer::render_frame_packet(RenderFramePacket&& packet)->void
	{
		threading::assert_render_thread();
		gs_command_queue = std::move(packet.gs_commands);
		mesh_command_queue = std::move(packet.mesh_commands);
		point_command_queue = std::move(packet.point_commands);
		triangle_lights = std::move(packet.triangle_lights);
		gpu_scene.rt_instance_masks = std::move(packet.rt_instance_masks);
		skip_gs_render = packet.skip_gs_render;
		frame_render_settings = packet.render_settings;

		auto rg = rg_renderer->temporal_graph();
		const auto frame_desc = packet.frame_desc;
		const auto camera_params = packet.camera_params;
		const auto environment = packet.environment;
		const auto delta_dt = packet.delta_t;
		const auto swapchain_extent = packet.swapchain_extent;
		apply_environment_frame(environment);
		post->update_pre_exposure(frame_render_settings, delta_dt);

		rg_renderer->prepare_frame_constants(rg,
			[this, frame_desc, camera_params, delta_dt](rhi::DynamicConstants& dynamic_constants)->rg::FrameConstantsLayout {
				return prepare_frame_constants(dynamic_constants, frame_desc, camera_params, delta_dt);
			}
		);
		rg_renderer->prepare_frame(rg,
			[this, frame_desc, environment, swapchain_extent](rg::TemporalGraph& rg) {
				auto main_img = prepare_render_graph(rg, frame_desc, environment);
				auto ui_img = ui_renderer->prepare_render_graph(rg);
				auto swap_chain = rg.get_swap_chain();

				rg::RenderPass::new_compute(
					rg.add_pass("final blit"),
					"/shaders/final_blit.hlsl")
					.read(main_img)
					.read(ui_img)
					.write(swap_chain)
					.constants(std::tuple{
						main_img.desc.extent_inv_extent_2d(),
						std::array<float,4>{ (float)swapchain_extent[0],
							  (float)swapchain_extent[1],
							  1.0f / swapchain_extent[0],
							  1.0f / swapchain_extent[1]}
						})
					.dispatch({ swapchain_extent[0], swapchain_extent[1], 1 });
			});

		rg_renderer->draw_frame(rg,
			rg_renderer->swap_chain);
	}

	auto DeferedRenderer::enqueue_render_command(std::function<void()>&& command) -> void
	{
		{
			std::lock_guard lock(render_command_mutex);
			pending_render_commands.push_back(std::move(command));
		}
		render_thread_cv.notify_one();
	}

	auto DeferedRenderer::flush_render_commands() -> void
	{
		if (!threading::is_render_thread() && render_thread_running.load(std::memory_order_acquire))
		{
			auto barrier = std::make_shared<std::promise<void>>();
			auto future = barrier->get_future();
			enqueue_render_command([barrier]() {
				barrier->set_value();
			});
			future.wait();
			return;
		}

		threading::assert_render_thread();
		std::vector<std::function<void()>> commands;
		{
			std::lock_guard lock(render_command_mutex);
			commands.swap(pending_render_commands);
		}

		for (auto& command : commands)
		{
			if (command)
				command();
		}
	}

	auto DeferedRenderer::wait_for_render_idle() -> void
	{
		if (!threading::is_render_thread() && render_thread_running.load(std::memory_order_acquire))
		{
			u64 target_frame = 0;
			{
				std::lock_guard lock(render_thread_mutex);
				target_frame = queued_render_frame_serial;
			}

			flush_render_commands();

			std::unique_lock lock(render_thread_mutex);
			render_thread_cv.wait(lock, [this, target_frame]() {
				std::lock_guard command_lock(render_command_mutex);
				return completed_render_frame_serial >= target_frame &&
					pending_render_frames.empty() &&
					pending_render_commands.empty() &&
					!render_thread_busy;
			});
			return;
		}

		threading::assert_render_thread();
		flush_render_commands();
		retire_deferred_releases(true);
	}

	auto DeferedRenderer::apply_environment_frame(const EnvironmentFrameParams& environment)->void
	{
		if (!environment.has_environment)
			return;

		if (environment.dirty)
			invalidate_pt_state();

		sun_color = environment.sun_color;
		sun_size_multiplier = environment.sun_size_multiplier;
		sky_ambient = environment.sky_ambient;
		sun_direction = environment.sun_direction;
	}

	auto DeferedRenderer::prepare_environment_resources(RenderFramePacket& packet)->void
	{
		threading::assert_render_thread();
		auto& environment = packet.environment;
		if (!environment.has_environment || environment.mode != (f32)Environment::Mode::HDR || environment.hdr_path.empty())
			return;

		auto cached = environment_texture_cache.find(environment.hdr_path);
		if (cached != environment_texture_cache.end())
		{
			environment.hdr_img = cached->second;
			return;
		}

		if (!std::filesystem::exists(environment.hdr_path))
			return;

		auto raw_img = asset::load_float_image(environment.hdr_path).convert(PixelFormat::R16G16B16A16_Float);
		auto img_desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, raw_img.dimensions)
			.with_usage(rhi::TextureUsageFlags::SAMPLED);
		const u32 PIXEL_BYTES = 4 * 2;
		rhi::ImageSubData sub_data = {
			(u8*)raw_img.data.data(),
			(u32)raw_img.data.size(),
			raw_img.dimensions[0] * PIXEL_BYTES,
			raw_img.dimensions[0] * raw_img.dimensions[1] * PIXEL_BYTES
		};
		environment.hdr_img = rg_renderer->device->create_texture(img_desc, { sub_data }, "ibl");
		environment_texture_cache[environment.hdr_path] = environment.hdr_img;
	}

	auto DeferedRenderer::upload_gpu_buffers(RenderFramePacket& packet)->void
	{
		threading::assert_render_thread();
		upload_gaussian_gpu_buffers(packet.gs_commands);
		upload_point_cloud_gpu_buffers(packet.point_commands);
		upload_mesh_gpu_buffers(packet);
	}

	auto DeferedRenderer::defer_release(std::function<void()>&& release) -> void
	{
		threading::assert_render_thread();
		if (!release)
			return;

		deferred_releases.push_back(DeferredRelease{
			render_frame_serial + 3,
			std::move(release)
		});
	}

	auto DeferedRenderer::retire_deferred_releases(bool release_all) -> void
	{
		threading::assert_render_thread();
		if (release_all)
		{
			for (auto& deferred : deferred_releases)
			{
				if (deferred.release)
					deferred.release();
			}
			deferred_releases.clear();
			return;
		}

		for (auto iter = deferred_releases.begin(); iter != deferred_releases.end();)
		{
			if (iter->retire_frame <= render_frame_serial)
			{
				if (iter->release)
					iter->release();
				iter = deferred_releases.erase(iter);
			}
			else
			{
				++iter;
			}
		}
		++render_frame_serial;
	}

	auto DeferedRenderer::upload_gaussian_gpu_buffers(const std::vector<RenderGSCommand>& gs_commands)->void
	{
		threading::assert_render_thread();
		auto device = rg_renderer->device;
		for (const auto& command : gs_commands)
		{
			auto* model = command.model.get();
			if (!model || !model->is_flag_set(AssetFlag::Loaded)) continue;
			if (!model->is_flag_set(AssetFlag::UploadedGpu))
				model->create_gpu_buffer(device, true);
			if (!model->is_flag_set(AssetFlag::UploadedGpu)) continue;
			model->splat_transforms.upload(device);
			u32 v_buf_id = gpu_scene.model_2_gs_buf_id.size();
			if (model->gaussians_buf && gpu_scene.model_2_gs_buf_id.find(model) == gpu_scene.model_2_gs_buf_id.end())
			{
				device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model->gaussians_buf.get(), v_buf_id * 4 + 0);
				device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model->gaussians_sh_0_buf.get(), v_buf_id * 4 + 1);
				device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model->gaussians_sh_n_buf.get(), v_buf_id * 4 + 2);
				device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model->splat_transforms.splat_transform_buffer.get(), v_buf_id * 4 + 3);

				device->write_descriptor_set(bindless_descriptor_set.get(), SPLAT_STATE_BINDING_ID, model->gaussian_state_buf.get(), v_buf_id);
				gpu_scene.model_2_gs_buf_id[model] = v_buf_id;
				skip_gs_render = false;
			}
		}
	}

	auto DeferedRenderer::upload_point_cloud_gpu_buffers(const std::vector<RenderPointCommand>& point_commands)->void
	{
		threading::assert_render_thread();
		auto device = rg_renderer->device;
		for (const auto& command : point_commands)
		{
			auto* model = command.model.get();
			if (!model || !model->is_flag_set(AssetFlag::Loaded)) continue;
			if (!model->is_flag_set(AssetFlag::UploadedGpu))
				model->create_gpu_buffer(device);
			if (!model->is_flag_set(AssetFlag::UploadedGpu)) continue;
			u32 v_buf_id = gpu_scene.model_2_point_buf_id.size();
			if (model->vertex_buffer && gpu_scene.model_2_point_buf_id.find(model) == gpu_scene.model_2_point_buf_id.end())
			{
				device->write_descriptor_set(bindless_descriptor_set.get(), POINT_BUF_BINDING_ID, model->vertex_buffer.get(), v_buf_id);
				gpu_scene.model_2_point_buf_id[model] = v_buf_id;
			}
		}
	}

	auto DeferedRenderer::is_material_texture_bound(const SharedPtr<asset::Texture>& texture)->bool
	{
		if (!texture)
			return true;
		if (texture->is_flag_set(AssetFlag::Invalid))
			return true;
		if (!texture->is_flag_set(AssetFlag::Loaded))
			return false;

		if (!texture->is_flag_set(AssetFlag::UploadedGpu))
			texture->upload_to_gpu(rg_renderer->device);
		if (!texture->is_flag_set(AssetFlag::UploadedGpu))
		{
			texture->set_flag(AssetFlag::Invalid);
			return true;
		}

		upload_image(texture->gpu_texture);

		return texture->is_flag_set(AssetFlag::UploadedGpu) &&
			gpu_scene.bindless_image_ids.find(texture->gpu_texture.get()) != gpu_scene.bindless_image_ids.end();
	}

	auto DeferedRenderer::are_material_textures_bound(const PBRMataterialTextures& textures)->bool
	{
		return is_material_texture_bound(textures.albedo) &&
			is_material_texture_bound(textures.emissive) &&
			is_material_texture_bound(textures.normal) &&
			is_material_texture_bound(textures.metallic) &&
			is_material_texture_bound(textures.roughness) &&
			is_material_texture_bound(textures.ao);
	}

	auto DeferedRenderer::update_material_texture_bindings(MaterialProperties& matprop, const PBRMataterialTextures& pbr_tex)->void
	{
		auto image_handle_or_default = [this](const SharedPtr<asset::Texture>& texture, u32 default_id)->u32 {
			if (!texture || !texture->gpu_texture)
				return default_id;
			return upload_image(texture->gpu_texture).handle;
		};

		matprop.albedo_map = image_handle_or_default(pbr_tex.albedo, WHITE_TEX_ID);
		matprop.emissive_map = image_handle_or_default(pbr_tex.emissive, WHITE_TEX_ID);
		matprop.normal_map = image_handle_or_default(pbr_tex.normal, NORMAL_TEX_ID);
		matprop.metallic_map = image_handle_or_default(pbr_tex.metallic, WHITE_TEX_ID);
		matprop.roughness_map = image_handle_or_default(pbr_tex.roughness, WHITE_TEX_ID);
		matprop.ao_map = image_handle_or_default(pbr_tex.ao, WHITE_TEX_ID);
	}

	auto DeferedRenderer::upload_mesh_materials(MeshModel* model)->int
	{
		int upload_material_num = 0;
		for (auto& mesh : model->get_meshes())
		{
			auto material = mesh->get_material().get();
			if(!material) continue;
			auto& matprop = material->get_properties();
			auto& pbr_tex = material->get_textures();
			if(!are_material_textures_bound(pbr_tex)) continue;
			//if(material.is_flag_set(AssetFlag::UploadedGpu) && !material.dirty_flag()) continue;
			if (!material->is_flag_set(AssetFlag::UploadedGpu))
			{
				gpu_scene.mat_2_mat_buf_id[material] = gpu_scene.material_datas.size();
				gpu_scene.material_datas.push_back(&matprop);
				update_material_texture_bindings(matprop, pbr_tex);
				upload_material(&matprop);
				material->set_flag(AssetFlag::UploadedGpu);
			}
			else if (material->dirty_flag())
			{
				update_material_texture_bindings(matprop, pbr_tex);
				upload_material(&matprop);
			}
			upload_material_num++;
		}
		return upload_material_num;
	}

	auto DeferedRenderer::record_mesh_instance_gpu_state(MeshModel* model, u32 entity_id, const maths::Transform& transform)->void
	{
		u32 model_id = gpu_scene.model_2_blas_id[model];
		gpu_scene.ent_2_model_id.push_back(model_id);
		gpu_scene.instance_transforms.push_back({});
		auto world_transform = glm::transpose(transform.get_world_matrix());
		gpu_scene.instance_transforms.back().transform = world_transform;
		if (gpu_scene.previous_transforms.find(entity_id) == gpu_scene.previous_transforms.end())
			gpu_scene.instance_transforms.back().previous_transform = world_transform;
		else
			gpu_scene.instance_transforms.back().previous_transform = glm::transpose(gpu_scene.previous_transforms[entity_id]);
		
		gpu_scene.instance_dynamic_constants.push_back(InstanceDynamicConstants{});
		auto& instance = gpu_scene.instance_dynamic_constants.back();
		instance.gemoetry_offset = gpu_scene.model_2_mesh_buf_id[model];
		instance.emissive_multiplier = 1.0f;
	}

	auto DeferedRenderer::upload_mesh_gpu_buffers(RenderFramePacket& packet)->void
	{
		threading::assert_render_thread();
		packet.mesh_commands.clear();
		packet.triangle_lights.clear();
		packet.rt_instance_masks.clear();
		gpu_scene.ent_2_model_id.clear(); gpu_scene.instance_transforms.clear();gpu_scene.instance_dynamic_constants.clear();
		for (const auto& request : packet.mesh_requests)
		{
			auto* model = request.model.get();
			if (!model) continue;
			if (!model->is_flag_set(AssetFlag::Loaded)) continue;
			auto upload_material_num = upload_mesh_materials(model);
			if (model->is_flag_set(AssetFlag::Loaded) && 
				!model->is_flag_set(AssetFlag::UploadedGpu)
				&& upload_material_num == model->get_meshes().size())
			{
				model->create_gpu_buffers(rg_renderer->device);
				upload_mesh_model(model);
				model->set_flag(AssetFlag::UploadedGpu);
				if (!gpu_scene.mesh_blas.empty())
					gpu_scene.model_2_blas_id[model] = gpu_scene.mesh_blas.size() - 1;
			}
			if (model->is_flag_set(AssetFlag::UploadedGpu))
			{
				auto mesh_instance_id = (u32)packet.rt_instance_masks.size();
				packet.rt_instance_masks.push_back(request.active ? 0xff : 0);
				record_mesh_instance_gpu_state(model, request.entity_id, request.transform);
				if (!request.active)
					continue;

				const auto& meshes = model->get_meshes();
				for (auto mesh : meshes)
				{
					if (!mesh)
						continue;

					RenderMeshCommand command;
					command.material_id = gpu_scene.mat_2_mat_buf_id[mesh->get_material().get()];
					command.mesh_instance_id = mesh_instance_id;
					command.mesh_id = gpu_scene.mesh_2_mesh_buf_id[mesh.get()];
					packet.mesh_commands.push_back(command);

					auto material = mesh->get_material().get();
					if (material)
					{
						auto emissive = material->get_properties().emissive;
						if(emissive.x > 0 || emissive.y > 0 || emissive.z > 0)
						{
							TriangleLight triangle_light = {};
							triangle_light.instance_id = mesh_instance_id;
							triangle_light.mesh_id = gpu_scene.mesh_2_mesh_buf_id[mesh.get()];
							triangle_light.triangle_count = mesh->get_index_count() / 3;
							packet.triangle_lights.push_back(triangle_light);
						}
					}
				}
			}
		}
	}

	auto DeferedRenderer::prepare_render_graph(rg::TemporalGraph& rg, const FrameParamDesc& frame_desc, const EnvironmentFrameParams& environment) -> rg::Handle<rhi::GpuTexture>
	{	
		rg.predefined_descriptor_set_layouts.insert({ 1, rg::PredefinedDescriptorSet{BINDLESS_DESCRIPTOR_SET_LAYOUT} });
     	
		for(auto& img_lut : image_luts)
        {
            img_lut->compute_if_needed(rg);
        }

		rg::Handle<rhi::GpuTexture> output;
		auto accum_img = rg.import_res<rhi::GpuTexture>(main_render_tex, rhi::AccessType::Nothing);
		auto depth_img = rg.import_res(depth_render_tex, rhi::AccessType::Nothing);
		if( skip_gs_render)
			return accum_img;
		rg::clear_color(rg, accum_img, { 0.0f,0.0f,0.0f,1.0f });
		rg::clear_depth(rg, depth_img, 0.0f);
#ifdef DS_PLATFORM_WINDOWS
		switch (frame_render_settings.render_mode)
		{
		case RenderMode::Hybrid:
		{
			if(!take_photon.get_value<bool>())
				taa->current_supersample_offset = supersample_offsets[frame_idx % supersample_offsets.size()];

			output =  prepare_render_graph_hybrid(rg, frame_desc, environment, accum_img,depth_img);
		}break;
		case RenderMode::PT:
		{
			taa->current_supersample_offset = glm::vec2(0);
			output = prepare_render_graph_pt(rg, frame_desc, environment, accum_img,depth_img);
		}break;
		default:
		{
			taa->current_supersample_offset = glm::vec2(0);
			output = prepare_render_graph_pt(rg, frame_desc, environment, accum_img,depth_img);
		}break;
		}
#elif DS_PLATFORM_MACOS
		taa->current_supersample_offset = glm::vec2(0);
		output = prepare_render_graph_pt(rg, frame_desc, environment, accum_img,depth_img);
#endif
		gpu_events.clear();
		return output;
	}
	auto DeferedRenderer::prepare_render_graph_hybrid(
			rg::TemporalGraph& rg, 
			const FrameParamDesc& frame_desc,
			const EnvironmentFrameParams& environment,
			rg::Handle<rhi::GpuTexture>& accum_img,
			rg::Handle<rhi::GpuTexture>& depth_img)->rg::Handle<rhi::GpuTexture>
	{
		if (environment.has_environment)
		{
			auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, frame_desc.render_extent).with_usage(
				rhi::TextureUsageFlags::SAMPLED |
				rhi::TextureUsageFlags::STORAGE |
				rhi::TextureUsageFlags::TRANSFER_DST);
			auto hybrid_accum_img = rg.get_or_create_temporal("root.accum", desc);

			auto ibl_tex = ibl->render(rg, environment.hdr_img, environment.ibl_params);
			auto sky_cube = ibl_tex ? ibl_tex.value() : sky::render_sky_cube(rg, glm::vec4(environment.color, environment.mode), environment.cube_resolution);
			auto convolved_sky_cube = sky::convolve_cube(rg, sky_cube);

			std::optional<rg::Handle<rhi::GpuRayTracingAcceleration>> tlas = this->prepare_top_level_acceleration(rg);
			auto normal = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R10G10B10A2_UNorm, accum_img.desc.extent_2d()), "normal");
			auto gbuffer = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32B32A32_Float, accum_img.desc.extent_2d()), "gbuffer");
			GbufferDepth gbuffer_depth(std::move(normal), std::move(gbuffer), std::move(depth_img));
			auto velocity_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, accum_img.desc.extent_2d()), "velocity");
			rasterizer->raster_gbuffer(rg, gbuffer_depth, velocity_img);

			auto reprojection_map = calculate_reprojection_map(
				rg,
				gbuffer_depth,
				velocity_img
			);

			auto ssgi_tex = ssgi->render(
				rg,
				gbuffer_depth,
				reprojection_map,
				hybrid_accum_img,
				bindless_descriptor_set.get()
			);

			auto irache_state = irache->prepare(rg);

			auto radiance_cache_state = radiance_cache::allocate_dummy_ouput(rg);
			std::optional<IracheIrradiancePendingSummation> traced_ircache = tlas ? irache_state.trace_irradiance(
				rg,
				convolved_sky_cube,
				bindless_descriptor_set.get(),
				tlas.value(),
				radiance_cache_state
			) : std::optional<IracheIrradiancePendingSummation>();

			auto sun_shadow_mask = tlas ? trace_sun_shadow_mask(rg, gbuffer_depth, tlas.value(), bindless_descriptor_set.get()) :
				rg.create<rhi::GpuTexture>(gbuffer_depth.geometric_normal.desc.with_format(PixelFormat::R8_UNorm));
			if(!tlas) rg::clear_color(rg,sun_shadow_mask,{1,1,1,1});
			auto reprojected_gi = restir_gi->reproject(rg, reprojection_map);

			auto denoised_shadow_mask = sun_size_multiplier > 0.0f ? shadow_denoise->render(rg, gbuffer_depth, sun_shadow_mask, reprojection_map) : sun_shadow_mask;
			if (traced_ircache) {
				irache_state.sum_up_irradiance_for_sampling(rg, *traced_ircache);
			}
			std::optional<rg::Handle<rhi::GpuTexture>> rtgi_irradiance;
			std::optional<RestirCandidates> rtgi_candidates;
			if (tlas)
			{
				auto gi_output = restir_gi->render(
					rg,
					std::move(reprojected_gi),
					gbuffer_depth,
					reprojection_map,
					convolved_sky_cube,
					bindless_descriptor_set.get(),
					irache_state,
					radiance_cache_state,
					*tlas,
					ssgi_tex
				);
				rtgi_candidates = gi_output.candidates;
				rtgi_irradiance = gi_output.screen_irradiance_tex;
			}

			auto traced_rtr = rtgi_irradiance ? rtr->trace(
				rg,
				gbuffer_depth,
				reprojection_map,
				sky_cube,
				bindless_descriptor_set.get(),
				*tlas,
				*rtgi_irradiance,
				*rtgi_candidates,
				irache_state,
				radiance_cache_state
			) : rtr->create_dummy_output(rg, gbuffer_depth);

			auto rt_reflection = traced_rtr.filter_temporal(rg, gbuffer_depth, reprojection_map);

			auto debug_out_tex = rg.create<rhi::GpuTexture>(
				rhi::GpuTextureDesc::new_2d(
					PixelFormat::R16G16B16A16_Float,
					gbuffer_depth.gbuffer.desc.extent_2d()
				), "debug_out_tex"
			);
			auto rtgi = rtgi_irradiance ? *rtgi_irradiance : rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, { 1,1 }));

			lighting_pass->lighting_gbuffer(
				rg,
				gbuffer_depth,
				denoised_shadow_mask,
				rt_reflection,
				rtgi,
				irache_state,
				radiance_cache_state,
				hybrid_accum_img,
				debug_out_tex,
				reprojection_map,
				sky_cube,
				convolved_sky_cube,
				bindless_descriptor_set.get(),
				(int)frame_render_settings.shade_mode
			);
			if (frame_render_settings.show_wireframe)
				rasterizer->raster_wire_frame(rg, debug_out_tex, depth_img);
			std::optional<rg::Handle<rhi::GpuTexture>>  anti_aliased;
			auto anti_aliased_tex = take_photon.get_value<bool>() ? debug_out_tex : anti_aliased.value_or(taa->render(
				rg,
				debug_out_tex,
				reprojection_map,
				gbuffer_depth.depth,
				this->temporal_upscale_extent
			).this_frame_out);
			if (accumulate_spp.get_value<int>() > 1)
			{
				anti_aliased_tex = taa->accumulate_blend(rg,anti_aliased_tex);
			}
			auto final_post_input = motion_blur(
				rg,
				anti_aliased_tex,
				gbuffer_depth.depth,
				reprojection_map
			);
			auto post_processed = post->render(
				rg,
				final_post_input,
				bindless_descriptor_set.get(),
				frame_render_settings
			);

			rg::RenderPass::new_compute(
				rg.add_pass("combine gaussian"),
				"/shaders/copy_blit.hlsl")
				.read(post_processed)
				.write(accum_img)
				.constants(std::tuple{
					post_processed.desc.extent_inv_extent_2d(),
					std::array<float,4>{ (float)accum_img.desc.extent[0],
						  (float)accum_img.desc.extent[1],
						  1.0f / accum_img.desc.extent[0],
						  1.0f / accum_img.desc.extent[1]}
					})
				.dispatch({ accum_img.desc.extent[0], accum_img.desc.extent[1], 1 });
			gaussian->render(rg, accum_img, depth_img);
			point_pass->render(rg, accum_img, depth_img);
			debug_render_pass->render(rg, accum_img, depth_img);
			return post_processed;
		}
		return accum_img;
	}

	extern auto render_path_tracing(rg::TemporalGraph& rg,
		rg::Handle<rhi::GpuTexture>& output_image,
		rg::Handle<rhi::GpuTexture>& depth_image,
		rg::Handle<rhi::GpuTexture>& sky_cube,
		rhi::DescriptorSet* bindless_descriptor_set,
		const std::optional<rg::Handle<rhi::GpuRayTracingAcceleration>>& tlas) -> void;

	auto DeferedRenderer::prepare_render_graph_pt(
		rg::TemporalGraph& rg, 
		const FrameParamDesc& frame_desc,
		const EnvironmentFrameParams& environment,
		rg::Handle<rhi::GpuTexture>& accum_img,
		rg::Handle<rhi::GpuTexture>& depth_img)->rg::Handle<rhi::GpuTexture>
	{
		auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32B32A32_Float, frame_desc.render_extent).with_usage(
			rhi::TextureUsageFlags::SAMPLED |
			rhi::TextureUsageFlags::STORAGE |
			rhi::TextureUsageFlags::TRANSFER_DST);
		auto pt_img = rg.get_or_create_temporal("pt.accum", desc);
		if (environment.has_environment)
		{
			auto ibl_tex = ibl->render(rg, environment.hdr_img, environment.ibl_params);
			auto sky_cube = ibl_tex ? ibl_tex.value() : sky::render_sky_cube(rg, glm::vec4(environment.color, environment.mode));//ibl_tex.value_or(sky::render_sky_cube(rg));
			//auto convoled_sky_cube = sky::convolve_cube(rg, sky_cube);

			if (reset_pt)
			{
				reset_pt = false;
				rg::clear_color(rg, pt_img, { 0.0f,0.0f,0.0f,0.0f });
			}

			if (rg.device->gpu_limits.ray_tracing_enabled)
			{
				auto tlas = prepare_top_level_acceleration(rg);
				render_path_tracing(rg, pt_img,depth_img, sky_cube, bindless_descriptor_set.get(), tlas);
			}

			auto output = post->render(rg,
				pt_img,
				bindless_descriptor_set.get(),
				frame_render_settings);

			rg::RenderPass::new_compute(
				rg.add_pass("copy blit"),
				"/shaders/copy_blit.hlsl")
				.read(output)
				.write(accum_img)
				.constants(std::tuple{
					output.desc.extent_inv_extent_2d(),
					std::array<float,4>{ (float)accum_img.desc.extent[0],
						  (float)accum_img.desc.extent[1],
						  1.0f / accum_img.desc.extent[0],
						  1.0f / accum_img.desc.extent[1]}
					})
				.dispatch({ accum_img.desc.extent[0], accum_img.desc.extent[1], 1 });
			gaussian->render(rg,accum_img, depth_img);
			point_pass->render(rg, accum_img, depth_img);
			debug_render_pass->render(rg, accum_img, depth_img);
			return output;
		}
		return accum_img;
	}
	auto DeferedRenderer::retire_frame(const std::vector<MeshFrameState>& mesh_frame_states) -> void
	{
		frame_idx += 1;
		if(gpu_scene.previous_transforms.size() != mesh_frame_states.size())
		{
			gpu_scene.previous_transforms.clear();
			invalidate_pt_state();
		}
		for (auto& mesh_frame_state : mesh_frame_states)
		{
			gpu_scene.previous_transforms[mesh_frame_state.entity_id] = mesh_frame_state.world_transform;
		}
	}

	auto DeferedRenderer::prepare_frame_constants(rhi::DynamicConstants& dynamic_constants, const FrameParamDesc& frame_desc, const CameraFrameParams& camera_params, f32 delta_t) -> rg::FrameConstantsLayout
	{
		ViewConstants view_constants(frame_desc.camera_matrices, prev_camera_matrix.value_or(frame_desc.camera_matrices), frame_desc.render_extent);
		view_constants.set_pixel_offset(taa->current_supersample_offset, frame_desc.render_extent);
		prev_camera_matrix = frame_desc.camera_matrices;

		std::vector<LightShaderData> scene_lights;
		lighting_pass->gather_lights(scene_lights, triangle_lights);
		irache->update_eye_position(view_constants.eye_position());
		u32 num_triangle_lights = 0;
		u32 scene_lights_count = scene_lights.size();
		RenderOverride	render_overrides;
		f32 real_sun_angular_radius = glm::radians(0.53f) * 0.5f;
		
		auto global_offset = dynamic_constants.push(FrameConstants{
			view_constants.transpose(),
			glm::vec4(sun_direction,0),
            frame_idx,
            delta_t,
            std::cos(sun_size_multiplier * real_sun_angular_radius),
			num_triangle_lights,
            glm::vec4(sun_color,1.0),
			sky_ambient,
			post->expos_state.pre_mult,
			post->expos_state.pre_mult_prev,
			post->expos_state.pre_mult_delta,
			scene_lights_count,
			render_overrides,
			camera_params.fov_rad,
			camera_params.dof_enabled,
			camera_params.focus_distance,
			camera_params.aperture,
			camera_params.camera_type,
			{0.0f, 0.0f, 0.0f},
			glm::vec4(irache->get_grid_center(),1.0),
			irache->constants()
		});
		auto instance_dynamic_parameters_offset = dynamic_constants.push_from_vec(gpu_scene.instance_dynamic_constants);
		auto triangle_lights_offset = instance_dynamic_parameters_offset;//dynamic_constants.push_from_vec(triangle_lights);
		auto scene_lights_offset = dynamic_constants.push_from_vec(scene_lights);
		return rg::FrameConstantsLayout{
			global_offset,
			instance_dynamic_parameters_offset,
			triangle_lights_offset,
			scene_lights_offset
		};
	}

	auto DeferedRenderer::release()->void
	{
		if (released)
			return;
		threading::assert_render_thread();
		wait_for_render_idle();
		released = true;
		DebugRenderer::Release();
        rg_renderer.reset();
		debug_render_pass.reset();
		gaussian.reset();
		taa.reset();
		ibl.reset();
		restir_gi.reset();
		rtr.reset();
		ssgi.reset();
		irache.reset();
		shadow_denoise.reset();
		post.reset();
		rasterizer.reset();
		lighting_pass.reset();
	}

	auto DeferedRenderer::handle_resize(uint32_t width, uint32_t height)->void
	{		
		threading::assert_render_thread();
		auto old_main_render_tex = std::move(main_render_tex);
		auto old_depth_render_tex = std::move(depth_render_tex);
		defer_release([old_main_render_tex = std::move(old_main_render_tex),
					   old_depth_render_tex = std::move(old_depth_render_tex)]() mutable {
			old_main_render_tex.reset();
			old_depth_render_tex.reset();
		});
		auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, {width,height}).with_usage(
			rhi::TextureUsageFlags::SAMPLED |
			rhi::TextureUsageFlags::STORAGE |
			rhi::TextureUsageFlags::TRANSFER_DST |
			rhi::TextureUsageFlags::TRANSFER_SRC | 
			rhi::TextureUsageFlags::COLOR_ATTACHMENT);
		main_render_tex = rg_renderer->device->create_texture(desc, {}, "main_tex");

		depth_render_tex = rg_renderer->device->create_texture(
							rhi::GpuTextureDesc::new_2d(PixelFormat::D32_Float,{width,height})
							.with_usage(rhi::TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT |
							rhi::TextureUsageFlags::TRANSFER_DST |
							rhi::TextureUsageFlags::TRANSFER_SRC |
							rhi::TextureUsageFlags::SAMPLED), 
							{}, "depth_tex");
		temporal_upscale_extent = main_render_tex->desc.extent_2d();
		irache->reset();
		invalidate_pt_state();
		rg_renderer->clear_resources();
		rg_renderer->swap_chain->reset_frame_index();
	}

	auto DeferedRenderer::handle_window_resize(uint32_t width, uint32_t height)->void
	{
		threading::assert_render_thread();
		rg_renderer->swap_chain->resize(width,height);
		handle_resize(width, height);
	}

	auto DeferedRenderer::set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform) -> void
	{
		override_camera = camera;
		override_camera_transform = overrideCameraTransform;
		if (threading::is_render_thread() || !render_thread_running.load(std::memory_order_acquire))
		{
			if (grid_renderer)
				grid_renderer->set_override_camera(camera, overrideCameraTransform);
		}
		else
		{
			enqueue_render_command([this, camera, overrideCameraTransform]() {
				if (grid_renderer)
					grid_renderer->set_override_camera(camera, overrideCameraTransform);
			});
		}
	}

	auto DeferedRenderer::handle_new_scene(Scene* scene)->void
	{
		threading::assert_render_thread();
		DS_UNUSED(scene);
	}

	auto DeferedRenderer::upload_mesh_model(MeshModel* model) -> void
	{
        auto device = rg_renderer->device;
		gpu_scene.model_2_mesh_buf_id[model] = gpu_scene.mesh_buf_id;
		auto mesh_data = (GpuMesh*)mesh_buffer->map(device);
		for (auto mesh : model->get_meshes())
		{
			if (!mesh || !mesh->get_vertex_buffer() || !mesh->get_index_buffer())
				continue;
			device->write_descriptor_set(bindless_descriptor_set.get(), VERTEX_BUF_BINDING_ID, mesh->get_vertex_buffer().get(), gpu_scene.mesh_buf_id);
			device->write_descriptor_set(bindless_descriptor_set.get(), INDEX_BUF_BINDING_ID, mesh->get_index_buffer().get(), gpu_scene.mesh_buf_id);
			GpuMesh gpu_mesh = {
			   mesh->get_vertex_pos_nor_offset(),
			   mesh->get_vertex_uv_offset(),
			   mesh->get_vertex_tangent_offset(),
			   mesh->get_vertex_color_offset(),
			   gpu_scene.mesh_buf_id,
			   gpu_scene.mesh_buf_id,
			   gpu_scene.mat_2_mat_buf_id[mesh->get_material().get()]
			};
			gpu_scene.mesh_2_mesh_buf_id[mesh.get()] = gpu_scene.mesh_buf_id;
			gpu_scene.mesh_buf_id_2_mesh[gpu_scene.mesh_buf_id] = mesh.get();
			mesh_data[gpu_scene.mesh_buf_id++] = gpu_mesh;
		}
		mesh_buffer->unmap(device);
		if (device->gpu_limits.ray_tracing_enabled)
        {
			//build tlas
			build_ray_tracing_buttom_level_acceleration(model);
		}
		invalidate_pt_state();
	}

	auto DeferedRenderer::upload_material(const MaterialProperties* material) -> void
	{
		auto mat_iter = std::find(gpu_scene.material_datas.begin(), gpu_scene.material_datas.end(), material);
        auto device = rg_renderer->device;
		if(mat_iter != gpu_scene.material_datas.end())
		{ 
			auto material_data = (MaterialProperties*)material_buffer->map(device);
			auto mat_buf_id = mat_iter - gpu_scene.material_datas.begin();
			material_data[mat_buf_id] = *material;
			material_buffer->unmap(device);
			invalidate_pt_state();
		}
	}

	auto DeferedRenderer::upload_image(const std::shared_ptr<rhi::GpuTexture>& image) -> BindlessImageHandle
	{
		if(!image) return {};
		if(gpu_scene.bindless_image_ids.find(image.get()) != gpu_scene.bindless_image_ids.end())
			return BindlessImageHandle{ gpu_scene.bindless_image_ids[image.get()] };
		auto handle = BindlessImageHandle{ gpu_scene.next_bindless_image_id };
		gpu_scene.next_bindless_image_id += 1;
        auto device = rg_renderer->device;
		rhi::DescriptorImageInfo    imge_info = {};
		imge_info.image_layout = rhi::ImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imge_info.view = image->view(device, rhi::GpuTextureViewDesc()).get();
		device->write_descriptor_set(bindless_descriptor_set.get(), TEX_BUF_BINDING_ID, handle.handle, imge_info);

		auto img_size = image->desc.extent_inv_extent_2d();
		gpu_scene.bindless_image_ids[image.get()] = handle.handle;
		bindless_texture_sizes->copy_from(device, (u8*)img_size.data(), sizeof(float) * 4, handle.handle * sizeof(float) * 4);
		return handle;
	}

	auto DeferedRenderer::add_image_lut(const std::shared_ptr<ImageLut>& computer, u64 id)->void
    {
        image_luts.push_back(computer);

        auto handle = upload_image(image_luts.back()->get_image());

        assert(id == handle.handle);
    }

	auto DeferedRenderer::register_event_render_graph(rg::RenderGraph& rg) ->void
	{
		rg_renderer->register_render_graph(rg);
		rg.predefined_descriptor_set_layouts.insert({ 1, rg::PredefinedDescriptorSet{BINDLESS_DESCRIPTOR_SET_LAYOUT} });
	}

	auto DeferedRenderer::prepare_top_level_acceleration(rg::RenderGraph& rg) -> std::optional<rg::Handle<rhi::GpuRayTracingAcceleration>>
	{
		auto device = rg_renderer->device;
		if(!tlas || !device->gpu_limits.ray_tracing_enabled ) return {};
		auto top_level_as = rg.import_res<rhi::GpuRayTracingAcceleration>(tlas, rhi::AccessType::AnyShaderReadOther);
		std::vector<rhi::RayTracingInstanceDesc>    rt_instance_desc(gpu_scene.ent_2_model_id.size());

        //parallel_for<size_t>(0, gpu_scene.ent_2_model_id.size(), [&](size_t idx){
		for (auto idx = 0; idx < gpu_scene.ent_2_model_id.size(); idx++) {
            const auto& inst = gpu_scene.instance_transforms[idx];
            auto model_id = gpu_scene.ent_2_model_id[idx];
			u8 mask = idx < gpu_scene.rt_instance_masks.size() ? gpu_scene.rt_instance_masks[idx] : 0xff;
            rt_instance_desc[idx] =
                rhi::RayTracingInstanceDesc{
                    gpu_scene.mesh_blas[model_id],
                    inst.transform,
                    model_id,
                    mask
            };
        };
		auto pass = rg.add_pass("rebuild pass");
		auto tlas_ref = pass.write(top_level_as, rhi::AccessType::TransferWrite);

		pass.render([rt_instance_desc = std::move(rt_instance_desc), tlas_ref = std::move(tlas_ref), device](rg::RenderPassApi& api) {
			auto& resources = api.resources;

			auto instance_buffer_address = resources.execution_params.device->fill_ray_tracing_instance_buffer(resources.dynamic_constants, rt_instance_desc);
			auto tlas_ = api.resources.rt_acceleration(tlas_ref);

			api.device()->rebuild_ray_tracing_top_acceleration(
				api.cb,
				instance_buffer_address,
				rt_instance_desc.size(),
				tlas_,
				&device->rt_scatch_buffer
			);
		});
		pass.rg->record_pass(std::move(pass.pass));
		return top_level_as;
	}

	auto DeferedRenderer::build_ray_tracing_top_level_acceleration()->void
	{
		auto device = rg_renderer->device;
		if( !device->gpu_limits.ray_tracing_enabled ) return;
		std::vector<rhi::RayTracingInstanceDesc>    rt_instance_desc(gpu_scene.instance_transforms.size());
        parallel_for<size_t>(0, gpu_scene.instance_transforms.size(), [&](size_t idx) {
            const auto& inst = gpu_scene.instance_transforms[idx];
            auto model_id = gpu_scene.ent_2_model_id[idx];
			u8 mask = idx < gpu_scene.rt_instance_masks.size() ? gpu_scene.rt_instance_masks[idx] : 0xff;
            rt_instance_desc[idx] =
                rhi::RayTracingInstanceDesc{
                    gpu_scene.mesh_blas[model_id],
                    inst.transform,
                    model_id,
                    mask
            };
        });
		//if( !tlas && rt_instance_desc.size() > 0)
		{
			tlas = device->create_ray_tracing_top_acceleration(
				rhi::RayTracingTopAccelerationDesc{
					rt_instance_desc,
					TLAS_PREALLOCATE_BYTES
				},
				device->rt_scatch_buffer
			);
		}
	}

	auto DeferedRenderer::build_ray_tracing_buttom_level_acceleration(MeshModel* model)->void
	{
		std::vector<rhi::RayTracingGeometryPart> geom_parts;
        auto device = rg_renderer->device;
		for (auto mesh : model->get_meshes_ref())
		{
			auto vertex_buffer_address = mesh->get_vertex_buffer()->device_address(device);
			auto index_buffer_address = mesh->get_index_buffer()->device_address(device);
			geom_parts.push_back(rhi::RayTracingGeometryPart{ mesh->get_vertex_count(), vertex_buffer_address, mesh->get_index_count(), index_buffer_address, u32(mesh->get_vertex_count() - 1) });
		}
		
		auto blas = device->create_ray_tracing_bottom_acceleration(
			rhi::RayTracingBottomAccelerationDesc{
				std::vector<rhi::RayTracingGeometryDesc>{
					rhi::RayTracingGeometryDesc{
						rhi::RayTracingGeometryType::Triangle,
						PixelFormat::R32G32B32_Float,
						sizeof(PackedPosNormal),
						geom_parts
					}
				}
			}
		);
		gpu_scene.mesh_blas.push_back(blas);
	}

	u32 DeferedRenderer::get_buf_id(GaussianModel* model)
	{
		if(gpu_scene.model_2_gs_buf_id.find(model) != gpu_scene.model_2_gs_buf_id.end())
			return gpu_scene.model_2_gs_buf_id[model];
		return 0xffffffff;
	}

	u32 DeferedRenderer::get_buf_id(PointCloud* model)
	{
		if(gpu_scene.model_2_point_buf_id.find(model) != gpu_scene.model_2_point_buf_id.end())
			return gpu_scene.model_2_point_buf_id[model];
		return 0xffffffff;
	}
}
