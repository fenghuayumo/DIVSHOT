#pragma once
#include "resource.h"
#include "graph.h"

namespace diverse
{
	namespace rg
	{
		using TemporalResourceKey = std::string;

		struct TemporalResource
		{
			enum class Type
			{
				Buffer,
				Image
			}ty;

			std::any value;

			std::shared_ptr<rhi::GpuTexture>& Image() {
				return std::any_cast<std::shared_ptr<rhi::GpuTexture>&>(value);
			}

			std::shared_ptr<rhi::GpuBuffer>& Buffer() {
				return std::any_cast<std::shared_ptr<rhi::GpuBuffer>&>(value);
			}

			static auto Image(const std::shared_ptr<rhi::GpuTexture>& img) -> TemporalResource
			{
				return { TemporalResource::Type::Image, img};
			}

			static auto Buffer(const std::shared_ptr<rhi::GpuBuffer>& buf) -> TemporalResource
			{
				return { TemporalResource::Type::Buffer, buf };
			}
		};

		struct ExportedResourceHandle
		{
			enum class Type
			{
				Buffer,
				Image
			}ty;

			std::any value;


			ExportedHandle<rhi::GpuTexture>& Image() {
				return std::any_cast<ExportedHandle<rhi::GpuTexture>&>(value);
			}

			ExportedHandle<rhi::GpuBuffer>& Buffer() {
				return std::any_cast<ExportedHandle<rhi::GpuBuffer>&>(value);
			}


			static auto Image(const ExportedHandle<rhi::GpuTexture>& img) -> ExportedResourceHandle
			{
				return { ExportedResourceHandle::Type::Image, img };
			}

			static auto Buffer(const ExportedHandle<rhi::GpuBuffer>& buf) -> ExportedResourceHandle
			{
				return { ExportedResourceHandle::Type::Buffer, buf };
			}
		};


		struct TemporalResourceState
		{
			enum class Type
			{
				Inert,
				Imported,
				Exported
			}ty;

			//struct Inert {
			//	TemporalResource resource;
			//	rhi::AccessType access_type;
			//};
			//struct Imported {
			//	TemporalResource resource;
			//	ExportableGraphResource handle;
			//};

			//struct Exported {
			//	TemporalResource resource;
			//	ExportedResourceHandle handle;
			//};

			std::any value;

			auto Inert() -> std::pair<TemporalResource, rhi::AccessType>&
			{
				return std::any_cast<std::pair<TemporalResource, rhi::AccessType>&>(value);
			}

			auto Imported() -> std::pair<TemporalResource, ExportableGraphResource>&
			{
				return std::any_cast<std::pair<TemporalResource, ExportableGraphResource>&>(value);
			}

			auto Exported() -> std::pair<TemporalResource, ExportedResourceHandle>&
			{
				return std::any_cast<std::pair<TemporalResource, ExportedResourceHandle>&>(value);
			}

			static auto Inert(const std::pair<TemporalResource, rhi::AccessType>& v)-> TemporalResourceState
			{
				return { TemporalResourceState::Type::Inert, v};
			}

			static auto Imported(const std::pair<TemporalResource, ExportableGraphResource>& v) -> TemporalResourceState
			{
				return { TemporalResourceState::Type::Imported,  v};
			}

			static auto Exported(const std::pair<TemporalResource, ExportedResourceHandle>& v) -> TemporalResourceState
			{
				return { TemporalResourceState::Type::Exported,  v };
			}
		};

		struct TemporalRenderGraphState
		{
			std::unordered_map<TemporalResourceKey, TemporalResourceState>	resources;
			auto retire_temporal(RenderGraph& rg)-> TemporalRenderGraphState;

			enum
			{
				Inert,
				Exported
			}status;
		};

		using ExportedTemporalRenderGraphState = TemporalRenderGraphState;

		struct TemporalGraph : public RenderGraph
		{
			rhi::GpuDevice* device;
			TemporalRenderGraphState    temporal_state;

			TemporalGraph(const TemporalRenderGraphState& state,rhi::GpuDevice* device);
			~TemporalGraph(){}
			auto get_or_create_temporal(
				const TemporalResourceKey& key,
				const rhi::GpuTextureDesc& desc) -> Handle<rhi::GpuTexture>;

			auto get_or_create_temporal(
				const TemporalResourceKey& key,
				const rhi::GpuBufferDesc& desc) -> Handle<rhi::GpuBuffer>;

			auto export_temporal() -> ExportedTemporalRenderGraphState;

			//auto clear_resources()->void { temporal_state.resources.clear(); }
		};
	}
}
