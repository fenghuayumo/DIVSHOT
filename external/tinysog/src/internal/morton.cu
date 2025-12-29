/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "morton.cuh"
#include "cuda_utils.cuh"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <thrust/device_ptr.h>
#include <thrust/sort.h>

#include "tinysog/expected.h"
namespace tinysog {
    namespace internal {

        /**
         * @brief Morton encoding helper: scatter 32-bit integer into every 3 bits of 64-bit
         */
        __device__ __forceinline__ uint64_t split_by_3(uint32_t a) {
            uint64_t x = a & 0x1fffff; // Keep lower 21 bits
            x = (x | x << 32) & 0x1f00000000ffff;
            x = (x | x << 16) & 0x1f0000ff0000ff;
            x = (x | x << 8) & 0x100f00f00f00f00f;
            x = (x | x << 4) & 0x10c30c30c30c30c3;
            x = (x | x << 2) & 0x1249249249249249;
            return x;
        }

        /**
         * @brief Morton encoding CUDA kernel
         */
        __global__ void morton_encode_kernel(
            const float* __restrict__ positions,
            const float* __restrict__ min_coords,
            const float* __restrict__ cube_size,
            int64_t* __restrict__ morton_codes,
            const int n) {

            const int idx = blockIdx.x * blockDim.x + threadIdx.x;
            if (idx >= n)
                return;

            // Read position
            const float x = positions[idx * 3 + 0];
            const float y = positions[idx * 3 + 1];
            const float z = positions[idx * 3 + 2];

            // Normalize to [0, 1]
            const double size = static_cast<double>(cube_size[0]);
            const double nx = (static_cast<double>(x) - static_cast<double>(min_coords[0])) / size;
            const double ny = (static_cast<double>(y) - static_cast<double>(min_coords[1])) / size;
            const double nz = (static_cast<double>(z) - static_cast<double>(min_coords[2])) / size;

            // Convert to 21-bit integer
            constexpr double factor = 2097151.0; // 2^21 - 1
            const uint32_t ix = static_cast<uint32_t>(nx * factor);
            const uint32_t iy = static_cast<uint32_t>(ny * factor);
            const uint32_t iz = static_cast<uint32_t>(nz * factor);

            // Calculate Morton code
            const uint64_t morton = split_by_3(ix) | (split_by_3(iy) << 1) | (split_by_3(iz) << 2);

            // Convert to signed int64 (compatible with sorting)
            constexpr int64_t offset = std::numeric_limits<int64_t>::min();
            morton_codes[idx] = static_cast<int64_t>(morton) + offset;
        }

        /**
         * @brief Calculate min/max values kernel
         */
        __global__ void compute_min_max_kernel(
            const float* __restrict__ positions,
            float* __restrict__ min_vals,
            float* __restrict__ max_vals,
            const int n) {

            __shared__ float s_min[3][256];
            __shared__ float s_max[3][256];

            const int tid = threadIdx.x;
            const int idx = blockIdx.x * blockDim.x + tid;

            // Initialize shared memory
            for (int d = 0; d < 3; ++d) {
                s_min[d][tid] = (idx < n) ? positions[idx * 3 + d] : INFINITY;
                s_max[d][tid] = (idx < n) ? positions[idx * 3 + d] : -INFINITY;
            }

            __syncthreads();

            // Reduction to find min/max values
            for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
                if (tid < stride) {
                    for (int d = 0; d < 3; ++d) {
                        s_min[d][tid] = fminf(s_min[d][tid], s_min[d][tid + stride]);
                        s_max[d][tid] = fmaxf(s_max[d][tid], s_max[d][tid + stride]);
                    }
                }
                __syncthreads();
            }

            // First thread writes back result
            if (tid == 0) {
                for (int d = 0; d < 3; ++d) {
                    atomicMin(reinterpret_cast<int*>(&min_vals[d]),
                              __float_as_int(s_min[d][0]));
                    atomicMax(reinterpret_cast<int*>(&max_vals[d]),
                              __float_as_int(s_max[d][0]));
                }
            }
        }

        tinysog::internal::expected<Tensor<int64_t>, std::string> morton_encode(
            const Tensor<float>& positions) {

            if (positions.shape.size() != 2 || positions.shape[1] != 3) {
                return tinysog::internal::unexpected(std::string("Positions must have shape [N, 3]"));
            }

            if (!positions.is_cuda) {
                return tinysog::internal::unexpected(std::string("Positions must be on CUDA device"));
            }

            const int n = static_cast<int>(positions.shape[0]);

            try {
                // Allocate output
                Tensor<int64_t> morton_codes({static_cast<size_t>(n)}, true);

                // Calculate bounding box
                Tensor<float> min_vals({3}, true);
                Tensor<float> max_vals({3}, true);

                // Initialize to extreme values
                float init_min[3] = {INFINITY, INFINITY, INFINITY};
                float init_max[3] = {-INFINITY, -INFINITY, -INFINITY};
                cuda_copy_h2d(min_vals.data, init_min, 3);
                cuda_copy_h2d(max_vals.data, init_max, 3);

                // Calculate min/max values
                int grid_size, block_size;
                get_grid_config(n, grid_size, block_size);
                compute_min_max_kernel<<<grid_size, block_size>>>(
                    positions.data, min_vals.data, max_vals.data, n);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                // Calculate cube size
                float min_cpu[3], max_cpu[3];
                cuda_copy_d2h(min_cpu, min_vals.data, 3);
                cuda_copy_d2h(max_cpu, max_vals.data, 3);

                float range[3] = {
                    max_cpu[0] - min_cpu[0],
                    max_cpu[1] - min_cpu[1],
                    max_cpu[2] - min_cpu[2]};

                float cube_size = std::max({range[0], range[1], range[2]});
                cube_size = std::max(cube_size, 1e-7f); // Avoid division by zero

                // Allocate temporary GPU memory
                float* d_cube_size = cuda_malloc<float>(1);
                cuda_copy_h2d(d_cube_size, &cube_size, 1);

                // Execute Morton encoding
                morton_encode_kernel<<<grid_size, block_size>>>(
                    positions.data,
                    min_vals.data,
                    d_cube_size,
                    morton_codes.data,
                    n);

                CUDA_CHECK(cudaGetLastError());
                cuda_synchronize();

                // Clean up temporary memory
                cuda_free(d_cube_size);

                return morton_codes;

            } catch (const std::exception& e) {
                return tinysog::internal::unexpected(std::string("Morton encoding failed: ") + e.what());
            }
        }

        tinysog::internal::expected<Tensor<int64_t>, std::string> morton_sort_indices(
            const Tensor<int64_t>& morton_codes) {

            if (morton_codes.shape.size() != 1) {
                return tinysog::internal::unexpected(std::string("Morton codes must be 1D tensor"));
            }

            if (!morton_codes.is_cuda) {
                return tinysog::internal::unexpected(std::string("Morton codes must be on CUDA device"));
            }

            const int n = static_cast<int>(morton_codes.shape[0]);

            try {
                // Create index sequence
                Tensor<int64_t> indices({static_cast<size_t>(n)}, true);

                // Create 0, 1, 2, ..., n-1 sequence on GPU
                thrust::device_ptr<int64_t> d_indices(indices.data);
                thrust::sequence(d_indices, d_indices + n, 0LL);

                // Create copy of Morton codes for sorting
                Tensor<int64_t> codes_copy({static_cast<size_t>(n)}, true);
                cuda_copy_d2d(codes_copy.data, morton_codes.data, n);

                // Sort indices by Morton codes
                thrust::device_ptr<int64_t> d_codes(codes_copy.data);
                thrust::sort_by_key(d_codes, d_codes + n, d_indices);

                cuda_synchronize();

                return indices;

            } catch (const std::exception& e) {
                return tinysog::internal::unexpected(std::string("Morton sort failed: ") + e.what());
            }
        }

    } // namespace internal
} // namespace tinysog




