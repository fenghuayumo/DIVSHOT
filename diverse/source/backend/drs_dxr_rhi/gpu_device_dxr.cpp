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

            auto descriptor_type_to_d3d12(DescriptorType type) -> D3D12_DESCRIPTOR_RANGE_TYPE
            {
                switch (type)
                {
                case DescriptorType::SAMPLED_IMAGE:
                case DescriptorType::COMBINED_IMAGE_SAMPLER:
                case DescriptorType::UNIFORM_TEXEL_BUFFER:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                case DescriptorType::STORAGE_IMAGE:
                case DescriptorType::STORAGE_TEXEL_BUFFER:
                case DescriptorType::ACCELERATION_STRUCTURE_KHR:
                case DescriptorType::ACCELERATION_STRUCTURE_NV:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                case DescriptorType::UNIFORM_BUFFER:
                case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                case DescriptorType::STORAGE_BUFFER:
                case DescriptorType::STORAGE_BUFFER_DYNAMIC:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                case DescriptorType::SAMPLER:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                default:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                }
            }

            auto create_root_signature(ID3D12Device5* device, const std::unordered_map<u32, DescriptorInfo>& descriptors, u32 push_constant_bytes = 0)
                -> Microsoft::WRL::ComPtr<ID3D12RootSignature>
            {
                std::vector<D3D12_ROOT_PARAMETER1> root_parameters;
                std::vector<std::vector<D3D12_DESCRIPTOR_RANGE1>> ranges_storage;

                // Group descriptors by binding
                std::unordered_map<u32, std::vector<DescriptorInfo>> grouped_descriptors;
                for (const auto& [binding, info] : descriptors)
                {
                    grouped_descriptors[binding].push_back(info);
                }

                // Create descriptor tables
                for (const auto& [binding, infos] : grouped_descriptors)
                {
                    D3D12_ROOT_PARAMETER1 root_param = {};
                    root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

                    std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;
                    for (const auto& info : infos)
                    {
                        D3D12_DESCRIPTOR_RANGE1 range = {};
                        range.RangeType = descriptor_type_to_d3d12(info.ty);
                        range.NumDescriptors = 1;
                        range.BaseShaderRegister = binding;
                        range.RegisterSpace = 0;
                        range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                        ranges.push_back(range);
                    }

                    ranges_storage.push_back(ranges);
                    root_param.DescriptorTable.pDescriptorRanges = ranges_storage.back().data();
                    root_param.DescriptorTable.NumDescriptorRanges = static_cast<u32>(ranges_storage.back().size());
                    root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

                    root_parameters.push_back(root_param);
                }

                // Add push constants if needed
                if (push_constant_bytes > 0)
                {
                    D3D12_ROOT_PARAMETER1 push_constant_param = {};
                    push_constant_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                    push_constant_param.Constants.ShaderRegister = 0;
                    push_constant_param.Constants.RegisterSpace = 0;
                    push_constant_param.Constants.Num32BitValues = push_constant_bytes / 4;
                    push_constant_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                    root_parameters.push_back(push_constant_param);
                }

                D3D12_ROOT_SIGNATURE_DESC1 root_signature_desc = {};
                root_signature_desc.NumParameters = static_cast<u32>(root_parameters.size());
                root_signature_desc.pParameters = root_parameters.data();
                root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

                D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc = {};
                versioned_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
                versioned_desc.Desc_1_1 = root_signature_desc;

                Microsoft::WRL::ComPtr<ID3DBlob> signature;
                Microsoft::WRL::ComPtr<ID3DBlob> error;
                throw_if_failed(D3D12SerializeVersionedRootSignature(&versioned_desc, &signature, &error), "D3D12SerializeVersionedRootSignature");

                Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
                throw_if_failed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)), "CreateRootSignature");

                return root_signature;
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

            auto pixel_format_to_dxgi(PixelFormat format) -> DXGI_FORMAT
            {
                switch (format)
                {
                case PixelFormat::R8_UNorm: return DXGI_FORMAT_R8_UNORM;
                case PixelFormat::R8G8_UNorm: return DXGI_FORMAT_R8G8_UNORM;
                case PixelFormat::R8G8B8A8_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
                case PixelFormat::B8G8R8A8_UNorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
                case PixelFormat::R16_UNorm: return DXGI_FORMAT_R16_UNORM;
                case PixelFormat::R16G16_UNorm: return DXGI_FORMAT_R16G16_UNORM;
                case PixelFormat::R16G16B16A16_UNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
                case PixelFormat::R16_Float: return DXGI_FORMAT_R16_FLOAT;
                case PixelFormat::R16G16_Float: return DXGI_FORMAT_R16G16_FLOAT;
                case PixelFormat::R16G16B16A16_Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case PixelFormat::R32_Float: return DXGI_FORMAT_R32_FLOAT;
                case PixelFormat::R32G32_Float: return DXGI_FORMAT_R32G32_FLOAT;
                case PixelFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
                case PixelFormat::R32_UInt: return DXGI_FORMAT_R32_UINT;
                case PixelFormat::R32G32_UInt: return DXGI_FORMAT_R32G32_UINT;
                case PixelFormat::R32G32B32A32_UInt: return DXGI_FORMAT_R32G32B32A32_UINT;
                case PixelFormat::D16_UNorm: return DXGI_FORMAT_D16_UNORM;
                case PixelFormat::D24_UNorm_S8_UInt: return DXGI_FORMAT_D24_UNORM_S8_UINT;
                case PixelFormat::D32_Float: return DXGI_FORMAT_D32_FLOAT;
                case PixelFormat::D32_Float_S8X24_UInt: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                case PixelFormat::BC1_UNorm: return DXGI_FORMAT_BC1_UNORM;
                case PixelFormat::BC2_UNorm: return DXGI_FORMAT_BC2_UNORM;
                case PixelFormat::BC3_UNorm: return DXGI_FORMAT_BC3_UNORM;
                case PixelFormat::BC4_UNorm: return DXGI_FORMAT_BC4_UNORM;
                case PixelFormat::BC5_UNorm: return DXGI_FORMAT_BC5_UNORM;
                case PixelFormat::BC6H_Uf16: return DXGI_FORMAT_BC6H_UF16;
                case PixelFormat::BC7_UNorm: return DXGI_FORMAT_BC7_UNORM;
                default: return DXGI_FORMAT_R8G8B8A8_UNORM;
                }
            }

            auto texture_type_to_d3d12(TextureType type) -> D3D12_RESOURCE_DIMENSION
            {
                switch (type)
                {
                case TextureType::Tex1d: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                case TextureType::Tex2d: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                case TextureType::Tex3d: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                default: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                }
            }

            auto texture_usage_to_flags(TextureUsageFlags usage) -> D3D12_RESOURCE_FLAGS
            {
                auto flags = D3D12_RESOURCE_FLAG_NONE;
                if (enum_has_any_flags(usage, TextureUsageFlags::STORAGE))
                    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                if (enum_has_any_flags(usage, TextureUsageFlags::COLOR_ATTACHMENT))
                    flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                if (enum_has_any_flags(usage, TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT))
                    flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                return flags;
            }

            auto texture_type_to_srv_dims(TextureType type, u32 array_elements) -> D3D12_SRV_DIMENSION
            {
                if (array_elements > 1)
                {
                    switch (type)
                    {
                    case TextureType::Tex1d: return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                    case TextureType::Tex2d: return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    case TextureType::Tex3d: return D3D12_SRV_DIMENSION_TEXTURE3D;
                    default: return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    }
                }
                else
                {
                    switch (type)
                    {
                    case TextureType::Tex1d: return D3D12_SRV_DIMENSION_TEXTURE1D;
                    case TextureType::Tex2d: return D3D12_SRV_DIMENSION_TEXTURE2D;
                    case TextureType::Tex3d: return D3D12_SRV_DIMENSION_TEXTURE3D;
                    default: return D3D12_SRV_DIMENSION_TEXTURE2D;
                    }
                }
            }

            auto blend_mode_to_d3d12(BlendMode mode) -> D3D12_BLEND
            {
                switch (mode)
                {
                case BlendMode::OneZero: return D3D12_BLEND_ONE;
                case BlendMode::ZeroSrcColor: return D3D12_BLEND_ZERO;
                case BlendMode::SrcAlphaOneMinusSrcAlpha: return D3D12_BLEND_SRC_ALPHA;
                case BlendMode::SrcAlphaOne: return D3D12_BLEND_SRC_ALPHA;
                case BlendMode::OneOneMinusSrcAlpha: return D3D12_BLEND_ONE;
                case BlendMode::OneOne: return D3D12_BLEND_ONE;
                default: return D3D12_BLEND_ONE;
                }
            }

            auto compare_func_to_d3d12(CompareFunc func) -> D3D12_COMPARISON_FUNC
            {
                switch (func)
                {
                case CompareFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
                case CompareFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
                case CompareFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
                case CompareFunc::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
                case CompareFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
                case CompareFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
                case CompareFunc::GreaterEqaul: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
                case CompareFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
                default: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
                }
            }

            auto cull_mode_to_d3d12(CullMode mode) -> D3D12_CULL_MODE
            {
                switch (mode)
                {
                case CullMode::FRONT: return D3D12_CULL_MODE_FRONT;
                case CullMode::BACK: return D3D12_CULL_MODE_BACK;
                case CullMode::FRONTANDBACK: return D3D12_CULL_MODE_NONE;
                case CullMode::NONE: return D3D12_CULL_MODE_NONE;
                default: return D3D12_CULL_MODE_BACK;
                }
            }

            auto polygon_mode_to_d3d12(PolygonMode mode) -> D3D12_FILL_MODE
            {
                switch (mode)
                {
                case PolygonMode::Fill: return D3D12_FILL_MODE_SOLID;
                case PolygonMode::WireFrame: return D3D12_FILL_MODE_WIREFRAME;
                case PolygonMode::Point: return D3D12_FILL_MODE_SOLID;
                default: return D3D12_FILL_MODE_SOLID;
                }
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
            SwapchainImage result = {};

            if (!swap_chain || back_buffers.empty())
            {
                DS_LOG_ERROR("Swapchain not properly initialized");
                return result;
            }

            // Get current back buffer index
            result.image_index = frame_index;
            // Note: Creating GpuTexture wrappers for back buffers requires more implementation
            // For now, return null - this needs proper texture wrapper creation
            result.image = nullptr;

            return result;
        }

        auto SwapchainDXR::present_image(const SwapchainImage& swap_chain, CommandBuffer* present_cb) -> void
        {
            if (!this->swap_chain)
            {
                DS_LOG_ERROR("Invalid swap chain image for presentation");
                return;
            }

            // Present with sync interval 1 (vsync enabled)
            HRESULT hr = this->swap_chain->Present(desc.vsync ? 1 : 0, 0);
            if (FAILED(hr))
            {
                DS_LOG_ERROR("Present failed: HRESULT 0x{:08X}", static_cast<u32>(hr));
            }

            frame_index = (frame_index + 1) % 3; // Triple buffering
        }

        auto SwapchainDXR::resize(u32 width, u32 height, bool /*force*/) -> void
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

            // Create descriptor heaps
            D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc = {};
            cbv_srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            cbv_srv_uav_heap_desc.NumDescriptors = DESCRIPTOR_HEAP_SIZE;
            cbv_srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            throw_if_failed(device->CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(&cbv_srv_uav_heap)), "CreateDescriptorHeap(CBV_SRV_UAV)");
            cbv_srv_uav_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc = {};
            sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            sampler_heap_desc.NumDescriptors = DESCRIPTOR_HEAP_SIZE / 4;
            sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            throw_if_failed(device->CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(&sampler_heap)), "CreateDescriptorHeap(Sampler)");
            sampler_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

            // Create ray tracing command signature for indirect dispatch
            if (supports_dxr)
            {
                D3D12_INDIRECT_ARGUMENT_DESC indirect_arg = {};
                indirect_arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;

                D3D12_COMMAND_SIGNATURE_DESC cmd_sig_desc = {};
                cmd_sig_desc.ByteStride = sizeof(D3D12_DISPATCH_RAYS_DESC);
                cmd_sig_desc.NumArgumentDescs = 1;
                cmd_sig_desc.pArgumentDescs = &indirect_arg;
                cmd_sig_desc.NodeMask = 0;

                throw_if_failed(device->CreateCommandSignature(&cmd_sig_desc, nullptr, IID_PPV_ARGS(&raytracing_command_signature)), "CreateCommandSignature(raytracing)");
            }

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

            std::vector<DeferedReleaseResource> pending_releases;
            {
                std::lock_guard<std::mutex> lock(release_mutex);
                pending_releases.swap(destroy_queue);
            }

            for (auto& release : pending_releases)
            {
                if (release.release)
                    release.release();
            }
        }

        auto GpuDeviceDXR::create_texture(const GpuTextureDesc& desc, const std::vector<ImageSubData>& initial_data, const char* name)->std::shared_ptr<GpuTexture>
        {
            // Determine resource dimension
            D3D12_RESOURCE_DIMENSION dimension = texture_type_to_d3d12(desc.image_type);

            // Convert pixel format
            DXGI_FORMAT dxgi_format = pixel_format_to_dxgi(desc.format);

            // Determine resource flags
            D3D12_RESOURCE_FLAGS resource_flags = texture_usage_to_flags(desc.usage);

            // Determine initial state
            D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
            if (enum_has_any_flags(desc.usage, TextureUsageFlags::COLOR_ATTACHMENT))
                initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            else if (enum_has_any_flags(desc.usage, TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT))
                initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            else if (enum_has_any_flags(desc.usage, TextureUsageFlags::STORAGE))
                initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            else if (enum_has_any_flags(desc.usage, TextureUsageFlags::SAMPLED))
                initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            // Determine heap type
            D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
            if (!initial_data.empty())
            {
                // Will use upload heap for initial data
            }

            // Create texture description
            D3D12_RESOURCE_DESC resource_desc = {};
            resource_desc.Dimension = dimension;
            resource_desc.Width = desc.extent[0];
            resource_desc.Height = desc.extent[1];
            resource_desc.DepthOrArraySize = desc.image_type == TextureType::Tex3d ? desc.extent[2] : desc.array_elements;
            resource_desc.MipLevels = desc.mip_levels;
            resource_desc.Format = dxgi_format;
            resource_desc.SampleDesc.Count = 1;
            resource_desc.SampleDesc.Quality = 0;
            resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resource_desc.Flags = resource_flags;

            // Determine if we need an upload heap
            bool needs_upload = !initial_data.empty();
            Microsoft::WRL::ComPtr<ID3D12Resource> upload_resource;

            // Create committed resource
            D3D12_HEAP_PROPERTIES heap_properties = {};
            heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heap_properties.CreationNodeMask = 1;
            heap_properties.VisibleNodeMask = 1;

            Microsoft::WRL::ComPtr<ID3D12Resource> texture_resource;
            throw_if_failed(device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &resource_desc,
                initial_state,
                nullptr,
                IID_PPV_ARGS(&texture_resource)),
                "CreateCommittedResource(texture)");

            if (name)
            {
                std::wstring wide_name(name, name + std::strlen(name));
                texture_resource->SetName(wide_name.c_str());
            }

            // Upload initial data if provided
            if (needs_upload && !initial_data.empty())
            {
                // Calculate upload buffer size
                UINT64 upload_buffer_size = 0;
                device->GetCopyableFootprints(&resource_desc, 0, 1, 0, nullptr, nullptr, nullptr, &upload_buffer_size);

                // Create upload heap
                D3D12_HEAP_PROPERTIES upload_heap_props = {};
                upload_heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
                upload_heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
                upload_heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

                D3D12_RESOURCE_DESC upload_desc = {};
                upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                upload_desc.Width = upload_buffer_size;
                upload_desc.Height = 1;
                upload_desc.DepthOrArraySize = 1;
                upload_desc.MipLevels = 1;
                upload_desc.Format = DXGI_FORMAT_UNKNOWN;
                upload_desc.SampleDesc.Count = 1;
                upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

                throw_if_failed(device->CreateCommittedResource(
                    &upload_heap_props,
                    D3D12_HEAP_FLAG_NONE,
                    &upload_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&upload_resource)),
                    "CreateCommittedResource(upload texture)");

                // Map and copy data
                u8* upload_data = nullptr;
                throw_if_failed(upload_resource->Map(0, nullptr, reinterpret_cast<void**>(&upload_data)), "Map upload texture");

                D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_footprint = {};
                UINT row_count = 0;
                UINT64 row_size_in_bytes = 0;
                device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &placed_footprint, &row_count, &row_size_in_bytes, static_cast<UINT64*>(nullptr));

                for (size_t i = 0; i < initial_data.size(); ++i)
                {
                    const auto& sub_data = initial_data[i];
                    u8* dst = upload_data + placed_footprint.Offset;
                    const u8* src = sub_data.data;

                    for (u32 row = 0; row < row_count; ++row)
                    {
                        std::memcpy(dst + row * placed_footprint.Footprint.RowPitch,
                                   src + row * sub_data.row_pitch,
                                   std::min<u64>(sub_data.row_pitch, row_size_in_bytes));
                    }
                }

                upload_resource->Unmap(0, nullptr);

                // Upload using setup command buffer
                with_setup_cb([this, &texture_resource, &upload_resource, placed_footprint](CommandBuffer* cmd) {
                    auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd);
                    if (!dxr_cb)
                        return;

                    D3D12_TEXTURE_COPY_LOCATION dst = {};
                    dst.pResource = texture_resource.Get();
                    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    dst.SubresourceIndex = 0;

                    D3D12_TEXTURE_COPY_LOCATION src = {};
                    src.pResource = upload_resource.Get();
                    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    src.PlacedFootprint = placed_footprint;

                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = texture_resource.Get();
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    barrier.Transition.Subresource = 0;

                    dxr_cb->command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                    dxr_cb->command_list->ResourceBarrier(1, &barrier);
                });
            }

            auto texture = std::make_shared<GpuTextureDXR>(desc);
            texture->resource = texture_resource;
            texture->current_state = initial_state;
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
            auto swapchain_dxr = std::make_shared<SwapchainDXR>(desc);

            if (!window_handle)
            {
                DS_LOG_ERROR("Invalid window handle for swapchain creation");
                return swapchain_dxr;
            }

            HWND hwnd = static_cast<HWND>(window_handle);

            // Describe the swap chain
            DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
            swap_chain_desc.Width = desc.dims[0];
            swap_chain_desc.Height = desc.dims[1];
            swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swap_chain_desc.Stereo = FALSE;
            swap_chain_desc.SampleDesc.Count = 1;
            swap_chain_desc.SampleDesc.Quality = 0;
            swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swap_chain_desc.BufferCount = 3; // Triple buffering
            swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
            swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            swap_chain_desc.Flags = 0;

            // Create swap chain
            Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;
            HRESULT hr = factory->CreateSwapChainForHwnd(
                graphics_queue.Get(),
                hwnd,
                &swap_chain_desc,
                nullptr,
                nullptr,
                &swap_chain1);

            if (FAILED(hr))
            {
                DS_LOG_ERROR("Failed to create swap chain: HRESULT 0x{:08X}", static_cast<u32>(hr));
                return swapchain_dxr;
            }

            // Query for IDXGISwapChain4
            if (FAILED(swap_chain1.As(&swapchain_dxr->swap_chain)))
            {
                DS_LOG_WARN("Failed to query IDXGISwapChain4, using IDXGISwapChain1");
                swapchain_dxr->swap_chain = Microsoft::WRL::ComPtr<IDXGISwapChain4>(static_cast<IDXGISwapChain4*>(swap_chain1.Get()));
            }

            // Disable Alt+Enter fullscreen toggle
            factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

            // Create back buffers
            swapchain_dxr->back_buffers.resize(swap_chain_desc.BufferCount);
            for (u32 i = 0; i < swap_chain_desc.BufferCount; ++i)
            {
                hr = swapchain_dxr->swap_chain->GetBuffer(i, IID_PPV_ARGS(&swapchain_dxr->back_buffers[i]));
                if (FAILED(hr))
                {
                    DS_LOG_ERROR("Failed to get swap chain back buffer {}: HRESULT 0x{:08X}", i, static_cast<u32>(hr));
                }
            }

            this->swapchain = swapchain_dxr;
            return swapchain_dxr;
        }

        auto GpuDeviceDXR::create_descriptor_set(GpuBuffer* dynamic_constants, const std::unordered_map<u32, DescriptorInfo>& descriptors, const char* name)->std::shared_ptr<DescriptorSet>
        {
            auto descriptor_set = std::make_shared<DescriptorSetDXR>();

            // Allocate descriptors from heaps
            for (const auto& [binding, info] : descriptors)
            {
                if (info.ty == DescriptorType::SAMPLER)
                {
                    if (sampler_heap_index >= DESCRIPTOR_HEAP_SIZE / 4)
                        throw std::runtime_error("Sampler descriptor heap exhausted");
                    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = sampler_heap->GetCPUDescriptorHandleForHeapStart();
                    cpu_handle.ptr += sampler_heap_index * sampler_descriptor_size;
                    descriptor_set->cpu_handles[binding] = cpu_handle;

                    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = sampler_heap->GetGPUDescriptorHandleForHeapStart();
                    gpu_handle.ptr += sampler_heap_index * sampler_descriptor_size;
                    descriptor_set->gpu_handles[binding] = gpu_handle;
                    sampler_heap_index++;
                }
                else
                {
                    if (cbv_srv_uav_heap_index >= DESCRIPTOR_HEAP_SIZE)
                        throw std::runtime_error("CBV_SRV_UAV descriptor heap exhausted");
                    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
                    cpu_handle.ptr += cbv_srv_uav_heap_index * cbv_srv_uav_descriptor_size;
                    descriptor_set->cpu_handles[binding] = cpu_handle;

                    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = cbv_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
                    gpu_handle.ptr += cbv_srv_uav_heap_index * cbv_srv_uav_descriptor_size;
                    descriptor_set->gpu_handles[binding] = gpu_handle;
                    cbv_srv_uav_heap_index++;
                }
            }

            return descriptor_set;
        }

        auto GpuDeviceDXR::create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors, const char* name) -> std::shared_ptr<DescriptorSet>
        {
            return std::make_shared<DescriptorSetDXR>();
        }

        auto GpuDeviceDXR::bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, std::vector<DescriptorSetBinding>& bindings, uint32 set_index)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_pipeline = static_cast<PipelineDXR*>(pipeline);
            if (!dxr_cb || !dxr_pipeline)
                return;

            // Set descriptor heaps
            ID3D12DescriptorHeap* heaps[] = { cbv_srv_uav_heap.Get(), sampler_heap.Get() };
            dxr_cb->command_list->SetDescriptorHeaps(2, heaps);

            // Create a temporary descriptor set for this binding
            auto temp_descriptor_set = std::make_shared<DescriptorSetDXR>();

            // Allocate descriptors from heaps and write descriptor data
            for (u32 binding_idx = 0; binding_idx < static_cast<u32>(bindings.size()); binding_idx++)
            {
                auto& binding = bindings[binding_idx];

                // Allocate descriptor from appropriate heap
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
                D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;

                bool is_sampler = false;

                // Determine heap type based on binding type
                switch (binding.ty)
                {
                case DescriptorSetBinding::Type::Image:
                case DescriptorSetBinding::Type::ImageArray:
                case DescriptorSetBinding::Type::Buffer:
                case DescriptorSetBinding::Type::RayTracingAcceleration:
                case DescriptorSetBinding::Type::DynamicBuffer:
                case DescriptorSetBinding::Type::DynamicStorageBuffer:
                    // Use CBV_SRV_UAV heap
                    if (cbv_srv_uav_heap_index >= DESCRIPTOR_HEAP_SIZE)
                    {
                        DS_LOG_ERROR("CBV_SRV_UAV descriptor heap exhausted");
                        continue;
                    }
                    cpu_handle = cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
                    cpu_handle.ptr += cbv_srv_uav_heap_index * cbv_srv_uav_descriptor_size;
                    gpu_handle = cbv_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
                    gpu_handle.ptr += cbv_srv_uav_heap_index * cbv_srv_uav_descriptor_size;
                    cbv_srv_uav_heap_index++;
                    break;
                default:
                    break;
                }

                // Store handles for binding
                temp_descriptor_set->cpu_handles[binding_idx] = cpu_handle;
                temp_descriptor_set->gpu_handles[binding_idx] = gpu_handle;

                // Write descriptor data based on binding type
                switch (binding.ty)
                {
                case DescriptorSetBinding::Type::Buffer:
                {
                    auto& buf = binding.Buffer();
                    auto dxr_buffer = static_cast<GpuBufferDXR*>(buf.buffer);
                    if (dxr_buffer && dxr_buffer->resource)
                    {
                        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
                        cbv_desc.BufferLocation = dxr_buffer->resource->GetGPUVirtualAddress();
                        cbv_desc.SizeInBytes = static_cast<u32>(dxr_buffer->desc.size);
                        device->CreateConstantBufferView(&cbv_desc, cpu_handle);
                    }
                    break;
                }
                case DescriptorSetBinding::Type::Image:
                {
                    // Image descriptor write - requires texture view
                    // This is not fully implemented as GpuTextureView::owner is not available
                    DS_LOG_WARN("DXR backend: Image descriptor binding not fully implemented");
                    break;
                }
                case DescriptorSetBinding::Type::ImageArray:
                {
                    DS_LOG_WARN("DXR backend: Image array descriptor binding not implemented");
                    break;
                }
                case DescriptorSetBinding::Type::RayTracingAcceleration:
                {
                    auto rt_accel = binding.RayTracingAcceleration();
                    auto rt_accel_dxr = static_cast<GpuRayTracingAccelerationDXR*>(rt_accel);
                    if (rt_accel_dxr && rt_accel_dxr->acceleration_structure)
                    {
                        // Create SRV for ray tracing acceleration structure
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                        srv_desc.RaytracingAccelerationStructure.Location = rt_accel_dxr->as_device_address(this);
                        device->CreateShaderResourceView(nullptr, &srv_desc, cpu_handle);
                    }
                    break;
                }
                case DescriptorSetBinding::Type::DynamicBuffer:
                {
                    auto& [dy_buf, offset] = binding.DynamicBuffer();
                    auto dxr_buffer = static_cast<GpuBufferDXR*>(dy_buf);
                    if (dxr_buffer && dxr_buffer->resource)
                    {
                        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
                        cbv_desc.BufferLocation = dxr_buffer->resource->GetGPUVirtualAddress() + offset;
                        cbv_desc.SizeInBytes = static_cast<u32>(dxr_buffer->desc.size);
                        device->CreateConstantBufferView(&cbv_desc, cpu_handle);
                    }
                    break;
                }
                case DescriptorSetBinding::Type::DynamicStorageBuffer:
                {
                    auto& [dy_buf, offset] = binding.DynamicStorageBuffer();
                    auto dxr_buffer = static_cast<GpuBufferDXR*>(dy_buf);
                    if (dxr_buffer && dxr_buffer->resource)
                    {
                        // Create UAV for storage buffer
                        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
                        uav_desc.Format = DXGI_FORMAT_UNKNOWN;
                        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                        uav_desc.Buffer.FirstElement = offset / sizeof(u32); // Assume 4-byte elements
                        uav_desc.Buffer.NumElements = static_cast<u32>((dxr_buffer->desc.size - offset) / sizeof(u32));
                        device->CreateUnorderedAccessView(dxr_buffer->resource.Get(), nullptr, &uav_desc, cpu_handle);
                    }
                    break;
                }
                }
            }

            // Bind descriptor tables for each binding
            for (const auto& [binding_idx, gpu_handle] : temp_descriptor_set->gpu_handles)
            {
                // Use binding index as root parameter index
                u32 root_param_index = binding_idx;

                switch (dxr_pipeline->ty)
                {
                case GpuPipeline::PieplineType::Compute:
                case GpuPipeline::PieplineType::RayTracing:
                    dxr_cb->command_list->SetComputeRootDescriptorTable(root_param_index, gpu_handle);
                    break;
                case GpuPipeline::PieplineType::Raster:
                    dxr_cb->command_list->SetGraphicsRootDescriptorTable(root_param_index, gpu_handle);
                    break;
                default:
                    break;
                }
            }
        }

        auto GpuDeviceDXR::bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, uint32 set_idx, DescriptorSet* set, u32 dynamic_offset_count, u32* dynamic_offset)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_pipeline = static_cast<PipelineDXR*>(pipeline);
            auto dxr_set = static_cast<DescriptorSetDXR*>(set);
            if (!dxr_cb || !dxr_pipeline || !dxr_set)
                return;

            // Set descriptor heaps
            ID3D12DescriptorHeap* heaps[] = { cbv_srv_uav_heap.Get(), sampler_heap.Get() };
            dxr_cb->command_list->SetDescriptorHeaps(2, heaps);

            // Bind descriptor tables for each binding in the descriptor set
            for (const auto& [binding, gpu_handle] : dxr_set->gpu_handles)
            {
                // Use binding index as root parameter index
                // This assumes the root signature was created with matching parameter indices
                u32 root_param_index = binding;

                switch (dxr_pipeline->ty)
                {
                case GpuPipeline::PieplineType::Compute:
                case GpuPipeline::PieplineType::RayTracing:
                    dxr_cb->command_list->SetComputeRootDescriptorTable(root_param_index, gpu_handle);
                    break;
                case GpuPipeline::PieplineType::Raster:
                    dxr_cb->command_list->SetGraphicsRootDescriptorTable(root_param_index, gpu_handle);
                    break;
                default:
                    break;
                }
            }

            // Handle dynamic offsets for buffer bindings
            // In D3D12, dynamic offsets are typically handled through root constants or root descriptors
            // For now, we'll skip dynamic offset handling as it requires specific root signature setup
            if (dynamic_offset_count > 0)
            {
                // Dynamic offset handling would require:
                // 1. Root signature with root constant/descriptor for dynamic offsets
                // 2. SetComputeRoot32BitConstants or SetComputeRootConstantBufferView calls
                // For simplicity, we skip this for now
                DS_LOG_WARN("DXR backend: Dynamic offsets not fully implemented in bind_descriptor_set");
            }
        }

        auto GpuDeviceDXR::bind_pipeline(CommandBuffer* cb, GpuPipeline* pipeline)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_pipeline = static_cast<PipelineDXR*>(pipeline);
            if (!dxr_cb || !dxr_pipeline)
                return;

            if (dxr_pipeline->pipeline_state || dxr_pipeline->state_object)
            {
                switch (dxr_pipeline->ty)
                {
                case GpuPipeline::PieplineType::Compute:
                    dxr_cb->command_list->SetPipelineState(dxr_pipeline->pipeline_state.Get());
                    if (dxr_pipeline->root_signature && dxr_pipeline->root_signature->root_signature)
                        dxr_cb->command_list->SetComputeRootSignature(dxr_pipeline->root_signature->root_signature.Get());
                    break;
                case GpuPipeline::PieplineType::Raster:
                    dxr_cb->command_list->SetPipelineState(dxr_pipeline->pipeline_state.Get());
                    if (dxr_pipeline->root_signature && dxr_pipeline->root_signature->root_signature)
                        dxr_cb->command_list->SetGraphicsRootSignature(dxr_pipeline->root_signature->root_signature.Get());
                    break;
                case GpuPipeline::PieplineType::RayTracing:
                    // Ray tracing pipelines are handled separately in trace_rays
                    break;
                default:
                    break;
                }
            }
        }
        auto GpuDeviceDXR::push_constants(CommandBuffer* cb, GpuPipeline* pipeline, u32 offset, u8* constants, u32 size_) -> void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_pipeline = static_cast<PipelineDXR*>(pipeline);
            if (!dxr_cb || !dxr_pipeline)
                return;

            // Push constants are implemented as root constants in DXR
            if (dxr_pipeline->root_signature && dxr_pipeline->root_signature->root_signature)
            {
                u32 root_index = 0; // Adjust based on root signature layout
                dxr_cb->command_list->SetComputeRoot32BitConstants(
                    root_index,
                    size_ / 4,
                    constants,
                    offset / 4
                );
            }
        }
        auto GpuDeviceDXR::create_compute_pipeline(const CompiledShaderCode& spirv, const ComputePipelineDesc& desc)->std::shared_ptr<ComputePipeline>
        {
            auto pipeline = std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::Compute);

            if (spirv.codes.empty())
                return pipeline;

            // Create root signature
            std::unordered_map<u32, DescriptorInfo> descriptors;
            // TODO: Extract descriptors from shader bytecode
            pipeline->root_signature = std::make_shared<RootSignatureDXR>();
            pipeline->root_signature->root_signature = create_root_signature(device.Get(), descriptors, desc.push_constant_bytes);

            // Create compute pipeline state
            D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.pRootSignature = pipeline->root_signature->root_signature.Get();
            pso_desc.CS.pShaderBytecode = spirv.codes.data();
            pso_desc.CS.BytecodeLength = spirv.codes.size();
            pso_desc.NodeMask = 0;

            throw_if_failed(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline->pipeline_state)), "CreateComputePipelineState");

            // Set group size from shader reflection or desc
            pipeline->group_size = { 1, 1, 1 }; // TODO: Extract from shader

            return pipeline;
        }
        auto GpuDeviceDXR::create_raster_pipeline(const std::vector<PipelineShader>& shaders, const RasterPipelineDesc& desc)->std::shared_ptr<RasterPipeline>
        {
            auto pipeline = std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::Raster);

            // Create root signature
            std::unordered_map<u32, DescriptorInfo> all_descriptors;
            u32 push_constant_bytes = desc.push_constants_bytes;

            // Collect descriptors from all shaders
            for (const auto& shader : shaders)
            {
                // TODO: Extract descriptors from shader reflection
            }

            pipeline->root_signature = std::make_shared<RootSignatureDXR>();
            pipeline->root_signature->root_signature = create_root_signature(device.Get(), all_descriptors, push_constant_bytes);

            // Find VS and PS shaders
            D3D12_SHADER_BYTECODE vs_bytecode = {};
            D3D12_SHADER_BYTECODE ps_bytecode = {};

            for (const auto& shader : shaders)
            {
                switch (shader.desc.stage)
                {
                case ShaderPipelineStage::Vertex:
                    vs_bytecode.pShaderBytecode = shader.code.codes.data();
                    vs_bytecode.BytecodeLength = shader.code.codes.size();
                    break;
                case ShaderPipelineStage::Pixel:
                    ps_bytecode.pShaderBytecode = shader.code.codes.data();
                    ps_bytecode.BytecodeLength = shader.code.codes.size();
                    break;
                default:
                    break;
                }
            }

            // Create blend state
            D3D12_BLEND_DESC blend_desc = {};
            blend_desc.AlphaToCoverageEnable = FALSE;
            blend_desc.IndependentBlendEnable = FALSE;
            blend_desc.RenderTarget[0].BlendEnable = desc.blend_enabled;
            blend_desc.RenderTarget[0].SrcBlend = blend_mode_to_d3d12(desc.blend_mode);
            blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blend_desc.RenderTarget[0].LogicOpEnable = FALSE;
            blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            // Create rasterizer state
            D3D12_RASTERIZER_DESC rasterizer_desc = {};
            rasterizer_desc.FillMode = polygon_mode_to_d3d12(desc.polygon_mode);
            rasterizer_desc.CullMode = cull_mode_to_d3d12(desc.cull_mode);
            rasterizer_desc.FrontCounterClockwise = (desc.face_order == FrontFaceOrder::CCW);
            rasterizer_desc.DepthBias = desc.depth_bias_enabled ? 1 : 0;
            rasterizer_desc.DepthBiasClamp = 0.0f;
            rasterizer_desc.SlopeScaledDepthBias = desc.depth_bias_enabled ? 1.0f : 0.0f;
            rasterizer_desc.DepthClipEnable = TRUE;
            rasterizer_desc.MultisampleEnable = false;
            rasterizer_desc.AntialiasedLineEnable = FALSE;
            rasterizer_desc.ForcedSampleCount = 0;
            rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

            // Create depth stencil state
            D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
            depth_stencil_desc.DepthEnable = desc.depth_test;
            depth_stencil_desc.DepthWriteMask = desc.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            depth_stencil_desc.DepthFunc = compare_func_to_d3d12(desc.depth_compare_op);
            depth_stencil_desc.StencilEnable = FALSE;
            depth_stencil_desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
            depth_stencil_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
            depth_stencil_desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            depth_stencil_desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            depth_stencil_desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            depth_stencil_desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            depth_stencil_desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            depth_stencil_desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            depth_stencil_desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

            // Create pipeline state description
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.pRootSignature = pipeline->root_signature->root_signature.Get();
            pso_desc.VS = vs_bytecode;
            pso_desc.PS = ps_bytecode;
            pso_desc.BlendState = blend_desc;
            pso_desc.SampleMask = UINT_MAX;
            pso_desc.RasterizerState = rasterizer_desc;
            pso_desc.DepthStencilState = depth_stencil_desc;
            pso_desc.InputLayout = {}; // Empty for full-screen quads
            pso_desc.InputLayout.NumElements = 0;
            pso_desc.InputLayout.pInputElementDescs = nullptr;
            pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            // Determine render target format
            pso_desc.NumRenderTargets = 1;
            pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            pso_desc.SampleDesc.Count = 1;
            pso_desc.SampleDesc.Quality = 0;
            pso_desc.NodeMask = 0;
            pso_desc.CachedPSO = {};
            pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

            throw_if_failed(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline->pipeline_state)), "CreateGraphicsPipelineState");

            // Set group size to default for raster
            pipeline->group_size = { 1, 1, 1 };

            return pipeline;
        }
        auto GpuDeviceDXR::create_ray_tracing_pipeline(const std::vector<PipelineShader>& shaders, const RayTracingPipelineDesc& desc) -> std::shared_ptr<RayTracingPipeline>
        {
            auto pipeline = std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::RayTracing);

            if (!supports_dxr)
                return pipeline;

            // Create root signature for ray tracing
            std::unordered_map<u32, DescriptorInfo> all_descriptors;
            for (const auto& shader : shaders)
            {
                // Collect descriptors from all shaders
                // TODO: Extract descriptor info from shader reflection
            }
            pipeline->root_signature = std::make_shared<RootSignatureDXR>();
            pipeline->root_signature->root_signature = create_root_signature(device.Get(), all_descriptors, 0);

            // Build shader export description
            std::vector<D3D12_EXPORT_DESC> exports;
            std::vector<std::wstring> shader_names;

            u32 raygen_count = 0, miss_count = 0, hit_count = 0;

            for (const auto& shader : shaders)
            {
                std::wstring shader_name;
                std::string export_name;

                // Determine shader type and create export name
                switch (shader.desc.stage)
                {
                case ShaderPipelineStage::RayGen:
                    export_name = "RayGen" + std::to_string(raygen_count++);
                    break;
                case ShaderPipelineStage::RayMiss:
                    export_name = "Miss" + std::to_string(miss_count++);
                    break;
                case ShaderPipelineStage::RayClosestHit:
                case ShaderPipelineStage::RayAnyHit:
                    export_name = "Hit" + std::to_string(hit_count++);
                    break;
                default:
                    continue;
                }

                shader_name = std::wstring(export_name.begin(), export_name.end());
                shader_names.push_back(shader_name);

                D3D12_EXPORT_DESC export_desc = {};
                export_desc.Name = shader_names.back().c_str();
                export_desc.ExportToRename = nullptr;
                export_desc.Flags = D3D12_EXPORT_FLAG_NONE;
                exports.push_back(export_desc);
            }

            // Create ray tracing pipeline subobject
            struct PipelineSubObject
            {
                D3D12_RAYTRACING_SHADER_CONFIG shader_config = {};
                D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
                std::vector<D3D12_HIT_GROUP_DESC> hit_groups;
            };

            PipelineSubObject subobj = {};
            subobj.shader_config.MaxAttributeSizeInBytes = 32; // D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES
            subobj.shader_config.MaxPayloadSizeInBytes = 32; // Adjust based on needs
            subobj.pipeline_config.MaxTraceRecursionDepth = desc.max_pipeline_ray_recursion_depth;

            // Build shader library descriptions
            std::vector<D3D12_SHADER_BYTECODE> shader_bytecodes;
            for (const auto& shader : shaders)
            {
                D3D12_SHADER_BYTECODE bytecode = {};
                bytecode.pShaderBytecode = shader.code.codes.data();
                bytecode.BytecodeLength = shader.code.codes.size();
                shader_bytecodes.push_back(bytecode);
            }

            // Create DXIL library subobject
            D3D12_DXIL_LIBRARY_DESC dxil_lib = {};
            dxil_lib.DXILLibrary = shader_bytecodes.empty() ? D3D12_SHADER_BYTECODE{} : shader_bytecodes[0];
            dxil_lib.NumExports = static_cast<u32>(exports.size());
            dxil_lib.pExports = exports.data();

            // Build subobject array
            std::vector<D3D12_STATE_SUBOBJECT> subobjects;
            D3D12_STATE_SUBOBJECT shader_config_obj = {};
            shader_config_obj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
            shader_config_obj.pDesc = &subobj.shader_config;
            subobjects.push_back(shader_config_obj);

            D3D12_STATE_SUBOBJECT pipeline_config_obj = {};
            pipeline_config_obj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
            pipeline_config_obj.pDesc = &subobj.pipeline_config;
            subobjects.push_back(pipeline_config_obj);

            D3D12_STATE_SUBOBJECT dxil_lib_obj = {};
            dxil_lib_obj.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
            dxil_lib_obj.pDesc = &dxil_lib;
            subobjects.push_back(dxil_lib_obj);

            D3D12_STATE_OBJECT_DESC pipeline_desc = {};
            pipeline_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
            pipeline_desc.NumSubobjects = static_cast<u32>(subobjects.size());
            pipeline_desc.pSubobjects = subobjects.data();

            // Create ray tracing pipeline state object
            Microsoft::WRL::ComPtr<ID3D12StateObject> state_object;
            HRESULT hr = device->CreateStateObject(&pipeline_desc, IID_PPV_ARGS(&state_object));
            if (SUCCEEDED(hr))
            {
                pipeline->state_object = state_object;

                // Get shader identifiers from state object
                Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> state_object_props;
                if (SUCCEEDED(state_object->QueryInterface(IID_PPV_ARGS(&state_object_props))))
                {
                    // Collect shader identifiers
                    std::vector<std::array<u8, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES>> raygen_ids;
                    std::vector<std::array<u8, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES>> miss_ids;
                    std::vector<std::array<u8, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES>> hit_ids;

                    // Re-traverse shaders to match export names
                    u32 rg = 0, ms = 0, ht = 0;
                    for (const auto& shader : shaders)
                    {
                        std::string export_name;
                        std::wstring export_name_w;
                        switch (shader.desc.stage)
                        {
                        case ShaderPipelineStage::RayGen:
                            export_name = "RayGen" + std::to_string(rg++);
                            export_name_w = std::wstring(export_name.begin(), export_name.end());
                            {
                                void* shader_id_ptr = state_object_props->GetShaderIdentifier(export_name_w.c_str());
                                if (shader_id_ptr)
                                {
                                    std::array<u8, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES> id;
                                    std::memcpy(id.data(), shader_id_ptr, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                                    raygen_ids.push_back(id);
                                }
                                else
                                {
                                    DS_LOG_WARN("Shader identifier not found: {}", export_name);
                                    raygen_ids.push_back({});
                                }
                            }
                            break;
                        case ShaderPipelineStage::RayMiss:
                            export_name = "Miss" + std::to_string(ms++);
                            export_name_w = std::wstring(export_name.begin(), export_name.end());
                            {
                                void* shader_id_ptr = state_object_props->GetShaderIdentifier(export_name_w.c_str());
                                if (shader_id_ptr)
                                {
                                    std::array<u8, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES> id;
                                    std::memcpy(id.data(), shader_id_ptr, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                                    miss_ids.push_back(id);
                                }
                                else
                                {
                                    DS_LOG_WARN("Shader identifier not found: {}", export_name);
                                    miss_ids.push_back({});
                                }
                            }
                            break;
                        case ShaderPipelineStage::RayClosestHit:
                        case ShaderPipelineStage::RayAnyHit:
                            export_name = "Hit" + std::to_string(ht++);
                            export_name_w = std::wstring(export_name.begin(), export_name.end());
                            {
                                void* shader_id_ptr = state_object_props->GetShaderIdentifier(export_name_w.c_str());
                                if (shader_id_ptr)
                                {
                                    std::array<u8, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES> id;
                                    std::memcpy(id.data(), shader_id_ptr, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                                    hit_ids.push_back(id);
                                }
                                else
                                {
                                    DS_LOG_WARN("Shader identifier not found: {}", export_name);
                                    hit_ids.push_back({});
                                }
                            }
                            break;
                        }
                    }

                    // Store shader identifiers for later use in shader table population
                    // We'll use them after the shader table buffer is created and mapped
                    pipeline->shader_table.raygen_ids = raygen_ids;
                    pipeline->shader_table.miss_ids = miss_ids;
                    pipeline->shader_table.hit_ids = hit_ids;
                }
                else
                {
                    DS_LOG_ERROR("Failed to query state object properties for shader identifiers");
                }
            }
            else
            {
                DS_LOG_WARN("Failed to create ray tracing pipeline: HRESULT 0x{:08X}", static_cast<u32>(hr));
            }

            // Create shader table buffer
            const u64 shader_record_size = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
            const u64 raygen_record_size = shader_record_size;
            const u64 miss_record_size = shader_record_size;
            const u64 hit_record_size = shader_record_size;

            const u64 raygen_size = raygen_count * raygen_record_size;
            const u64 miss_size = miss_count * miss_record_size;
            const u64 hit_size = hit_count * hit_record_size;

            const u64 total_shader_table_size = raygen_size + miss_size + hit_size;

            if (total_shader_table_size > 0)
            {
                GpuBufferDesc shader_table_desc = GpuBufferDesc::new_gpu_only(total_shader_table_size, BufferUsageFlags::STORAGE_BUFFER);
                auto shader_table_buffer = create_buffer(shader_table_desc, "RayTracingShaderTable", nullptr);
                auto shader_table_dxr = std::dynamic_pointer_cast<GpuBufferDXR>(shader_table_buffer);

                if (shader_table_dxr && shader_table_dxr->resource)
                {
                    pipeline->shader_table.buffer = shader_table_dxr->resource;
                    pipeline->shader_table.raygen_record_size = raygen_record_size;
                    pipeline->shader_table.miss_record_size = miss_record_size;
                    pipeline->shader_table.hit_record_size = hit_record_size;
                    pipeline->shader_table.raygen_section_size = raygen_size;
                    pipeline->shader_table.miss_section_size = miss_size;
                    pipeline->shader_table.hit_section_size = hit_size;
                    pipeline->shader_table.miss_offset = raygen_size;
                    pipeline->shader_table.hit_offset = raygen_size + miss_size;

                    // Map and populate shader table with shader identifiers
                    D3D12_RANGE read_range = { 0, 0 };
                    shader_table_dxr->resource->Map(0, &read_range, reinterpret_cast<void**>(&pipeline->shader_table.mapped_data));

                    // Populate shader table with shader identifiers
                    if (pipeline->shader_table.mapped_data)
                    {
                        u8* shader_table_ptr = pipeline->shader_table.mapped_data;

                        // Fill RayGen records
                        for (const auto& id : pipeline->shader_table.raygen_ids)
                        {
                            std::memcpy(shader_table_ptr, id.data(), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                            shader_table_ptr += raygen_record_size;
                        }

                        // Fill Miss records
                        for (const auto& id : pipeline->shader_table.miss_ids)
                        {
                            std::memcpy(shader_table_ptr, id.data(), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                            shader_table_ptr += miss_record_size;
                        }

                        // Fill Hit records
                        for (const auto& id : pipeline->shader_table.hit_ids)
                        {
                            std::memcpy(shader_table_ptr, id.data(), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                            shader_table_ptr += hit_record_size;
                        }
                    }
                }
            }

            return pipeline;
        }
        auto GpuDeviceDXR::create_mesh_shader_pipeline(const std::vector<PipelineShader>& shaders, const MeshShaderPipelineDesc& desc) -> std::shared_ptr<MeshShaderPipeline>
        {
            auto pipeline = std::make_shared<PipelineDXR>(GpuPipeline::PieplineType::MeshShader);

            // Check mesh shader support
            D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))) ||
                options7.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED)
            {
                DS_LOG_WARN("Mesh shaders are not supported on this device");
                return pipeline;
            }

            DS_LOG_WARN("DXR backend: create_mesh_shader_pipeline not fully implemented - mesh shaders require D3D12_GRAPHICS_PIPELINE_STATE_DESC1");
            return pipeline;
        }
        auto GpuDeviceDXR::create_render_pass(const RenderPassDesc&, const char*) -> std::shared_ptr<RenderPass> { return std::make_shared<RenderPassDXR>(); }
        auto GpuDeviceDXR::begin_frame()->DeviceFrame* { active_frame = (active_frame + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT; return &frames[active_frame]; }
        auto GpuDeviceDXR::end_frame(DeviceFrame*)->void
        {
            std::lock_guard<std::mutex> lock(release_mutex);
            for (auto& release : destroy_queue)
                ++release.frame_counter;

            for (auto it = destroy_queue.begin(); it != destroy_queue.end();)
            {
                if (it->is_can_release())
                {
                    if (it->release)
                        it->release();
                    it = destroy_queue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        auto GpuDeviceDXR::begin_cmd(CommandBuffer* cb)->void { cb->begin(); }
        auto GpuDeviceDXR::end_cmd(CommandBuffer* cb)->void { cb->end(); }
        auto GpuDeviceDXR::submit_cmd(CommandBuffer* cb)->void { execute_cmd(cb); }
        auto GpuDeviceDXR::execute_cmd(CommandBuffer* cb)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                unsupported("execute_cmd for non-DXR command buffer");
            ID3D12CommandList* lists[] = { dxr_cb->command_list.Get() };
            graphics_queue->ExecuteCommandLists(1, lists);
            dxr_cb->fence_value++;
            throw_if_failed(graphics_queue->Signal(dxr_cb->fence.Get(), dxr_cb->fence_value), "ID3D12CommandQueue::Signal");
        }
        auto GpuDeviceDXR::record_image_barrier(CommandBuffer* cb, const ImageBarrier& barrier)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto texture_dx = static_cast<GpuTextureDXR*>(barrier.image);
            if (!dxr_cb || !texture_dx)
                return;

            D3D12_RESOURCE_BARRIER d3d_barrier = {};
            d3d_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            d3d_barrier.Transition.pResource = nullptr; // TODO: Get D3D12 resource from texture
            d3d_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            d3d_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            d3d_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            dxr_cb->command_list->ResourceBarrier(1, &d3d_barrier);
        }

        auto GpuDeviceDXR::record_buffer_barrier(CommandBuffer* cb, const BufferBarrier& barrier)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto buffer_dx = static_cast<GpuBufferDXR*>(barrier.buffer);
            if (!dxr_cb || !buffer_dx)
                return;

            D3D12_RESOURCE_BARRIER d3d_barrier = {};
            d3d_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            d3d_barrier.Transition.pResource = buffer_dx->resource.Get();
            d3d_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            d3d_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            d3d_barrier.Transition.Subresource = 0;

            dxr_cb->command_list->ResourceBarrier(1, &d3d_barrier);
        }

        auto GpuDeviceDXR::record_global_barrier(CommandBuffer* cb, const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            D3D12_RESOURCE_BARRIER d3d_barrier = {};
            d3d_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            d3d_barrier.UAV.pResource = nullptr;

            dxr_cb->command_list->ResourceBarrier(1, &d3d_barrier);
        }
        auto GpuDeviceDXR::dispatch(CommandBuffer* cb, const std::array<u32, 3>& group_dim, const std::array<u32, 3>& group_size)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            dxr_cb->command_list->Dispatch(group_dim[0], group_dim[1], group_dim[2]);
        }
        auto GpuDeviceDXR::dispatch_indirect(CommandBuffer* cb, GpuBuffer* args_buffer, u64 args_buffer_offset)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_buffer = static_cast<GpuBufferDXR*>(args_buffer);
            if (!dxr_cb || !dxr_buffer)
                return;

            // Create command signature for compute dispatch (cached per device)
            static Microsoft::WRL::ComPtr<ID3D12CommandSignature> compute_cmd_sig;
            if (!compute_cmd_sig)
            {
                D3D12_INDIRECT_ARGUMENT_DESC indirect_args = {};
                indirect_args.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

                D3D12_COMMAND_SIGNATURE_DESC cmd_sig_desc = {};
                cmd_sig_desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
                cmd_sig_desc.NumArgumentDescs = 1;
                cmd_sig_desc.pArgumentDescs = &indirect_args;
                cmd_sig_desc.NodeMask = 0;

                throw_if_failed(device->CreateCommandSignature(&cmd_sig_desc, nullptr, IID_PPV_ARGS(&compute_cmd_sig)), "CreateCommandSignature(compute)");
            }

            dxr_cb->command_list->ExecuteIndirect(compute_cmd_sig.Get(), 1, dxr_buffer->resource.Get(), args_buffer_offset, nullptr, 0);
        }
        auto GpuDeviceDXR::write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, rhi::GpuBuffer* buffer, u32 array_index)->void
        {
            auto dxr_set = static_cast<DescriptorSetDXR*>(descriptor_set);
            if (!dxr_set || !dxr_set->cpu_handles.count(dst_binding))
                return;

            auto dxr_buffer = static_cast<GpuBufferDXR*>(buffer);
            if (!dxr_buffer)
                return;

            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = dxr_set->cpu_handles[dst_binding];
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
            cbv_desc.BufferLocation = dxr_buffer->resource->GetGPUVirtualAddress();
            cbv_desc.SizeInBytes = static_cast<u32>(dxr_buffer->desc.size);
            device->CreateConstantBufferView(&cbv_desc, cpu_handle);
        }

        auto GpuDeviceDXR::write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, u32 array_index, const rhi::DescriptorImageInfo& img_info) -> void
        {
            // TODO: Implement image descriptor write
            // GpuTextureView doesn't have owner field in base RHI
            // Need to either add it or track texture differently
            DS_LOG_WARN("DXR backend: write_descriptor_set(image) not fully implemented - GpuTextureView::owner not available");
        }
        auto GpuDeviceDXR::clear_depth_stencil(CommandBuffer* cb, GpuTexture* texture, float depth, u32 stencil)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto texture_dx = static_cast<GpuTextureDXR*>(texture);
            if (!dxr_cb || !texture_dx || !texture_dx->resource)
                return;

            // Create temporary DSV descriptor
            D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle;
            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            heap_desc.NumDescriptors = 1;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heap_desc.NodeMask = 0;

            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap;
            if (SUCCEEDED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&dsv_heap))))
            {
                dsv_handle = dsv_heap->GetCPUDescriptorHandleForHeapStart();

                // Create DSV
                D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
                dsv_desc.Format = pixel_format_to_dxgi(texture->desc.format);
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;
                device->CreateDepthStencilView(texture_dx->resource.Get(), &dsv_desc, dsv_handle);

                // Clear
                dxr_cb->command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
            }
        }

        auto GpuDeviceDXR::clear_color(CommandBuffer* cb, GpuTexture* texture, const std::array<f32, 4>& color) -> void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto texture_dx = static_cast<GpuTextureDXR*>(texture);
            if (!dxr_cb || !texture_dx || !texture_dx->resource)
                return;

            // Create temporary RTV descriptor
            D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heap_desc.NumDescriptors = 1;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heap_desc.NodeMask = 0;

            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap;
            if (SUCCEEDED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap))))
            {
                rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();

                // Create RTV
                D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
                rtv_desc.Format = pixel_format_to_dxgi(texture->desc.format);
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice = 0;
                rtv_desc.Texture2D.PlaneSlice = 0;
                device->CreateRenderTargetView(texture_dx->resource.Get(), &rtv_desc, rtv_handle);

                // Clear
                dxr_cb->command_list->ClearRenderTargetView(rtv_handle, color.data(), 0, nullptr);
            }
        }
        auto GpuDeviceDXR::copy_image(GpuTexture* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void
        {
            auto src_dx = static_cast<GpuTextureDXR*>(src);
            auto dst_dx = static_cast<GpuTextureDXR*>(dst);
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd_buf);
            if (!src_dx || !dst_dx || !dxr_cb || !src_dx->resource || !dst_dx->resource)
                return;

            // Transition resources
            D3D12_RESOURCE_BARRIER barriers[2] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = src_dx->resource.Get();
            barriers[0].Transition.StateBefore = src_dx->current_state;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource = dst_dx->resource.Get();
            barriers[1].Transition.StateBefore = dst_dx->current_state;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            dxr_cb->command_list->ResourceBarrier(2, barriers);

            // Copy texture
            D3D12_TEXTURE_COPY_LOCATION src_location = {};
            src_location.pResource = src_dx->resource.Get();
            src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src_location.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION dst_location = {};
            dst_location.pResource = dst_dx->resource.Get();
            dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst_location.SubresourceIndex = 0;

            dxr_cb->command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

            // Transition back
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.StateAfter = src_dx->current_state;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.StateAfter = dst_dx->current_state;
            dxr_cb->command_list->ResourceBarrier(2, barriers);
        }

        auto GpuDeviceDXR::copy_image(GpuBuffer* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void
        {
            auto src_dx = static_cast<GpuBufferDXR*>(src);
            auto dst_dx = static_cast<GpuTextureDXR*>(dst);
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd_buf);
            if (!src_dx || !dst_dx || !dxr_cb || !src_dx->resource || !dst_dx->resource)
                return;

            // Transition destination
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = dst_dx->resource.Get();
            barrier.Transition.StateBefore = dst_dx->current_state;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.Subresource = 0;

            dxr_cb->command_list->ResourceBarrier(1, &barrier);

            // Get buffer footprint
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_footprint = {};
            UINT row_count = 0;
            UINT64 row_size = 0;
            D3D12_RESOURCE_DESC resource_desc = dst_dx->resource->GetDesc();
            device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &placed_footprint, &row_count, &row_size, static_cast<UINT64*>(nullptr));

            // Copy buffer to texture
            D3D12_TEXTURE_COPY_LOCATION src_location = {};
            src_location.pResource = src_dx->resource.Get();
            src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src_location.PlacedFootprint = placed_footprint;

            D3D12_TEXTURE_COPY_LOCATION dst_location = {};
            dst_location.pResource = dst_dx->resource.Get();
            dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst_location.SubresourceIndex = 0;

            dxr_cb->command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

            // Transition back
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = dst_dx->current_state;
            dxr_cb->command_list->ResourceBarrier(1, &barrier);
        }

        auto GpuDeviceDXR::update_texture(GpuTexture* src, const std::vector<ImageSubData>& update_data, const TextureRegion& tex_region)->void
        {
            auto src_dx = static_cast<GpuTextureDXR*>(src);
            if (!src_dx || !src_dx->resource || update_data.empty())
                return;

            // Calculate upload buffer size
            D3D12_RESOURCE_DESC resource_desc = src_dx->resource->GetDesc();
            UINT64 upload_buffer_size = 0;
            device->GetCopyableFootprints(&resource_desc, 0, 1, 0, nullptr, nullptr, nullptr, &upload_buffer_size);

            // Create upload heap
            D3D12_HEAP_PROPERTIES upload_heap_props = {};
            upload_heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
            upload_heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            upload_heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC upload_desc = {};
            upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            upload_desc.Width = upload_buffer_size;
            upload_desc.Height = 1;
            upload_desc.DepthOrArraySize = 1;
            upload_desc.MipLevels = 1;
            upload_desc.Format = DXGI_FORMAT_UNKNOWN;
            upload_desc.SampleDesc.Count = 1;
            upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            Microsoft::WRL::ComPtr<ID3D12Resource> upload_resource;
            throw_if_failed(device->CreateCommittedResource(
                &upload_heap_props,
                D3D12_HEAP_FLAG_NONE,
                &upload_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&upload_resource)),
                "CreateCommittedResource(upload texture update)");

            // Map and copy data
            u8* upload_data = nullptr;
            throw_if_failed(upload_resource->Map(0, nullptr, reinterpret_cast<void**>(&upload_data)), "Map upload texture update");

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_footprint = {};
            UINT row_count = 0;
            UINT64 row_size_in_bytes = 0;
            device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &placed_footprint, &row_count, &row_size_in_bytes, nullptr);

            for (size_t i = 0; i < update_data.size(); ++i)
            {
                const auto& sub_data = update_data[i];
                u8* dst = upload_data + placed_footprint.Offset;
                const u8* src_ptr = sub_data.data;

                for (u32 row = 0; row < row_count; ++row)
                {
                    u64 copy_size = std::min<u64>(sub_data.row_pitch, row_size_in_bytes);
                    std::memcpy(dst + row * placed_footprint.Footprint.RowPitch,
                               src_ptr + row * sub_data.row_pitch,
                               copy_size);
                }
            }

            upload_resource->Unmap(0, nullptr);

            // Upload using setup command buffer
            with_setup_cb([this, &src_dx, &upload_resource, &placed_footprint, &tex_region](CommandBuffer* cmd) {
                auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd);
                if (!dxr_cb)
                    return;

                // Transition to copy dest
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = src_dx->resource.Get();
                barrier.Transition.StateBefore = src_dx->current_state;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.Subresource = 0;
                dxr_cb->command_list->ResourceBarrier(1, &barrier);

                // Copy
                D3D12_TEXTURE_COPY_LOCATION dst = {};
                dst.pResource = src_dx->resource.Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = 0;

                D3D12_TEXTURE_COPY_LOCATION src_loc = {};
                src_loc.pResource = upload_resource.Get();
                src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src_loc.PlacedFootprint = placed_footprint;

                D3D12_BOX src_box = {};
                src_box.left = tex_region.image_offset[0];
                src_box.top = tex_region.image_offset[1];
                src_box.front = tex_region.image_offset[2];
                src_box.right = tex_region.image_offset[0] + tex_region.image_extent[0];
                src_box.bottom = tex_region.image_offset[1] + tex_region.image_extent[1];
                src_box.back = tex_region.image_offset[2] + tex_region.image_extent[2];

                dxr_cb->command_list->CopyTextureRegion(&dst, tex_region.image_offset[0], tex_region.image_offset[1], tex_region.image_offset[2], &src_loc, &src_box);

                // Transition back
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = src_dx->current_state;
                dxr_cb->command_list->ResourceBarrier(1, &barrier);
            });
        }
        auto GpuDeviceDXR::set_point_size(CommandBuffer*, float)->void {}
        auto GpuDeviceDXR::set_line_width(CommandBuffer*, float)->void {}
        auto GpuDeviceDXR::set_viewport(CommandBuffer* cb, const ViewPort& view, const Scissor& scissors)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            D3D12_VIEWPORT d3d_viewport = {};
            d3d_viewport.TopLeftX = view.x;
            d3d_viewport.TopLeftY = view.y;
            d3d_viewport.Width = view.width;
            d3d_viewport.Height = view.height;
            d3d_viewport.MinDepth = view.min_depth;
            d3d_viewport.MaxDepth = view.max_depth;

            D3D12_RECT d3d_scissor = {};
            d3d_scissor.left = scissors.offset[0];
            d3d_scissor.top = scissors.offset[1];
            d3d_scissor.right = scissors.offset[0] + static_cast<LONG>(scissors.extent[0]);
            d3d_scissor.bottom = scissors.offset[1] + static_cast<LONG>(scissors.extent[1]);

            dxr_cb->command_list->RSSetViewports(1, &d3d_viewport);
            dxr_cb->command_list->RSSetScissorRects(1, &d3d_scissor);
        }
        auto GpuDeviceDXR::begin_render_pass(CommandBuffer* cb, const std::array<u32, 2>& dims, RenderPass* render_pass, const std::vector<rhi::GpuTexture*>& color_desc, rhi::GpuTexture* depth_desc)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handles;
            std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> rtv_heaps;

            // Transition color attachments and create RTVs
            for (auto* color_tex : color_desc)
            {
                auto texture_dx = static_cast<GpuTextureDXR*>(color_tex);
                if (!texture_dx || !texture_dx->resource)
                    continue;

                // Transition to render target state
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = texture_dx->resource.Get();
                barrier.Transition.StateBefore = texture_dx->current_state;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.Subresource = 0;
                dxr_cb->command_list->ResourceBarrier(1, &barrier);
                texture_dx->current_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

                // Create temporary RTV
                D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
                heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                heap_desc.NumDescriptors = 1;
                heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                heap_desc.NodeMask = 0;

                Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap;
                if (SUCCEEDED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap))))
                {
                    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();

                    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
                    rtv_desc.Format = pixel_format_to_dxgi(texture_dx->desc.format);
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                    rtv_desc.Texture2D.MipSlice = 0;
                    device->CreateRenderTargetView(texture_dx->resource.Get(), &rtv_desc, rtv_handle);

                    rtv_handles.push_back(rtv_handle);
                    rtv_heaps.push_back(rtv_heap);
                }
            }

            // Transition depth attachment and create DSV
            D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap;

            if (depth_desc)
            {
                auto texture_dx = static_cast<GpuTextureDXR*>(depth_desc);
                if (texture_dx && texture_dx->resource)
                {
                    // Transition to depth write state
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = texture_dx->resource.Get();
                    barrier.Transition.StateBefore = texture_dx->current_state;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                    barrier.Transition.Subresource = 0;
                    dxr_cb->command_list->ResourceBarrier(1, &barrier);
                    texture_dx->current_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

                    // Create temporary DSV
                    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
                    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                    heap_desc.NumDescriptors = 1;
                    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                    heap_desc.NodeMask = 0;

                    if (SUCCEEDED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&dsv_heap))))
                    {
                        dsv_handle = dsv_heap->GetCPUDescriptorHandleForHeapStart();

                        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
                        dsv_desc.Format = pixel_format_to_dxgi(texture_dx->desc.format);
                        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                        dsv_desc.Texture2D.MipSlice = 0;
                        device->CreateDepthStencilView(texture_dx->resource.Get(), &dsv_desc, dsv_handle);
                    }
                }
            }

            // Set render targets
            if (!rtv_handles.empty())
            {
                dxr_cb->command_list->OMSetRenderTargets(
                    static_cast<u32>(rtv_handles.size()),
                    rtv_handles.data(),
                    FALSE,
                    dsv_heap.Get() ? &dsv_handle : nullptr);
            }

            // Set viewport
            D3D12_VIEWPORT viewport = {};
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<float>(dims[0]);
            viewport.Height = static_cast<float>(dims[1]);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            dxr_cb->command_list->RSSetViewports(1, &viewport);

            // Set scissor
            D3D12_RECT scissor = {};
            scissor.left = 0;
            scissor.top = 0;
            scissor.right = static_cast<LONG>(dims[0]);
            scissor.bottom = static_cast<LONG>(dims[1]);
            dxr_cb->command_list->RSSetScissorRects(1, &scissor);
        }

        auto GpuDeviceDXR::end_render_pass(CommandBuffer* cb)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            // Note: Actual resource state transitions will happen when textures are used again
            // This is a simplified implementation - in production you'd track and restore states
        }
        auto GpuDeviceDXR::create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc& desc) -> std::shared_ptr<GpuRayTracingAcceleration>
        {
            if (!supports_dxr)
                throw std::runtime_error("DXR is not supported on this device");

            // Build geometry descriptions for D3D12
            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs;

            for (const auto& geo : desc.geometries)
            {
                if (geo.geometry_type != RayTracingGeometryType::Triangle)
                {
                    DS_LOG_WARN("Non-triangle geometry types not yet supported in DXR backend");
                    continue;
                }

                D3D12_RAYTRACING_GEOMETRY_DESC geo_desc = {};
                geo_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geo_desc.Triangles.Transform3x4 = 0; // No transform

                // Set vertex format
                switch (geo.vertex_format)
                {
                case PixelFormat::R32G32B32_Float:
                case PixelFormat::R32G32B32A32_Float:
                    geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
                    break;
                case PixelFormat::R16G16B16A16_Float:
                    geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
                    break;
                default:
                    geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
                    break;
                }

                geo_desc.Triangles.VertexCount = static_cast<u32>(geo.parts.empty() ? 0 : geo.parts[0].vertex_count);
                geo_desc.Triangles.VertexBuffer.StartAddress = geo.parts.empty() ? 0 : geo.parts[0].vertex_buffer_address;
                geo_desc.Triangles.VertexBuffer.StrideInBytes = geo.vertex_stride;

                // Index data
                if (!geo.parts.empty() && geo.parts[0].index_count > 0)
                {
                    geo_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT; // Assume 32-bit indices
                    geo_desc.Triangles.IndexCount = static_cast<u32>(geo.parts[0].index_count);
                    geo_desc.Triangles.IndexBuffer = geo.parts[0].index_buffer_address;
                }
                else
                {
                    geo_desc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
                    geo_desc.Triangles.IndexCount = 0;
                    geo_desc.Triangles.IndexBuffer = 0;
                }

                geo_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
                geometry_descs.push_back(geo_desc);
            }

            if (geometry_descs.empty())
                return std::make_shared<GpuRayTracingAccelerationDXR>(nullptr);

            // Get required sizes for acceleration structure
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
            build_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            build_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            build_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            build_inputs.NumDescs = static_cast<u32>(geometry_descs.size());
            build_inputs.pGeometryDescs = geometry_descs.data();

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
            device->GetRaytracingAccelerationStructurePrebuildInfo(&build_inputs, &prebuild_info);

            // Allocate acceleration structure buffer
            GpuBufferDesc as_buffer_desc = GpuBufferDesc::new_gpu_only(
                prebuild_info.ResultDataMaxSizeInBytes,
                BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR);

            auto as_buffer = std::dynamic_pointer_cast<GpuBufferDXR>(
                create_buffer(as_buffer_desc, "BottomLevelAccelerationStructure", nullptr));

            if (!as_buffer || !as_buffer->resource)
                throw std::runtime_error("Failed to create bottom-level acceleration structure buffer");

            // Allocate scratch buffer
            GpuBufferDesc scratch_desc = GpuBufferDesc::new_gpu_only(
                prebuild_info.ScratchDataSizeInBytes,
                BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::SHADER_DEVICE_ADDRESS);

            auto scratch_buffer = std::dynamic_pointer_cast<GpuBufferDXR>(
                create_buffer(scratch_desc, "BottomLevelAccelerationStructureScratch", nullptr));

            if (!scratch_buffer || !scratch_buffer->resource)
                throw std::runtime_error("Failed to create bottom-level acceleration structure scratch buffer");

            // Build acceleration structure on setup command buffer
            with_setup_cb([this, &build_inputs, &as_buffer, &scratch_buffer, &prebuild_info](CommandBuffer* cmd) {
                auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd);
                if (!dxr_cb)
                    return;

                // Get raytracing command list
                Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> rt_cmd_list;
                if (FAILED(dxr_cb->command_list->QueryInterface(IID_PPV_ARGS(&rt_cmd_list))))
                {
                    DS_LOG_ERROR("Failed to query raytracing command list for BLAS build");
                    return;
                }

                D3D12_GPU_VIRTUAL_ADDRESS scratch_addr = scratch_buffer->resource->GetGPUVirtualAddress();

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
                build_desc.Inputs = build_inputs;
                build_desc.ScratchAccelerationStructureData = scratch_addr;
                build_desc.DestAccelerationStructureData = as_buffer->resource->GetGPUVirtualAddress();
                build_desc.SourceAccelerationStructureData = 0;
                // UAV barrier not needed for single AS build
                rt_cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);
            });

            auto result = std::make_shared<GpuRayTracingAccelerationDXR>(as_buffer);
            result->acceleration_structure = as_buffer->resource;
            result->acceleration_structure_size = prebuild_info.ResultDataMaxSizeInBytes;
            result->is_top_level = false;

            return result;
        }
        auto GpuDeviceDXR::create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc& desc, const RayTracingAccelerationScratchBuffer& scratch_buffer) -> std::shared_ptr<GpuRayTracingAcceleration>
        {
            if (!supports_dxr)
                throw std::runtime_error("DXR is not supported on this device");

            if (desc.instances.empty())
                return std::make_shared<GpuRayTracingAccelerationDXR>(nullptr);

            // Calculate required size for instance descriptions
            const u64 instance_desc_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
            const u64 instance_buffer_size = desc.instances.size() * instance_desc_size;

            // Create instance buffer
            GpuBufferDesc instance_buffer_desc = GpuBufferDesc::new_cpu_to_gpu(
                instance_buffer_size,
                BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::SHADER_DEVICE_ADDRESS);

            auto instance_buffer = std::dynamic_pointer_cast<GpuBufferDXR>(
                create_buffer(instance_buffer_desc, "TLASInstanceBuffer", nullptr));

            if (!instance_buffer || !instance_buffer->resource)
                throw std::runtime_error("Failed to create TLAS instance buffer");

            // Fill instance descriptions
            std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instance_descs;

            for (const auto& instance : desc.instances)
            {
                auto blas_dx = static_cast<GpuRayTracingAccelerationDXR*>(instance.blas.get());
                if (!blas_dx)
                    continue;

                D3D12_RAYTRACING_INSTANCE_DESC inst_desc = {};
                inst_desc.Transform[0][0] = instance.transformation[0][0];
                inst_desc.Transform[0][1] = instance.transformation[0][1];
                inst_desc.Transform[0][2] = instance.transformation[0][2];
                inst_desc.Transform[0][3] = instance.transformation[0][3];
                inst_desc.Transform[1][0] = instance.transformation[1][0];
                inst_desc.Transform[1][1] = instance.transformation[1][1];
                inst_desc.Transform[1][2] = instance.transformation[1][2];
                inst_desc.Transform[1][3] = instance.transformation[1][3];
                inst_desc.Transform[2][0] = instance.transformation[2][0];
                inst_desc.Transform[2][1] = instance.transformation[2][1];
                inst_desc.Transform[2][2] = instance.transformation[2][2];
                inst_desc.Transform[2][3] = instance.transformation[2][3];
                inst_desc.InstanceID = instance.mesh_index;
                inst_desc.InstanceMask = instance.mask;
                inst_desc.InstanceContributionToHitGroupIndex = 0;
                inst_desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;

                inst_desc.AccelerationStructure = blas_dx->acceleration_structure->GetGPUVirtualAddress();
                instance_descs.push_back(inst_desc);
            }

            // Copy instance descriptions to buffer
            auto dst = instance_buffer->map(this);
            std::memcpy(dst, instance_descs.data(), instance_buffer_size);
            instance_buffer->unmap(this);

            // Get required sizes for TLAS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
            build_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            build_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            build_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            build_inputs.NumDescs = static_cast<u32>(desc.instances.size());
            build_inputs.InstanceDescs = instance_buffer->resource->GetGPUVirtualAddress();

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
            device->GetRaytracingAccelerationStructurePrebuildInfo(&build_inputs, &prebuild_info);

            // Use preallocated size if specified, otherwise use calculated size
            u64 as_size = desc.preallocate_bytes > 0 ? desc.preallocate_bytes : prebuild_info.ResultDataMaxSizeInBytes;

            // Allocate acceleration structure buffer
            GpuBufferDesc as_buffer_desc = GpuBufferDesc::new_gpu_only(
                as_size,
                BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR);

            auto as_buffer = std::dynamic_pointer_cast<GpuBufferDXR>(
                create_buffer(as_buffer_desc, "TopLevelAccelerationStructure", nullptr));

            if (!as_buffer || !as_buffer->resource)
                throw std::runtime_error("Failed to create top-level acceleration structure buffer");

            // Check if scratch buffer is provided and is large enough
            Microsoft::WRL::ComPtr<ID3D12Resource> scratch_resource;
            u64 scratch_size = prebuild_info.ScratchDataSizeInBytes;

            if (scratch_buffer.buffer)
            {
                auto scratch_dx = std::dynamic_pointer_cast<GpuBufferDXR>(scratch_buffer.buffer);
                if (scratch_dx && scratch_dx->resource)
                {
                    if (scratch_dx->desc.size >= scratch_size)
                    {
                        scratch_resource = scratch_dx->resource;
                    }
                }
            }

            // Allocate scratch buffer if not provided or too small
            if (!scratch_resource)
            {
                GpuBufferDesc scratch_desc = GpuBufferDesc::new_gpu_only(
                    scratch_size,
                    BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::SHADER_DEVICE_ADDRESS);

                auto temp_scratch = std::dynamic_pointer_cast<GpuBufferDXR>(
                    create_buffer(scratch_desc, "TopLevelAccelerationStructureScratch", nullptr));

                if (temp_scratch && temp_scratch->resource)
                    scratch_resource = temp_scratch->resource;
            }

            if (!scratch_resource)
                throw std::runtime_error("Failed to create scratch buffer for TLAS build");

            // Build acceleration structure on setup command buffer
            with_setup_cb([this, &build_inputs, &as_buffer, &scratch_resource, &prebuild_info](CommandBuffer* cmd) {
                auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd);
                if (!dxr_cb)
                    return;

                // Get raytracing command list
                Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> rt_cmd_list;
                if (FAILED(dxr_cb->command_list->QueryInterface(IID_PPV_ARGS(&rt_cmd_list))))
                {
                    DS_LOG_ERROR("Failed to query raytracing command list for TLAS build");
                    return;
                }

                D3D12_GPU_VIRTUAL_ADDRESS scratch_addr = scratch_resource->GetGPUVirtualAddress();

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
                build_desc.Inputs = build_inputs;
                build_desc.ScratchAccelerationStructureData = scratch_addr;
                build_desc.DestAccelerationStructureData = as_buffer->resource->GetGPUVirtualAddress();
                build_desc.SourceAccelerationStructureData = 0;
                rt_cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);
            });

            auto result = std::make_shared<GpuRayTracingAccelerationDXR>(as_buffer);
            result->acceleration_structure = as_buffer->resource;
            result->acceleration_structure_size = as_size;
            result->is_top_level = true;

            return result;
        }
        auto GpuDeviceDXR::rebuild_ray_tracing_top_acceleration(CommandBuffer* cb, u64 instance_buffer_address, u64 instance_count, GpuRayTracingAcceleration* tlas, RayTracingAccelerationScratchBuffer* scratch_buffer)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto tlas_dx = static_cast<GpuRayTracingAccelerationDXR*>(tlas);
            if (!dxr_cb || !tlas_dx || !supports_dxr)
                return;

            // Get raytracing command list
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> rt_cmd_list;
            if (FAILED(dxr_cb->command_list->QueryInterface(IID_PPV_ARGS(&rt_cmd_list))))
            {
                DS_LOG_ERROR("Failed to query raytracing command list for TLAS rebuild");
                return;
            }

            // Get scratch buffer
            Microsoft::WRL::ComPtr<ID3D12Resource> scratch_resource;
            if (scratch_buffer && scratch_buffer->buffer)
            {
                auto scratch_dx = std::dynamic_pointer_cast<GpuBufferDXR>(scratch_buffer->buffer);
                if (scratch_dx && scratch_dx->resource)
                    scratch_resource = scratch_dx->resource;
            }

            if (!scratch_resource)
            {
                DS_LOG_ERROR("Scratch buffer not provided for TLAS rebuild");
                return;
            }

            // Setup build inputs
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
            build_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            build_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            build_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            build_inputs.NumDescs = static_cast<u32>(instance_count);
            build_inputs.InstanceDescs = instance_buffer_address;

            // Build acceleration structure
            D3D12_GPU_VIRTUAL_ADDRESS scratch_addr = scratch_resource->GetGPUVirtualAddress();

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
            build_desc.Inputs = build_inputs;
            build_desc.ScratchAccelerationStructureData = scratch_addr;
            build_desc.DestAccelerationStructureData = tlas_dx->acceleration_structure->GetGPUVirtualAddress();
            build_desc.SourceAccelerationStructureData = tlas_dx->acceleration_structure->GetGPUVirtualAddress();
            rt_cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);
        }
        auto GpuDeviceDXR::bind_vertex_buffers(CommandBuffer* cb, const GpuBuffer* const* vertexBuffers, uint32_t slot, uint32_t count, const uint32_t* strides, const uint64_t* offsets)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb || !vertexBuffers)
                return;

            std::vector<D3D12_VERTEX_BUFFER_VIEW> vb_views(count);
            for (u32 i = 0; i < count; ++i)
            {
                auto buffer_dx = static_cast<const GpuBufferDXR*>(vertexBuffers[i]);
                if (buffer_dx && buffer_dx->resource)
                {
                    vb_views[i].BufferLocation = buffer_dx->resource->GetGPUVirtualAddress() + (offsets ? offsets[i] : 0);
                    vb_views[i].SizeInBytes = static_cast<u32>(buffer_dx->desc.size);
                    vb_views[i].StrideInBytes = strides ? strides[i] : 0;
                }
            }

            dxr_cb->command_list->IASetVertexBuffers(slot, count, vb_views.data());
        }

        auto GpuDeviceDXR::bind_index_buffer(CommandBuffer* cb, const GpuBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto buffer_dx = static_cast<const GpuBufferDXR*>(indexBuffer);
            if (!dxr_cb || !buffer_dx || !buffer_dx->resource)
                return;

            D3D12_INDEX_BUFFER_VIEW ib_view = {};
            ib_view.BufferLocation = buffer_dx->resource->GetGPUVirtualAddress() + offset;
            ib_view.SizeInBytes = static_cast<u32>(buffer_dx->desc.size);
            ib_view.Format = (format == IndexBufferFormat::UINT16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

            dxr_cb->command_list->IASetIndexBuffer(&ib_view);
        }
        auto GpuDeviceDXR::draw(CommandBuffer* cb, uint32_t vertexCount, uint32_t startVertexLocation) ->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            dxr_cb->command_list->DrawInstanced(vertexCount, 1, startVertexLocation, 0);
        }

        auto GpuDeviceDXR::draw_indexed(CommandBuffer* cb, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            dxr_cb->command_list->DrawIndexedInstanced(indexCount, 1, startIndexLocation, baseVertexLocation, 0);
        }

        auto GpuDeviceDXR::draw_instanced(CommandBuffer* cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            dxr_cb->command_list->DrawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
        }

        auto GpuDeviceDXR::draw_indexed_instanced(CommandBuffer* cb, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            dxr_cb->command_list->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
        }
        auto GpuDeviceDXR::draw_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto args_dx = static_cast<const GpuBufferDXR*>(args);
            if (!dxr_cb || !args_dx || !args_dx->resource)
                return;

            // Create command signature for indirect draw (should be cached)
            D3D12_INDIRECT_ARGUMENT_DESC indirect_args = {};
            indirect_args.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

            D3D12_COMMAND_SIGNATURE_DESC cmd_sig_desc = {};
            cmd_sig_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
            cmd_sig_desc.NumArgumentDescs = 1;
            cmd_sig_desc.pArgumentDescs = &indirect_args;
            cmd_sig_desc.NodeMask = 0;

            Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmd_sig;
            throw_if_failed(device->CreateCommandSignature(&cmd_sig_desc, nullptr, IID_PPV_ARGS(&cmd_sig)), "CreateCommandSignature(draw)");

            dxr_cb->command_list->ExecuteIndirect(cmd_sig.Get(), 1, args_dx->resource.Get(), args_offset, nullptr, 0);
        }

        auto GpuDeviceDXR::draw_indexed_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto args_dx = static_cast<const GpuBufferDXR*>(args);
            if (!dxr_cb || !args_dx || !args_dx->resource)
                return;

            // Create command signature for indirect indexed draw (should be cached)
            D3D12_INDIRECT_ARGUMENT_DESC indirect_args = {};
            indirect_args.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

            D3D12_COMMAND_SIGNATURE_DESC cmd_sig_desc = {};
            cmd_sig_desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
            cmd_sig_desc.NumArgumentDescs = 1;
            cmd_sig_desc.pArgumentDescs = &indirect_args;
            cmd_sig_desc.NodeMask = 0;

            Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmd_sig;
            throw_if_failed(device->CreateCommandSignature(&cmd_sig_desc, nullptr, IID_PPV_ARGS(&cmd_sig)), "CreateCommandSignature(draw_indexed)");

            dxr_cb->command_list->ExecuteIndirect(cmd_sig.Get(), 1, args_dx->resource.Get(), args_offset, nullptr, 0);
        }

        auto GpuDeviceDXR::draw_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto args_dx = static_cast<const GpuBufferDXR*>(args);
            auto count_dx = static_cast<const GpuBufferDXR*>(count);
            if (!dxr_cb || !args_dx || !count_dx)
                return;

            // D3D12 doesn't have direct indirect count support like Vulkan
            // This would need to be implemented differently or emulated
            DS_LOG_WARN("DXR backend: draw_instanced_indirect_count not fully implemented");
        }

        auto GpuDeviceDXR::draw_indexed_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto args_dx = static_cast<const GpuBufferDXR*>(args);
            auto count_dx = static_cast<const GpuBufferDXR*>(count);
            if (!dxr_cb || !args_dx || !count_dx)
                return;

            // D3D12 doesn't have direct indirect count support like Vulkan
            DS_LOG_WARN("DXR backend: draw_indexed_instanced_indirect_count not fully implemented");
        }
        auto GpuDeviceDXR::draw_mesh_tasks(CommandBuffer* cb, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            // D3D12 doesn't have a direct DrawMeshTasks equivalent
            // Mesh shaders are invoked via DrawInstanced with specific vertex counts
            // This is a simplified implementation
            DS_LOG_WARN("DXR backend: draw_mesh_tasks implementation is simplified");
            dxr_cb->command_list->DrawInstanced(3 * group_count_x * group_count_y * group_count_z, 1, 0, 0);
        }

        auto GpuDeviceDXR::draw_mesh_tasks_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, uint32_t draw_count, uint32_t stride)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto args_dx = static_cast<const GpuBufferDXR*>(args);
            if (!dxr_cb || !args_dx)
                return;

            // Mesh shader indirect drawing would require specific command signature
            DS_LOG_WARN("DXR backend: draw_mesh_tasks_indirect not fully implemented");
        }

        auto GpuDeviceDXR::draw_mesh_tasks_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count, uint32_t stride)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            if (!dxr_cb)
                return;

            DS_LOG_WARN("DXR backend: draw_mesh_tasks_indirect_count not fully implemented");
        }
        auto GpuDeviceDXR::with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback)->void { setup_cb->begin(); callback(setup_cb.get()); setup_cb->end(); execute_cmd(setup_cb.get()); setup_cb->wait(); }
        auto GpuDeviceDXR::copy_buffer(CommandBuffer* cmd, GpuBuffer* src, u64 src_offset, GpuBuffer* dst, u64 dst_offset, u64 size_)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd);
            auto src_dx = static_cast<GpuBufferDXR*>(src);
            auto dst_dx = static_cast<GpuBufferDXR*>(dst);
            if (!dxr_cb || !src_dx || !dst_dx)
                return;

            D3D12_RESOURCE_BARRIER barriers[2] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = src_dx->resource.Get();
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.Subresource = 0;

            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource = dst_dx->resource.Get();
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.Subresource = 0;

            dxr_cb->command_list->ResourceBarrier(2, barriers);

            dxr_cb->command_list->CopyBufferRegion(
                dst_dx->resource.Get(),
                dst_offset,
                src_dx->resource.Get(),
                src_offset,
                size_
            );

            // Transition back to common state
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            dxr_cb->command_list->ResourceBarrier(2, barriers);
        }
        auto GpuDeviceDXR::trace_rays(CommandBuffer* cb, RayTracingPipeline* rtpipeline, const std::array<u32, 3>& threads)->void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_pipeline = static_cast<PipelineDXR*>(rtpipeline);
            if (!dxr_cb || !dxr_pipeline || !supports_dxr)
                return;

            // Get raytracing command list (native DXR)
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> rt_cmd_list;
            if (FAILED(dxr_cb->command_list->QueryInterface(IID_PPV_ARGS(&rt_cmd_list))))
            {
                DS_LOG_ERROR("Failed to query raytracing command list4");
                return;
            }

            // Unmap shader table if mapped
            if (dxr_pipeline->shader_table.mapped_data && dxr_pipeline->shader_table.buffer)
            {
                dxr_pipeline->shader_table.buffer->Unmap(0, nullptr);
                dxr_pipeline->shader_table.mapped_data = nullptr;
            }

            // Get GPU addresses for shader table regions
            D3D12_GPU_VIRTUAL_ADDRESS raygen_addr = dxr_pipeline->shader_table.buffer->GetGPUVirtualAddress();
            D3D12_GPU_VIRTUAL_ADDRESS miss_addr = raygen_addr + dxr_pipeline->shader_table.miss_offset;
            D3D12_GPU_VIRTUAL_ADDRESS hit_addr = raygen_addr + dxr_pipeline->shader_table.hit_offset;
            D3D12_GPU_VIRTUAL_ADDRESS callable_addr = 0;

            // Set pipeline state and dispatch rays
            if (!dxr_pipeline->state_object)
                return;

            rt_cmd_list->SetPipelineState1(dxr_pipeline->state_object.Get());

            D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};
            dispatch_desc.RayGenerationShaderRecord.StartAddress = raygen_addr;
            dispatch_desc.RayGenerationShaderRecord.SizeInBytes = dxr_pipeline->shader_table.raygen_section_size;

            dispatch_desc.MissShaderTable.StartAddress = miss_addr;
            dispatch_desc.MissShaderTable.StrideInBytes = dxr_pipeline->shader_table.miss_record_size;
            dispatch_desc.MissShaderTable.SizeInBytes = dxr_pipeline->shader_table.miss_section_size;

            dispatch_desc.HitGroupTable.StartAddress = hit_addr;
            dispatch_desc.HitGroupTable.StrideInBytes = dxr_pipeline->shader_table.hit_record_size;
            dispatch_desc.HitGroupTable.SizeInBytes = dxr_pipeline->shader_table.hit_section_size;

            dispatch_desc.CallableShaderTable.StartAddress = callable_addr;
            dispatch_desc.CallableShaderTable.StrideInBytes = 0;
            dispatch_desc.CallableShaderTable.SizeInBytes = 0;

            dispatch_desc.Width = threads[0];
            dispatch_desc.Height = threads[1];
            dispatch_desc.Depth = threads[2];

            rt_cmd_list->DispatchRays(&dispatch_desc);
        }
        auto GpuDeviceDXR::trace_rays_indirect(CommandBuffer* cb, RayTracingPipeline* rtpipeline, u64 args_buffer_address) -> void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto dxr_pipeline = static_cast<PipelineDXR*>(rtpipeline);
            if (!dxr_cb || !dxr_pipeline || !supports_dxr || !raytracing_command_signature)
                return;

            // Get raytracing command list
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> rt_cmd_list4;
            if (FAILED(dxr_cb->command_list->QueryInterface(IID_PPV_ARGS(&rt_cmd_list4))))
            {
                DS_LOG_ERROR("Failed to query raytracing command list4");
                return;
            }

            // Unmap shader table if mapped
            if (dxr_pipeline->shader_table.mapped_data && dxr_pipeline->shader_table.buffer)
            {
                dxr_pipeline->shader_table.buffer->Unmap(0, nullptr);
                dxr_pipeline->shader_table.mapped_data = nullptr;
            }

            // NOTE: D3D12 ExecuteIndirect requires an ID3D12Resource* for the args buffer,
            // but the RHI interface only provides a GPU virtual address (to match Vulkan).
            // This is a fundamental API mismatch that prevents full implementation.
            //
            // Future improvements would require either:
            // 1. Modifying the RHI interface to accept GpuBuffer* instead of u64 address
            // 2. Maintaining a GPU address -> ID3D12Resource mapping in DXR backend
            //
            // For now, this implementation provides the structure but cannot execute
            // indirect ray tracing without buffer resource tracking.

            DS_LOG_WARN("DXR backend: trace_rays_indirect requires buffer resource (not just address) for ExecuteIndirect. RHI interface limitation prevents full implementation.");

            // The implementation would be:
            // dxr_cb->command_list->ExecuteIndirect(
            //     raytracing_command_signature.Get(),
            //     1,  // MaxCommandCount
            //     args_buffer_resource,  // ID3D12Resource* - not available from args_buffer_address
            //     0,  // ArgsBufferOffset
            //     nullptr,  // CountBuffer
            //     0   // CountBufferOffset
            // );
        }
        auto GpuDeviceDXR::event_begin(const char*, CommandBuffer*)->void {}
        auto GpuDeviceDXR::event_end(CommandBuffer*) -> void {}
        auto GpuDeviceDXR::set_name(GpuResource* resource, const char* name)const->void
        {
            auto texture_dx = static_cast<GpuTextureDXR*>(resource);
            if (texture_dx && texture_dx->resource && name)
            {
                std::wstring wide_name(name, name + std::strlen(name));
                texture_dx->resource->SetName(wide_name.c_str());
                return;
            }

            auto buffer_dx = static_cast<GpuBufferDXR*>(resource);
            if (buffer_dx && buffer_dx->resource && name)
            {
                std::wstring wide_name(name, name + std::strlen(name));
                buffer_dx->resource->SetName(wide_name.c_str());
                return;
            }
        }

        auto GpuDeviceDXR::destroy_resource(GpuResource* resource)->void
        {
            // D3D12 uses reference counting via ComPtr, so explicit destruction is not needed
            // Resources are automatically released when their smart pointers go out of scope
        }

        auto GpuDeviceDXR::defer_release(std::function<void()> f)->void
        {
            std::lock_guard<std::mutex> lock(release_mutex);
            destroy_queue.push_back(DeferedReleaseResource{ f, 0 });
        }

        auto GpuDeviceDXR::export_image(rhi::GpuTexture* image)->std::vector<u8>
        {
            auto texture_dx = static_cast<GpuTextureDXR*>(image);
            if (!texture_dx || !texture_dx->resource)
                return {};

            // Create staging resource for readback
            D3D12_RESOURCE_DESC resource_desc = texture_dx->resource->GetDesc();
            D3D12_HEAP_PROPERTIES heap_props = {};
            heap_props.Type = D3D12_HEAP_TYPE_READBACK;
            heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            Microsoft::WRL::ComPtr<ID3D12Resource> staging_resource;
            throw_if_failed(device->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &resource_desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&staging_resource)),
                "CreateCommittedResource(staging for export)");

            // Copy texture to staging
            with_setup_cb([this, &texture_dx, &staging_resource](CommandBuffer* cmd) {
                auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cmd);
                if (!dxr_cb)
                    return;

                // Transition source to copy source
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = texture_dx->resource.Get();
                barrier.Transition.StateBefore = texture_dx->current_state;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                barrier.Transition.Subresource = 0;
                dxr_cb->command_list->ResourceBarrier(1, &barrier);

                // Copy
                D3D12_TEXTURE_COPY_LOCATION src = {};
                src.pResource = texture_dx->resource.Get();
                src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                src.SubresourceIndex = 0;

                D3D12_TEXTURE_COPY_LOCATION dst = {};
                dst.pResource = staging_resource.Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = 0;

                dxr_cb->command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

                // Transition back
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
                barrier.Transition.StateAfter = texture_dx->current_state;
                dxr_cb->command_list->ResourceBarrier(1, &barrier);
            });

            // Map and read back
            std::vector<u8> result;
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT row_count = 0;
            UINT64 row_size = 0;
            device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &footprint, &row_count, &row_size, nullptr);

            u8* mapped_data = nullptr;
            if (SUCCEEDED(staging_resource->Map(0, nullptr, reinterpret_cast<void**>(&mapped_data))))
            {
                u64 total_size = row_size * row_count;
                result.resize(total_size);
                std::memcpy(result.data(), mapped_data, total_size);
                staging_resource->Unmap(0, nullptr);
            }

            return result;
        }

        auto GpuDeviceDXR::blit_image(rhi::GpuTexture* src, rhi::GpuTexture* dst, CommandBuffer* cmd_buf) ->void
        {
            // D3D12 doesn't have a direct blit equivalent like Vulkan
            // This would need to be implemented using a compute shader or copy operations
            DS_LOG_WARN("DXR backend: blit_image not fully implemented - use copy_image instead");

            // For now, just do a simple copy
            if (cmd_buf)
            {
                copy_image(src, dst, cmd_buf);
            }
        }
        auto GpuDeviceDXR::fill_buffer(CommandBuffer* cb, GpuBuffer* buffer, uint32_t value) -> void
        {
            auto dxr_cb = static_cast<GpuCommandBufferDXR*>(cb);
            auto buffer_dx = static_cast<GpuBufferDXR*>(buffer);
            if (!dxr_cb || !buffer_dx || !buffer_dx->resource)
                return;

            // Create temporary UAV descriptor heap
            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heap_desc.NumDescriptors = 1;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            heap_desc.NodeMask = 0;

            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> uav_heap;
            if (SUCCEEDED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&uav_heap))))
            {
                D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu_handle = uav_heap->GetCPUDescriptorHandleForHeapStart();
                D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu_handle = uav_heap->GetGPUDescriptorHandleForHeapStart();

                // Create UAV
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
                uav_desc.Format = DXGI_FORMAT_R32_UINT;
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                uav_desc.Buffer.FirstElement = 0;
                uav_desc.Buffer.NumElements = static_cast<u32>(buffer_dx->desc.size / sizeof(u32));
                uav_desc.Buffer.StructureByteStride = 0;
                uav_desc.Buffer.CounterOffsetInBytes = 0;
                uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

                device->CreateUnorderedAccessView(buffer_dx->resource.Get(), nullptr, &uav_desc, uav_cpu_handle);

                // Clear UAV
                u32 clear_value[4] = { value, value, value, value };
                ID3D12DescriptorHeap* heaps[] = { uav_heap.Get() };
                dxr_cb->command_list->SetDescriptorHeaps(1, heaps);
                dxr_cb->command_list->ClearUnorderedAccessViewUint(
                    uav_gpu_handle,
                    uav_cpu_handle,
                    buffer_dx->resource.Get(),
                    clear_value,
                    0, nullptr);
            }
        }
        auto GpuDeviceDXR::get_graphics_cmd_buffer()->CommandBuffer* { return setup_cb.get(); }

        auto create_dxr_device(u32 device_index)->GpuDevice*
        {
            return new GpuDeviceDXR(device_index);
        }
    }
}

#endif
