#include "core/string.h"
#include "defered_renderer.h"
#include "engine/application.h"
#include "drs_rg/simple_pass.h"
#include "backend/drs_rhi/buffer_builder.h"
#include "engine/window.h"
#include "engine/input.h"
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
	void set_gaussian_render_type(GaussianRenderType ty)
	{
		g_render_settings.gs_vis_type = (int)ty;
	}
	GaussianRenderType get_gaussian_render_type() 
	{
		return (GaussianRenderType)g_render_settings.gs_vis_type;
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
		release();
	}
	void DeferedRenderer::init()
	{
		auto swapchain_extent = Application::get().get_window_size();
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

		upload_image(ResourceManager<asset::Texture>::get().get_default_resource("white")->gpu_texture);
		upload_image(ResourceManager<asset::Texture>::get().get_default_resource("normal")->gpu_texture);
		build_ray_tracing_top_level_acceleration();
	}

	void DeferedRenderer::render()
	{
		if(!current_scene) return;
		upload_gpu_buffers();

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
			return;

		FrameParamDesc frame_desc;

		auto rg = rg_renderer->temporal_graph();
		float delta_dt = 1.0f / 60.0f; //TODO
		auto swapchain_extent = Application::get().get_window_size();
		frame_desc.render_extent = main_render_tex->desc.extent_2d();//swapchain_extent;
		auto projection = camera->get_projection_matrix();
		auto invProj = glm::inverse(projection);
		auto invView = camera_transform->get_world_matrix();
		auto view = glm::inverse(invView);

		frame_desc.camera_matrices = CameraMatrices{
			projection,
			invProj,
			view,
			invView
		};
		gs_command_queue.clear();
		mesh_command_queue.clear();
		point_command_queue.clear();
		triangle_lights.clear();

		if( prev_camera_matrix && prev_camera_matrix->world_to_view != frame_desc.camera_matrices.world_to_view )
			reset_pt = true;

		auto group = registry.group<GaussianComponent>(entt::get<maths::Transform>);
		for (auto gs_ent : group)
		{
			if (!Entity(gs_ent, current_scene).active())
				continue;

			const auto& [gs_com, trans] = group.get<GaussianComponent, maths::Transform>(gs_ent);
			if(!gs_com.ModelRef->is_flag_set(AssetFlag::UploadedGpu) || !gs_com.participate_render) continue;
			if (gs_com.ModelRef->gaussians_buf && model_2_gs_buf_id.find(gs_com.ModelRef.get()) != model_2_gs_buf_id.end())
			{ 
				auto offset = -gs_com.black_point + gs_com.brightness;
				auto scale = 1.0f / (gs_com.white_point - gs_com.black_point);
				gs_command_queue.push_back(RenderGSCommand{ trans, 
											gs_com.ModelRef, 
											(u32)gs_com.sh_degree,
											g_render_settings.select_color,
											g_render_settings.locked_color,
											glm::vec4(gs_com.albedo_color.xyz * scale, gs_com.transparency),
											glm::vec3(offset,offset,offset)});
				
				const auto crop = Entity(gs_ent, current_scene).try_get_component<GaussianCrop>();
				if (crop)
				{
					for (auto& crop_data : crop->get_crop_data())
					{
						gs_command_queue.back().crop_data.push_back(&crop_data);
					}
				}
			}
		}
		//pointcloud
		auto pointcloud_group = registry.group<PointCloudComponent>(entt::get<maths::Transform>);
		for(auto pcd : pointcloud_group)
		{
			if (!Entity(pcd, current_scene).active())
				continue;

			const auto& [pcd_com, trans] = pointcloud_group.get<PointCloudComponent, maths::Transform>(pcd);
			if(!pcd_com.ModelRef->is_flag_set(AssetFlag::UploadedGpu) ) continue;
			if (pcd_com.ModelRef->vertex_buffer && model_2_point_buf_id.find(pcd_com.ModelRef.get()) != model_2_point_buf_id.end())
			{ 
				point_command_queue.push_back(RenderPointCommand{ trans, 
											pcd_com.ModelRef});
			}
		}
		auto meshgroup = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
		// if (!tlas && instantce_transforms.size() > 0 && ent_2_model_id.size() == meshgroup.size())
		// 	build_ray_tracing_top_level_acceleration();
		for (auto mesh_ent : meshgroup)
		{
			if (!Entity(mesh_ent, current_scene).active())
				continue;

			const auto& [model, trans] = meshgroup.get<MeshModelComponent, maths::Transform>(mesh_ent);
			if (!model.ModelRef) continue;
			if(!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu)) continue;
			const auto& meshes = model.ModelRef->get_meshes();
			auto idx = meshgroup.find(mesh_ent) - meshgroup.begin();
			for (auto mesh : meshes)
			{
				auto& worldTransform = trans.get_world_matrix();
				RenderMeshCommand command;
				command.material_id = mat_2_mat_buf_id[mesh->get_material().get()];
				command.mesh_instance_id = idx;
				command.mesh_id = mesh_2_mesh_buf_id[mesh.get()];
				mesh_command_queue.push_back(command);
				//mesh light
				auto material = mesh->get_material().get();
				if (material)
				{
					auto emissive = material->get_properties().emissive;
					if(emissive.x > 0 || emissive.y > 0 || emissive.z > 0)
					{
						TriangleLight triangle_light = {};
						triangle_light.instance_id = idx;
						triangle_light.mesh_id = mesh_2_mesh_buf_id[mesh.get()];
						triangle_light.triangle_count = mesh->get_index_count() / 3;
						triangle_lights.push_back(triangle_light);
					}
				}
			}
		}
		rg_renderer->prepare_frame_constants(rg,
			[&](rhi::DynamicConstants& dynamic_constants)->rg::FrameConstantsLayout {
				return prepare_frame_constants(dynamic_constants, frame_desc, delta_dt);
			}
		);
		rg_renderer->prepare_frame(rg,
			[&](rg::TemporalGraph& rg) {
				auto main_img = prepare_render_graph(rg, frame_desc);
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
		retire_frame();
	}

	auto DeferedRenderer::upload_gpu_buffers()->void
	{
		auto& registry = current_scene->get_registry();
		auto group = registry.group<GaussianComponent>(entt::get<maths::Transform>);
		for (auto gs_ent : group)
		{
			const auto& [model, trans] = group.get<GaussianComponent, maths::Transform>(gs_ent);
			if (!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu)) continue;
			u32 v_buf_id = model_2_gs_buf_id.size();
			if (model.ModelRef->gaussians_buf && model_2_gs_buf_id.find(model.ModelRef.get()) == model_2_gs_buf_id.end())
			{
				g_device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model.ModelRef->gaussians_buf.get(), v_buf_id * 4 + 0);
				g_device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model.ModelRef->gaussians_color_buf.get(), v_buf_id * 4 + 1);
				g_device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model.ModelRef->gaussians_sh_buf.get(), v_buf_id * 4 + 2);
				g_device->write_descriptor_set(bindless_descriptor_set.get(), GS_BINDING_ID, model.ModelRef->splat_transforms.splat_transform_buffer.get(), v_buf_id * 4 + 3);

				g_device->write_descriptor_set(bindless_descriptor_set.get(), SPLAT_STATE_BINDING_ID, model.ModelRef->gaussian_state_buf.get(), v_buf_id);
				model_2_gs_buf_id[model.ModelRef] = v_buf_id;
			}
		}
		//
		auto pointcloud_group = registry.group<PointCloudComponent>(entt::get<maths::Transform>);
		for (auto pcd_ent : pointcloud_group)
		{
			const auto& [model, trans] = pointcloud_group.get<PointCloudComponent, maths::Transform>(pcd_ent);
			if (!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu)) continue;
			u32 v_buf_id = model_2_point_buf_id.size();
			if (model.ModelRef->vertex_buffer && model_2_point_buf_id.find(model.ModelRef.get()) == model_2_point_buf_id.end())
			{
				g_device->write_descriptor_set(bindless_descriptor_set.get(), POINT_BUF_BINDING_ID, model.ModelRef->vertex_buffer.get(), v_buf_id);
				model_2_point_buf_id[model.ModelRef] = v_buf_id;
			}
		}
		auto mmesh_group = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
		ent_2_model_id.clear(); instantce_transforms.clear();instance_dynamic_constants.clear();
		for (auto ent : mmesh_group)
		{
			const auto& [model, t] = mmesh_group.get<MeshModelComponent, maths::Transform>(ent);
			if (!model.ModelRef) continue;
			if (!model.ModelRef->is_flag_set(AssetFlag::Loaded)) continue;
			int upload_material_num = 0;
			for (auto& mesh : model.ModelRef->get_meshes())
			{
				auto material = mesh->get_material().get();
				if(!material) continue;
				auto& matprop = material->get_properties();
				auto& pbr_tex = material->get_textures();
				if(!pbr_tex.is_upload_2_gpu()) continue;
				//if(material.is_flag_set(AssetFlag::UploadedGpu) && !material.dirty_flag()) continue;
				auto has_binded = [&](const SharedPtr<asset::Texture>& texture)->bool{
					if(!texture) return true; //no texture, use default
					if(texture->is_flag_set(AssetFlag::UploadedGpu) && 
						bindless_image_ids.find(texture->gpu_texture.get()) != bindless_image_ids.end())
						return true;
					return false;
				};
				auto has_binded_pbr_tex = [&](PBRMataterialTextures& texures) {
					return has_binded(texures.albedo) &&
							has_binded(texures.emissive) &&
							has_binded(texures.normal) &&
							has_binded(texures.metallic) &&
							has_binded(texures.roughness) &&
							has_binded(texures.ao);
				};
				if (!material->is_flag_set(AssetFlag::UploadedGpu) || !has_binded_pbr_tex(pbr_tex))
				{
					mat_2_mat_buf_id[material] = material_datas.size();
					material_datas.push_back(&matprop);
					matprop.albedo_map = pbr_tex.albedo ? (pbr_tex.albedo->gpu_texture ? upload_image(pbr_tex.albedo->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.emissive_map = pbr_tex.emissive ? (pbr_tex.emissive->gpu_texture ? upload_image(pbr_tex.emissive->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.normal_map = pbr_tex.normal ? (pbr_tex.normal->gpu_texture ? upload_image(pbr_tex.normal->gpu_texture).handle : NORMAL_TEX_ID) : NORMAL_TEX_ID;
					matprop.metallic_map = pbr_tex.metallic ? (pbr_tex.metallic->gpu_texture ? upload_image(pbr_tex.metallic->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.roughness_map = pbr_tex.roughness ? (pbr_tex.roughness->gpu_texture ? upload_image(pbr_tex.roughness->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.ao_map = pbr_tex.ao ? (pbr_tex.ao->gpu_texture ? upload_image(pbr_tex.ao->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					upload_material(&matprop);
					material->set_flag(AssetFlag::UploadedGpu);
				}
				else if (material->dirty_flag())
				{
					matprop.albedo_map = pbr_tex.albedo ? (pbr_tex.albedo->gpu_texture ? upload_image(pbr_tex.albedo->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.emissive_map = pbr_tex.emissive ? (pbr_tex.emissive->gpu_texture ? upload_image(pbr_tex.emissive->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.normal_map = pbr_tex.normal ? (pbr_tex.normal->gpu_texture ? upload_image(pbr_tex.normal->gpu_texture).handle : NORMAL_TEX_ID) : NORMAL_TEX_ID;
					matprop.metallic_map = pbr_tex.metallic ? (pbr_tex.metallic->gpu_texture ? upload_image(pbr_tex.metallic->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.roughness_map = pbr_tex.roughness ? (pbr_tex.roughness->gpu_texture ? upload_image(pbr_tex.roughness->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					matprop.ao_map = pbr_tex.ao ? (pbr_tex.ao->gpu_texture ? upload_image(pbr_tex.ao->gpu_texture).handle : WHITE_TEX_ID) : WHITE_TEX_ID;
					upload_material(&matprop);
				}
				upload_material_num++;
			}
			if (model.ModelRef->is_flag_set(AssetFlag::Loaded) && 
				!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu)
				&& upload_material_num == model.ModelRef->get_meshes().size())
			{
				upload_mesh_model(model.ModelRef);
				model.ModelRef->set_flag(AssetFlag::UploadedGpu);
				model_2_blas_id[model.ModelRef.get()] = mesh_blas.size() - 1;
			}
			if (model.ModelRef->is_flag_set(AssetFlag::UploadedGpu))
			{
				u32 model_id = model_2_blas_id[model.ModelRef.get()];
				ent_2_model_id.push_back(model_id);
				instantce_transforms.push_back({});
				auto world_transform = glm::transpose(t.get_world_matrix());
				instantce_transforms.back().transform = world_transform;
				if (previous_transforms.find((u32)ent) == previous_transforms.end())
					instantce_transforms.back().previous_transform = world_transform;
				else
					instantce_transforms.back().previous_transform = glm::transpose(previous_transforms[(u32)ent]);
				
				instance_dynamic_constants.push_back(InstanceDynamicConstants{});
				auto& instance = instance_dynamic_constants.back();
				instance.gemoetry_offset = model_2_mesh_buf_id[model.ModelRef.get()];
				instance.emissive_multiplier = 1.0f;
			}
		}
	}

	auto DeferedRenderer::prepare_render_graph(rg::TemporalGraph& rg, const FrameParamDesc& frame_desc) -> rg::Handle<rhi::GpuTexture>
	{	
		rg.predefined_descriptor_set_layouts.insert({ 1, rg::PredefinedDescriptorSet{BINDLESS_DESCRIPTOR_SET_LAYOUT} });
     	
		for(auto& img_lut : image_luts)
        {
            img_lut->compute_if_needed(rg);
        }
		post->update_pre_exposure();

		rg::Handle<rhi::GpuTexture> output;
		auto accum_img = rg.import_res<rhi::GpuTexture>(main_render_tex, rhi::AccessType::Nothing);
		auto depth_img = rg.import_res(depth_render_tex, rhi::AccessType::Nothing);
		rg::clear_color(rg, accum_img, { 0.0f,0.0f,0.0f,1.0f });
		rg::clear_depth(rg, depth_img, 0.0f);
#ifdef DS_PLATFORM_WINDOWS
		switch (g_render_settings.render_mode)
		{
		case RenderMode::Hybrid:
		{
			if(!take_photon.get_value<bool>())
				taa->current_supersample_offset = supersample_offsets[frame_idx % supersample_offsets.size()];

			output =  prepare_render_graph_hybrid(rg, frame_desc,accum_img,depth_img);
		}break;
		case RenderMode::PT:
		{
			taa->current_supersample_offset = glm::vec2(0);
			output = prepare_render_graph_pt(rg, frame_desc,accum_img,depth_img);
		}break;
		default:
		{
			taa->current_supersample_offset = glm::vec2(0);
			output = prepare_render_graph_pt(rg, frame_desc,accum_img,depth_img);
		}break;
		}
#elif DS_PLATFORM_MACOS
		taa->current_supersample_offset = glm::vec2(0);
		output = prepare_render_graph_pt(rg, frame_desc,accum_img,depth_img);
#endif
		gpu_events.clear();
		return output;
	}
	auto DeferedRenderer::prepare_render_graph_hybrid(
			rg::TemporalGraph& rg, 
			const FrameParamDesc& frame_desc,
			rg::Handle<rhi::GpuTexture>& accum_img,
			rg::Handle<rhi::GpuTexture>& depth_img)->rg::Handle<rhi::GpuTexture>
	{
		auto enviroment_view = current_scene->get_entity_manager()->get_entities_with_type<diverse::Environment>();
		if (!enviroment_view.empty())
		{
			auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, frame_desc.render_extent).with_usage(
				rhi::TextureUsageFlags::SAMPLED |
				rhi::TextureUsageFlags::STORAGE |
				rhi::TextureUsageFlags::TRANSFER_DST);
			auto hybrid_accum_img = rg.get_or_create_temporal("root.accum", desc);

			auto ent = enviroment_view.front().get_component<Environment>();
			if (ent.dirty_flag)
				invalidate_pt_state();
			const auto color = ent.get_color();
			sun_color = ent.mode == Environment::Mode::Pure ? glm::vec3(0.0f) : ent.sun_color;
			sun_size_multiplier = ent.sun_size_multiplier;
			sky_ambient = glm::vec4(ent.sky_ambient, ent.intensity);
			std::shared_ptr<rhi::GpuTexture>	hdr_img = ent.mode == Environment::Mode::HDR ? ent.get_enviroment_map() : nullptr;
			if (ent.mode == Environment::Mode::SunSky)
			{
				auto* sun_controller = enviroment_view.front().try_get_component<SunController>();
				if (sun_controller)
				{
					sun_direction = sun_controller->towards_sun();
				}
			}

			auto ibl_tex = ibl->render(rg, hdr_img, { ent.theta, ent.phi });
			auto sky_cube = ibl_tex ? ibl_tex.value() : sky::render_sky_cube(rg, glm::vec4(color, (f32)ent.mode), ent.cubeResolution);
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
				*tlas,
				bindless_descriptor_set.get(),
				(int)g_render_settings.shade_mode
			);
			if (g_render_settings.show_wireframe)
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
				bindless_descriptor_set.get()
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
		rg::Handle<rhi::GpuTexture>& accum_img,
		rg::Handle<rhi::GpuTexture>& depth_img)->rg::Handle<rhi::GpuTexture>
	{
		auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32B32A32_Float, frame_desc.render_extent).with_usage(
			rhi::TextureUsageFlags::SAMPLED |
			rhi::TextureUsageFlags::STORAGE |
			rhi::TextureUsageFlags::TRANSFER_DST);
		auto pt_img = rg.get_or_create_temporal("pt.accum", desc);
		auto enviroment_view = current_scene->get_entity_manager()->get_entities_with_type<diverse::Environment>();
		if (!enviroment_view.empty())
		{
			auto ent = enviroment_view.front().get_component<Environment>();
			if (ent.dirty_flag)
				invalidate_pt_state();
			const auto color = ent.get_color();
			sun_color = ent.mode == Environment::Mode::Pure ? glm::vec3(0.0f) : ent.sun_color;
			sun_size_multiplier = ent.sun_size_multiplier;
			sky_ambient = glm::vec4(ent.sky_ambient, ent.intensity);
			std::shared_ptr<rhi::GpuTexture>	hdr_img = ent.mode == Environment::Mode::HDR ? ent.get_enviroment_map() : nullptr;
			if (ent.mode == Environment::Mode::SunSky)
			{
				auto* sun_controller = enviroment_view.front().try_get_component<SunController>();
				if (sun_controller)
				{
					sun_direction = sun_controller->towards_sun();
				}
			}

			auto ibl_tex = ibl->render(rg, hdr_img, { ent.theta, ent.phi });
			auto sky_cube = ibl_tex ? ibl_tex.value() : sky::render_sky_cube(rg,glm::vec4(color, (f32)ent.mode));//ibl_tex.value_or(sky::render_sky_cube(rg));
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
				bindless_descriptor_set.get());

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
	auto DeferedRenderer::retire_frame() -> void
	{
		frame_idx += 1;
		auto& registry = current_scene->get_registry();
		auto mmesh_group = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
		if(previous_transforms.size() != mmesh_group.size())
		{
			previous_transforms.clear();
			invalidate_pt_state();
		}
		for (auto ent : mmesh_group)
		{
			const auto& [model, t] = mmesh_group.get<MeshModelComponent, maths::Transform>(ent);
			previous_transforms[(u32)ent] = t.get_world_matrix();
		}
	}

	auto DeferedRenderer::prepare_frame_constants(rhi::DynamicConstants& dynamic_constants, const FrameParamDesc& frame_desc, f32 delta_t) -> rg::FrameConstantsLayout
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
			glm::vec4(irache->get_grid_center(),1.0),
			irache->constants()
		});
		auto instance_dynamic_parameters_offset = dynamic_constants.push_from_vec(instance_dynamic_constants);
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
		auto main_tex = main_render_tex.get();
		auto depth_tex = depth_render_tex.get();
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
		rg_renderer->swap_chain->resize(width,height);
		handle_resize(width, height);
	}

	auto DeferedRenderer::set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform) -> void
	{
		override_camera = camera;
		override_camera_transform = overrideCameraTransform;
		grid_renderer->set_override_camera(camera, camera_transform);
	}

	auto DeferedRenderer::handle_new_scene(Scene* scene)->void
	{
		set_current_scene(scene);
	}

	auto DeferedRenderer::upload_mesh_model(MeshModel* model) -> void
	{
        auto device = get_global_device();
		model_2_mesh_buf_id[model] = mesh_buf_id;
		auto mesh_data = (GpuMesh*)mesh_buffer->map(device);
		for (auto mesh : model->get_meshes())
		{
			g_device->write_descriptor_set(bindless_descriptor_set.get(), VERTEX_BUF_BINDING_ID, mesh->get_vertex_buffer().get(), mesh_buf_id);
			g_device->write_descriptor_set(bindless_descriptor_set.get(), INDEX_BUF_BINDING_ID, mesh->get_index_buffer().get(), mesh_buf_id);
			GpuMesh gpu_mesh = {
			   mesh->get_vertex_pos_nor_offset(),
			   mesh->get_vertex_uv_offset(),
			   mesh->get_vertex_tangent_offset(),
			   mesh->get_vertex_color_offset(),
			   mesh_buf_id,
			   mesh_buf_id,
			   mat_2_mat_buf_id[mesh->get_material().get()]
			};
			mesh_2_mesh_buf_id[mesh.get()] = mesh_buf_id;
			mesh_buf_id_2_mesh[mesh_buf_id] = mesh.get();
			mesh_data[mesh_buf_id++] = gpu_mesh;
		}
		mesh_buffer->unmap(device);
		if (g_device->gpu_limits.ray_tracing_enabled)
        {
			//build tlas
			build_ray_tracing_buttom_level_acceleration(model);
		}
		invalidate_pt_state();
	}

	auto DeferedRenderer::upload_material(const MaterialProperties* material) -> void
	{
		auto mat_iter = std::find(material_datas.begin(), material_datas.end(), material);
        auto device = get_global_device();
		if(mat_iter != material_datas.end())
		{ 
			auto material_data = (MaterialProperties*)material_buffer->map(device);
			auto mat_buf_id = mat_iter - material_datas.begin();
			material_data[mat_buf_id] = *material;
			material_buffer->unmap(device);
			invalidate_pt_state();
		}
	}

	auto DeferedRenderer::upload_image(const std::shared_ptr<rhi::GpuTexture>& image) -> BindlessImageHandle
	{
		if(!image) return {};
		if(bindless_image_ids.find(image.get()) != bindless_image_ids.end())
			return BindlessImageHandle{ bindless_image_ids[image.get()] };
		auto handle = BindlessImageHandle{ next_bindless_image_id };
		next_bindless_image_id += 1;
        auto device = get_global_device();
		rhi::DescriptorImageInfo    imge_info = {};
		imge_info.image_layout = rhi::ImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imge_info.view = image->view(device, rhi::GpuTextureViewDesc()).get();
		g_device->write_descriptor_set(bindless_descriptor_set.get(), TEX_BUF_BINDING_ID, handle.handle, imge_info);

		auto img_size = image->desc.extent_inv_extent_2d();
		bindless_image_ids[image.get()] = handle.handle;
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
		if(!tlas || !g_device->gpu_limits.ray_tracing_enabled ) return {};
		auto top_level_as = rg.import_res<rhi::GpuRayTracingAcceleration>(tlas, rhi::AccessType::AnyShaderReadOther);
		std::vector<rhi::RayTracingInstanceDesc>    rt_instance_desc(ent_2_model_id.size());

		auto& registry = current_scene->get_registry();
		auto meshgroup = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
        //parallel_for<size_t>(0, ent_2_model_id.size(), [&](size_t idx){
		for (auto idx = 0; idx < ent_2_model_id.size(); idx++) {
            const auto& inst = instantce_transforms[idx];
            auto model_id = ent_2_model_id[idx];
            auto entity = *(meshgroup.begin() + idx);
            u8 mask = Entity(entity,current_scene).active() ? 0xff : 0;
            rt_instance_desc[idx] =
                rhi::RayTracingInstanceDesc{
                    mesh_blas[model_id],
                    inst.transform,
                    model_id,
                    mask
            };
        };
		auto pass = rg.add_pass("rebuild pass");
		auto tlas_ref = pass.write(top_level_as, rhi::AccessType::TransferWrite);

		pass.render([rt_instance_desc = std::move(rt_instance_desc), tlas_ref = std::move(tlas_ref), this](rg::RenderPassApi& api) {
			auto& resources = api.resources;

			auto instance_buffer_address = resources.execution_params.device->fill_ray_tracing_instance_buffer(resources.dynamic_constants, rt_instance_desc);
			auto tlas_ = api.resources.rt_acceleration(tlas_ref);

			api.device()->rebuild_ray_tracing_top_acceleration(
				api.cb,
				instance_buffer_address,
				rt_instance_desc.size(),
				tlas_,
				&g_device->rt_scatch_buffer
			);
		});
		pass.rg->record_pass(std::move(pass.pass));
		return top_level_as;
	}

	auto DeferedRenderer::build_ray_tracing_top_level_acceleration()->void
	{
		if( !g_device->gpu_limits.ray_tracing_enabled ) return;
		std::vector<rhi::RayTracingInstanceDesc>    rt_instance_desc(instantce_transforms.size());
		if(current_scene )
		{ 
			auto& registry = current_scene->get_registry();
			auto meshgroup = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
            parallel_for<size_t>(0,meshgroup.size(),[&](size_t idx){
                const auto& inst = instantce_transforms[idx];
                auto model_id = ent_2_model_id[idx];
                auto entity = *(meshgroup.begin() + idx);
                u8 mask = Entity(entity, current_scene).active() ? 0xff : 0;
                rt_instance_desc[idx] =
                    rhi::RayTracingInstanceDesc{
                        mesh_blas[model_id],
                        inst.transform,
                        model_id,
                        mask
                };
            });
		}
		//if( !tlas && rt_instance_desc.size() > 0)
		{
			tlas = g_device->create_ray_tracing_top_acceleration(
				rhi::RayTracingTopAccelerationDesc{
					rt_instance_desc,
					TLAS_PREALLOCATE_BYTES
				},
				g_device->rt_scatch_buffer
			);
		}
	}

	auto DeferedRenderer::build_ray_tracing_buttom_level_acceleration(MeshModel* model)->void
	{
		std::vector<rhi::RayTracingGeometryPart> geom_parts;
        auto device = get_global_device();
		for (auto mesh : model->get_meshes_ref())
		{
			auto vertex_buffer_address = mesh->get_vertex_buffer()->device_address(device);
			auto index_buffer_address = mesh->get_index_buffer()->device_address(device);
			geom_parts.push_back(rhi::RayTracingGeometryPart{ mesh->get_vertex_count(), vertex_buffer_address, mesh->get_index_count(), index_buffer_address, u32(mesh->get_vertex_count() - 1) });
		}
		
		auto blas = g_device->create_ray_tracing_bottom_acceleration(
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
		mesh_blas.push_back(blas);
	}

	u32 DeferedRenderer::get_buf_id(GaussianModel* model)
	{
		if(model_2_gs_buf_id.find(model) != model_2_gs_buf_id.end())
			return model_2_gs_buf_id[model];
		return 0xffffffff;
	}

	u32 DeferedRenderer::get_buf_id(PointCloud* model)
	{
		if(model_2_point_buf_id.find(model) != model_2_point_buf_id.end())
			return model_2_point_buf_id[model];
		return 0xffffffff;
	}
}
