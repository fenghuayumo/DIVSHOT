#include "drs_rg/temporal.h"
#include "drs_rg/simple_pass.h"
#include "defered_renderer.h"
#include <optional>
#include "assets/texture.h"
#include "drs_rg/image_op.h"

namespace diverse
{
    auto mesh_pick(DeferedRenderer* renderer,
		std::shared_ptr<rhi::GpuBuffer>& image_mask_buffer,
		std::shared_ptr<rhi::GpuBuffer>& output_pick_buffer,
		const glm::uvec4& surface_dim) -> void
	{		
		rg::RenderGraph rg;
        renderer->register_event_render_graph(rg);
        auto bindless_descriptor_set = renderer->binldess_descriptorset();
        auto tlas = renderer->prepare_top_level_acceleration(rg);
        if( !tlas ) return;
		auto image_mask_buf = rg.import_res(image_mask_buffer, rhi::AccessType::Nothing);
		auto pick_buffer = rg.import_res(output_pick_buffer, rhi::AccessType::Nothing);
		rg::RenderPass::new_rt(
			rg.add_pass("mesh_pick"),
			rhi::ShaderSource{ "/shaders/rt/mesh_pick_point.rgen.hlsl" },
			{
				rhi::ShaderSource{ "/shaders/rt/gbuffer.rmiss.hlsl" },
				rhi::ShaderSource{ "/shaders/rt/shadow.rmiss.hlsl" }
			},
			{
				rhi::ShaderSource{ "/shaders/rt/custom.rchit.hlsl" }
			}
		)
		.read(image_mask_buf)
		.write(pick_buffer)
        .constants(glm::uvec4(surface_dim.x, surface_dim.y, surface_dim.z, surface_dim.w))
		.raw_descriptor_set(1, bindless_descriptor_set)
		.trace_rays(tlas.value(), {surface_dim.x,surface_dim .y,1});

        rg.execute();
	}

	auto uv_map_intersect(DeferedRenderer* renderer,
		std::shared_ptr<rhi::GpuBuffer>& image_mask_buffer,
		std::shared_ptr<rhi::GpuTexture>& uv2pos_texture,
		std::shared_ptr<rhi::GpuBuffer>& output_pick_buffer,
		const glm::mat4& transform,
		const glm::uvec4& surface_dim) -> void
	{
		rg::RenderGraph rg;
        renderer->register_event_render_graph(rg);
		auto bindless_descriptor_set = renderer->binldess_descriptorset();
		auto image_mask_buf = rg.import_res(image_mask_buffer, rhi::AccessType::Nothing);
		auto uv2pos_texture_rg = rg.import_res(uv2pos_texture, rhi::AccessType::Nothing);
		auto converage_texture_rg = rg.import_res(output_pick_buffer, rhi::AccessType::Nothing);

		struct Constants{
			glm::uvec4 surface_dim;
			glm::mat4 transform;
		};
		Constants constants;
		constants.surface_dim = surface_dim;
		constants.transform = transform;
		rg::RenderPass::new_compute(
			rg.add_pass("coverage setup"),
			"/shaders/converage_uvmap.hlsl")
			.read(image_mask_buf)
			.read(uv2pos_texture_rg)
			.write(converage_texture_rg)
			.constants(constants)
			.raw_descriptor_set(1, bindless_descriptor_set)
			.dispatch({surface_dim.z,surface_dim .w,1});
		
		rg.execute();
	}
}