#include "gpu_texture.h"
#include "gpu_buffer.h"
#include "gpu_raytracing.h"
#include "gpu_device.h"

namespace diverse
{
	namespace rhi
	{
		
		GpuResource::~GpuResource()
		{

		}

		GpuBuffer::~GpuBuffer()
		{
			 //g_device->destroy_resource(this);
		}

		auto GpuBuffer::export_data() -> std::vector<u8>
		{
			auto device = get_owner_device();
			if (!device)
				device = get_global_device();
			std::vector<u8> data(desc.size);
			if (device)
				copy_to(device, data.data(), desc.size, 0);
			return data;
		}

		GpuTexture::~GpuTexture()
		{
		}
		auto GpuTexture::export_texture() -> std::vector<u8>
		{
			auto device = get_owner_device();
			if (!device)
				device = get_global_device();
			return device ? device->export_image(this) : std::vector<u8>{};
		}

		GpuTextureView::~GpuTextureView()
		{
			
		}
		GpuBufferView::~GpuBufferView()
		{

		}
	}
}
