#include "resource_registry.h"

namespace diverse
{
    namespace rg
    {
        auto ResourceRegistry::compute_pipeline(const RgComputePipelineHandle& pipeline)->std::shared_ptr<rhi::ComputePipeline>
        {
            auto handle = this->pipelines.compute[pipeline.id];
            return execution_params.pipeline_cache->get_compute(handle);
        }

        auto ResourceRegistry::raster_pipeline(const RgRasterPipelineHandle& pipeline)->std::shared_ptr<rhi::RasterPipeline>
        {
            auto handle = this->pipelines.raster[pipeline.id];
            return execution_params.pipeline_cache->get_raster(handle);
        }

        auto ResourceRegistry::ray_tracing_pipeline(const RgRtPipelineHandle& pipeline)->std::shared_ptr<rhi::RayTracingPipeline>
        {
            auto handle = this->pipelines.rt[pipeline.id];
            return execution_params.pipeline_cache->get_ray_tracing(handle);
        }
    }
}