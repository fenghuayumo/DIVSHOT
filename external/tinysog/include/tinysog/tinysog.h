/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include "expected.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace tinysog {

    /**
     * @brief Gaussian point cloud data structure
     *
     * Pointers can point to either CUDA device memory or CPU memory
     */
    struct GaussianData {
        // Number of Gaussian points
        size_t count = 0;

        // Positions [count, 3] - float
        float* means = nullptr;

        // Rotation quaternions [count, 4] - float (w, x, y, z)
        float* rotations = nullptr;

        // Scales [count, 3] - float (logarithmic space)
        float* scales = nullptr;

        // Base color [count, 3] - float
        float* sh0 = nullptr;

        // Opacity [count] - float (logit space)
        float* opacity = nullptr;

        // Higher-order spherical harmonics [count, num_coeffs, 3] - float (optional)
        float* shN = nullptr;
        int sh_degree = 0;      // Spherical harmonics degree (0-3)
        int sh_num_coeffs = 0;  // Number of coefficients per color channel

        // Scene scale factor
        float scene_scale = 1.0f;
        
        // Memory location flag (true = CPU memory, false = CUDA device memory)
        bool is_cpu = false;
    };

    /**
     * @brief Writer options
     */
    struct WriterOptions {
        // Number of k-means clustering iterations
        int iterations = 10;

        // Whether to output as .sog bundle (true) or directory (false)
        bool bundle = true;

        // WebP compression quality (0-100, 100 is lossless)
        int webp_quality = 100;
    };

    /**
     * @brief Reader options
     */
    struct ReaderOptions {
        // Whether to load to CPU memory only (not upload to GPU)
        bool cpu_only = false;

        // Whether to validate only without actually loading data
        bool validate_only = false;
    };

    /**
     * @brief Write SOG format file
     *
     * @param output_path Output path (.sog file or directory)
     * @param data Gaussian point cloud data (must be in CUDA device memory)
     * @param options Writer options
     * @return Returns void on success, error message on failure
     */
    tinysog::internal::expected<void, std::string> write_sog(
        const char* output_path,
        const GaussianData& data,
        const WriterOptions& options = WriterOptions{});

    /**
     * @brief Read SOG format file
     *
     * @param input_path Input path (.sog file, directory, or meta.json)
     * @param options Reader options
     * @return Returns Gaussian data (in CUDA device memory) on success, error message on failure
     */
    tinysog::internal::expected<GaussianData, std::string> read_sog(
        const char* input_path,
        const ReaderOptions& options = ReaderOptions{});

    /**
     * @brief Allocate CUDA device memory for GaussianData
     *
     * @param count Number of Gaussian points
     * @param sh_degree Spherical harmonics degree (0-3), 0 means only sh0
     * @return Returns allocated data structure on success, error message on failure
     */
    tinysog::internal::expected<GaussianData, std::string> allocate_gaussian_data(
        size_t count,
        int sh_degree = 0);

    /**
     * @brief Free CUDA device memory of GaussianData
     *
     * @param data Data to be freed
     */
    void free_gaussian_data(GaussianData& data);

    /**
     * @brief Get library version information
     *
     * @return Version string
     */
    const char* get_version();

} // namespace tinysog

