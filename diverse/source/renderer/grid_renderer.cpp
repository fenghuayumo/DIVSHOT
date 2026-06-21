#include "grid_renderer.h"

#include "assets/mesh_factory.h"

#include "assets/asset_system.h"

#include "assets/asset_id.h"

#include "drs_rg/simple_pass.h"

namespace diverse

{



	GridRenderer::GridRenderer(rhi::GpuDevice* dev)

		: device(dev)

	{

		init();

		grid_res = 1.4f;

		grid_size = 500.0f;

	}

	GridRenderer::~GridRenderer() = default;



	auto GridRenderer::init() -> void

	{

		quad = create_quad();

		if (quad)

		{

			if (!quad->id.is_valid())

				quad->id = GenerateAssetId();

			auto& asset_sys = AssetSystem::get_instance();

			asset_sys.register_cpu_mesh(quad);

			quad_gpu = asset_sys.gpu_system().request_mesh(quad->id, UploadPriority::Critical);

		}

		rhi::RenderPassDesc desc = {

			{

				rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).discard_input(),

			},

			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).discard_input()

		};

		grid_render_pass = device->create_render_pass(desc);



	}

	auto GridRenderer::handle_resize(u32 width, u32 height) -> void

	{

	}

	auto GridRenderer::render(rg::RenderGraph& rg, rg::Handle<rhi::GpuTexture>& color_img, rg::Handle<rhi::GpuTexture>& depth_img) -> void

	{

		if( !frame_params.valid || !quad_gpu.is_valid())

			return;

		//current_buffer_id = 0;

		auto pass = rg.add_pass("raster grid line");

		auto pipeline_desc = rhi::RasterPipelineDesc()

			.with_render_pass(grid_render_pass)

			.with_cull_mode(rhi::CullMode::BACK)

			.with_blend_mode(rhi::BlendMode::SrcAlphaOneMinusSrcAlpha)

			.with_primitive_type(rhi::PrimitiveTopType::TriangleList);

		auto pipeline = pass.register_raster_pipeline(

			{

			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/debug_draw/grid_vs.hlsl"}),

			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/debug_draw/grid_ps.hlsl"})

			},

			std::move(pipeline_desc)

		);

		auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);

		auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);



		struct UBOFragConstant

		{

			glm::vec4 cameraPos;

			glm::vec4 cameraForward;

			float Near;

			float Far;

			float maxDistance;

			float p1;

		}frag_constant;

		frag_constant.Near = frame_params.near_plane;

		frag_constant.Far = frame_params.far_plane;

		frag_constant.cameraPos = glm::vec4(frame_params.camera_position, 1.0f);

		frag_constant.cameraForward = glm::vec4(frame_params.camera_forward, 1.0f);



		frag_constant.maxDistance = max_distance;

		pass.render([this, color_img = std::move(color_ref),

			depth = std::move(depth_ref),

			frag_constant,

			pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {

				auto [width, height, _] = color_img.desc.extent;



				auto frag_const_offset = api.dynamic_constants()

					->push(frag_constant);



				api.begin_render_pass(

					*grid_render_pass,

					{ width, height },

					{

						std::pair{color_img,rhi::GpuTextureViewDesc()},

					},

					std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }

				);



				api.set_default_view_and_scissor({ width,height });



				std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstants(frag_const_offset) };

				auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)

					.descriptor_set(0, &bindings);

				/*						.raw_descriptor_set(1, bindless_descriptor_set);*/

				auto bound_raster = api.bind_raster_pipeline(res);



				auto device = api.device();



				device->bind_index_buffer(api.cb, quad_gpu.index_buffer.get(), rhi::IndexBufferFormat::UINT32, 0);

				rhi::GpuBuffer* vertex_buffers[] = {

					quad_gpu.vertex_buffer.get()

				};

				u32 strides[] = {

					sizeof(Vertex)

				};

				u64 offsets[] = {

					0

				};

				device->bind_vertex_buffers(api.cb, vertex_buffers, 0, 1, strides, offsets);

				device->draw_indexed(api.cb, 6, 0, 0);



				api.end_render_pass();

			});



		pass.rg->record_pass(std::move(pass.pass));



		frame_params = {};

	}

	auto GridRenderer::set_frame_params(const GridFrameParams& params) -> void

	{

		frame_params = params;

	}

}

