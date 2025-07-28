#ifdef DS_RENDER_METAL
#include "gpu_buffer_metal.h"

namespace diverse
{
    namespace rhi
    {
        GpuBufferMetal::GpuBufferMetal(const GpuBufferDesc& desc, id<MTLBuffer> buf)
            : GpuBuffer(desc), id_buf(buf)
        {
        }
        
        u64	GpuBufferMetal::device_address(const struct GpuDevice* device)
        {
            return id_buf.gpuAddress;
        }
        
        auto GpuBufferMetal::map(const GpuDevice* device) -> u8*
        {
            data_buf = static_cast<u8*>(id_buf.contents);
            return data_buf;
        }

        auto GpuBufferMetal::unmap(const GpuDevice* device) -> void
        {
         
        }

    }
}

#endif