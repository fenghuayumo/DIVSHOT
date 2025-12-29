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
         * @brief CUDA-accelerated k-means clustering result
         */
        struct KMeansResult {
            Tensor<float> centroids;  // [k, dim]
            Tensor<int32_t> labels;   // [n]
        };

        /**
         * @brief GPU-accelerated k-means clustering (multi-dimensional)
         *
         * @param data Input data [n, dim]
         * @param k Number of clusters
         * @param iterations Maximum number of iterations
         * @param tolerance Convergence tolerance
         * @return Clustering result (centroids and labels)
         */
        tinysog::internal::expected<KMeansResult, std::string> kmeans(
            const Tensor<float>& data,
            int k,
            int iterations = 10,
            float tolerance = 1e-4f);

        /**
         * @brief GPU-accelerated 1D k-means clustering
         *
         * Optimized for 1D data, uses uniform initialization and sorted optimization
         *
         * @param data Input data [n]
         * @param k Number of clusters (typically 256)
         * @param iterations Maximum number of iterations
         * @return Clustering result (sorted centroids and labels)
         */
        tinysog::internal::expected<KMeansResult, std::string> kmeans_1d(
            const Tensor<float>& data,
            int k = 256,
            int iterations = 10);

    } // namespace internal
} // namespace tinysog

