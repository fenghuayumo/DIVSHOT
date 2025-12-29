/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <cuda_runtime.h>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace tinysog {
    namespace internal {

        /**
         * @brief CUDA error checking macro
         */
#define CUDA_CHECK(call)                                                                                    \
    do {                                                                                                    \
        cudaError_t err = call;                                                                             \
        if (err != cudaSuccess) {                                                                           \
            throw std::runtime_error(std::string("CUDA error at ") + __FILE__ + ":" + std::to_string(__LINE__) + \
                                     " - " + cudaGetErrorString(err));                                      \
        }                                                                                                   \
    } while (0)

        /**
         * @brief Initialize CUDA device (lazy initialization)
         */
        inline void ensure_cuda_initialized() {
            static bool initialized = false;
            if (!initialized) {
                int device_count = 0;
                CUDA_CHECK(cudaGetDeviceCount(&device_count));
                if (device_count == 0) {
                    throw std::runtime_error("No CUDA devices found");
                }
                
                // Get current device or set default
                int current_device = -1;
                cudaError_t err = cudaGetDevice(&current_device);
                if (err != cudaSuccess || current_device < 0) {
                    CUDA_CHECK(cudaSetDevice(0));
                }
                
                // Force initialization by allocating and freeing a small amount of memory
                void* dummy = nullptr;
                CUDA_CHECK(cudaMalloc(&dummy, 1));
                CUDA_CHECK(cudaFree(dummy));
                
                initialized = true;
            }
        }

        /**
         * @brief Allocate CUDA device memory
         */
        template <typename T>
        T* cuda_malloc(size_t count) {
            ensure_cuda_initialized();
            T* ptr = nullptr;
            CUDA_CHECK(cudaMalloc(&ptr, count * sizeof(T)));
            return ptr;
        }

        /**
         * @brief Free CUDA device memory
         */
        template <typename T>
        void cuda_free(T* ptr) {
            if (ptr) {
                CUDA_CHECK(cudaFree(ptr));
            }
        }

        /**
         * @brief Copy from host to device
         */
        template <typename T>
        void cuda_copy_h2d(T* dst, const T* src, size_t count) {
            CUDA_CHECK(cudaMemcpy(dst, src, count * sizeof(T), cudaMemcpyHostToDevice));
        }

        /**
         * @brief Copy from device to host
         */
        template <typename T>
        void cuda_copy_d2h(T* dst, const T* src, size_t count) {
            CUDA_CHECK(cudaMemcpy(dst, src, count * sizeof(T), cudaMemcpyDeviceToHost));
        }

        /**
         * @brief Device memory copy
         */
        template <typename T>
        void cuda_copy_d2d(T* dst, const T* src, size_t count) {
            CUDA_CHECK(cudaMemcpy(dst, src, count * sizeof(T), cudaMemcpyDeviceToDevice));
        }

        /**
         * @brief Zero device memory
         */
        template <typename T>
        void cuda_memset(T* ptr, int value, size_t count) {
            CUDA_CHECK(cudaMemset(ptr, value, count * sizeof(T)));
        }

        /**
         * @brief Synchronize all CUDA operations
         */
        inline void cuda_synchronize() {
            CUDA_CHECK(cudaDeviceSynchronize());
        }

        /**
         * @brief Get CUDA thread configuration
         */
        inline void get_grid_config(int n, int& grid_size, int& block_size, int max_block_size = 256) {
            block_size = max_block_size;
            grid_size = (n + block_size - 1) / block_size;
        }

    } // namespace internal
} // namespace tinysog

