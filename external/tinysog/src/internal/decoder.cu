/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "decoder.h"
#include "compression.cuh"
#include "cuda_utils.cuh"
#include <algorithm>

#include "tinysog/expected.h"
namespace tinysog {
    namespace internal {

        namespace {

            /**
             * @brief Decode positions CUDA kernel
             */
            __global__ void decode_positions_kernel(
                const uint8_t* __restrict__ means_l,
                const uint8_t* __restrict__ means_u,
                const float* __restrict__ min_vals,
                const float* __restrict__ max_vals,
                float* __restrict__ positions,
                const int n) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int pixel_idx = idx * 4;

                // Reconstruct 16-bit values
                uint16_t x16 = means_l[pixel_idx + 0] | (means_u[pixel_idx + 0] << 8);
                uint16_t y16 = means_l[pixel_idx + 1] | (means_u[pixel_idx + 1] << 8);
                uint16_t z16 = means_l[pixel_idx + 2] | (means_u[pixel_idx + 2] << 8);

                // Denormalize
                float x_norm = x16 / 65535.0f;
                float y_norm = y16 / 65535.0f;
                float z_norm = z16 / 65535.0f;

                float x_log = x_norm * (max_vals[0] - min_vals[0]) + min_vals[0];
                float y_log = y_norm * (max_vals[1] - min_vals[1]) + min_vals[1];
                float z_log = z_norm * (max_vals[2] - min_vals[2]) + min_vals[2];

                // Inverse log transform
                positions[idx * 3 + 0] = inverse_log_transform(x_log);
                positions[idx * 3 + 1] = inverse_log_transform(y_log);
                positions[idx * 3 + 2] = inverse_log_transform(z_log);
            }

            /**
             * @brief Decode quaternions CUDA kernel
             */
            __global__ void decode_quaternions_kernel(
                const uint8_t* __restrict__ quats,
                float* __restrict__ rotations,
                const int n) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int pixel_idx = idx * 4;

                uint8_t a = quats[pixel_idx + 0];
                uint8_t b = quats[pixel_idx + 1];
                uint8_t c = quats[pixel_idx + 2];
                uint8_t type = quats[pixel_idx + 3];

                float w, x, y, z;
                unpack_quaternion(a, b, c, type, w, x, y, z);

                rotations[idx * 4 + 0] = w;
                rotations[idx * 4 + 1] = x;
                rotations[idx * 4 + 2] = y;
                rotations[idx * 4 + 3] = z;
            }

            /**
             * @brief Decode from codebook kernel
             */
            __global__ void decode_scales_kernel(
                const uint8_t* __restrict__ indices,
                const float* __restrict__ codebook,
                float* __restrict__ output,
                const int n) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int pixel_idx = idx * 4;

                // Decode 3 scale components
                uint8_t idx_x = indices[pixel_idx + 0];
                uint8_t idx_y = indices[pixel_idx + 1];
                uint8_t idx_z = indices[pixel_idx + 2];

                output[idx * 3 + 0] = codebook[idx_x];
                output[idx * 3 + 1] = codebook[idx_y];
                output[idx * 3 + 2] = codebook[idx_z];
            }

            /**
             * @brief Decode shN palette centroids
             */
            __global__ void decode_shn_centroids_kernel(
                const uint8_t* __restrict__ centroids_img,
                const float* __restrict__ codebook,
                float* __restrict__ palette_out,
                const int palette_size,
                const int coeffs_per_channel) {
                
                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                const int total_pixels = palette_size * coeffs_per_channel;
                if (idx >= total_pixels) return;

                const int palette_idx = idx / coeffs_per_channel;
                const int coeff_idx = idx % coeffs_per_channel;
                const int pixel_idx = idx * 4;

                // Decode RGB from codebook (band-major ordering)
                for (int c = 0; c < 3; ++c) {
                    uint8_t code = centroids_img[pixel_idx + c];
                    float value = codebook[code];
                    
                    int out_idx = palette_idx * (coeffs_per_channel * 3) + coeff_idx + c * coeffs_per_channel;
                    if (out_idx < palette_size * coeffs_per_channel * 3) {
                        palette_out[out_idx] = value;
                    }
                }
            }

            /**
             * @brief Decode shN using palette lookup
             */
            __global__ void decode_shn_kernel(
                const uint8_t* __restrict__ labels_img,
                const float* __restrict__ palette,
                float* __restrict__ output,
                const int n,
                const int coeffs_per_channel) {
                
                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n) return;

                const int pixel_idx = idx * 4;
                
                // Decode 16-bit label
                int label = labels_img[pixel_idx + 0] | (labels_img[pixel_idx + 1] << 8);
                
                // Copy all coefficients from palette
                const int total_coeffs = coeffs_per_channel * 3;
                for (int i = 0; i < total_coeffs; ++i) {
                    output[idx * total_coeffs + i] = palette[label * total_coeffs + i];
                }
            }

            /**
             * @brief Decode colors and opacity kernel
             */
            __global__ void decode_sh0_opacity_kernel(
                const uint8_t* __restrict__ indices,
                const float* __restrict__ codebook,
                float* __restrict__ sh0_output,
                float* __restrict__ opacity_output,
                const int n) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int pixel_idx = idx * 4;

                // Decode RGB colors
                uint8_t idx_r = indices[pixel_idx + 0];
                uint8_t idx_g = indices[pixel_idx + 1];
                uint8_t idx_b = indices[pixel_idx + 2];

                sh0_output[idx * 3 + 0] = codebook[idx_r];
                sh0_output[idx * 3 + 1] = codebook[idx_g];
                sh0_output[idx * 3 + 2] = codebook[idx_b];

                // Decode opacity (keep in [0,1] space, do NOT convert to logit)
                // The encoder saves opacity in [0,1] space, multiplied by 255
                opacity_output[idx] = indices[pixel_idx + 3] / 255.0f;
            }

        } // anonymous namespace

        tinysog::internal::expected<DecodedGaussianData, std::string> decode_sog(
            const SogMetadata& metadata,
            const std::map<std::string, WebPImage>& images,
            bool cuda) {

            const int n = metadata.count;
            int width = metadata.width;
            int height = metadata.height;

            if (width == 0 || height == 0) {
                width = (static_cast<int>(std::ceil(std::sqrt(n) / 4.0)) * 4);
                height = (static_cast<int>(std::ceil(n / static_cast<float>(width) / 4.0)) * 4);
            }

            try {
                DecodedGaussianData result;

                // Allocate output tensors (always on GPU for decoding, convert to CPU later if needed)
                result.means = Tensor<float>({static_cast<size_t>(n), 3}, true);
                result.rotations = Tensor<float>({static_cast<size_t>(n), 4}, true);
                result.scales = Tensor<float>({static_cast<size_t>(n), 3}, true);
                result.sh0 = Tensor<float>({static_cast<size_t>(n), 1, 3}, true);
                result.opacity = Tensor<float>({static_cast<size_t>(n), 1}, true);

                // Decode positions
                {
                    auto it_l = images.find("means_l.webp");
                    auto it_u = images.find("means_u.webp");

                    if (it_l == images.end() || it_u == images.end()) {
                        return tinysog::internal::unexpected(std::string("Missing position textures"));
                    }

                    // Upload to GPU
                    Tensor<uint8_t> means_l_gpu({static_cast<size_t>(width * height * 4)}, true);
                    Tensor<uint8_t> means_u_gpu({static_cast<size_t>(width * height * 4)}, true);

                    cuda_copy_h2d(means_l_gpu.data, it_l->second.data.data(), width * height * 4);
                    cuda_copy_h2d(means_u_gpu.data, it_u->second.data.data(), width * height * 4);

                    // Upload boundary values
                    Tensor<float> min_vals({3}, true);
                    Tensor<float> max_vals({3}, true);
                    cuda_copy_h2d(min_vals.data, metadata.means_mins, 3);
                    cuda_copy_h2d(max_vals.data, metadata.means_maxs, 3);

                    // Decode
                    int grid_size, block_size;
                    get_grid_config(n, grid_size, block_size);

                    decode_positions_kernel<<<grid_size, block_size>>>(
                        means_l_gpu.data, means_u_gpu.data,
                        min_vals.data, max_vals.data,
                        result.means.data, n);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();
                }

                // Decode quaternions
                {
                    auto it = images.find("quats.webp");
                    if (it == images.end()) {
                        return tinysog::internal::unexpected(std::string("Missing quaternion texture"));
                    }

                    Tensor<uint8_t> quats_gpu({static_cast<size_t>(width * height * 4)}, true);
                    cuda_copy_h2d(quats_gpu.data, it->second.data.data(), width * height * 4);

                    int grid_size, block_size;
                    get_grid_config(n, grid_size, block_size);

                    decode_quaternions_kernel<<<grid_size, block_size>>>(
                        quats_gpu.data, result.rotations.data, n);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();
                }

                // 3. Decode scales (from codebook)
                {
                    auto it = images.find("scales.webp");
                    if (it == images.end()) {
                        return tinysog::internal::unexpected(std::string("Missing scales texture"));
                    }

                    Tensor<uint8_t> scales_gpu({static_cast<size_t>(width * height * 4)}, true);
                    cuda_copy_h2d(scales_gpu.data, it->second.data.data(), width * height * 4);

                    // Upload codebook
                    Tensor<float> codebook_gpu({256}, true);
                    cuda_copy_h2d(codebook_gpu.data, 
                                 metadata.scales_codebook.data(),
                                 std::min(metadata.scales_codebook.size(), size_t(256)));

                    int grid_size, block_size;
                    get_grid_config(n, grid_size, block_size);

                    decode_scales_kernel<<<grid_size, block_size>>>(
                        scales_gpu.data,
                        codebook_gpu.data,
                        result.scales.data,
                        n);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();
                }

                // 4. Decode colors and opacity (from codebook)
                {
                    auto it = images.find("sh0.webp");
                    if (it == images.end()) {
                        return tinysog::internal::unexpected(std::string("Missing color texture"));
                    }

                    Tensor<uint8_t> sh0_gpu({static_cast<size_t>(width * height * 4)}, true);
                    cuda_copy_h2d(sh0_gpu.data, it->second.data.data(), width * height * 4);

                    // Upload color codebook
                    Tensor<float> codebook_gpu({256}, true);
                    cuda_copy_h2d(codebook_gpu.data,
                                 metadata.sh0_codebook.data(),
                                 std::min(metadata.sh0_codebook.size(), size_t(256)));

                    // Temporary tensors for flat outputs
                    Tensor<float> sh0_flat({static_cast<size_t>(n), 3}, true);
                    Tensor<float> opacity_flat({static_cast<size_t>(n)}, true);

                    int grid_size, block_size;
                    get_grid_config(n, grid_size, block_size);

                    decode_sh0_opacity_kernel<<<grid_size, block_size>>>(
                        sh0_gpu.data,
                        codebook_gpu.data,
                        sh0_flat.data,
                        opacity_flat.data,
                        n);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();

                    // Reshape sh0 from [n,3] to [n,1,3]
                    cuda_copy_d2d(result.sh0.data, sh0_flat.data, n * 3);
                    
                    // Copy opacity from [n] to [n,1]
                    cuda_copy_d2d(result.opacity.data, opacity_flat.data, n);
                }

                // 5. Decode higher-order spherical harmonics (if present)
                if (metadata.shN_data && metadata.sh_degree > 0) {
                    auto& shN_info = *metadata.shN_data;
                    
                    auto it_centroids = images.find("shN_centroids.webp");
                    auto it_labels = images.find("shN_labels.webp");
                    
                    if (it_centroids == images.end() || it_labels == images.end()) {
                        return tinysog::internal::unexpected(std::string("Missing shN textures"));
                    }

                    const int palette_size = shN_info.palette_size;
                    const int coeffs_per_channel = shN_info.coeffs;
                    const int total_coeffs = coeffs_per_channel * 3;
                    
                    // Allocate output tensor
                    result.shN = Tensor<float>({static_cast<size_t>(n), static_cast<size_t>(total_coeffs)}, true);

                    // Step 1: Decode palette from centroids image + codebook
                    const int centroids_width = 64 * coeffs_per_channel;
                    const int centroids_height = (palette_size + 63) / 64;
                    
                    Tensor<uint8_t> centroids_gpu({static_cast<size_t>(centroids_width * centroids_height * 4)}, true);
                    cuda_copy_h2d(centroids_gpu.data, 
                                 it_centroids->second.data.data(), 
                                 centroids_width * centroids_height * 4);

                    // Upload shN codebook
                    Tensor<float> shn_codebook_gpu({256}, true);
                    cuda_copy_h2d(shn_codebook_gpu.data,
                                 shN_info.codebook.data(),
                                 std::min(shN_info.codebook.size(), size_t(256)));

                    // Decode palette
                    Tensor<float> palette_gpu({static_cast<size_t>(palette_size * total_coeffs)}, true);
                    
                    int grid_palette, block_palette;
                    get_grid_config(palette_size * coeffs_per_channel, grid_palette, block_palette);

                    decode_shn_centroids_kernel<<<grid_palette, block_palette>>>(
                        centroids_gpu.data,
                        shn_codebook_gpu.data,
                        palette_gpu.data,
                        palette_size,
                        coeffs_per_channel);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();

                    // Step 2: Decode shN using labels image + palette
                    Tensor<uint8_t> labels_gpu({static_cast<size_t>(width * height * 4)}, true);
                    cuda_copy_h2d(labels_gpu.data, 
                                 it_labels->second.data.data(), 
                                 width * height * 4);

                    int grid_size, block_size;
                    get_grid_config(n, grid_size, block_size);

                    decode_shn_kernel<<<grid_size, block_size>>>(
                        labels_gpu.data,
                        palette_gpu.data,
                        result.shN.data,
                        n,
                        coeffs_per_channel);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();
                }

            // Set sh_degree from metadata
            result.sh_degree = metadata.sh_degree;

            // If requested CPU data, convert back to CPU
            if (!cuda) {
                result.means.to_cpu();
                result.rotations.to_cpu();
                result.scales.to_cpu();
                result.sh0.to_cpu();
                result.opacity.to_cpu();
                if (result.shN.data != nullptr) {
                    result.shN.to_cpu();
                }
            }

            return result;

            } catch (const std::exception& e) {
                return tinysog::internal::unexpected(std::string("Decoding failed: ") + e.what());
            }
        }

    } // namespace internal
} // namespace tinysog




