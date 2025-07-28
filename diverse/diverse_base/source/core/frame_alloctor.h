#pragma once
#include "core/base_type.h"
#include <vector>
namespace diverse
{
    struct FrameAllocator
    {
        //u8 data[1024*1024*256];
        std::vector<u8>	data;
        u32 buf_offset = 0;
        u32 capcity = 1024 * 1024 * 256;
        FrameAllocator()
        {
            data.resize(capcity);
        }

        template<typename T>
        T* allocate()
        {
            assert(buf_offset < capcity);
            T* t_data = reinterpret_cast<T*>(data.data() + buf_offset);
            buf_offset += sizeof(T);
            return t_data;
        }

        template<typename T>
        T* allocate(T const& t)
        {
            assert(buf_offset < capcity);
            auto t_size = sizeof(T);
            T* t_data = reinterpret_cast<T*>(data.data() + buf_offset);
            memcpy(t_data, &t, t_size);
            buf_offset += t_size;
            return t_data;
        }

        void free()
        {
            buf_offset = 0;
        }
    };
}