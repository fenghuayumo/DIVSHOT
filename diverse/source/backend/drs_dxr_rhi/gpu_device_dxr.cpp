#include "gpu_device_dxr.h"

#if defined(DS_PLATFORM_WINDOWS)

#include "core/ds_log.h"
#include <algorithm>
#include <cstring>

namespace diverse
{
    namespace rhi
    {
        namespace
        {
            auto throw_if_failed(HRESULT hr, const char* what) -> void
            {
                if (FAILED(hr))
                    throw std::runtime_error(fmt::format("{} failed with HRESULT 0x{:08X}", what, static_cast<u32>(hr)));
            }

            [[noreturn]] auto unsupported(const char* what) -> void
            {
                throw std::runtime_error(fmt::format("DXR RHI backend does not implement {} yet", what));
            }

            auto heap_type_for_memory(MemoryUsage usage) -> D3D12_HEAP_TYPE
            {
                switch (usage)
                {
                case CPU_ONLY:
                case CPU_TO_GPU:
                case CPU_COPY:
                    return D3D12_HEAP_TYPE_UPLOAD;
                case GPU_TO_CPU:
                    return D3D12_HEAP_TYPE_READBACK;
                case GPU_ONLY:
                case Unknown:
                default:
                    return D3D12_HEAP_TYPE_DEFAULT;
                }
            }

            auto initial_state_for_memory(MemoryUsage usage) -> D3D12_RESOURCE_STATES
            {
                switch (usage)
                {
                case CPU_ONLY:
                case CPU_TO_GPU:
                case CPU_COPY:
                    return D3D12_RESOURCE_STATE_GENERIC_READ;
                case GPU_TO_CPU:
                    return D3D12_RESOURCE_STATE_COPY_DEST;
                case GPU_ONLY:
                case Unknown:
                default:
                    return D3D12_RESOURCE_STATE_COMMON;
                }
            }

            auto buffer_flags(BufferUsageFlags usage) -> D3D12_RESOURCE_FLAGS
            {
                auto flags = D3D12_RESOURCE_FLAG_NONE;
                if (enum_has_any_flags(usage, BufferUsageFlags::STORAGE_BUFFER) ||
                    enum_has_any_flags(usage, BufferUsageFlags::STORAGE_TEXEL_BUFFER) ||
                    enum_has_any_flags(usage, BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR))
                    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                return flags;
            }

            auto pick_adapter(IDXGIFactory6* factory, u32 requested_index) -> Microsoft::WRL::ComPtr<IDXGIAdapter1>
            {
                std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter1>> adapters;
                for (UINT index = 0;; ++index)
                {
                    Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
                    if (factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND)
                        break;

                    DXGI_ADAPTER_DESC1 desc = {};
                    candidate->GetDesc1(&desc);
                    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                        continue;
                    if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
                        adapters.push_back(candidate);
                }

                if (adapters.empty())
                    throw std::runtime_error("No D3D12-compatible GPU found");

                const auto selected = requested_index < adapters.size() ? requested_index : 0;
                return adapters[selected];
            }
        }

        GpuCommandBufferDXR::GpuCommandBufferDXR(GpuDeviceDXR* dev)
            : owner(dev)
        {
            throw_if_failed(owner->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "CreateCommandAllocator");
            throw_if_failed(owner->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)), "CreateCommandList");
            command_list->Close();
            throw_if_failed(owner->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");
            fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!fence_event)
                throw std::runtime_error("CreateEvent failed for DXR command fence");
        }

        GpuCommandBufferDXR::~GpuCommandBufferDXR()
        {
            if (fence_event)
                CloseHandle(fence_event);
        }

        auto GpuCommandBufferDXR::begin() -> void
        {
            throw_if_failed(allocator->Reset(), "ID3D12CommandAllocator::Reset");
            throw_if_failed(command_list->Reset(allocator.Get(), nullptr), "ID3D12GraphicsCommandList::Reset");
        }

        auto GpuCommandBufferDXR::end() -> void
        {
            throw_if_failed(command_list->Close(), "ID3D12GraphicsCommandList::Close");
        }

        auto GpuCommandBufferDXR::wait() -> void
        {
            if (fence->GetCompletedValue() < fence_value)
            {
                throw_if_failed(fence->SetEventOnCompletion(fence_value, fence_event), "ID3D12Fence::SetEventOnCompletion");
                WaitForSingleObject(fence_event, INFINITE);
            }
        }

        GpuBufferDXR::GpuBufferDXR(const GpuBufferDesc& buffer_desc, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
            : GpuBuffer(buffer_desc), resource(std::move(resource))
        {
        }

        auto GpuBufferDXR::device_address(const GpuDevice* device) -> u64
        {
            return resource ? resource->GetGPUVirtualAddress() : 0;
        }

        auto GpuBufferDXR::view(const GpuDevice* device, const GpuBufferViewDesc& view_desc) -> std::shared_ptr<GpuBufferView>
        {
            return std::make_shared<GpuBufferViewDXR>();
        }

        auto GpuBufferDXR::map(const GpuDevice* device) -> u8*
        {
            if (!resource)
                return nullptr;
            if (!mapped)
                throw_if_failed(resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "ID3D12Resource::Map");
            return mapped;
        }

        auto GpuBufferDXR::unmap(const GpuDevice* device) -> void
        {
            if (resource && mapped)
            {
                resource->Unmap(0, nullptr);
                mapped = nullptr;
            }
        }

        GpuTextureDXR::GpuTextureDXR(const GpuTextureDesc& texture_desc)
        {
            desc = texture_desc;
        }

        auto GpuTextureDXR::view(const GpuDevice* device, const GpuTextureViewDesc& view_desc) -> std::shared_ptr<GpuTextureView>
        {
            return std::make_shared<GpuTextureViewDXR>();
        }

        auto GpuRayTracingAccelerationDXR::as_device_address(const GpuDevice* device) -> u64
        {
            return backing_buffer ? backing_buffer->device_address(device) : 0;
        }

        auto SwapchainDXR::acquire_next_image() -> SwapchainImage
        {
            unsupported("swapchain image acquisition");
        }

        auto SwapchainDXR::present_image(const SwapchainImage& swap_chain, CommandBuffer* present_cb) -> void
        {
            unsupported("swapchain presentation");
        }

        auto SwapchainDXR::resize(u32 width, u32 height) -> void
        {
            desc.dims = { width, height };
        }

        GpuDeviceDXR::GpuDeviceDXR(u32 device_index)
        {
            UINT factory_flags = 0;
#if defined(DS_DEBUG)
            Microsoft::WRL::ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            {
                debug->EnableDebugLayer();
                factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif
            throw_if_failed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
            adapter = pick_adapter(factory.Get(), device_index);
            throw_if_failed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice");

            DXGI_ADAPTER_DESC1 adapter_desc = {};
            adapter->GetDesc1(&adapter_desc);
            char adapter_name[128] = {};
            WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1, adapter_name, static_cast<int>(std::size(adapter_name)), nullptr, nullptr);
            DS_LOG_INFO("Selected DXR adapter: {}", adapter_name);

            D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
                supports_dxr = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

            gpu_limits.ray_tracing_enabled = supports_dxr;
            gpu_limits.rayQuery = supports_dxr;
            gpu_limits.support_bindless = false;
            gpu_limits.max_per_stage_descriptor_sampled_images = 1024;
            gpu_limits.max_per_stage_descriptor_storage_images = 1024;
            gpu_limits.max_per_stage_descriptor_storage_buffers = 1024;
            gpu_limits.max_per_stage_descriptor_unifrom_texel_buffers = 1024;
            gpu_limits.minUniformBufferOffsetAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
            gpu_limits.minStorageBufferOffsetAlignment = 16;
            gpu_limits.minTexelBufferOffsetAlignment = 16;
            gpu_limits.vram_size = adapter_desc.DedicatedVideoMemory;

            D3D12_COMMAND_QUEUE_DESC queue_desc = {};
            queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&graphics_queue)), "CreateCommandQueue");

            setup_cb = std::make_unique<GpuCommandBufferDXR>(this);
            for (auto& frame : frames)
            {
                auto main_cb = std::make_shared<GpuCommandBufferDXR>(this);
                auto present_cb = std::make_shared<GpuCommandBufferDXR>(this);
                frame = DeviceFrameDXR(main_cb, present_cb);
            }
        }

        GpuDeviceDXR::~GpuDeviceDXR()
        {
            if (graphics_queue && setup_cb)
                setup_cb->wait();
        }

        auto GpuDeviceDXR::create_texture(const GpuTextureDesc& desc, const std::vector<ImageSubData>& initial_data, const char* name)->std::shared_ptr<GpuTexture>
        {
            if (!initial_data.empty())
                DS_LOG_WARN("DXR texture upload is not implemented yet; creating placeholder texture '{}'", name ? name : "");
            auto texture = std::make_shared<GpuTextureDXR>(desc);
            texture->set_owner_device(this);
            return texture;
        }

        auto GpuDeviceDXR::create_buffer(const GpuBufferDesc& desc, const char* name, uint8* initial_data)->std::shared_ptr<GpuBuffer>
        {
            D3D12_HEAP_PROPERTIES heap_properties = {};
            heap_properties.Type = heap_type_for_memory(desc.memory_usage);
            heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heap_properties.CreationNodeMask = 1;
            heap_properties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC resource_desc = {};
            resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resource_desc.Alignment = desc.align;
            resource_desc.Width = desc.size;
            resource_desc.Height = 1;
            resource_desc.DepthOrArraySize = 1;
            resource_desc.MipLevels = 1;
            resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            resource_desc.SampleDesc.Count = 1;
            resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resource_desc.Flags = buffer_flags(desc.usage);

            Microsoft::WRL::ComPtr<ID3D12Resource> resource;
            throw_if_failed(device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &resource_desc,
                initial_state_for_memory(desc.memory_usage),
                nullptr,
                IID_PPV_ARGS(&resource)),
                "CreateCommittedResource(buffer)");

            if (name)
            {
                std::wstring wide_name(name, name + std::strlen(name));
                resource->SetName(wide_name.c_str());
            }

            auto buffer = std::make_shared<GpuBufferDXR>(desc, resource);
            buffer->set_owner_device(this);
            if (initial_data)
            {
                if (heap_properties.Type == D3D12_HEAP_TYPE_UPLOAD)
                {
                    auto dst = buffer->map(this);
                    std::memcpy(dst, initial_data, static_cast<size_t>(desc.size));
                    buffer->unmap(this);
                }
                else
                {
                    DS_LOG_WARN("DXR default-heap initial buffer upload is not implemented yet for '{}'", name ? name : "");
                }
            }
            return buffer;
        }

        auto GpuDeviceDXR::create_render_command_buffer(const char* name) -> std::shared_ptr<CommandBuffer>
        {
            return std::make_shared<GpuCommandBufferDXR>(this);
        }

        auto GpuDeviceDXR::create_swapchain(SwapchainDesc desc, void* window_handle) -> std::shared_ptr<Swapchain>
        {
            swapchain = std::make_shared<SwapchainDXR>(desc);
            return swapchain;
        }

        auto GpuDeviceDXR::create_descriptor_set(GpuBuffer* dynamic_constants, const std::unordered_map<u32, DescriptorInfo>& descriptors, const char* name)->std::shared_ptr<DescriptorSet>
        {
            return std::make_shared<DescriptorSetDXR>();
        }

        auto GpuDeviceDXR::create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors, const char* name) -> std::shared_ptr<DescriptorSet>
        {
            return std::make_shared<DescriptorSetDXR>();
        }

        auto GpuDeviceDXR::bind_descriptor_set(CommandBuffer*, GpuPipeline*, std::vector<DescriptorSetBinding>&, uint32)->void { unsupported("bind_descriptor_set"); }
        auto GpuDeviceDXR::bind_descriptor_set(CommandBuffer*, GpuPipeline*, uint32, DescriptorSet*, u32, u32*)->void { unsupported("bind_descriptor_set"); }
        auto GpuDeviceDXR::bind_pipeline(CommandBuffer*, GpuPipeline*)->void { unsupported("bind_pipeline"); }
        auto GpuDeviceDXR::push_constants(CommandBuffer*, GpuPipeline*, u32, u8*, u32) -> void { unsupported("push_constants"); }
        auto GpuDeviceDXR::create_compute_pipeline(const CompiledShaderCode&, const ComputePipelineDesc&)->std::shared_ptr<ComputePipeline> { return std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::Compute); }
        auto GpuDeviceDXR::create_raster_pipeline(const std::vector<PipelineShader>&, const RasterPipelineDesc&)->std::shared_ptr<RasterPipeline> { return std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::Raster); }
        auto GpuDeviceDXR::create_ray_tracing_pipeline(const std::vector<PipelineShader>&, const RayTracingPipelineDesc&) -> std::shared_ptr<RayTracingPipeline> { return std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::RayTracing); }
        auto GpuDeviceDXR::create_mesh_shader_pipeline(const std::vector<PipelineShader>&, const MeshShaderPipelineDesc&) -> std::shared_ptr<MeshShaderPipeline> { return std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::MeshShader); }
        auto GpuDeviceDXR::create_render_pass(const RenderPassDesc&, const char*) -> std::shared_ptr<RenderPass> { return std::make_shared<RenderPassDXR>(); }
        auto GpuDeviceDXR::begin_frame()->DeviceFrame* { active_frame = (active_frame + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT; return &frames[active_frame]; }
        auto GpuDeviceDXR::end_frame(DeviceFrame*)->void {}
        auto GpuDeviceDXR::begin_cmd(CommandBuffer* cb)->void { cb->begin(); }
        auto GpuDeviceDXR::end_cmd(CommandBuffer* cb)->void { cb->end(); }
        auto GpuDeviceDXR::submit_cmd(CommandBuffer* cb)->void { execute_cmd(cb); }
        auto GpuDeviceDXR::execute_cmd(CommandBuffer* cb)->void
        {
            auto dxr_cb = dynamic_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                unsupported("execute_cmd for non-DXR command buffer");
            ID3D12CommandList* lists[] = { dxr_cb->command_list.Get() };
            graphics_queue->ExecuteCommandLists(1, lists);
            dxr_cb->fence_value++;
            throw_if_failed(graphics_queue->Signal(dxr_cb->fence.Get(), dxr_cb->fence_value), "ID3D12CommandQueue::Signal");
        }
        auto GpuDeviceDXR::record_image_barrier(CommandBuffer*, const ImageBarrier&)->void { unsupported("record_image_barrier"); }
        auto GpuDeviceDXR::record_buffer_barrier(CommandBuffer*, const BufferBarrier&)->void { unsupported("record_buffer_barrier"); }
        auto GpuDeviceDXR::record_global_barrier(CommandBuffer*, const std::vector<rhi::AccessType>&, const std::vector<rhi::AccessType>&)->void { unsupported("record_global_barrier"); }
        auto GpuDeviceDXR::dispatch(CommandBuffer*, const std::array<u32, 3>&, const std::array<u32, 3>&)->void { unsupported("dispatch"); }
        auto GpuDeviceDXR::dispatch_indirect(CommandBuffer*, GpuBuffer*, u64)->void { unsupported("dispatch_indirect"); }
        auto GpuDeviceDXR::write_descriptor_set(DescriptorSet*, u32, rhi::GpuBuffer*, u32)->void { unsupported("write_descriptor_set(buffer)"); }
        auto GpuDeviceDXR::write_descriptor_set(DescriptorSet*, u32, u32, const rhi::DescriptorImageInfo&) -> void { unsupported("write_descriptor_set(image)"); }
        auto GpuDeviceDXR::clear_depth_stencil(CommandBuffer*, GpuTexture*, float, u32)->void { unsupported("clear_depth_stencil"); }
        auto GpuDeviceDXR::clear_color(CommandBuffer*, GpuTexture*, const std::array<f32, 4>&) -> void { unsupported("clear_color"); }
        auto GpuDeviceDXR::copy_image(GpuTexture*, GpuTexture*, CommandBuffer*) -> void { unsupported("copy_image(texture)"); }
        auto GpuDeviceDXR::copy_image(GpuBuffer*, GpuTexture*, CommandBuffer*) -> void { unsupported("copy_image(buffer_to_texture)"); }
        auto GpuDeviceDXR::update_texture(GpuTexture*, const std::vector<ImageSubData>&, const TextureRegion&)->void { unsupported("update_texture"); }
        auto GpuDeviceDXR::set_point_size(CommandBuffer*, float)->void {}
        auto GpuDeviceDXR::set_line_width(CommandBuffer*, float)->void {}
        auto GpuDeviceDXR::set_viewport(CommandBuffer*, const ViewPort&, const Scissor&)->void { unsupported("set_viewport"); }
        auto GpuDeviceDXR::begin_render_pass(CommandBuffer*, const std::array<u32, 2>&, RenderPass*, const std::vector<rhi::GpuTexture*>&, rhi::GpuTexture*)->void { unsupported("begin_render_pass"); }
        auto GpuDeviceDXR::end_render_pass(CommandBuffer*)->void { unsupported("end_render_pass"); }
        auto GpuDeviceDXR::create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc&)-> std::shared_ptr<GpuRayTracingAcceleration> { unsupported("create_ray_tracing_bottom_acceleration"); }
        auto GpuDeviceDXR::create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc&, const RayTracingAccelerationScratchBuffer&)-> std::shared_ptr<GpuRayTracingAcceleration> { unsupported("create_ray_tracing_top_acceleration"); }
        auto GpuDeviceDXR::rebuild_ray_tracing_top_acceleration(CommandBuffer*, u64, u64, GpuRayTracingAcceleration*, RayTracingAccelerationScratchBuffer*)->void { unsupported("rebuild_ray_tracing_top_acceleration"); }
        auto GpuDeviceDXR::bind_vertex_buffers(CommandBuffer*, const GpuBuffer* const*, uint32_t, uint32_t, const uint32_t*, const uint64_t*)->void { unsupported("bind_vertex_buffers"); }
        auto GpuDeviceDXR::bind_index_buffer(CommandBuffer*, const GpuBuffer*, const IndexBufferFormat, uint64_t)->void { unsupported("bind_index_buffer"); }
        auto GpuDeviceDXR::draw(CommandBuffer*, uint32_t, uint32_t) ->void { unsupported("draw"); }
        auto GpuDeviceDXR::draw_indexed(CommandBuffer*, uint32_t, uint32_t, int32_t)->void { unsupported("draw_indexed"); }
        auto GpuDeviceDXR::draw_instanced(CommandBuffer*, uint32_t, uint32_t, uint32_t, uint32_t)->void { unsupported("draw_instanced"); }
        auto GpuDeviceDXR::draw_indexed_instanced(CommandBuffer*, uint32_t, uint32_t, uint32_t, int32_t, uint32_t)->void { unsupported("draw_indexed_instanced"); }
        auto GpuDeviceDXR::draw_instanced_indirect(CommandBuffer*, const GpuBuffer*, uint64_t)->void { unsupported("draw_instanced_indirect"); }
        auto GpuDeviceDXR::draw_indexed_instanced_indirect(CommandBuffer*, const GpuBuffer*, uint64_t)->void { unsupported("draw_indexed_instanced_indirect"); }
        auto GpuDeviceDXR::draw_instanced_indirect_count(CommandBuffer*, const GpuBuffer*, uint64_t, const GpuBuffer*, uint64_t, uint32_t)->void { unsupported("draw_instanced_indirect_count"); }
        auto GpuDeviceDXR::draw_indexed_instanced_indirect_count(CommandBuffer*, const GpuBuffer*, uint64_t, const GpuBuffer*, uint64_t, uint32_t)->void { unsupported("draw_indexed_instanced_indirect_count"); }
        auto GpuDeviceDXR::draw_mesh_tasks(CommandBuffer*, uint32_t, uint32_t, uint32_t)->void { unsupported("draw_mesh_tasks"); }
        auto GpuDeviceDXR::draw_mesh_tasks_indirect(CommandBuffer*, const GpuBuffer*, uint64_t, uint32_t, uint32_t)->void { unsupported("draw_mesh_tasks_indirect"); }
        auto GpuDeviceDXR::draw_mesh_tasks_indirect_count(CommandBuffer*, const GpuBuffer*, uint64_t, const GpuBuffer*, uint64_t, uint32_t, uint32_t)->void { unsupported("draw_mesh_tasks_indirect_count"); }
        auto GpuDeviceDXR::with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback)->void { setup_cb->begin(); callback(setup_cb.get()); setup_cb->end(); execute_cmd(setup_cb.get()); setup_cb->wait(); }
        auto GpuDeviceDXR::copy_buffer(CommandBuffer*, GpuBuffer*, u64, GpuBuffer*, u64, u64)->void { unsupported("copy_buffer"); }
        auto GpuDeviceDXR::trace_rays(CommandBuffer*, RayTracingPipeline*, const std::array<u32, 3>&)->void { unsupported("trace_rays"); }
        auto GpuDeviceDXR::trace_rays_indirect(CommandBuffer*, RayTracingPipeline*, u64) -> void { unsupported("trace_rays_indirect"); }
        auto GpuDeviceDXR::event_begin(const char*, CommandBuffer*)->void {}
        auto GpuDeviceDXR::event_end(CommandBuffer*) -> void {}
        auto GpuDeviceDXR::set_name(GpuResource*, const char*)const->void {}
        auto GpuDeviceDXR::destroy_resource(GpuResource*)->void {}
        auto GpuDeviceDXR::defer_release(std::function<void()> f)->void { f(); }
        auto GpuDeviceDXR::export_image(rhi::GpuTexture*)->std::vector<u8> { unsupported("export_image"); }
        auto GpuDeviceDXR::blit_image(rhi::GpuTexture*, rhi::GpuTexture*, CommandBuffer*) ->void { unsupported("blit_image"); }
        auto GpuDeviceDXR::fill_buffer(CommandBuffer*, GpuBuffer*, uint32_t) -> void { unsupported("fill_buffer"); }
        auto GpuDeviceDXR::get_graphics_cmd_buffer()->CommandBuffer* { return setup_cb.get(); }

        auto create_dxr_device(u32 device_index)->GpuDevice*
        {
            return new GpuDeviceDXR(device_index);
        }
    }
}

#endif
