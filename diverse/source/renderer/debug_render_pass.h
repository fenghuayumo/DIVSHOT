#pragma once
#include "backend/drs_rhi/drs_rhi.h"
#include "backend/drs_rhi/gpu_device.h"
#include "renderable2d.h"
#include "drs_rg/renderer.h"

namespace diverse
{
	struct LineVertexData
	{
		glm::vec3 vertex;
		glm::vec4 colour;

		bool operator==(const LineVertexData& other) const
		{
			return vertex == other.vertex && colour == other.colour;
		}
	};

	struct PointVertexData
	{
		glm::vec3 vertex;
		glm::vec4 colour;
		glm::vec2 size;
		glm::vec2 uv;

		bool operator==(const PointVertexData& other) const
		{
			return vertex == other.vertex && colour == other.colour && size == other.size && uv == other.uv;
		}
	};

	struct Render2DLimits
	{
		u32 MaxQuads = 1000;
		u32 QuadsSize = RENDERER2D_VERTEX_SIZE * 4;
		u32 BufferSize = 1000 * RENDERER2D_VERTEX_SIZE * 4;
		u32 IndiciesSize = 1000 * 6;
		u32 MaxTextures = 16;
		u32 MaxBatchDrawCalls = 100;

		void SetMaxQuads(u32 quads)
		{
			MaxQuads = quads;
			BufferSize = MaxQuads * RENDERER2D_VERTEX_SIZE * 4;
			IndiciesSize = MaxQuads * 6;
		}
	};

    struct DebugRenderPass
    {
        DebugRenderPass(struct DeferedRenderer* render);
        ~DebugRenderPass();

        auto render(rg::TemporalGraph& rg,rg::Handle<rhi::GpuTexture>& color_img,rg::Handle<rhi::GpuTexture>& depth_img)->void;

    	public:
		struct DS_EXPORT RenderCommand2D
		{
			Renderable2D* renderable = nullptr;
			glm::mat4 transform;
		};

		typedef std::vector<RenderCommand2D> CommandQueue2D;
		struct Renderer2DData
		{
			CommandQueue2D command_queue2D;
			std::vector<std::vector<std::shared_ptr<rhi::GpuBuffer>>> vertex_buffers;

			u32 batch_drawcall_index = 0;
			u32 index_count = 0;

			std::shared_ptr<rhi::GpuBuffer> index_buffer = nullptr;
			VertexData* vertex_data = nullptr;

			Render2DLimits	limits;
		};
		struct DebugDrawData
		{
			std::vector<std::shared_ptr<rhi::GpuBuffer>> line_vertex_buffers;
			std::shared_ptr<rhi::GpuBuffer> line_index_buffer;

			std::shared_ptr<rhi::GpuBuffer> point_index_buffer = nullptr;
			std::vector<std::shared_ptr<rhi::GpuBuffer>> point_vertex_buffers;

			LineVertexData*  line_buffer = nullptr;
			PointVertexData* point_buffer = nullptr;

			u32 line_index_count = 0;
			u32 point_index_count = 0;
			u32 line_batch_drawcall_index = 0;
			u32 point_batch_drawcall_index = 0;

			Renderer2DData renderer2d_data;
		};
		DebugDrawData debug_draw_data;
		std::shared_ptr<rhi::RenderPass>	debug_line_pass;
		std::vector<LineVertexData*>		line_buffer;
		std::vector<PointVertexData*>		point_buffers;
		std::vector<VertexData*>		    quad_buffers;
		friend	struct DeferedRenderer;
    protected:
        struct DeferedRenderer* renderer;
    };
}