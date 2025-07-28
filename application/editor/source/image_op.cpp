#include "image_op.h"
#include <utility/thread_pool.h>
#include <backend/drs_rhi/gpu_device.h>
namespace diverse
{
    ImagePaintOperation::ImagePaintOperation(std::shared_ptr<rhi::GpuTexture>& img_ptr,
        const ImagePaintColorAdjustment& state,
        const std::vector<int>& pts)
        : image_texture(img_ptr), 
        paint_state(state),
        paint_points(pts)
    {
		// Initialize paint points or other necessary data
	}

    auto ImagePaintOperation::apply() -> void
    {
        // Apply the paint operation to the image using the paint points
        auto device = get_global_device();
        rhi::TextureRegion tex_region;
        auto w = image_texture->desc.extent[0];
        auto h = image_texture->desc.extent[1];
        std::vector<u8> img_data = device->export_image(image_texture.get());
        parallel_for<size_t>(0, paint_points.size(), [&](size_t i) {
			auto& point = paint_points[i];
            if(point <= 0) return;
            auto stride = 4;
            auto pixel_ptr = (u8*)(img_data.data() + i * stride);
            auto data = glm::vec3(pixel_ptr[0], pixel_ptr[1], pixel_ptr[2]) / 255.0f;
            data = (1-paint_state.mix_weight) * data + paint_state.color * paint_state.mix_weight;
            pixel_ptr[0] = (u8)(data.x * 255);
            pixel_ptr[1] = (u8)(data.y * 255);
            pixel_ptr[2] = (u8)(data.z * 255);
		});
        rhi::ImageSubData update_data = {img_data.data(),(u32)img_data.size(),w * 4, w * h * 4};
        device->update_texture(image_texture.get(),{update_data},tex_region);
    }

    auto ImagePaintOperation::undo() -> void
    {
        // Undo the paint operation by removing the paint points
        auto device = get_global_device();
        const auto eps = 1.0f/ 256.f;
        auto w = image_texture->desc.extent[0];
        auto h = image_texture->desc.extent[1];
        std::vector<u8> img_data = device->export_image(image_texture.get());
        rhi::TextureRegion tex_region;
        parallel_for<size_t>(0, paint_points.size(), [&](size_t i) {
            auto& point = paint_points[i];
            if (point <= 0) return;
            auto stride = 4;
            auto pixel_ptr = (u8*)(img_data.data() + i * stride);
            auto pixel = glm::vec3(pixel_ptr[0], pixel_ptr[1], pixel_ptr[2]) / 255.0f;
            auto data = (pixel - paint_state.color.x * paint_state.mix_weight) / std::max<f32>(paint_state.mix_weight, eps);
            pixel_ptr[0] = (u8)(data.x * 255);
            pixel_ptr[1] = (u8)(data.y * 255);
            pixel_ptr[2] = (u8)(data.z * 255);
        });

        rhi::ImageSubData update_data = { img_data.data(), (u32)img_data.size(),w * 4, w * h * 4 };
        device->update_texture(image_texture.get(), {update_data}, tex_region);
    }
}