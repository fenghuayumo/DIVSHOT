/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once


#include "tinysog/expected.h"
#include "data_types.h"
#include <expected>
#include <string>
#include <vector>

namespace tinysog {
    namespace internal {

        /**
         * @brief Encode WebP image (lossless)
         *
         * @param data RGBA data
         * @param width Width
         * @param height Height
         * @return Encoded WebP data
         */
        tinysog::internal::expected<std::vector<uint8_t>, std::string> encode_webp(
            const uint8_t* data,
            int width,
            int height,
            int quality = 100);

        /**
         * @brief Decode WebP image
         *
         * @param data WebP encoded data
         * @param size Data size
         * @return Decoded RGBA image
         */
        tinysog::internal::expected<WebPImage, std::string> decode_webp(
            const uint8_t* data,
            size_t size);

    } // namespace internal
} // namespace tinysog

