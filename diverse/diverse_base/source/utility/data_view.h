#pragma once
#include "core/core.h"
#include <vector>
namespace diverse
{
    struct DataView
    {
        DataView(u64 size);
        DataView(u8* data, u64 size);
        // void append();
        void setFloat32(u64 offset, float v);
        void setUint8(u64 offset,u8 v);
        void setUint32(u64 offset,u32 v);
        u8 getUint8(u64 offset);
        u32 getUint32(u64 offset);
        float getFloat32(u64 offset);

        size_t  size() const {return buffer.size(); }
        u8*  data() {return buffer.data(); }
    protected:
        std::vector<u8> buffer;
        u64 cur_offset = 0;
    };
}