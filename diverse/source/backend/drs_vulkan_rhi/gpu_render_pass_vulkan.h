#pragma once
#include "backend/drs_rhi/gpu_render_pass.h"
#include "gpu_texture_vulkan.h"
#include <unordered_map>

namespace diverse
{
    namespace rhi
    {

        inline auto to_vk(RenderPassAttachmentDesc render_pass_desc, VkImageLayout initial_layout, VkImageLayout final_layout) ->VkAttachmentDescription
        {
            VkAttachmentDescription desc = {};
            desc.format = pixel_format_2_vk(render_pass_desc.format);
            desc.loadOp = (VkAttachmentLoadOp)render_pass_desc.load_op;
            desc.storeOp = (VkAttachmentStoreOp)render_pass_desc.store_op;
            desc.initialLayout = initial_layout;
            desc.finalLayout = final_layout;
            desc.samples = (VkSampleCountFlagBits)(render_pass_desc.samples);

            return desc;
        }

        struct FramebufferCacheKey
        {
            std::array<uint32, 2>	dims;

            std::vector<std::pair<TextureUsageFlags, TextureCreateFlags>> attachments;

            FramebufferCacheKey(const std::array<uint32, 2>& dim,
                const std::vector<GpuTexture*>& color_attachments,
                const GpuTexture* depth_stencil_attachment)
                : dims(dim)
            {
                for (const auto& tex : color_attachments)
                {
                    attachments.push_back(
                        {tex->desc.usage, tex->desc.flags}
                    );
                }
                attachments.push_back(
                    { depth_stencil_attachment->desc.usage, depth_stencil_attachment->desc.flags }
                );
            }
            FramebufferCacheKey(){}
        };
    }
}

template<>
struct std::equal_to<diverse::rhi::FramebufferCacheKey>
{
    bool operator()(const diverse::rhi::FramebufferCacheKey& _Left, const diverse::rhi::FramebufferCacheKey& _Right) const noexcept
    {
        bool bRet = _Left.dims[0] == _Right.dims[0]
            && _Left.dims[1] == _Right.dims[1]
            && _Left.attachments.size() == _Right.attachments.size();

        for (auto i = 0; i < _Left.attachments.size(); i++)
        {
            bRet &= (uint32)(_Left.attachments[i].first) == (uint32)(_Right.attachments[i].first);
            bRet &= (uint32)(_Left.attachments[i].second) == (uint32)(_Right.attachments[i].second);
        }
        return bRet;
    }
};

template<>
struct std::hash<diverse::rhi::FramebufferCacheKey>
{
    std::size_t operator()(diverse::rhi::FramebufferCacheKey const& s) const noexcept
    {
        auto hash_code = diverse::hash<u32>()(uint32(s.dims[0]));
        diverse::hash_combine(hash_code, uint32(s.dims[1]));
        for (auto [usga, flag] : s.attachments)
        {
            diverse::hash_combine(hash_code, (uint32)usga);
            diverse::hash_combine(hash_code, (uint32)flag);
        }
        return hash_code;
    }
};

namespace diverse
{
    namespace rhi
    {
        struct FrameBufferCache
        {
            std::unordered_map<FramebufferCacheKey, VkFramebuffer>	entries;
            std::vector<RenderPassAttachmentDesc> attachment_desc;
            VkRenderPass	render_pass;
            uint32				color_attachment_count;

            FrameBufferCache() = default;
            FrameBufferCache(VkRenderPass render_pass, const std::vector<RenderPassAttachmentDesc>& color_attachments,const std::optional<RenderPassAttachmentDesc>& depth_attachment);
            ~FrameBufferCache();
            auto get_or_create(const struct GpuDeviceVulkan& device, const FramebufferCacheKey& key) -> VkFramebuffer;
        };


        struct RenderPassVulkan : public RenderPass
        {
            ~RenderPassVulkan();
            VkRenderPass	render_pass;
            FrameBufferCache	framebuffer_cache;

            void begin_render_pass() override;
            void end_render_pass() override;
        };
    }
}