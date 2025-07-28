#pragma once

#include "gpu_raytracing.h"
#include "utility/thread_pool.h"
namespace diverse
{
    namespace rhi
    {
        struct GpuDevice;
        using ComputePipelineHandle = uint64;
        using RasterPipelineHandle = uint64;
        using RtPipelineHandle = uint64;

        struct CompileShader
        {
            std::string path;
            std::string profile;
        };

        struct CompiledPipelineShaders
        {
            std::vector<PipelineShader> shaders; 
        };

        struct CompilePipelineShaders
        {
            std::vector<PipelineShaderDesc> shader_descs;
        };

        struct CompileTaskOutput
        {
            enum 
            {
                Compute,
                Raster,
                Rt
            }ty;

            std::any value;

            auto compute() -> std::pair<ComputePipelineHandle, CompiledShaderCode>&
            {
                return std::any_cast<std::pair<ComputePipelineHandle, CompiledShaderCode>&>(value);
            }
            auto rt() -> std::pair<RtPipelineHandle, CompiledPipelineShaders>&
            {
                return std::any_cast<std::pair<RtPipelineHandle, CompiledPipelineShaders>&>(value);
            }

            auto raster() -> std::pair<RasterPipelineHandle, CompiledPipelineShaders>&
            {
                return std::any_cast<std::pair<RasterPipelineHandle, CompiledPipelineShaders>&>(value);
            }

            static auto compute(std::pair<ComputePipelineHandle, CompiledShaderCode>&& v) -> CompileTaskOutput
            {
                return CompileTaskOutput{Compute, std::move(v)};
            }

            static auto rt(std::pair<RtPipelineHandle, CompiledPipelineShaders>&& v) -> CompileTaskOutput
            {
                return CompileTaskOutput{ Rt, std::move(v) };
            }

            static auto raster(std::pair<RasterPipelineHandle, CompiledPipelineShaders>&& v) -> CompileTaskOutput
            {
                return CompileTaskOutput{ Raster, std::move(v) };
            }
        };

        struct CompilePipelineShadersLazyWorker
        {
            using Output = CompileTaskOutput;
            std::vector<PipelineShaderDesc> shader_descs;
            bool dirty_flag = false;
            i64 last_compiled_time = 0;
            auto run_rt(RtPipelineHandle handle) -> CompilePipelineShadersLazyWorker::Output;
            auto run_raster(RasterPipelineHandle handle) -> CompilePipelineShadersLazyWorker::Output;
            auto need_compile()->bool;
        };

        struct CompileShaderLazyWorker
        {
            using Output = CompileTaskOutput;

            std::string path;
            std::string profile;
            std::vector<std::pair<std::string, std::string>> defines;
            std::string entry_point = "main";
            bool dirty_flag = false;
            i64 last_compiled_time = 0;

            auto run(const ComputePipelineHandle& handle) -> CompileShaderLazyWorker::Output;
            auto need_compile()->bool;
        };

        struct ComputePipelineCacheEntry
        {
            CompileShaderLazyWorker worker;
            ComputePipelineDesc desc;
            std::optional<std::shared_ptr<ComputePipeline>> pipeline;
        };

        struct RasterPipelineCacheEntry
        {
            CompilePipelineShadersLazyWorker worker;
            RasterPipelineDesc desc;
            std::optional<std::shared_ptr<RasterPipeline>> pipeline;
        };

        struct RtPipelineCacheEntry
        {
            CompilePipelineShadersLazyWorker worker;
            RayTracingPipelineDesc desc;
            std::optional<std::shared_ptr<RayTracingPipeline>> pipeline;
        };

        struct PipelineCache
        {
            PipelineCache();
            std::unordered_map<ComputePipelineHandle, ComputePipelineCacheEntry> compute_entries;
            std::unordered_map<RasterPipelineHandle, RasterPipelineCacheEntry> raster_entries;
            std::unordered_map<RtPipelineHandle, RtPipelineCacheEntry> rt_entries;

            std::unordered_map<u64, ComputePipelineHandle> compute_shader_to_handle;
            std::unordered_map<u64, RasterPipelineHandle> raster_shaders_to_handle;
            std::unordered_map<u64, RtPipelineHandle> rt_shaders_to_handle;

            auto prepare_frame(GpuDevice* device)->bool;

            auto invalidate_stale_pipelines()->void;
            auto parallel_compile_shaders(rhi::GpuDevice* device)->bool;

            auto register_compute(const ComputePipelineDesc& desc)-> ComputePipelineHandle;
            auto get_compute(ComputePipelineHandle handle) -> std::shared_ptr<ComputePipeline>;

            auto register_raster(const std::vector< PipelineShaderDesc>& shaders,const RasterPipelineDesc& desc) -> RasterPipelineHandle;
            auto get_raster(RasterPipelineHandle handle) -> std::shared_ptr<RasterPipeline>;

            auto register_ray_tracing(const std::vector< PipelineShaderDesc>& shaders, const RayTracingPipelineDesc& desc) -> RtPipelineHandle;
            auto get_ray_tracing(RtPipelineHandle handle) -> std::shared_ptr<RayTracingPipeline>;

            auto refresh_shaders()->void;
        };

    }
}
