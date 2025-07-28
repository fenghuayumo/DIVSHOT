#pragma once

namespace diverse
{
    namespace rhi
    {
        struct  CommandBuffer
        {
            /* data */

            virtual auto begin()->void {}
            virtual auto end()->void{}
            virtual auto wait()->void{}
        };
        
    }
}
