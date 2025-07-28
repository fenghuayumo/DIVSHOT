#pragma once
#include "gpu_buffer.h"
#include "utility/concepts_utils.h"
namespace diverse
{
    namespace rhi
    {
        struct DynamicConstants 
        {
            std::shared_ptr<GpuBuffer> buffer;
            struct GpuDevice* device; 
            uint64 frame_offset_bytes = 0;
            uint32 frame_parity = 0;
            
            auto advance_frame() -> void
            {
                this->frame_parity = (this->frame_parity + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT;
                this->frame_offset_bytes = 0;
            }
            auto current_offset() -> uint32
            {
               return (this->frame_parity * DYNAMIC_CONSTANTS_SIZE_BYTES + this->frame_offset_bytes);
            }
            auto current_device_address(const struct GpuDevice* device) -> uint64
            {
               return this->buffer->device_address(device) + this->current_offset();
            }

            template<typename T>
            auto push(T const& t) -> uint32
            {
                auto t_size = sizeof(T);
                assert( (frame_offset_bytes + t_size) < DYNAMIC_CONSTANTS_SIZE_BYTES);
                auto buffer_offset = current_offset();
                if constexpr (is_tuple<T>::value) 
                {
                    auto buffer_ptr = buffer->map(device);
                    auto src_offset = 0;
                    loop(std::make_index_sequence<std::tuple_size<T>::value>{},
                        [&]<std::size_t i>() {
                        auto value = std::get<i>(t);
                        std::memcpy(buffer_ptr + buffer_offset + src_offset, &value, sizeof(value));
                        src_offset += sizeof(value);
                    });
                    buffer->unmap(device);
                }
                else
                    buffer->copy_from(device,reinterpret_cast<const u8*>(&t), t_size, buffer_offset);
                auto t_size_aligned = (t_size + DYNAMIC_CONSTANTS_ALIGNMENT - 1) & ~(DYNAMIC_CONSTANTS_ALIGNMENT - 1);
                frame_offset_bytes += t_size_aligned;
                
                return buffer_offset;
            }

            template<typename T>
            auto push_from_vec(const std::vector<T>& vec) -> uint32
            {
                auto t_size = sizeof(T);
                auto t_align = 4;
                assert((frame_offset_bytes + t_size) < DYNAMIC_CONSTANTS_SIZE_BYTES);
                assert(DYNAMIC_CONSTANTS_ALIGNMENT % t_align == 0);

                auto buffer_offset = current_offset();
                auto dst_offset = buffer_offset;
                buffer->copy_from(device, reinterpret_cast<const u8*>(vec.data()), t_size * vec.size(), dst_offset);
                dst_offset += t_size * vec.size();
                frame_offset_bytes += (dst_offset - buffer_offset + DYNAMIC_CONSTANTS_ALIGNMENT - 1) & ~(DYNAMIC_CONSTANTS_ALIGNMENT - 1);
                return buffer_offset;
            }

        };
    }
}