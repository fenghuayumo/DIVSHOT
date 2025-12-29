/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <cuda_runtime.h>
#include <cstdint>

namespace tinysog {
    namespace internal {

        /**
         * @brief Logarithmic transformation (for position compression)
         */
        __device__ __forceinline__ float log_transform(float value) {
            float sign = (value >= 0.0f) ? 1.0f : -1.0f;
            return sign * logf(fabsf(value) + 1.0f);
        }

        /**
         * @brief Inverse logarithmic transformation
         */
        __device__ __forceinline__ float inverse_log_transform(float value) {
            float sign = (value >= 0.0f) ? 1.0f : -1.0f;
            return sign * (expf(fabsf(value)) - 1.0f);
        }

        /**
         * @brief Pack quaternion into 4 uint8
         *
         * Uses special encoding: save index of largest component, quantize other 3 components to 8 bits
         */
        __device__ inline void pack_quaternion(
            float w, float x, float y, float z,
            uint8_t& a, uint8_t& b, uint8_t& c, uint8_t& type) {

            // Normalize
            float len = sqrtf(w * w + x * x + y * y + z * z);
            if (len > 0.0f) {
                w /= len;
                x /= len;
                y /= len;
                z /= len;
            } else {
                w = 1.0f;
                x = 0.0f;
                y = 0.0f;
                z = 0.0f;
            }

            // Find the largest component
            float max_val = fabsf(w);
            int max_idx = 0;

            if (fabsf(x) > max_val) {
                max_val = fabsf(x);
                max_idx = 1;
            }
            if (fabsf(y) > max_val) {
                max_val = fabsf(y);
                max_idx = 2;
            }
            if (fabsf(z) > max_val) {
                max_val = fabsf(z);
                max_idx = 3;
            }

            // Ensure largest component is positive
            if ((max_idx == 0 && w < 0) ||
                (max_idx == 1 && x < 0) ||
                (max_idx == 2 && y < 0) ||
                (max_idx == 3 && z < 0)) {
                w = -w;
                x = -x;
                y = -y;
                z = -z;
            }

            // sqrt(2) scaling
            constexpr float sqrt2 = 1.41421356237f;
            w *= sqrt2;
            x *= sqrt2;
            y *= sqrt2;
            z *= sqrt2;

            // Pack other 3 components
            float v0, v1, v2;
            if (max_idx == 0) {
                // w is largest, save x,y,z
                v0 = x;
                v1 = y;
                v2 = z;
            } else if (max_idx == 1) {
                // x is largest, save w,y,z
                v0 = w;
                v1 = y;
                v2 = z;
            } else if (max_idx == 2) {
                // y is largest, save w,x,z
                v0 = w;
                v1 = x;
                v2 = z;
            } else {
                // z is largest, save w,x,y
                v0 = w;
                v1 = x;
                v2 = y;
            }

            // Quantize to [0, 255]
            a = static_cast<uint8_t>(fminf(fmaxf((v0 * 0.5f + 0.5f) * 255.0f, 0.0f), 255.0f));
            b = static_cast<uint8_t>(fminf(fmaxf((v1 * 0.5f + 0.5f) * 255.0f, 0.0f), 255.0f));
            c = static_cast<uint8_t>(fminf(fmaxf((v2 * 0.5f + 0.5f) * 255.0f, 0.0f), 255.0f));
            type = 252 + max_idx;
        }

        /**
         * @brief Unpack quaternion
         */
        __device__ inline void unpack_quaternion(
            uint8_t a, uint8_t b, uint8_t c, uint8_t type,
            float& w, float& x, float& y, float& z) {

            int largest = type - 252;
            if (largest < 0 || largest > 3) {
                largest = 0;
            }

            // Dequantize
            constexpr float sqrt2 = 1.41421356237f;
            float v0 = (a / 255.0f - 0.5f) * sqrt2;
            float v1 = (b / 255.0f - 0.5f) * sqrt2;
            float v2 = (c / 255.0f - 0.5f) * sqrt2;

            // Reconstruct largest component
            float largest_val = sqrtf(fmaxf(1.0f - (v0 * v0 + v1 * v1 + v2 * v2), 0.0f));

            // Reconstruct quaternion based on which component is largest
            if (largest == 0) {
                w = largest_val;
                x = v0;
                y = v1;
                z = v2;
            } else if (largest == 1) {
                w = v0;
                x = largest_val;
                y = v1;
                z = v2;
            } else if (largest == 2) {
                w = v0;
                x = v1;
                y = largest_val;
                z = v2;
            } else {
                w = v0;
                x = v1;
                y = v2;
                z = largest_val;
            }

            // Normalize
            float len = sqrtf(w * w + x * x + y * y + z * z);
            if (len > 0.0f) {
                w /= len;
                x /= len;
                y /= len;
                z /= len;
            }
        }

        /**
         * @brief Sigmoid function
         */
        __device__ __forceinline__ float sigmoid(float x) {
            return 1.0f / (1.0f + expf(-x));
        }

        /**
         * @brief Inverse sigmoid function (logit)
         */
        __device__ __forceinline__ float inverse_sigmoid(float x) {
            x = fminf(fmaxf(x, 1e-5f), 1.0f - 1e-5f);
            return logf(x / (1.0f - x));
        }

    } // namespace internal
} // namespace tinysog

