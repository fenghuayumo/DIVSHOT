/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once


#include "tinysog/expected.h"
#include "data_types.h"
#include <expected>
#include <string>

namespace tinysog {
    namespace internal {

        /**
         * @brief Compute Morton encoding for 3D positions (Z-order curve)
         *
         * Morton encoding maps 3D spatial positions to 1D curve, maintaining spatial locality,
         * used to improve cache hit rate during rendering
         *
         * @param positions Position data [n, 3]
         * @return Morton codes [n]
         */
        tinysog::internal::expected<Tensor<int64_t>, std::string> morton_encode(
            const Tensor<float>& positions);

        /**
         * @brief Sort by Morton codes and return indices
         *
         * @param morton_codes Morton codes [n]
         * @return Sorted indices [n]
         */
        tinysog::internal::expected<Tensor<int64_t>, std::string> morton_sort_indices(
            const Tensor<int64_t>& morton_codes);

    } // namespace internal
} // namespace tinysog

