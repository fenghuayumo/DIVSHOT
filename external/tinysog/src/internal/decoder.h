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
         * @brief SOG decoding result
         */
        struct DecodedGaussianData {
            Tensor<float> means;      // [n, 3]
            Tensor<float> rotations;  // [n, 4]
            Tensor<float> scales;     // [n, 3]
            Tensor<float> sh0;        // [n, 1, 3]
            Tensor<float> opacity;    // [n, 1]
            Tensor<float> shN;        // [n, num_coeffs, 3] (optional)
            int sh_degree = 0;
        };

        /**
         * @brief Decode Gaussian data from SOG format
         *
         * @param metadata SOG metadata
         * @param images Image data dictionary
         * @param cuda Whether to return CUDA tensors
         * @return Decoded Gaussian data
         */
        tinysog::internal::expected<DecodedGaussianData, std::string> decode_sog(
            const SogMetadata& metadata,
            const std::map<std::string, WebPImage>& images,
            bool cuda = true);

    } // namespace internal
} // namespace tinysog

