#pragma once

#include "edit_op.h"
namespace diverse
{
    namespace rhi
    {
		struct GpuBuffer;
		struct GpuTexture;
	};
    enum class ImageEditOpType
    {
        Paint = 0,
        None,
    };
    class ImageEditOperation : public EditOperation
    {
    public:
        ImageEditOperation() = default;
        virtual ~ImageEditOperation() = default;
        virtual void apply() = 0;
        virtual void undo() = 0;
    };

    struct EditGpuTexture
    {
        std::shared_ptr<rhi::GpuTexture> image_texture;
        std::shared_ptr<rhi::GpuBuffer> image_buffer;
    };

    struct ImagePaintColorAdjustment
    {
        glm::vec3 color = glm::vec3(1,1,1);
        f32 mix_weight = 1.0f;
    };
    class ImagePaintOperation : public ImageEditOperation
    {
    public:
        ImagePaintOperation(std::shared_ptr<rhi::GpuTexture>& image_texture,
                            const ImagePaintColorAdjustment& state,
                            const std::vector<int>& paint_points);
        ~ImagePaintOperation() override = default;
        void apply() override;
        void undo() override;
    protected:
        std::vector<int> paint_points;
        std::shared_ptr<rhi::GpuTexture> image_texture;
        ImagePaintColorAdjustment paint_state;
    };
}