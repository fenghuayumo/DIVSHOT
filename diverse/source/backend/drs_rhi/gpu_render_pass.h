#pragma once

#include "pixel_format.h"
#include "gpu_texture.h"
#include <vector>
#include <array>
#include <optional>
namespace diverse
{
    namespace rhi
    {
        enum AttachmentLoadOp
        {
            LOAD_OP_LOAD = 0,
            LOAD_OP_CLEAR = 1,
            LOAD_OP_DONT_CARE = 2
        };

        enum AttachmentStoreOp
        {
            STORE_OP_STORE = 0,
            STORE_OP_DONT_CARE = 1
        };

        enum SampleCountFlag
        {
           SAMPLE_COUNT_1 = 0x00000001,
           SAMPLE_COUNT_2 = 0x00000002,
           SAMPLE_COUNT_4 = 0x00000004,
           SAMPLE_COUNT_8 = 0x00000008,
           SAMPLE_COUNT_16 = 0x00000010,
           SAMPLE_COUNT_32 = 0x00000020,
           SAMPLE_COUNT_64 = 0x00000040
        };

        struct RenderPassAttachmentDesc
        {
            PixelFormat	format;
            AttachmentLoadOp	load_op;
            AttachmentStoreOp	store_op;
            SampleCountFlag	samples;

            static auto create(PixelFormat format) -> RenderPassAttachmentDesc
            {
                return {
                    format,
                    AttachmentLoadOp::LOAD_OP_LOAD,
                    AttachmentStoreOp::STORE_OP_STORE,
                    SampleCountFlag::SAMPLE_COUNT_1,
                };
            }

            inline auto discard_output() -> RenderPassAttachmentDesc
            {
                store_op = AttachmentStoreOp::STORE_OP_DONT_CARE;
                return *this;
            }

            inline auto clear_input() -> RenderPassAttachmentDesc
            {
                load_op = AttachmentLoadOp::LOAD_OP_CLEAR;
                return *this;
            }

            inline auto discard_input() -> RenderPassAttachmentDesc
            {
                load_op = AttachmentLoadOp::LOAD_OP_DONT_CARE;
                return *this;
            }
            inline auto load_input()->RenderPassAttachmentDesc
            {
                load_op = AttachmentLoadOp::LOAD_OP_LOAD;
                return *this;
            }
        };

        struct RenderPassDesc
        {
            std::vector<RenderPassAttachmentDesc>	color_attachments;
            std::optional<RenderPassAttachmentDesc>	depth_attachment;
        };

        struct RenderPass
        {
            /*FrameBufferCache*/ 
            virtual void begin_render_pass() = 0;
            virtual void end_render_pass() = 0;
        };
    }
}