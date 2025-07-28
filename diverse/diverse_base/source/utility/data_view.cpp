#include "data_view.h"

namespace diverse
{
    DataView::DataView(u64 size)
    {
        buffer.resize(size);
    }
    DataView::DataView(u8* data, u64 size)
    {
        buffer.resize(size);
        memcpy(buffer.data(), data, size);
    }
    void DataView::setFloat32(u64 offset, float v)
    {  
        memcpy(buffer.data() + offset, &v, sizeof(float));
    }

    void DataView::setUint8(u64 offset, u8 v)
    {
        // memcpy(buffer.data() + offset, &v, sizeof(u8));
        buffer[offset] = v;
    }

    void DataView::setUint32(u64 offset, u32 v)
    {
        memcpy(buffer.data()+offset, &v, sizeof(u32));
    }

    u8 DataView::getUint8(u64 offset)
    {
       return buffer[offset];
    }

    u32 DataView::getUint32(u64 offset)
    {
        u32 v = 0;
        memcpy(&v, buffer.data() + offset, sizeof(u32));
        return v;    
    }

    float DataView::getFloat32(u64 offset)
    {
        float v = 0;
        memcpy(&v, buffer.data() + offset, sizeof(float));
        return v;
    }

}