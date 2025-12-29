/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tinysog {
    namespace internal {

        /**
         * @brief CPU-side tensor data structure (replacement for torch::Tensor)
         */
        template <typename T>
        struct Tensor {
            T* data = nullptr;           // CPU or GPU memory pointer
            std::vector<size_t> shape;   // Dimension shape
            bool is_cuda = false;        // Whether on GPU
            bool owns_data = true;       // Whether owns memory

            Tensor() = default;

            Tensor(const std::vector<size_t>& shape_, bool cuda = false)
                : shape(shape_), is_cuda(cuda), owns_data(true) {
                size_t total = 1;
                for (auto s : shape)
                    total *= s;
                allocate(total);
            }

            ~Tensor() {
                if (owns_data && data) {
                    free_memory();
                }
            }

            // Disable copy
            Tensor(const Tensor&) = delete;
            Tensor& operator=(const Tensor&) = delete;

            // Enable move
            Tensor(Tensor&& other) noexcept
                : data(other.data),
                  shape(std::move(other.shape)),
                  is_cuda(other.is_cuda),
                  owns_data(other.owns_data) {
                other.data = nullptr;
                other.owns_data = false;
            }

            Tensor& operator=(Tensor&& other) noexcept {
                if (this != &other) {
                    if (owns_data && data) {
                        free_memory();
                    }
                    data = other.data;
                    shape = std::move(other.shape);
                    is_cuda = other.is_cuda;
                    owns_data = other.owns_data;
                    other.data = nullptr;
                    other.owns_data = false;
                }
                return *this;
            }

            size_t size(int dim) const {
                if (dim < 0)
                    dim += static_cast<int>(shape.size());
                return shape[dim];
            }

            size_t total_size() const {
                size_t total = 1;
                for (auto s : shape)
                    total *= s;
                return total;
            }

            size_t byte_size() const {
                return total_size() * sizeof(T);
            }

            void allocate(size_t count);
            void free_memory();
            void to_cuda();
            void to_cpu();
        };

        /**
         * @brief SOG metadata structure
         */
    struct SogMetadata {
        int version = 2;
        int count = 0;
        int width = 0;
        int height = 0;
        int sh_degree = 0;

        // Position range
        float means_mins[3];
        float means_maxs[3];

            // Scale codebook
            std::vector<float> scales_codebook;

            // Color codebook
            std::vector<float> sh0_codebook;

            // Spherical harmonics data (optional)
            struct ShNData {
                std::vector<float> codebook;
                int palette_size = 0;
                int bands = 0;
                int coeffs = 0;
            };
            std::unique_ptr<ShNData> shN_data;

            // Copy constructor and assignment deleted to prevent issues
            SogMetadata(const SogMetadata&) = delete;
            SogMetadata& operator=(const SogMetadata&) = delete;
            
            // Move constructor and assignment
            SogMetadata(SogMetadata&&) noexcept = default;
            SogMetadata& operator=(SogMetadata&&) noexcept = default;
            
            SogMetadata() = default;
            ~SogMetadata() = default;
        };

        /**
         * @brief WebP image data
         */
        struct WebPImage {
            std::vector<uint8_t> data;  // RGBA format
            int width = 0;
            int height = 0;
            int channels = 4;
        };

    } // namespace internal
} // namespace tinysog

