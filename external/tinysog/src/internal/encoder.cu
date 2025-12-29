/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "encoder.h"
#include "compression.cuh"
#include "cuda_utils.cuh"
#include "kmeans.cuh"
#include "morton.cuh"
#include "webp_utils.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstdio>

// Thrust headers
#include <thrust/device_ptr.h>
#include <thrust/copy.h>
#include <thrust/sequence.h>
#include <thrust/iterator/permutation_iterator.h>
#include <thrust/iterator/transform_iterator.h>
#include <thrust/iterator/counting_iterator.h>

#include "tinysog/expected.h"
namespace tinysog {
    namespace internal {

        namespace {

            // Helper for atomic min/max on floats
            __device__ void atomicMinFloat(float* addr, float value) {
                int* addr_as_int = reinterpret_cast<int*>(addr);
                int old = *addr_as_int, assumed;
                do {
                    assumed = old;
                    old = atomicCAS(addr_as_int, assumed,
                                    __float_as_int(fminf(value, __int_as_float(assumed))));
                } while (assumed != old);
            }

            __device__ void atomicMaxFloat(float* addr, float value) {
                int* addr_as_int = reinterpret_cast<int*>(addr);
                int old = *addr_as_int, assumed;
                do {
                    assumed = old;
                    old = atomicCAS(addr_as_int, assumed,
                                    __float_as_int(fmaxf(value, __int_as_float(assumed))));
                } while (assumed != old);
            }

            /**
             * @brief Apply log transform and calculate min/max values
             * @param n Total number of elements (not number of points)
             */
            __global__ void apply_log_transform_kernel(
                const float* __restrict__ input,
                float* __restrict__ output,
                float* __restrict__ min_vals,
                float* __restrict__ max_vals,
                const int total_elements) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;

                if (idx < total_elements) {
                    float val = log_transform(input[idx]);
                    output[idx] = val;

                    // Atomic update min/max values (dim = which coordinate: x, y, or z)
                    int dim = idx % 3;
                    atomicMinFloat(&min_vals[dim], val);
                    atomicMaxFloat(&max_vals[dim], val);
                }
            }

            /**
             * @brief Encode positions as 16-bit images (lower and upper)
             */
            __global__ void encode_positions_kernel(
                const float* __restrict__ positions_log,
                const float* __restrict__ min_vals,
                const float* __restrict__ max_vals,
                const int64_t* __restrict__ sort_indices,
                uint8_t* __restrict__ means_l,
                uint8_t* __restrict__ means_u,
                const int n,
                const int width) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int64_t orig_idx = sort_indices[idx];
                const int pixel_idx = idx * 4; // RGBA

                // Normalize to [0, 1]
                float x = (positions_log[orig_idx * 3 + 0] - min_vals[0]) /
                          (max_vals[0] - min_vals[0] + 1e-10f);
                float y = (positions_log[orig_idx * 3 + 1] - min_vals[1]) /
                          (max_vals[1] - min_vals[1] + 1e-10f);
                float z = (positions_log[orig_idx * 3 + 2] - min_vals[2]) /
                          (max_vals[2] - min_vals[2] + 1e-10f);

                // Quantize to 16-bit
                uint16_t x16 = static_cast<uint16_t>(fminf(fmaxf(x, 0.0f), 1.0f) * 65535.0f);
                uint16_t y16 = static_cast<uint16_t>(fminf(fmaxf(y, 0.0f), 1.0f) * 65535.0f);
                uint16_t z16 = static_cast<uint16_t>(fminf(fmaxf(z, 0.0f), 1.0f) * 65535.0f);

                // Separate into low 8 bits and high 8 bits
                means_l[pixel_idx + 0] = x16 & 0xff;
                means_l[pixel_idx + 1] = y16 & 0xff;
                means_l[pixel_idx + 2] = z16 & 0xff;
                means_l[pixel_idx + 3] = 255; // Alpha

                means_u[pixel_idx + 0] = (x16 >> 8) & 0xff;
                means_u[pixel_idx + 1] = (y16 >> 8) & 0xff;
                means_u[pixel_idx + 2] = (z16 >> 8) & 0xff;
                means_u[pixel_idx + 3] = 255; // Alpha
            }

            /**
             * @brief Encode quaternions
             */
            __global__ void encode_quaternions_kernel(
                const float* __restrict__ rotations,
                const int64_t* __restrict__ sort_indices,
                uint8_t* __restrict__ quats,
                const int n) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int64_t orig_idx = sort_indices[idx];
                const int pixel_idx = idx * 4;

                // Read quaternion (w, x, y, z)
                float w = rotations[orig_idx * 4 + 0];
                float x = rotations[orig_idx * 4 + 1];
                float y = rotations[orig_idx * 4 + 2];
                float z = rotations[orig_idx * 4 + 3];

                // Pack
                uint8_t a, b, c, type;
                pack_quaternion(w, x, y, z, a, b, c, type);

                quats[pixel_idx + 0] = a;
                quats[pixel_idx + 1] = b;
                quats[pixel_idx + 2] = c;
                quats[pixel_idx + 3] = type;
            }

            /**
             * @brief Encode data using cluster labels (column-major layout)
             */
            __global__ void encode_with_labels_kernel(
                const int32_t* __restrict__ labels,
                const int64_t* __restrict__ sort_indices,
                uint8_t* __restrict__ output,
                const int n,
                const int stride) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int64_t orig_idx = sort_indices[idx];
                const int pixel_idx = idx * 4;

                // Labels are in column-major order: [all x, all y, all z, ...]
                // So label for component s of point orig_idx is at: labels[s * n + orig_idx]
                for (int s = 0; s < stride; ++s) {
                    int label_idx = s * n + orig_idx;  // ✅ Column-major indexing
                    output[pixel_idx + s] = static_cast<uint8_t>(labels[label_idx]);
                }

                // Fill alpha channel
                if (stride < 4) {
                    output[pixel_idx + 3] = 255;
                }
            }

            /**
             * @brief Convert row-major to column-major layout
             */
            __global__ void row_to_column_major_kernel(
                const float* __restrict__ input,
                float* __restrict__ output,
                const int n,
                const int stride) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                for (int d = 0; d < stride; ++d) {
                    output[d * n + idx] = input[idx * stride + d];
                }
            }

            /**
             * @brief Pack shN centroids with band-major ordering
             */
            __global__ void pack_shn_centroids_kernel(
                const int32_t* __restrict__ codebook_labels,
                uint8_t* __restrict__ output,
                const int palette_size,
                const int coeffs_per_channel) {
                
                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                const int total_pixels = palette_size * coeffs_per_channel;
                if (idx >= total_pixels) return;

                const int palette_idx = idx / coeffs_per_channel;
                const int coeff_idx = idx % coeffs_per_channel;
                
                const int pixel_idx = idx * 4;

                // Band-major ordering: [R0, G0, B0, R1, G1, B1, ...]
                for (int c = 0; c < 3; ++c) {
                    int data_idx = palette_idx * (coeffs_per_channel * 3) + coeff_idx + c * coeffs_per_channel;
                    if (data_idx < palette_size * coeffs_per_channel * 3) {
                        output[pixel_idx + c] = static_cast<uint8_t>(codebook_labels[data_idx]);
                    }
                }
            }

            /**
             * @brief Pack shN labels (16-bit palette indices)
             */
            __global__ void pack_shn_labels_kernel(
                const int32_t* __restrict__ palette_labels,
                const int64_t* __restrict__ sort_idx,
                uint8_t* __restrict__ output,
                const int count) {
                
                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= count) return;

                const int64_t orig_idx = sort_idx[idx];
                const int pixel_idx = idx * 4;
                
                int32_t label = palette_labels[orig_idx];
                output[pixel_idx + 0] = label & 0xff;
                output[pixel_idx + 1] = (label >> 8) & 0xff;
                output[pixel_idx + 2] = 0;
            }

            /**
             * @brief Encode colors and opacity
             */
            __global__ void encode_sh0_opacity_kernel(
                const int32_t* __restrict__ color_labels,
                const float* __restrict__ opacity,
                const int64_t* __restrict__ sort_indices,
                uint8_t* __restrict__ sh0_data,
                const int n) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n)
                    return;

                const int64_t orig_idx = sort_indices[idx];
                const int pixel_idx = idx * 4;

                // Color labels (column-major layout: [all R, all G, all B])
                sh0_data[pixel_idx + 0] = static_cast<uint8_t>(color_labels[0 * n + orig_idx]);  // R
                sh0_data[pixel_idx + 1] = static_cast<uint8_t>(color_labels[1 * n + orig_idx]);  // G
                sh0_data[pixel_idx + 2] = static_cast<uint8_t>(color_labels[2 * n + orig_idx]);  // B

                // Opacity (already sigmoid-applied, in [0,1] space)
                float opacity_val = opacity[orig_idx];
                sh0_data[pixel_idx + 3] = static_cast<uint8_t>(opacity_val * 255.0f);
            }

        } // anonymous namespace

        tinysog::internal::expected<EncodedSogData, std::string> encode_sog(
            const Tensor<float>& means,
            const Tensor<float>& rotations,
            const Tensor<float>& scales,
            const Tensor<float>& sh0,
            const Tensor<float>& opacity,
            const Tensor<float>* shN,
            int sh_degree,
            const EncoderConfig& config) {

            if (!means.is_cuda || !rotations.is_cuda || !scales.is_cuda ||
                !sh0.is_cuda || !opacity.is_cuda) {
                return tinysog::internal::unexpected(std::string("All input tensors must be on CUDA device"));
            }

            const int n = means.shape[0];

            try {
                EncodedSogData result;

                // Calculate texture dimensions
                int width = (static_cast<int>(std::ceil(std::sqrt(n) / 4.0)) * 4);
                int height = (static_cast<int>(std::ceil(n / static_cast<float>(width) / 4.0)) * 4);

                // Set metadata
                result.metadata.version = 2;
                result.metadata.count = n;
                result.metadata.width = width;
                result.metadata.height = height;

                // Step 1: Morton sorting
                Tensor<int64_t> sort_indices({static_cast<size_t>(n)}, true);
                if (config.use_morton_sort) {
                    auto morton_codes = morton_encode(means);
                    if (!morton_codes) {
                        return tinysog::internal::unexpected(morton_codes.error());
                    }
                    auto indices = morton_sort_indices(morton_codes.value());
                    if (!indices) {
                        return tinysog::internal::unexpected(indices.error());
                    }
                    sort_indices = std::move(indices.value());
                } else {
                    // Use sequential indices
                    thrust::device_ptr<int64_t> d_indices(sort_indices.data);
                    thrust::sequence(d_indices, d_indices + n, 0LL);
                }

                // Step 2: Encode positions (log transform + 16-bit quantization)
                Tensor<float> means_log({static_cast<size_t>(n), 3}, true);
                Tensor<float> min_vals({3}, true);
                Tensor<float> max_vals({3}, true);

                float init_min[3] = {INFINITY, INFINITY, INFINITY};
                float init_max[3] = {-INFINITY, -INFINITY, -INFINITY};
                cuda_copy_h2d(min_vals.data, init_min, 3);
                cuda_copy_h2d(max_vals.data, init_max, 3);

                int grid_size, block_size;
                get_grid_config(n * 3, grid_size, block_size);

                apply_log_transform_kernel<<<grid_size, block_size>>>(
                    means.data, means_log.data,
                    min_vals.data, max_vals.data, n * 3);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                // Copy min/max values to CPU
                cuda_copy_d2h(result.metadata.means_mins, min_vals.data, 3);
                cuda_copy_d2h(result.metadata.means_maxs, max_vals.data, 3);

                // Create position images
                Tensor<uint8_t> means_l_data({static_cast<size_t>(width * height * 4)}, true);
                Tensor<uint8_t> means_u_data({static_cast<size_t>(width * height * 4)}, true);

                cuda_memset(means_l_data.data, 255, width * height * 4);
                cuda_memset(means_u_data.data, 255, width * height * 4);

                get_grid_config(n, grid_size, block_size);
                encode_positions_kernel<<<grid_size, block_size>>>(
                    means_log.data, min_vals.data, max_vals.data,
                    sort_indices.data,
                    means_l_data.data, means_u_data.data,
                    n, width);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                // Convert to WebP
                means_l_data.to_cpu();
                means_u_data.to_cpu();

                auto webp_l = encode_webp(means_l_data.data, width, height, config.webp_quality);
                auto webp_u = encode_webp(means_u_data.data, width, height, config.webp_quality);

                if (!webp_l || !webp_u) {
                    return tinysog::internal::unexpected(std::string("Failed to encode position WebP images"));
                }

                WebPImage img_l, img_u;
                img_l.data = std::move(webp_l.value());
                img_l.width = width;
                img_l.height = height;
                img_u.data = std::move(webp_u.value());
                img_u.width = width;
                img_u.height = height;

                result.images["means_l.webp"] = std::move(img_l);
                result.images["means_u.webp"] = std::move(img_u);

                // Step 3: Encode quaternions
                Tensor<uint8_t> quats_data({static_cast<size_t>(width * height * 4)}, true);
                cuda_memset(quats_data.data, 255, width * height * 4);

                encode_quaternions_kernel<<<grid_size, block_size>>>(
                    rotations.data, sort_indices.data, quats_data.data, n);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                quats_data.to_cpu();
                auto webp_quats = encode_webp(quats_data.data, width, height, config.webp_quality);
                if (!webp_quats) {
                    return tinysog::internal::unexpected(std::string("Failed to encode quaternions WebP"));
                }

                WebPImage img_quats;
                img_quats.data = std::move(webp_quats.value());
                img_quats.width = width;
                img_quats.height = height;
                result.images["quats.webp"] = std::move(img_quats);

                // Step 4: Encode scales (k-means clustering)
                // Flatten scale data to 1D (column-major order)
                Tensor<float> scales_flat({static_cast<size_t>(n * 3)}, true);
                
                // Convert row-major to column-major using kernel
                int grid_size_convert, block_size_convert;
                get_grid_config(n, grid_size_convert, block_size_convert);
                
                row_to_column_major_kernel<<<grid_size_convert, block_size_convert>>>(
                    scales.data, scales_flat.data, n, 3);
                
                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                // 1D k-means clustering
                auto scales_kmeans = kmeans_1d(scales_flat, 256, config.kmeans_iterations);
                if (!scales_kmeans) {
                    return tinysog::internal::unexpected(scales_kmeans.error());
                }

                // Create scales image
                Tensor<uint8_t> scales_img({static_cast<size_t>(width * height * 4)}, true);
                cuda_memset(scales_img.data, 255, width * height * 4);

                // Encode scale labels
                encode_with_labels_kernel<<<grid_size, block_size>>>(
                    scales_kmeans->labels.data,
                    sort_indices.data,
                    scales_img.data,
                    n, 3);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                scales_img.to_cpu();
                auto webp_scales = encode_webp(scales_img.data, width, height, config.webp_quality);
                if (!webp_scales) {
                    return tinysog::internal::unexpected(std::string("Failed to encode scales WebP"));
                }

                WebPImage img_scales;
                img_scales.data = std::move(webp_scales.value());
                img_scales.width = width;
                img_scales.height = height;
                result.images["scales.webp"] = std::move(img_scales);

                // Save scales codebook
                scales_kmeans->centroids.to_cpu();
                result.metadata.scales_codebook.resize(256);
                std::memcpy(result.metadata.scales_codebook.data(),
                           scales_kmeans->centroids.data, 
                           256 * sizeof(float));

                // Step 5: Encode colors (k-means clustering)
                Tensor<float> colors_flat({static_cast<size_t>(n * 3)}, true);
                
                // Convert row-major to column-major using kernel
                row_to_column_major_kernel<<<grid_size_convert, block_size_convert>>>(
                    sh0.data, colors_flat.data, n, 3);
                
                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                auto colors_kmeans = kmeans_1d(colors_flat, 256, config.kmeans_iterations);
                if (!colors_kmeans) {
                    return tinysog::internal::unexpected(colors_kmeans.error());
                }
                
                // Create sh0 image (contains colors and opacity)
                Tensor<uint8_t> sh0_img({static_cast<size_t>(width * height * 4)}, true);
                cuda_memset(sh0_img.data, 0, width * height * 4);

                encode_sh0_opacity_kernel<<<grid_size, block_size>>>(
                    colors_kmeans->labels.data,
                    opacity.data,
                    sort_indices.data,
                    sh0_img.data,
                    n);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                sh0_img.to_cpu();
                auto webp_sh0 = encode_webp(sh0_img.data, width, height, config.webp_quality);
                if (!webp_sh0) {
                    return tinysog::internal::unexpected(std::string("Failed to encode sh0 WebP"));
                }

                WebPImage img_sh0;
                img_sh0.data = std::move(webp_sh0.value());
                img_sh0.width = width;
                img_sh0.height = height;
                result.images["sh0.webp"] = std::move(img_sh0);

                // Save color codebook
                colors_kmeans->centroids.to_cpu();
                result.metadata.sh0_codebook.resize(256);
                std::memcpy(result.metadata.sh0_codebook.data(),
                           colors_kmeans->centroids.data,
                           256 * sizeof(float));

                // Step 6: Encode higher-order spherical harmonics (if present)
                if (sh_degree > 0 && shN != nullptr && shN->total_size() > 0) {
                    if (!shN->is_cuda) {
                        return tinysog::internal::unexpected(std::string("shN tensor must be on CUDA device"));
                    }

                    // Calculate number of coefficients based on sh_degree
                    // shN has shape [n, num_coeffs * 3] where num_coeffs = (sh_degree+1)^2 - 1
                    int num_coeffs_per_channel = 0;
                    if (sh_degree == 1) num_coeffs_per_channel = 3;
                    else if (sh_degree == 2) num_coeffs_per_channel = 8;
                    else if (sh_degree == 3) num_coeffs_per_channel = 15;
                    else {
                        return tinysog::internal::unexpected(std::string("Unsupported SH degree: ") + std::to_string(sh_degree));
                    }

                    size_t total_coeffs = num_coeffs_per_channel * 3;  // All channels
                    
                    // Verify shape
                    if (shN->shape.size() != 1 || shN->shape[0] != n * total_coeffs) {
                        return tinysog::internal::unexpected(
                            std::string("Invalid shN shape: expected [") + std::to_string(n * total_coeffs) + 
                            "], got [" + std::to_string(shN->shape[0]) + "]");
                    }

                    // Calculate palette size (matches TypeScript logic)
                    int palette_size = std::min(64,
                        std::max(1, static_cast<int>(std::pow(2, std::floor(std::log2(n / 1024.0)))) * 1024));
                    palette_size = std::min(palette_size, n);

                    // Stage 1: N-dimensional k-means to create palette
                    // Create a non-owning tensor view for shN data
                    Tensor<float> shN_view({static_cast<size_t>(n), total_coeffs}, true);
                    shN_view.data = const_cast<float*>(shN->data);
                    shN_view.owns_data = false;
                    
                    auto sh_palette_result = kmeans(shN_view, palette_size, config.kmeans_iterations);
                    if (!sh_palette_result) {
                        return tinysog::internal::unexpected(sh_palette_result.error());
                    }

                    int actual_palette_size = sh_palette_result->centroids.shape[0];

                    // Stage 2: 1D k-means to create 256-entry codebook from palette
                    // Flatten the palette centroids (shape is [palette_size, total_coeffs])
                    Tensor<float> palette_flat({static_cast<size_t>(actual_palette_size * total_coeffs)}, true);
                    size_t flatten_size = actual_palette_size * total_coeffs;
                    cudaMemcpy(palette_flat.data, sh_palette_result->centroids.data, flatten_size * sizeof(float), cudaMemcpyDeviceToDevice);
                    CUDA_CHECK(cudaGetLastError());
                    
                    auto sh_codebook_result = kmeans_1d(palette_flat, 256, config.kmeans_iterations);
                    if (!sh_codebook_result) {
                        return tinysog::internal::unexpected(sh_codebook_result.error());
                    }

                    // Create centroids texture (palette with codebook indices)
                    const int centroids_width = 64 * num_coeffs_per_channel;
                    const int centroids_height = (actual_palette_size + 63) / 64;
                    
                    Tensor<uint8_t> centroids_img({static_cast<size_t>(centroids_width * centroids_height * 4)}, true);
                    cuda_memset(centroids_img.data, 255, centroids_width * centroids_height * 4);

                    // Encode centroids using codebook labels (band-major ordering)
                    int grid_centroids, block_centroids;
                    get_grid_config(actual_palette_size * num_coeffs_per_channel, grid_centroids, block_centroids);

                    pack_shn_centroids_kernel<<<grid_centroids, block_centroids>>>(
                        sh_codebook_result->labels.data,
                        centroids_img.data,
                        actual_palette_size,
                        num_coeffs_per_channel);
                    
                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();

                    centroids_img.to_cpu();
                    auto webp_centroids = encode_webp(centroids_img.data, centroids_width, centroids_height, config.webp_quality);
                    if (!webp_centroids) {
                        return tinysog::internal::unexpected(std::string("Failed to encode shN centroids WebP"));
                    }

                    WebPImage img_centroids;
                    img_centroids.data = std::move(webp_centroids.value());
                    img_centroids.width = centroids_width;
                    img_centroids.height = centroids_height;
                    result.images["shN_centroids.webp"] = std::move(img_centroids);

                    // Create labels texture (palette index for each Gaussian)
                    Tensor<uint8_t> labels_img({static_cast<size_t>(width * height * 4)}, true);
                    cuda_memset(labels_img.data, 255, width * height * 4);

                    // Encode labels (16-bit palette indices)
                    pack_shn_labels_kernel<<<grid_size, block_size>>>(
                        sh_palette_result->labels.data,
                        sort_indices.data,
                        labels_img.data,
                        n);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();

                    labels_img.to_cpu();
                    auto webp_labels = encode_webp(labels_img.data, width, height, config.webp_quality);
                    if (!webp_labels) {
                        return tinysog::internal::unexpected(std::string("Failed to encode shN labels WebP"));
                    }

                    WebPImage img_labels;
                    img_labels.data = std::move(webp_labels.value());
                    img_labels.width = width;
                    img_labels.height = height;
                    result.images["shN_labels.webp"] = std::move(img_labels);

                    // Save shN metadata
                    result.metadata.sh_degree = sh_degree;
                    result.metadata.shN_data = std::make_unique<SogMetadata::ShNData>();
                    result.metadata.shN_data->palette_size = actual_palette_size;
                    result.metadata.shN_data->bands = sh_degree;
                    result.metadata.shN_data->coeffs = num_coeffs_per_channel;
                    
                    sh_codebook_result->centroids.to_cpu();
                    result.metadata.shN_data->codebook.resize(256);
                    std::memcpy(result.metadata.shN_data->codebook.data(),
                               sh_codebook_result->centroids.data,
                               256 * sizeof(float));
                }

                return result;

            } catch (const std::exception& e) {
                return tinysog::internal::unexpected(std::string("Encoding failed: ") + e.what());
            }
        }

    } // namespace internal
} // namespace tinysog




