 #pragma once
 #include "gpu_device.h"

 namespace diverse
 {

 struct PendingBufferUpload
 {
    std::vector<u8> data;
    u64 offset;
 };

 class BufferBuilder
 {
     /* data */
     std::vector<PendingBufferUpload> pending_uploads;
     u64 cur_offset = 0;
public:
     auto current_offset()->u64
     {
        return cur_offset;
     }

     template<typename T>
     auto append(const std::vector<T>& data,u64 alignment)->u64
     {
         auto data_start = (cur_offset + alignment - 1) & ~(alignment - 1);
         auto data_len = data.size() * sizeof(T);

         std::vector<u8> data_bytes(data_len);
         memcpy(data_bytes.data(), data.data(), data_len);
         pending_uploads.push_back({ data_bytes, data_start});
         cur_offset = data_start + data_len;

         return data_start;
     }

     template<typename T>
     auto append(const T* data,u64 size, u64 alignment) -> u64
     {
         auto data_start = (cur_offset + alignment - 1) & ~(alignment - 1);
         auto data_len = size;

         std::vector<u8> data_bytes(data_len);
         memcpy(data_bytes.data(), data, data_len);
         pending_uploads.push_back({ data_bytes, data_start });
         cur_offset = data_start + data_len;

         return data_start;
     }

     auto upload(rhi::GpuDevice* device,
                 rhi::GpuBuffer* buffer,
                 u64 buffer_offset)->void
     {
         const u64 STAGING_BYTES = 16 * 1024 * 1024;
         auto buffer_desc = rhi::GpuBufferDesc::new_cpu_to_gpu(
             STAGING_BYTES,
             rhi::BufferUsageFlags::TRANSFER_SRC
         );

         auto staging_buffer = device->create_buffer(buffer_desc, 
             "BufferBuilder staging",
             nullptr);

         struct UploadChunk {
             u64 pending_idx;
             u64 start;
             u64 end;
         };

         std::vector<UploadChunk> chunks;
         for (auto pending_idx = 0; pending_idx< pending_uploads.size();pending_idx++)
         {
            auto& pending = pending_uploads[pending_idx];
            auto byte_count = pending.data.size();
            auto chunk_count = (byte_count + STAGING_BYTES - 1) / STAGING_BYTES;
            for(auto chunk =0;chunk<chunk_count;chunk++)
            { 
                chunks.push_back(UploadChunk{
                    (u64)pending_idx,
                    chunk * STAGING_BYTES,
                    std::min<u64>( (chunk+1) * STAGING_BYTES, byte_count)
                });
            }
         }

         for (auto& [pending_idx, range_start, range_end] : chunks)
         {
             auto& pending = pending_uploads[pending_idx];
             auto data = staging_buffer->map(device);
             memcpy(data, pending.data.data() + range_start, range_end-range_start);
             staging_buffer->unmap(device);

             device->with_setup_cb([&](rhi::CommandBuffer* cb) {
                
                device->copy_buffer(cb, staging_buffer.get(), 0, buffer, buffer_offset + pending.offset + range_start, range_end - range_start);
             });
         }
     }
 };
 }
