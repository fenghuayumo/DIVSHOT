#pragma once
#include "drs_rg/simple_pass.h"
#include "drs_rg/temporal.h"
#include "utility/file_utils.h"
#include "assets/gaussian_model.h"
#include "maths/bounding_box.h"
#include "maths/bounding_sphere.h"
#include "maths/transform.h"
#include <glm/glm.hpp>
namespace diverse
{    
	struct GSplatRenderOutput 
	{
		rg::Handle<rhi::GpuTexture> color;
		rg::Handle<rhi::GpuTexture> depth;
		rg::Handle<rhi::GpuTexture> normal;
	};

	struct SplatRenderData
	{
		u32 buf_id;
		GaussianModel* model;
		const maths::Transform& transform;
		glm::vec4 	select_color;
		glm::vec4 	locked_color;
	};

	// Camera type for GUT rendering
	enum class GaussianCameraType : u32
	{
		Pinhole = 0,
		Fisheye = 1
	};


	struct GaussianRenderPass
	{
        GaussianRenderPass(class DeferedRenderer* renderer);
        GaussianRenderPass();

		auto render(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput;

		// New GUT mesh shader render pass for fisheye camera support
		auto render_gut_mesh_shader(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput;

	protected:

		auto render_points(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img,
			const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>;
		auto render_color_points(
			rg::RenderGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img,
			const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>;
		
		auto render_outline(
			rg::RenderGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img,
			rg::Handle<rhi::GpuTexture>& outline_tex,
			const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>;		
		
		auto render_splats(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput;

	protected:
        class DeferedRenderer* renderer;

		std::shared_ptr<rhi::RenderPass> gs_point_render_pass;
		std::shared_ptr<rhi::RenderPass> gsplat_render_pass;
		std::shared_ptr<rhi::RenderPass> gut_mesh_render_pass;
	};
}
