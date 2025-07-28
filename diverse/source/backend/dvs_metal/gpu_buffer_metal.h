#pragma once
#include "backend/drs_rhi/gpu_buffer.h"
#include "dvs_metal_utils.h"
namespace diverse
{
	namespace rhi
	{
		struct GpuDeviceMetal;
		struct GpuBufferMetal : public GpuBuffer
		{
            GpuBufferMetal(const GpuBufferDesc& desc, id<MTLBuffer> buf);
            u64 device_address(const struct GpuDevice* device) override;

			auto map(const struct GpuDevice* device) -> u8* override;
			auto unmap(const GpuDevice* device) -> void override;

            id<MTLBuffer>  id_buf;
		};
	}
}
