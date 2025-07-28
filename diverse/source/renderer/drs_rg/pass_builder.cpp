#include "pass_builder.h"
#include "backend/drs_rhi/gpu_pipeline.h"
#include "graph.h"
namespace diverse 
{

	namespace rg
	{
		PassBuilder::PassBuilder(RenderGraph* graph, uint32 pass_id, RecordedPass&& pss)
		:rg(graph), pass_idx(pass_id), pass(std::move(pss))
		{
		}

		//PassBuilder::~PassBuilder()
		//{
		//	rg.record_pass(std::move(*pass));
		//}

		auto PassBuilder::register_compute_pipeline(const std::string& path, const std::vector<std::pair<std::string, std::string>>& defines) -> RgComputePipelineHandle
		{
			rhi::ComputePipelineDesc desc;
			desc.source = { path,"main", defines};
			desc.name = pass.name;
			//desc.source
			//TODO:
			return register_compute_pipeline_with_desc(std::move(desc));
		}
		auto PassBuilder::register_compute_pipeline_with_desc(rhi::ComputePipelineDesc&& desc) -> RgComputePipelineHandle
		{
			auto id = static_cast<uint32>(rg->compute_pipelines.size());
			for (auto [set_idx, layout] : rg->predefined_descriptor_set_layouts)
			{
				desc.descriptor_set_opts[set_idx] = std::pair{ set_idx, rhi::DescriptorSetLayoutOpts{rhi::DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL, layout.bindings} };
			}

			rg->compute_pipelines.push_back(std::move(RgComputePipeline{desc}));
			return RgComputePipelineHandle{id};
		}

		auto PassBuilder::register_raster_pipeline(const std::vector<rhi::PipelineShaderDesc>& shaders, rhi::RasterPipelineDesc&& desc) -> RgRasterPipelineHandle
		{
			auto id = static_cast<uint32>(rg->raster_pipelines.size());
			for (auto [set_idx, layout] : rg->predefined_descriptor_set_layouts)
			{
				desc.descriptor_set_opts[set_idx] = std::pair{set_idx, rhi::DescriptorSetLayoutOpts{rhi::DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL, layout.bindings}};
			}

			rg->raster_pipelines.push_back(std::move(RgRasterPipeline{ desc, shaders }));
			return RgRasterPipelineHandle{ id };
		}
		auto PassBuilder::register_ray_tracing_pipeline(const std::vector<rhi::PipelineShaderDesc>& shaders, rhi::RayTracingPipelineDesc&& desc) -> RgRtPipelineHandle
		{
			auto id = static_cast<uint32>(rg->rt_pipelines.size());
			for (auto [set_idx, layout] : rg->predefined_descriptor_set_layouts)
			{

				desc.descriptor_set_opts[set_idx] = std::pair{set_idx, rhi::DescriptorSetLayoutOpts{rhi::DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL, layout.bindings}};

			}

			rg->rt_pipelines.push_back(std::move(RgRtPipeline{ desc, shaders }));
			return RgRtPipelineHandle{ id };
		}
		auto PassBuilder::render(std::function<void(RenderPassApi&)>&& fn)->void
		{
			this->pass.render_fn = std::move(fn);

		}
	}
}

