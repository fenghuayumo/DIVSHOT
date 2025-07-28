#pragma once

#include "resource.h"

namespace diverse
{
	namespace rg
	{

		struct AnyRenderResource
		{
			enum class Type
			{
				OwnedImage,
				ImportedImage,
				OwnedBuffer,
				ImportedBuffer,
				ImportedRayTracingAcceleration,
				Pending
			}ty;

			std::any value;

			auto image()->std::shared_ptr<rhi::GpuTexture>&
			{
				return std::any_cast<std::shared_ptr<rhi::GpuTexture>&>(value);
			}

			auto buffer()->std::shared_ptr<rhi::GpuBuffer>&
			{
				return std::any_cast<std::shared_ptr<rhi::GpuBuffer>&>(value);
			}

			auto ray_tracing_acceleration()->std::shared_ptr<rhi::GpuRayTracingAcceleration>&
			{
				return std::any_cast<std::shared_ptr<rhi::GpuRayTracingAcceleration>&>(value);
			}

			auto pending() -> GraphResourceInfo
			{
				return std::any_cast<GraphResourceInfo&>(value);
			}

			static auto image(const std::shared_ptr<rhi::GpuTexture>& v) -> AnyRenderResource
			{
				return { AnyRenderResource::Type::OwnedImage, v };
			}

			static auto buffer(const std::shared_ptr<rhi::GpuBuffer>& v) -> AnyRenderResource
			{
				return { AnyRenderResource::Type::OwnedBuffer, v };
			}
			static auto imported_image(const std::shared_ptr<rhi::GpuTexture>& v) -> AnyRenderResource
			{
				return { AnyRenderResource::Type::ImportedImage, v };
			}

			static auto imported_buffer(const std::shared_ptr<rhi::GpuBuffer>& v) -> AnyRenderResource
			{
				return { AnyRenderResource::Type::ImportedBuffer, v };
			}

			static auto ray_tracing_acceleration(const std::shared_ptr<rhi::GpuRayTracingAcceleration>& v) -> AnyRenderResource
			{
				return { AnyRenderResource::Type::ImportedRayTracingAcceleration, v };
			}

			static auto pending(const GraphResourceInfo& v) -> AnyRenderResource
			{
				return { AnyRenderResource::Type::Pending, v };
			}
		};

		struct RegistryResource
		{
			AnyRenderResource resource;
			rhi::AccessType	access_type;
		};

		struct ResourceRegistry
		{
			RenderGraphExecutionParams execution_params;
			std::vector<RegistryResource> resources;
			RenderGraphPipelines pipelines;
			rhi::DynamicConstants* dynamic_constants;

			template<typename ViewType>
			requires std::derived_from<ViewType, GpuViewType>
			auto image(const Ref<rhi::GpuTexture,ViewType>& resource)->rhi::GpuTexture*
			{
				return image_from_raw_handle<ViewType>(resource.handle);
			}
			template<typename ViewType>
			requires std::derived_from<ViewType, GpuViewType>
			auto image_from_raw_handle(const GraphRawResourceHandle& handle)->rhi::GpuTexture*
			{
				return resources[handle.id].resource.image().get();
			}
			template<typename ViewType>
				requires std::derived_from<ViewType, GpuViewType>
			auto buffer(const Ref<rhi::GpuBuffer, ViewType>& resource) -> rhi::GpuBuffer*
			{
				return buffer_from_raw_handle<ViewType>(resource.handle);
			}
			template<typename ViewType>
				requires std::derived_from<ViewType, GpuViewType>
			auto buffer_from_raw_handle(const GraphRawResourceHandle& handle) -> rhi::GpuBuffer*
			{
				return resources[handle.id].resource.buffer().get();
			}

			template<typename ViewType>
				requires std::derived_from<ViewType, GpuViewType>
			auto rt_acceleration(const Ref<rhi::GpuRayTracingAcceleration, ViewType>& resource) ->rhi::GpuRayTracingAcceleration*
			{
				return rt_acceleration_from_raw_handle<ViewType>(resource.handle);
			}

			template<typename ViewType>
				requires std::derived_from<ViewType, GpuViewType>
			auto rt_acceleration_from_raw_handle(const GraphRawResourceHandle& handle)->rhi::GpuRayTracingAcceleration*
			{
				return resources[handle.id].resource.ray_tracing_acceleration().get();
			}

			auto image_view(GraphRawResourceHandle&& resource, const rhi::GpuTextureViewDesc& view_desc)->rhi::GpuTextureView*
			{
				auto& img = resources[resource.id].resource.image();
				auto device = (const rhi::GpuDevice*)execution_params.device;
				return img->view(device, view_desc).get();
			}

			auto buffer_view(GraphRawResourceHandle&& resource, const rhi::GpuBufferViewDesc& view_desc) -> rhi::GpuBufferView*
			{
				auto& buffer = resources[resource.id].resource.buffer();
				auto device = (const rhi::GpuDevice*)execution_params.device;
				return buffer->view(device, view_desc).get();
			}

			auto compute_pipeline(const RgComputePipelineHandle& pipeline)->std::shared_ptr<rhi::ComputePipeline>;

			auto raster_pipeline(const RgRasterPipelineHandle& pipeline)->std::shared_ptr<rhi::RasterPipeline>;

			auto ray_tracing_pipeline(const RgRtPipelineHandle& pipeline)->std::shared_ptr<rhi::RayTracingPipeline>;
		};
	}
}