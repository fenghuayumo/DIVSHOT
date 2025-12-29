/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once


#include "tinysog/expected.h"
#include "data_types.h"
#include <expected>
#include <map>
#include <string>

namespace tinysog {
    namespace internal {

        /**
         * @brief SOG encoder configuration
         */
        struct EncoderConfig {
            int kmeans_iterations = 10;
            int webp_quality = 100;
            bool use_morton_sort = true;
        };

        /**
         * @brief SOG encoding result
         */
        struct EncodedSogData {
            SogMetadata metadata;
            std::map<std::string, WebPImage> images;
        };

        /**
         * @brief Encode Gaussian data to SOG format
         *
         * @param means Positions [n, 3]
         * @param rotations Rotations [n, 4]
         * @param scales Scales [n, 3]
         * @param sh0 Base color [n, 3]
         * @param opacity Opacity [n]
         * @param shN Higher-order spherical harmonics [n, num_coeffs, 3] (optional)
         * @param sh_degree Spherical harmonics degree
         * @param config Encoder configuration
         * @return Encoded data
         */
        tinysog::internal::expected<EncodedSogData, std::string> encode_sog(
            const Tensor<float>& means,
            const Tensor<float>& rotations,
            const Tensor<float>& scales,
            const Tensor<float>& sh0,
            const Tensor<float>& opacity,
            const Tensor<float>* shN,
            int sh_degree,
            const EncoderConfig& config);

    } // namespace internal
} // namespace tinysog

