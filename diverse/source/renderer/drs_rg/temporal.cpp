#include "temporal.h"
#include "core/ds_log.h"

namespace diverse
{
	namespace rg
	{
		TemporalGraph::TemporalGraph(const TemporalRenderGraphState& state, rhi::GpuDevice* d)
		: device(d), temporal_state(state)
		{
		}

		auto TemporalGraph::get_or_create_temporal(const TemporalResourceKey& key,const rhi::GpuTextureDesc& desc) -> Handle<rhi::GpuTexture>
		{
			auto it = temporal_state.resources.find(key);
			if (it != temporal_state.resources.end())
			{
				auto& state = it->second;
				if (state.ty == TemporalResourceState::Type::Inert)
				{
					auto& [resource, access_type] = state.Inert();
					if (resource.ty == TemporalResource::Type::Image)
					{
						auto handle = this->import_res(resource.Image(), access_type);

						state = TemporalResourceState::Imported({resource,ExportableGraphResource::Image(handle) });
						return handle;
					}
					else if (resource.ty == TemporalResource::Type::Buffer)
					{
						//throw std::runtime_error(fmt::format("Resource{  } is a buffer, but an image was requested", key));
						DS_LOG_ERROR("Resource{} is a buffer, but an image was requested", key);
						return{};
					}
				}
				else if (state.ty == TemporalResourceState::Type::Imported)
				{
					DS_LOG_ERROR("Temporal resource already taken:  {}", key);
					return{};
				}
			}
		
			auto resource = device->create_texture(desc,{}, key.c_str());
			auto handle = this->import_res(resource, rhi::AccessType::Nothing);
			temporal_state.resources[key] = TemporalResourceState::Imported({ TemporalResource::Image(resource), ExportableGraphResource::Image(handle) });
			return handle;
		}

		auto TemporalGraph::get_or_create_temporal(const TemporalResourceKey& key,const rhi::GpuBufferDesc& desc) -> Handle<rhi::GpuBuffer>
		{
			auto it = temporal_state.resources.find(key);
			if (it != temporal_state.resources.end())
			{
				auto& state = it->second;
				if (state.ty == TemporalResourceState::Type::Inert)
				{
					auto& [resource, access_type] = state.Inert();
					if (resource.ty == TemporalResource::Type::Buffer)
					{
						auto handle = this->import_res(resource.Buffer(), access_type);

						state = TemporalResourceState::Imported({ resource,ExportableGraphResource::Buffer(handle) });
						return handle;
					}
					else if (resource.ty == TemporalResource::Type::Image)
					{
						//throw std::runtime_error(fmt::format("Resource{  } is a buffer, but an image was requested", key));
						DS_LOG_ERROR("Resource{} is a buffer, but an image was requested", key);
						return {};
					}
				}
				else if (state.ty == TemporalResourceState::Type::Imported)
				{
					DS_LOG_ERROR("Temporal resource already taken:  {}", key);
					return {};
				}
			}
			//zero init
			auto init_data = std::vector<u8>(desc.size, 0);
			auto resource = device->create_buffer(desc, key.c_str(), init_data.data());
			auto handle = this->import_res(resource, rhi::AccessType::Nothing);
			temporal_state.resources[key] = TemporalResourceState::Imported({ TemporalResource::Buffer(resource), ExportableGraphResource::Buffer(handle) });
			return handle;
		}

		auto TemporalGraph::export_temporal() -> ExportedTemporalRenderGraphState
		{
			for (auto& [key, state] : temporal_state.resources)
			{
				if (state.ty == TemporalResourceState::Type::Inert){
					
				}
				else if(state.ty == TemporalResourceState::Type::Imported){
					auto& [resouce, handle] = state.Imported();
					if (handle.ty == ExportableGraphResource::Type::Image) {
						auto export_handle = this->export_res(handle.Image(), rhi::AccessType::Nothing);
						state = TemporalResourceState::Exported({resouce, ExportedResourceHandle::Image(export_handle) });
					}
					else if( handle.ty == ExportableGraphResource::Type::Buffer){
						auto export_handle = this->export_res(handle.Buffer(), rhi::AccessType::Nothing);
						state = TemporalResourceState::Exported({ resouce, ExportedResourceHandle::Buffer(export_handle) });
					}
				}
			}

			return { temporal_state };
		}

		auto ExportedTemporalRenderGraphState::retire_temporal(RenderGraph& rg) -> TemporalRenderGraphState
		{
			for (auto& [key, state] : resources)
			{
				if (state.ty == TemporalResourceState::Type::Inert) {
					// Nothing to do here
				}
				else if (state.ty == TemporalResourceState::Type::Imported) {
					//unreachable
				}
				else if(state.ty == TemporalResourceState::Type::Exported){
					auto& [resource, handle] = state.Exported();
					if (handle.ty == ExportedResourceHandle::Type::Image) {
						state = TemporalResourceState::Inert({resource, rg.exported_resource(handle.Image()).second});
					}
					else if (handle.ty == ExportedResourceHandle::Type::Buffer) {
						state = TemporalResourceState::Inert({ resource, rg.exported_resource(handle.Buffer()).second });
					}
				}
			}
			return { resources };
		}

	}
}

