#pragma  once
#include "backend/drs_rhi/gpu_render_pass.h"
#include "dvs_metal_utils.h"
namespace diverse
{
    namespace  rhi
    {
        struct FrameBufferCacheMetal
        {
            // std::unordered_map<FramebufferCacheKey, MTLRenderPassDescriptor*> cache;
        };

        struct RenderPassMetal : public RenderPass
        {
            void begin_render_pass() override;
            void end_render_pass() override;
            
            MTLRenderPassDescriptor* render_pass;
            RenderPassDesc  desc;
            // u32				color_attachment_count;
        };
    
        auto to_metal_load_op(u32 load_op)->MTLLoadAction;
        auto to_metal_store_op(u32 store_op)->MTLStoreAction;
    }
}
