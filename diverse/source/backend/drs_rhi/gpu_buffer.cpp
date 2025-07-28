#include "gpu_buffer.h"

auto diverse::rhi::buffer_access_type_to_usage_flags(AccessType access_type) -> BufferUsageFlags
{
	switch (access_type)
	{
	case diverse::rhi::IndirectBuffer:
		return BufferUsageFlags::INDIRECT_BUFFER;
	case diverse::rhi::IndexBuffer:
		return BufferUsageFlags::INDEX_BUFFER;
	case diverse::rhi::VertexBuffer:
	case diverse::rhi::VertexShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::VertexShaderReadOther:
	case diverse::rhi::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::TessellationControlShaderReadOther:
	case diverse::rhi::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::TessellationEvaluationShaderReadOther:
	case diverse::rhi::GeometryShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::GeometryShaderReadOther:
	case diverse::rhi::ComputeShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::ComputeShaderReadOther:
	case diverse::rhi::AnyShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::AnyShaderReadOther:
	case diverse::rhi::FragmentShaderReadSampledImageOrUniformTexelBuffer:
		return BufferUsageFlags::UNIFORM_TEXEL_BUFFER;

	case diverse::rhi::VertexShaderReadUniformBuffer:
	case diverse::rhi::TessellationControlShaderReadUniformBuffer:
	case diverse::rhi::TessellationEvaluationShaderReadUniformBuffer:
	case diverse::rhi::GeometryShaderReadUniformBuffer:
	case diverse::rhi::FragmentShaderReadUniformBuffer:
		return BufferUsageFlags::UNIFORM_BUFFER;
	case diverse::rhi::ComputeShaderReadUniformBuffer:
	case diverse::rhi::AnyShaderReadUniformBuffer:
		return BufferUsageFlags::UNIFORM_BUFFER;
	case diverse::rhi::AnyShaderReadUniformBufferOrVertexBuffer:
		return BufferUsageFlags::UNIFORM_BUFFER | BufferUsageFlags::VERTEX_BUFFER;
	case diverse::rhi::TransferRead:
		return BufferUsageFlags::TRANSFER_SRC;
	case diverse::rhi::VertexShaderWrite:
	case diverse::rhi::TessellationControlShaderWrite:
	case diverse::rhi::TessellationEvaluationShaderWrite:
	case diverse::rhi::GeometryShaderWrite:
	case diverse::rhi::FragmentShaderWrite:
	case diverse::rhi::ComputeShaderWrite:
	case diverse::rhi::AnyShaderWrite:
		return BufferUsageFlags::STORAGE_BUFFER;
	case diverse::rhi::TransferWrite:
		return BufferUsageFlags::TRANSFER_DST;
	case diverse::rhi::General:
		return BufferUsageFlags::STORAGE_BUFFER;
	default:
		return BufferUsageFlags::UNIFORM_BUFFER;
	}
}

namespace diverse
{
	void rhi::GpuBuffer::copy_to(const struct GpuDevice* device, u8* dst_data, u32 dst_bytes, u32 src_offset)
	{
		data_buf = map(device);
		memcpy(dst_data, data_buf + src_offset, dst_bytes);
		unmap(device);
	}

	void rhi::GpuBuffer::copy_from(const struct GpuDevice* device, const u8* src_data, u32 src_bytes, u32 dst_offset)
	{
		//data_buf
		data_buf = map(device);
		memcpy(data_buf + dst_offset, src_data, src_bytes);
		unmap(device);
	}
}
