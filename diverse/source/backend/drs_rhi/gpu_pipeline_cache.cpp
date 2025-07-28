
#include "gpu_pipeline_cache.h"
#include "gpu_device.h"
#include "utility/file_utils.h"
#include "shader_compiler.h"
#include "core/ds_log.h"

namespace diverse
{
    namespace rhi
    {
        std::string get_shader_asset_path()
        {
            static std::string ShaderAssetPath = getInstallDirectory() + "/../diverse/assets/";
            return ShaderAssetPath;
        }

        struct ShaderIncludeProvider : public diverse::IncludeProvider
        {
            auto resolve_path(const std::string& path, const std::string parent_file) -> diverse::ResolvedInclude<std::string>
            {
                std::string resolved_path = "";
                if (path.size() > 0 && path[0] == '/') {
                    resolved_path = path;
                }
                else {
                    resolved_path = "/" + std::filesystem::path(parent_file).parent_path().string();
                    resolved_path += "/" + path;
                };

                return diverse::ResolvedInclude<std::string>{resolved_path, resolved_path};
            }
            auto get_include(const std::string& path) -> std::string
            {
                std::string text;
#ifndef DS_PRODUCTION
                auto shader_path = get_shader_asset_path() + path;
                if(!loadText(shader_path, text))
                {
                    DS_LOG_ERROR("open file: {} error", shader_path);
                }
#endif
                return text;
            }

            auto get_working_path()->std::string
            {
                return get_shader_asset_path();
            }
        };
        PipelineCache::PipelineCache()
        {
            shader_compiler_init();
        }
        auto PipelineCache::prepare_frame(GpuDevice* device)->bool
        {
            invalidate_stale_pipelines();
            return parallel_compile_shaders(device);
        }

        auto PipelineCache::invalidate_stale_pipelines()->void
        {
            for (auto& [handle,entry] : compute_entries)
            {
                if (entry.pipeline && entry.worker.need_compile())
                    entry.pipeline = {};
            }
            for (auto& [handle, entry] : raster_entries)
            {
                if (entry.pipeline && entry.worker.need_compile())
                    entry.pipeline = {};
            }
            for (auto& [handle, entry] : rt_entries)
            {
                if (entry.pipeline && entry.worker.need_compile())
                    entry.pipeline = {};
            }
        }

        auto PipelineCache::parallel_compile_shaders(rhi::GpuDevice* device)->bool
        {
            ThreadPool pool;
            std::vector<std::future<CompileTaskOutput>> futures;
            for (auto& entry : compute_entries)
            {
                if( !entry.second.pipeline )
                { 
                    auto f = std::bind(&CompileShaderLazyWorker::run, &entry.second.worker, entry.first);
                    futures.emplace_back(pool.enqueue_task(std::move(f)));
                }
            }
            for (auto& entry : rt_entries)
            {
                if (!entry.second.pipeline)
                { 
                    auto f = std::bind(&CompilePipelineShadersLazyWorker::run_rt, &entry.second.worker, entry.first);
                    futures.emplace_back(pool.enqueue_task(std::move(f)));
                }
            }
            for (auto& entry : raster_entries)
            {
                if (!entry.second.pipeline)
                {
                    auto f = std::bind(&CompilePipelineShadersLazyWorker::run_raster,&entry.second.worker, entry.first);
                    futures.emplace_back(pool.enqueue_task(std::move(f)));
                }
            }

            auto shader_tasks = diverse::wait_and_get_all(std::move(futures));
            for (auto& compiled : shader_tasks)
            {
                switch ( compiled.ty )
                {
                case CompileTaskOutput::Compute:
                {
                    auto& [handle, compile_s] = compiled.compute();
                    auto& entry = compute_entries[handle];
                    entry.pipeline = device->create_compute_pipeline(compile_s, entry.desc);
                }
                break;
                case CompileTaskOutput::Raster:
                {
                    auto& [handle, compile_s] = compiled.raster();
                    auto& entry = raster_entries[handle];
                    std::vector<PipelineShader> compiled_shaders;
                    for (const auto& shader : compile_s.shaders)
                    {
                        compiled_shaders.push_back(PipelineShader{shader.code,shader.desc});
                    }
                    entry.pipeline = device->create_raster_pipeline(compiled_shaders, entry.desc);
                }
                break;
                case CompileTaskOutput::Rt:
                {
                    auto& [handle, compile_s] = compiled.rt();
                    auto& entry = rt_entries[handle];
                    std::vector<PipelineShader> compiled_shaders;
                    for (const auto& shader : compile_s.shaders)
                    {
                        compiled_shaders.push_back(PipelineShader{ shader.code,shader.desc });
                    }
                    entry.pipeline  = device->create_ray_tracing_pipeline(compiled_shaders, entry.desc);
                }
                break;
                default:
                    break;
                }
            }
            return true;
        }
        auto PipelineCache::register_compute(const ComputePipelineDesc& desc) -> ComputePipelineHandle
        {
            auto hash = std::hash<rhi::ShaderSource>{}(desc.source);
            auto it = compute_shader_to_handle.find(hash);
            if (it != compute_shader_to_handle.end())
            {
                return it->second;
            }
            auto handle = ComputePipelineHandle{compute_entries.size()};
            compute_shader_to_handle.insert({ hash, handle });
            CompileShaderLazyWorker compile_task = {desc.source.path, "cs", desc.source.defines};
            compute_entries.insert({handle, ComputePipelineCacheEntry{compile_task, desc, {}}});
            return handle;
        }

        auto PipelineCache::get_compute(ComputePipelineHandle handle) -> std::shared_ptr<ComputePipeline>
        {
            return compute_entries.at(handle).pipeline.value();
        }

        auto PipelineCache::register_raster(const std::vector<PipelineShaderDesc>& shaders, const RasterPipelineDesc& desc) -> RasterPipelineHandle
        {
            auto hash = std::hash<std::vector<PipelineShaderDesc>>{}(shaders);
            auto it = raster_shaders_to_handle.find(hash);
            if (it != raster_shaders_to_handle.end())
            {
                return it->second;
            }
            auto handle = RasterPipelineHandle{raster_entries.size()};
            raster_shaders_to_handle.insert({hash, handle});
            CompilePipelineShadersLazyWorker compile_task = {shaders};
            raster_entries.insert({handle, RasterPipelineCacheEntry {std::move(compile_task),desc, {}}});
            return handle;
        }

        auto PipelineCache::get_raster(RasterPipelineHandle handle) -> std::shared_ptr<RasterPipeline>
        {
            return raster_entries.at(handle).pipeline.value();
        }

        auto PipelineCache::register_ray_tracing(const std::vector<PipelineShaderDesc>& shaders, const RayTracingPipelineDesc& desc) -> RtPipelineHandle
        {
            auto hash = std::hash<std::vector<PipelineShaderDesc>>{}(shaders);
            auto it = rt_shaders_to_handle.find(hash);
            if (it != rt_shaders_to_handle.end())
            {
                return it->second;
            }
            auto handle = RtPipelineHandle{ rt_entries.size() };
            rt_shaders_to_handle.insert({ hash, handle });
            CompilePipelineShadersLazyWorker compile_task = {shaders};
            rt_entries.insert({ handle, RtPipelineCacheEntry {std::move(compile_task),desc, {}} });
            return handle;
        }
        auto PipelineCache::get_ray_tracing(RtPipelineHandle handle) -> std::shared_ptr<RayTracingPipeline>
        {
            return rt_entries.at(handle).pipeline.value();
        }

        auto PipelineCache::refresh_shaders()->void
        {
            for (auto& [handle,entry] : compute_entries)
                entry.worker.dirty_flag = true;
            for (auto& [handle, entry] : raster_entries)
                entry.worker.dirty_flag = true;
            for (auto& [handle, entry] : rt_entries)
                entry.worker.dirty_flag = true;
        }

        auto CompileShaderLazyWorker::run(const ComputePipelineHandle& handle) -> CompileShaderLazyWorker::Output
        {
            const auto instanllDir = get_shader_asset_path();
            const auto name = instanllDir + this->path;
            ShaderIncludeProvider   provider;
            auto source = diverse::process_file(this->path, &provider, "");
            i64 last_write_time = 0;
            for (const auto& s : source)
                last_compiled_time = std::max<i64>(last_write_time, s.last_write_time);

            auto target_profile = fmt::format("{}_6_4", profile);
            auto spirv = diverse::compile_generic_shader_hlsl(name, source, target_profile.c_str(), defines, entry_point);
            return  CompileTaskOutput::compute({handle, CompiledShaderCode{std::move(spirv)}});
        }

        auto CompilePipelineShadersLazyWorker::run_rt(RtPipelineHandle handle) -> CompilePipelineShadersLazyWorker::Output
        {
            CompiledPipelineShaders compiled_shaders;
            const auto instanllDir = get_shader_asset_path();
            i64 last_write_time = 0;
            for (const auto& desc : shader_descs)
            {
                auto path = instanllDir + desc.source.path;
                ShaderIncludeProvider   provider;
                auto source = diverse::process_file(desc.source.path, &provider, "");
                for (const auto& s : source)
                    last_write_time = std::max<i64>(last_write_time, s.last_write_time);

                std::string profile;
                switch (desc.stage)
                {
                case ShaderPipelineStage::Vertex:
                    profile = "vs";
                    break;
                case ShaderPipelineStage::Pixel:
                   profile = "ps";
                   break;
                case ShaderPipelineStage::RayGen:
                case ShaderPipelineStage::RayMiss:
                case ShaderPipelineStage::RayClosestHit:
                case ShaderPipelineStage::RayAnyHit:
                    profile = "lib";
                    break;
                default:
                    DS_LOG_ERROR("not support shader type");
                    break;
                }
                auto target_profile = fmt::format("{}_6_4", profile);
                auto spirv = diverse::compile_generic_shader_hlsl(path, source, target_profile.c_str(), desc.source.defines, desc.source.entry);
                compiled_shaders.shaders.emplace_back(PipelineShader{ CompiledShaderCode{ std::move(spirv) } , desc});
            }
            this->last_compiled_time = last_write_time;
            return CompileTaskOutput::rt({handle, std::move(compiled_shaders) });
        }

        auto CompilePipelineShadersLazyWorker::run_raster(RasterPipelineHandle handle) -> CompilePipelineShadersLazyWorker::Output
        {
            CompiledPipelineShaders compiled_shaders;
            auto instanllDir = get_shader_asset_path();
            i64 last_write_time = 0;
            for (const auto& desc : shader_descs)
            {
                auto path = instanllDir + desc.source.path;
                ShaderIncludeProvider   provider;
                auto source = diverse::process_file(desc.source.path, &provider, "");
                for (const auto& s : source)
                    last_write_time = std::max<i64>(last_write_time, s.last_write_time);
                std::string profile;
                switch (desc.stage)
                {
                case ShaderPipelineStage::Vertex:
                    profile = "vs";
                    break;
                case ShaderPipelineStage::Geometry:
                    profile = "gs";
                    break;
                case ShaderPipelineStage::Pixel:
                    profile = "ps";
                    break;
                default:
                    DS_LOG_ERROR("{not support shader type}");
                    break;
                }
                auto target_profile = fmt::format("{}_6_4", profile);
                auto spirv = diverse::compile_generic_shader_hlsl(path, source, target_profile.c_str(), desc.source.defines, desc.source.entry);
                compiled_shaders.shaders.emplace_back(PipelineShader{ CompiledShaderCode{ std::move(spirv) } , desc });
            }
            this->last_compiled_time = last_write_time;
            return CompileTaskOutput::raster({ handle, std::move(compiled_shaders) });
        }

        auto CompileShaderLazyWorker::need_compile() -> bool
        {
            if( dirty_flag )
            {
                //check file data time
                const auto instanllDir = get_shader_asset_path();
                const auto name = instanllDir + this->path;
                ShaderIncludeProvider   provider;
                auto source = diverse::process_file(this->path, &provider, "");
                i64 last_time = 0;
                for (const auto& s : source) {
                    last_time = std::max<i64>(last_time, s.last_write_time);
                }
                dirty_flag = false;
                if(last_time > last_compiled_time) return true;
            }
            return false;
        }
        auto CompilePipelineShadersLazyWorker::need_compile() -> bool
        {
            if( dirty_flag )
            {
                auto instanllDir = get_shader_asset_path();
                i64 last_write_time = 0;
                for (const auto& desc : shader_descs)
                {
                    auto path = instanllDir + desc.source.path;
                    ShaderIncludeProvider   provider;
                    auto source = diverse::process_file(desc.source.path, &provider, "");
                    for (const auto& s : source)
                        last_write_time = std::max<i64>(last_write_time, s.last_write_time);
                }
                dirty_flag = false;
                if (last_write_time > last_compiled_time) return true;
            }
            return false;
        }
    }
}
