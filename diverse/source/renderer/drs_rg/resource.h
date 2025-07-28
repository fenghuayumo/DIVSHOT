#pragma once
#include "backend/drs_rhi/gpu_device.h"
#include <any>
#include <concepts>

namespace diverse
{
	namespace rg
	{
		struct ResourceLifeTime
        {
            std::optional<uint32> last_access;
        };

        struct ResourceInfo
        {
            std::vector<ResourceLifeTime>   _lifetimes;
            std::vector<rhi::TextureUsageFlags> image_usage_flags;
            std::vector<rhi::BufferUsageFlags> buffer_usage_flags;
        };


        struct FrameConstantsLayout
        {
			u32		globals_offset = 0;
			u32		instance_dynamic_parameters_offset = 0;
			u32		triangle_lights_offset = 0;
			u32		scene_lights_offset = 0;
        };

        struct RenderGraphExecutionParams
        {
            rhi::GpuDevice* device;
            rhi::PipelineCache* pipeline_cache;
            rhi::DescriptorSet*   frame_descriptor_set;
            FrameConstantsLayout frame_constants_layout;

        };

        struct RenderGraphPipelines
        {
            std::vector<rhi::ComputePipelineHandle>  compute;
            std::vector<rhi::RasterPipelineHandle> raster;
            std::vector<rhi::RtPipelineHandle> rt;
        };

		struct GraphResourceDesc
		{
			enum class Type
			{
				Image,
				Buffer,
				RayTracingAcceleration
			}ty;

			GraphResourceDesc(const rhi::GpuTextureDesc& desc)
			: ty(GraphResourceDesc::Type::Image), value(desc)
			{
			}

			GraphResourceDesc(const rhi::GpuBufferDesc& desc)
				: ty(GraphResourceDesc::Type::Buffer), value(desc)
			{
			}

			auto image_desc()const->rhi::GpuTextureDesc
			{
				return std::any_cast<rhi::GpuTextureDesc>(value);
			}
			auto buffer_desc()const -> rhi::GpuBufferDesc
			{
				return std::any_cast<rhi::GpuBufferDesc>(value);
			}

			static auto image_desc(const rhi::GpuTextureDesc& desc)->GraphResourceDesc
			{
				return GraphResourceDesc(desc);
			}

			static auto buffer_desc(const rhi::GpuBufferDesc& desc) -> GraphResourceDesc
			{
				return GraphResourceDesc(desc);
			}
			std::any value;
		};

		struct GraphRawResourceHandle {
			uint32 id;
			uint32 version;

			auto next_version() -> GraphRawResourceHandle {
				return{
					id,
					version + 1
				};
			}
		};


		struct GraphResourceCreateInfo
		{
			GraphResourceDesc desc;
			std::string_view  name;
		};

		struct GraphResourceImportInfo
		{
			enum class Type
			{
				Image,
				Buffer,
				RayTracingAcceleration,
				SwapchainImage
			}ty;

			std::any value;

			auto Image() -> std::pair<std::shared_ptr<rhi::GpuTexture>, rhi::AccessType>
			{
				return	std::any_cast<std::pair<std::shared_ptr<rhi::GpuTexture>, rhi::AccessType>>(value);
			}

			auto Buffer() -> std::pair<std::shared_ptr<rhi::GpuBuffer>, rhi::AccessType>
			{
				return std::any_cast<std::pair<std::shared_ptr<rhi::GpuBuffer>, rhi::AccessType>>(value);
			}

			auto RayTracingAcceleration() -> std::pair<std::shared_ptr<rhi::GpuRayTracingAcceleration>, rhi::AccessType>
			{
				return std::any_cast<std::pair<std::shared_ptr<rhi::GpuRayTracingAcceleration>, rhi::AccessType>>(value);
			}

			static auto  SwapchainImage() -> GraphResourceImportInfo
			{
				return { GraphResourceImportInfo ::Type::SwapchainImage, 0};
			}
		};

		struct GraphResourceInfo
		{
			enum class Type
			{
				Created,
				Imported
			}ty;

			std::any value;

			auto graph_resource_create_info() -> GraphResourceCreateInfo&
			{
				return std::any_cast<GraphResourceCreateInfo&>(value);
			}

			auto graph_resource_import_info() -> GraphResourceImportInfo&
			{
				return std::any_cast<GraphResourceImportInfo&>(value);
			}

			static auto Imported(GraphResourceImportInfo&& v)->GraphResourceInfo
			{
				return { GraphResourceInfo ::Type::Imported, v};
			}

			static auto Created(GraphResourceCreateInfo&& v) -> GraphResourceInfo
			{
				return { GraphResourceInfo::Type::Created, v };
			}
		};

		struct RgComputePipelineHandle{uint32 id;};

		struct RgRasterPipelineHandle { uint32 id; };

		struct RgRtPipelineHandle { uint32 id; };

		struct RgComputePipeline
		{
			rhi::ComputePipelineDesc desc;
		};

		struct RgRasterPipeline
		{
			rhi::RasterPipelineDesc desc;
			std::vector<rhi::PipelineShaderDesc> shaders;
		};

		struct RgRtPipeline
		{
			rhi::RayTracingPipelineDesc desc;
			std::vector<rhi::PipelineShaderDesc> shaders;
		};
		template<typename ResType>
		requires std::derived_from<ResType,diverse::rhi::GpuResource>
		struct Handle
		{
			GraphRawResourceHandle raw;
			typename ResType::Desc desc;

			auto get_desc() const ->typename ResType::Desc
			{
				return desc;
			}
		};

		struct PredefinedDescriptorSet
		{
			std::unordered_map<uint32, rhi::DescriptorInfo>	bindings;
		};

		template<typename ResType>
		requires std::derived_from<ResType, diverse::rhi::GpuResource>
		struct ExportedHandle
		{
			GraphRawResourceHandle raw;
		};


		struct ExportableGraphResource
		{
			enum class Type
			{
				Image,
				Buffer,
				RayTracingAcceleration
			}ty;
			std::any value;

			auto Image()->Handle<rhi::GpuTexture>&
			{
				return std::any_cast<Handle<rhi::GpuTexture>&>(value);
			}

			auto Buffer()->Handle<rhi::GpuBuffer>&
			{
				return std::any_cast<Handle<rhi::GpuBuffer>&>(value);
			}

			static auto Image(const Handle<rhi::GpuTexture>& handle) -> ExportableGraphResource
			{
				return { ExportableGraphResource::Type::Image, handle};
			}
			static auto Buffer(const Handle<rhi::GpuBuffer>& handle) -> ExportableGraphResource
			{
				return { ExportableGraphResource::Type::Buffer, handle };
			}

			auto raw() -> GraphRawResourceHandle 
			{
				if (ty == Type::Image) {
					return Image().raw;
				}
				else  {
					return Buffer().raw;
				}
			}
		};


		struct RenderPassImageBinding
		{
			GraphRawResourceHandle	handle;
			rhi::GpuTextureViewDesc	view_desc;
			rhi::ImageLayout image_layout;
		};

		struct RenderPassBufferBinding
		{
			GraphRawResourceHandle handle;
		};

		struct RenderPassRayTracingAccelerationBinding
		{
			GraphRawResourceHandle handle;
		};

		struct RenderPassBinding
		{
			enum class Type
			{
				Image,
				ImageArray,
				Buffer,
				RayTracingAcceleration,
				DynamicConstants,
				DynamicConstantsStorageBuffer
			}ty;

			std::any value;

			auto Image() -> RenderPassImageBinding&
			{
				return std::any_cast<RenderPassImageBinding&>(value);
			}

			auto ImageArray()->std::vector<RenderPassImageBinding>&
			{
				return std::any_cast<std::vector<RenderPassImageBinding>&>(value);
			}

			auto Buffer()-> RenderPassBufferBinding&
			{
				return std::any_cast<RenderPassBufferBinding&>(value);
			}

			auto RayTracingAcceleration() -> RenderPassRayTracingAccelerationBinding&
			{
				return std::any_cast<RenderPassRayTracingAccelerationBinding&>(value);
			}

			auto DynamicConstants() -> uint32&
			{
				return std::any_cast<uint32&>(value);
			}

			auto DynamicConstantsStorageBuffer() -> uint32&
			{
				return std::any_cast<uint32&>(value);
			}

			static auto Image(const RenderPassImageBinding& binding) -> RenderPassBinding
			{
				return { RenderPassBinding ::Type::Image, binding};
			}

			static auto ImageArray(const std::vector<RenderPassImageBinding>& binding) -> RenderPassBinding
			{
				return { RenderPassBinding::Type::ImageArray, binding };
			}

			static auto Buffer(const RenderPassBufferBinding& binding) -> RenderPassBinding
			{
				return { RenderPassBinding::Type::Buffer, binding };
			}

			static auto RayTracingAcceleration(const RenderPassRayTracingAccelerationBinding& binding) -> RenderPassBinding
			{
				return { RenderPassBinding::Type::RayTracingAcceleration, binding };
			}

			static auto DynamicConstants(const uint32& binding) -> RenderPassBinding
			{
				return { RenderPassBinding::Type::DynamicConstants, binding };
			}

			static auto DynamicConstantsStorageBuffer(const uint32& binding) -> RenderPassBinding
			{
				return { RenderPassBinding::Type::DynamicConstantsStorageBuffer, binding };
			}
		};


		struct BindRgRef
		{
			virtual auto bind()->RenderPassBinding = 0;
		};

		struct GpuViewType
		{
			virtual bool writable() = 0;
		};

		struct GpuSrv : public GpuViewType {
			bool writable() {return false;}
		};
		struct GpuUav : public GpuViewType {
			bool writable() { return true; }
		};
		struct GpuRt : public GpuViewType {
			bool writable() { return true; }
		};

		template<typename ResType, typename ViewType>
			requires std::derived_from<ResType, diverse::rhi::GpuResource>&&
			std::derived_from<ViewType, GpuViewType>
				struct Ref
			{
				GraphRawResourceHandle handle;
				typename ResType::Desc desc;

				//auto bind() -> RenderPassBinding;
			};

		template<>
		struct Ref<rhi::GpuTexture, GpuSrv> : public BindRgRef
		{
			GraphRawResourceHandle handle;
			rhi::GpuTextureDesc desc;
			Ref(const GraphRawResourceHandle& h, const rhi::GpuTextureDesc& des)
				:handle(h), desc(des)
			{}
			Ref() = default;
			auto bind_view(const rhi::GpuTextureViewDesc& view_desc) -> RenderPassBinding
			{
				return {
					RenderPassBinding::Type::Image,
					RenderPassImageBinding{
						handle,
						view_desc,
						rhi::ImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					}
				};
			}
			auto bind() -> RenderPassBinding override
			{
				return bind_view(rhi::GpuTextureViewDesc());
			}
		};

		template<>
		struct Ref<rhi::GpuTexture, GpuUav> : public BindRgRef
		{
			GraphRawResourceHandle handle;
			rhi::GpuTextureDesc desc;
			Ref(const GraphRawResourceHandle& h, const rhi::GpuTextureDesc& des)
				:handle(h), desc(des)
			{}
			Ref() = default;
			auto bind_view(const rhi::GpuTextureViewDesc& view_desc) -> RenderPassBinding
			{
				return {
					RenderPassBinding::Type::Image,
					RenderPassImageBinding{
						handle,
						view_desc,
						rhi::ImageLayout::IMAGE_LAYOUT_GENERAL
					}
				};
			}
			auto bind() -> RenderPassBinding override
			{
				return bind_view(rhi::GpuTextureViewDesc());
			}
		};


		template<>
		struct Ref<rhi::GpuBuffer, GpuUav> : public BindRgRef
		{
			GraphRawResourceHandle handle;
			rhi::GpuBufferDesc desc;
			Ref(const GraphRawResourceHandle& h, const rhi::GpuBufferDesc& des)
				:handle(h), desc(des)
			{}
			Ref() = default;
			auto bind() -> RenderPassBinding override
			{
				return RenderPassBinding::Buffer(RenderPassBufferBinding{
					handle
				});
			}
		};

		template<>
		struct Ref<rhi::GpuBuffer, GpuSrv> : public BindRgRef
		{
			GraphRawResourceHandle handle;
			rhi::GpuBufferDesc desc;
			Ref(const GraphRawResourceHandle& h, const rhi::GpuBufferDesc& des)
				:handle(h), desc(des)
			{}
			Ref() = default;
			auto bind() -> RenderPassBinding override
			{
				return RenderPassBinding::Buffer(RenderPassBufferBinding{
					handle
				});
			}
		};

		template<>
		struct Ref<rhi::GpuRayTracingAcceleration, GpuSrv> : public BindRgRef
		{
			GraphRawResourceHandle handle;
			rhi::RayTracingAccelerationDesc desc;
			Ref(const GraphRawResourceHandle& h, const rhi::RayTracingAccelerationDesc& des)
				:handle(h), desc(des)
			{}
			Ref() = default;
			auto bind() -> RenderPassBinding override
			{
				return RenderPassBinding::RayTracingAcceleration(RenderPassRayTracingAccelerationBinding{
						handle
				});
			}
		};

}
}
