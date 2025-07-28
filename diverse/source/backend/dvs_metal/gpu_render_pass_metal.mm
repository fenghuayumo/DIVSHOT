#ifdef DS_RENDER_API_METAL
#include "gpu_render_pass_metal.h"

namespace diverse
{
    namespace rhi
    {
        auto to_metal_load_op(u32 loadop)->MTLLoadAction
        {
            switch (loadop) {
                case 0:
                    return  MTLLoadActionLoad;
                case 1:
                    return MTLLoadActionClear;
                case 2:
                    return MTLLoadActionDontCare;
            }
            return MTLLoadActionClear;
        }
        
        auto to_metal_store_op(u32 storeop)->MTLStoreAction
        {
            switch (storeop) {
                case 0:
                    return  MTLStoreActionStore;
                case 1:
                    return MTLStoreActionDontCare;
                default:
                    break;
            }
            return MTLStoreActionStore;
        }
        void RenderPassMetal::begin_render_pass()
        {
            
        }
    
        void RenderPassMetal::end_render_pass()
        {
            
        }
    }
}
#endif