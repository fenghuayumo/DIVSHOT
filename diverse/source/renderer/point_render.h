#pragma once
#include "drs_rg/simple_pass.h"
#include "drs_rg/temporal.h"
#include "utility/file_utils.h"
#include "assets/point_cloud.h"
#include "maths/bounding_box.h"
#include "maths/bounding_sphere.h"
#include "maths/transform.h"
#include <glm/glm.hpp>

namespace diverse
{
    struct PointRenderPass
	{
        PointRenderPass(struct DeferedRenderer* renderer);
        PointRenderPass();

		auto render(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img) -> rg::Handle<rhi::GpuTexture>;
	protected:
        struct DeferedRenderer* renderer;

		std::shared_ptr<rhi::RenderPass> point_render_pass;
    };    
}