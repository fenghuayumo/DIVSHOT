#include "gaussian_gpu_utils.h"

#include "assets/gaussian_model.h"
#include "assets/gpu_assets.h"
#include "utility/pack_utils.h"
#include "utility/thread_pool.h"
#include "core/base_type.h"
#include <algorithm>
#include <cmath>

namespace diverse
{
    namespace
    {
        auto sigmoid = [](float v) {
            if (v > 0)
                return 1.0f / (1.0f + std::exp(-v));
            const float t = std::exp(v);
            return t / (1.0f + t);
        };
    }

    GaussianPackedCpuData pack_gaussian_asset(const GaussianAsset& asset)
    {
        GaussianPackedCpuData packed;
        const size_t count = asset.pos.size();
        packed.gaussians.resize(count);
        packed.sh_0.resize(count);
        packed.sh_n.resize(count);

        constexpr float SH_C0 = 0.28209479177387814f;
        parallel_for<size_t>(0, count, [&](size_t k) {
            Gaussian& gaussian = packed.gaussians[k];
            glm::uvec2& gs_color = packed.sh_0[k];

            gaussian.position.xyz = asset.pos[k];

            float length2 = 0.0f;
            for (int j = 0; j < 4; ++j)
                length2 += asset.rot[k][j] * asset.rot[k][j];
            const float length = std::sqrt(length2);
            glm::vec4 rot_t;
            for (int j = 0; j < 4; ++j)
                rot_t[j] = asset.rot[k][j] / length;

            const auto rotation0 = glm::packHalf2x16(glm::vec2(rot_t[0], rot_t[1]));
            const auto rotation1 = glm::packHalf2x16(glm::vec2(rot_t[2], rot_t[3]));

            glm::vec3 scale_t;
            for (int j = 0; j < 3; ++j)
                scale_t[j] = std::exp(asset.scales[k][j]);
            const auto scale0 = glm::packHalf2x16(glm::vec2(scale_t[0], scale_t[1]));
            const auto scale1 = glm::packHalf2x16(glm::vec2(scale_t[2], sigmoid(asset.opacities[k])));

            gaussian.rotation_scale = glm::uvec4(rotation0, rotation1, scale0, scale1);

            const float r = (asset.shs_0[k][0] * SH_C0 + 0.5f);
            const float g = (asset.shs_0[k][1] * SH_C0 + 0.5f);
            const float b = (asset.shs_0[k][2] * SH_C0 + 0.5f);
            gs_color.x = glm::packHalf2x16(glm::vec2(r, g));
            gs_color.y = glm::packHalf2x16(glm::vec2(b, 0.0f));

            std::array<float, 45> c{};
            for (auto j = 0; j < 15; ++j)
            {
                c[j * 3 + 0] = asset.shs_n[k][j * 3 + 0];
                c[j * 3 + 1] = asset.shs_n[k][j * 3 + 1];
                c[j * 3 + 2] = asset.shs_n[k][j * 3 + 2];
            }

            float max_val = c[0];
            for (auto j = 1; j < 15 * 3; ++j)
                max_val = std::max(max_val, std::abs(c[j]));

            if (max_val != 0.0f)
            {
                for (auto j = 0; j < 15; ++j)
                {
                    c[j * 3 + 0] /= max_val;
                    c[j * 3 + 1] /= max_val;
                    c[j * 3 + 2] /= max_val;
                }
            }

            auto& sh1to3 = packed.sh_n[k].sh1to3;
            sh1to3.x = *reinterpret_cast<u32*>(&max_val);
            sh1to3.y = pack_unit_direction_11_10_11(glm::vec3(c[0], c[1], c[2]));
            sh1to3.z = pack_unit_direction_11_10_11(glm::vec3(c[3], c[4], c[5]));
            sh1to3.w = pack_unit_direction_11_10_11(glm::vec3(c[6], c[7], c[8]));

            auto& sh4to7 = packed.sh_n[k].sh4to7;
            sh4to7.x = pack_unit_direction_11_10_11(glm::vec3(c[9], c[10], c[11]));
            sh4to7.y = pack_unit_direction_11_10_11(glm::vec3(c[12], c[13], c[14]));
            sh4to7.z = pack_unit_direction_11_10_11(glm::vec3(c[15], c[16], c[17]));
            sh4to7.w = pack_unit_direction_11_10_11(glm::vec3(c[18], c[19], c[20]));

            auto& sh8to11 = packed.sh_n[k].sh8to11;
            sh8to11.x = pack_unit_direction_11_10_11(glm::vec3(c[21], c[22], c[23]));
            sh8to11.y = pack_unit_direction_11_10_11(glm::vec3(c[24], c[25], c[26]));
            sh8to11.z = pack_unit_direction_11_10_11(glm::vec3(c[27], c[28], c[29]));
            sh8to11.w = pack_unit_direction_11_10_11(glm::vec3(c[30], c[31], c[32]));

            auto& sh12to15 = packed.sh_n[k].sh12to15;
            sh12to15.x = pack_unit_direction_11_10_11(glm::vec3(c[33], c[34], c[35]));
            sh12to15.y = pack_unit_direction_11_10_11(glm::vec3(c[36], c[37], c[38]));
            sh12to15.z = pack_unit_direction_11_10_11(glm::vec3(c[39], c[40], c[41]));
            sh12to15.w = pack_unit_direction_11_10_11(glm::vec3(c[42], c[43], c[44]));
        });

        return packed;
    }

    void upload_gaussian_state_buffer(
        rhi::GpuDevice* device,
        rhi::GpuBuffer* state_buf,
        size_t splat_count,
        const std::vector<uint8_t>& splat_state,
        const std::vector<uint16_t>& splat_transform_index,
        bool initialize_zero)
    {
        if (!device || !state_buf || splat_count == 0)
            return;

        auto* states_data = reinterpret_cast<u32*>(state_buf->map(device));
        if (!states_data)
            return;

        parallel_for<size_t>(0, splat_count, [&](size_t i) {
            if (initialize_zero)
            {
                states_data[i] = 0;
                return;
            }

            uint state = states_data[i];
            if (i < splat_state.size())
                state = setOpState(state, splat_state[i]);
            if (i < splat_transform_index.size())
                state = setTransformIndex(state, splat_transform_index[i]);
            states_data[i] = state;
        });

        state_buf->unmap(device);
    }

    GaussianBufferUpload upload_gaussian_buffers(
        rhi::GpuDevice* device,
        const GaussianPackedCpuData& packed,
        const GaussianBufferUpload* existing,
        size_t active_splat_count,
        int max_splats,
        bool compact,
        const std::vector<uint8_t>& splat_state,
        const std::vector<uint16_t>& splat_transform_index)
    {
        GaussianBufferUpload result;
        if (!device || active_splat_count == 0 || packed.gaussians.empty())
            return result;

        const bool can_refresh = existing
            && existing->gaussians_buf
            && existing->gaussians_buf->desc.size >= active_splat_count * sizeof(Gaussian);

        if (can_refresh)
        {
            result = *existing;

            {
                auto* data = reinterpret_cast<Gaussian*>(result.gaussians_buf->map(device));
                parallel_for<size_t>(0, active_splat_count, [&](size_t i) {
                    data[i] = packed.gaussians[i];
                });
                result.gaussians_buf->unmap(device);
            }
            {
                auto* data = reinterpret_cast<PackedVertexColor*>(result.sh_0_buf->map(device));
                parallel_for<size_t>(0, active_splat_count, [&](size_t i) {
                    data[i] = packed.sh_0[i];
                });
                result.sh_0_buf->unmap(device);
            }
            {
                auto* data = reinterpret_cast<PackedVertexSH*>(result.sh_n_buf->map(device));
                parallel_for<size_t>(0, active_splat_count, [&](size_t i) {
                    data[i] = packed.sh_n[i];
                });
                result.sh_n_buf->unmap(device);
            }
            upload_gaussian_state_buffer(
                device,
                result.state_buf.get(),
                active_splat_count,
                splat_state,
                splat_transform_index,
                false);

            result.gpu_memory_size = existing->gpu_memory_size;
            return result;
        }

        const int scale_factor = static_cast<int>(std::ceil(
            static_cast<double>(active_splat_count) / static_cast<double>(std::max(1, max_splats))));
        const size_t num_gaussians = compact ? active_splat_count : static_cast<size_t>(scale_factor * max_splats);

        const auto buffer_flags = rhi::BufferUsageFlags::STORAGE_BUFFER
            | rhi::BufferUsageFlags::VERTEX_BUFFER
            | rhi::BufferUsageFlags::TRANSFER_DST;

        result.gaussians_buf = device->create_buffer(
            rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(Gaussian), buffer_flags),
            "gaussian_buf",
            nullptr);
        result.sh_0_buf = device->create_buffer(
            rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(PackedVertexColor), buffer_flags),
            "gaussian_sh_0_buf",
            nullptr);
        result.sh_n_buf = device->create_buffer(
            rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(PackedVertexSH), buffer_flags),
            "gaussian_sh_n_buf",
            nullptr);
        result.points_key_buf = device->create_buffer(
            rhi::GpuBufferDesc::new_gpu_only(num_gaussians * sizeof(u32), buffer_flags),
            "points_key_buf",
            nullptr);
        result.points_value_buf = device->create_buffer(
            rhi::GpuBufferDesc::new_gpu_only(num_gaussians * sizeof(u32), buffer_flags),
            "points_value_buf",
            nullptr);
        result.state_buf = device->create_buffer(
            rhi::GpuBufferDesc::new_cpu_to_gpu(num_gaussians * sizeof(u32), buffer_flags),
            "gaussian_state_buf",
            nullptr);

        {
            auto* data = reinterpret_cast<Gaussian*>(result.gaussians_buf->map(device));
            parallel_for<size_t>(0, active_splat_count, [&](size_t i) {
                data[i] = packed.gaussians[i];
            });
            result.gaussians_buf->unmap(device);
        }
        {
            auto* data = reinterpret_cast<PackedVertexColor*>(result.sh_0_buf->map(device));
            parallel_for<size_t>(0, active_splat_count, [&](size_t i) {
                data[i] = packed.sh_0[i];
            });
            result.sh_0_buf->unmap(device);
        }
        {
            auto* data = reinterpret_cast<PackedVertexSH*>(result.sh_n_buf->map(device));
            parallel_for<size_t>(0, active_splat_count, [&](size_t i) {
                data[i] = packed.sh_n[i];
            });
            result.sh_n_buf->unmap(device);
        }
        upload_gaussian_state_buffer(
            device,
            result.state_buf.get(),
            active_splat_count,
            splat_state,
            splat_transform_index,
            true);

        result.allocated_splat_count = num_gaussians;
        result.gpu_memory_size =
            num_gaussians * (sizeof(Gaussian) + sizeof(PackedVertexColor) + sizeof(PackedVertexSH) + sizeof(u32))
            + num_gaussians * sizeof(u32) * 2;
        return result;
    }

    GaussianGpu make_gaussian_gpu(
        const GaussianBufferUpload& upload,
        uint32_t resident_version,
        uint32_t bindless_slot)
    {
        GaussianGpu gpu;
        gpu.gaussians_buf = upload.gaussians_buf;
        gpu.sh_0_buf = upload.sh_0_buf;
        gpu.sh_n_buf = upload.sh_n_buf;
        gpu.state_buf = upload.state_buf;
        gpu.points_key_buf = upload.points_key_buf;
        gpu.points_value_buf = upload.points_value_buf;
        gpu.resident_version = resident_version;
        gpu.bindless_slot = bindless_slot;
        gpu.gpu_memory_size = upload.gpu_memory_size;
        return gpu;
    }

} // namespace diverse
