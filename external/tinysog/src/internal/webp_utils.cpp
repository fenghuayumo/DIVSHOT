/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "webp_utils.h"
#include <webp/decode.h>
#include <webp/encode.h>

#include "tinysog/expected.h"
namespace tinysog {
    namespace internal {

        tinysog::internal::expected<std::vector<uint8_t>, std::string> encode_webp(
            const uint8_t* data,
            int width,
            int height,
            int quality) {

            if (!data || width <= 0 || height <= 0) {
                return tinysog::internal::unexpected(std::string("Invalid input parameters"));
            }

            uint8_t* output = nullptr;
            size_t output_size = 0;

            // Lossless encoding
            output_size = WebPEncodeLosslessRGBA(
                data,
                width,
                height,
                width * 4,
                &output);

            if (output_size == 0 || !output) {
                if (output)
                    WebPFree(output);
                return tinysog::internal::unexpected(std::string("WebP encoding failed"));
            }

            // Copy to vector
            std::vector<uint8_t> result(output, output + output_size);
            WebPFree(output);

            return result;
        }

        tinysog::internal::expected<WebPImage, std::string> decode_webp(
            const uint8_t* data,
            size_t size) {

            if (!data || size == 0) {
                return tinysog::internal::unexpected(std::string("Invalid input data"));
            }

            WebPImage result;

            // Get image information
            if (!WebPGetInfo(data, size, &result.width, &result.height)) {
                return tinysog::internal::unexpected(std::string("Failed to get WebP info"));
            }

            // Decode as RGBA
            uint8_t* decoded = WebPDecodeRGBA(
                data,
                size,
                &result.width,
                &result.height);

            if (!decoded) {
                return tinysog::internal::unexpected(std::string("Failed to decode WebP image"));
            }

            // Copy data
            size_t data_size = result.width * result.height * 4;
            result.data.assign(decoded, decoded + data_size);
            result.channels = 4;

            WebPFree(decoded);

            return result;
        }

    } // namespace internal
} // namespace tinysog




