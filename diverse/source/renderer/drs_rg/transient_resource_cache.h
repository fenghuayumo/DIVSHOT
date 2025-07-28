#pragma once
#include "backend/drs_rhi/gpu_buffer.h"
#include "backend/drs_rhi/gpu_texture.h"
#include <optional>
namespace diverse
{
    namespace rg
    {
        struct TransientResourceCache
        {
            std::unordered_map<rhi::GpuTextureDesc,std::deque<std::shared_ptr<rhi::GpuTexture>>> images;
            std::unordered_map<rhi::GpuBufferDesc,std::deque<std::shared_ptr<rhi::GpuBuffer>>>   buffers;
        
            auto get_image(const rhi::GpuTextureDesc& desc)->std::optional<std::shared_ptr<rhi::GpuTexture>>
            {
                auto it = images.find(desc);
                if( it != images.end())
                {
                    if(!it->second.empty())
                    {
                        auto back =  it->second.front();
                        it->second.pop_front();
                        return back;
                    }
                }   
                return {};
            }

            auto get_or_insert_image(const rhi::GpuTextureDesc& desc,const std::shared_ptr<rhi::GpuTexture>& texture)->std::optional<std::shared_ptr<rhi::GpuTexture>>
            {
                auto it = images.find(desc);
                if( it != images.end())
                {
                    auto back =  it->second.front();
                    it->second.pop_front();
                    return back;
                }
                images[desc].push_back(texture);
                return texture;
            }

            auto insert_image(const std::shared_ptr<rhi::GpuTexture>& image)
            {
                auto it = images.find(image->desc);
                if( it != images.end())
                {
                    it->second.push_back(image);
                }else
                {
                    images.insert({image->desc, {image}});
                }
            }

            auto get_buffer(const rhi::GpuBufferDesc& desc)->std::optional<std::shared_ptr<rhi::GpuBuffer>>
            {
               auto it = buffers.find(desc);
                if( it != buffers.end())
                {
                    if(!it->second.empty())
                    { 
                        auto back =  it->second.front();
                        it->second.pop_front();
                        return back;
                    }
                }   
                return {};
            }

            auto insert_buffer(const std::shared_ptr<rhi::GpuBuffer>& buffer)->void
            {
                auto it = buffers.find(buffer->desc);
                if( it != buffers.end())
                {
                    it->second.push_back(buffer);
                }else
                {
                    buffers.insert({buffer->desc, {buffer}});
                }
            }

            auto get_or_insert_buffer(const rhi::GpuBufferDesc& desc,const std::shared_ptr<rhi::GpuBuffer>& buffer)->std::optional<std::shared_ptr<rhi::GpuBuffer>>
            {
                auto it = buffers.find(desc);
                if( it != buffers.end())
                {
                    auto back =  it->second.front();
                    it->second.pop_front();
                    return back;
                }
                buffers[desc].push_back(buffer);
                return buffer;
            }

            auto clear_resource()
            {
                images.clear();
				buffers.clear();
            }
        };
    }
}