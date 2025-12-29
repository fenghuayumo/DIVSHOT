/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <tinysog/tinysog.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <sog_file>\n";
        return 1;
    }

    std::cout << "=== Deep SOG Verification Tool ===\n\n";
    std::cout << "Reading: " << argv[1] << "\n\n";

    // Read SOG file
    tinysog::ReaderOptions opts;
    opts.cpu_only = true;
    
    auto result = tinysog::read_sog(argv[1], opts);
    if (!result) {
        std::cerr << "Error: " << result.error() << "\n";
        return 1;
    }

    auto data = result.value();
    size_t n = data.count;
    
    std::cout << "Loaded " << n << " Gaussians\n\n";

    // Test 1: Check for NaN/Inf
    std::cout << "=== Test 1: Checking for NaN/Inf ===\n";
    auto check_validity = [](const char* name, const float* data, size_t count) {
        int nan_count = 0;
        int inf_count = 0;
        for (size_t i = 0; i < count; i++) {
            if (std::isnan(data[i])) nan_count++;
            if (std::isinf(data[i])) inf_count++;
        }
        if (nan_count > 0 || inf_count > 0) {
            std::cout << "  ❌ " << name << ": " << nan_count << " NaN, " 
                      << inf_count << " Inf\n";
            return false;
        } else {
            std::cout << "  ✓ " << name << ": OK\n";
            return true;
        }
    };
    
    bool valid = true;
    valid &= check_validity("Positions", data.means, n * 3);
    valid &= check_validity("Scales", data.scales, n * 3);
    valid &= check_validity("Rotations", data.rotations, n * 4);
    valid &= check_validity("SH0", data.sh0, n * 3);
    valid &= check_validity("Opacity", data.opacity, n);
    
    if (data.shN != nullptr && data.sh_num_coeffs > 0) {
        std::cout << "\n  SH Degree: " << data.sh_degree << " (coeffs per channel: " << data.sh_num_coeffs << ")\n";
        valid &= check_validity("  ShN", data.shN, n * data.sh_num_coeffs * 3);
    } else {
        std::cout << "\n  No higher-order SH data\n";
    }

    // Test 2: Check quaternion normalization
    std::cout << "\n=== Test 2: Quaternion Normalization ===\n";
    int unnormalized_count = 0;
    float max_norm_error = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float w = data.rotations[i * 4 + 0];
        float x = data.rotations[i * 4 + 1];
        float y = data.rotations[i * 4 + 2];
        float z = data.rotations[i * 4 + 3];
        float norm = std::sqrt(w*w + x*x + y*y + z*z);
        float norm_error = std::abs(norm - 1.0f);
        max_norm_error = std::max(max_norm_error, norm_error);
        if (norm_error > 0.01f) {
            unnormalized_count++;
            if (unnormalized_count <= 3) {
                std::cout << "  Warning: Quat[" << i << "] norm=" << norm 
                          << " [" << w << ", " << x << ", " << y << ", " << z << "]\n";
            }
        }
    }
    if (unnormalized_count == 0) {
        std::cout << "  ✓ All quaternions normalized (max error: " << max_norm_error << ")\n";
    } else {
        std::cout << "  ⚠ " << unnormalized_count << " quaternions not normalized (max error: " 
                  << max_norm_error << ")\n";
    }

    // Test 3: Check opacity range
    std::cout << "\n=== Test 3: Opacity Range ===\n";
    int out_of_range = 0;
    for (size_t i = 0; i < n; i++) {
        float op = data.opacity[i];
        if (op < 0.0f || op > 1.0f) {
            out_of_range++;
            if (out_of_range <= 3) {
                std::cout << "  Warning: Opacity[" << i << "] = " << op << "\n";
            }
        }
    }
    if (out_of_range == 0) {
        std::cout << "  ✓ All opacity values in [0, 1]\n";
    } else {
        std::cout << "  ❌ " << out_of_range << " opacity values out of range\n";
        valid = false;
    }

    // Test 4: Check for duplicate positions (possible Morton sort issue)
    std::cout << "\n=== Test 4: Position Uniqueness ===\n";
    std::vector<std::tuple<float, float, float>> positions;
    for (size_t i = 0; i < std::min(n, size_t(1000)); i++) {
        positions.push_back({data.means[i*3], data.means[i*3+1], data.means[i*3+2]});
    }
    std::sort(positions.begin(), positions.end());
    int duplicate_count = 0;
    for (size_t i = 1; i < positions.size(); i++) {
        if (positions[i] == positions[i-1]) {
            duplicate_count++;
        }
    }
    if (duplicate_count == 0) {
        std::cout << "  ✓ No duplicate positions in first 1000 points\n";
    } else {
        std::cout << "  ⚠ " << duplicate_count << " duplicate positions found in first 1000\n";
    }

    // Test 5: Check scale range (log space)
    std::cout << "\n=== Test 5: Scale Range (log space) ===\n";
    float scale_min = 1e30f, scale_max = -1e30f;
    for (size_t i = 0; i < n * 3; i++) {
        scale_min = std::min(scale_min, data.scales[i]);
        scale_max = std::max(scale_max, data.scales[i]);
    }
    std::cout << "  Scale range: [" << scale_min << ", " << scale_max << "]\n";
    if (scale_min < -20.0f || scale_max > 10.0f) {
        std::cout << "  ⚠ Unusual scale range (expected roughly [-20, 5])\n";
    } else {
        std::cout << "  ✓ Scale range looks reasonable\n";
    }

    // Test 6: Check SH0 range
    std::cout << "\n=== Test 6: SH0 Range ===\n";
    float sh0_min = 1e30f, sh0_max = -1e30f;
    for (size_t i = 0; i < n * 3; i++) {
        sh0_min = std::min(sh0_min, data.sh0[i]);
        sh0_max = std::max(sh0_max, data.sh0[i]);
    }
    std::cout << "  SH0 range: [" << sh0_min << ", " << sh0_max << "]\n";
    if (sh0_min < -5.0f || sh0_max > 15.0f) {
        std::cout << "  ⚠ Unusual SH0 range (expected roughly [-3, 12])\n";
    } else {
        std::cout << "  ✓ SH0 range looks reasonable\n";
    }

    // Test 7: Check for constant values (possible encoding bug)
    std::cout << "\n=== Test 7: Value Variation ===\n";
    auto check_variation = [](const char* name, const float* data, size_t count, size_t stride) {
        float first = data[0];
        bool all_same = true;
        for (size_t i = 1; i < std::min(count * stride, size_t(100)); i += stride) {
            if (std::abs(data[i] - first) > 1e-6f) {
                all_same = false;
                break;
            }
        }
        if (all_same) {
            std::cout << "  ❌ " << name << ": First 100 values are identical (" << first << ")\n";
            return false;
        } else {
            std::cout << "  ✓ " << name << ": Values vary\n";
            return true;
        }
    };
    
    valid &= check_variation("Position X", data.means + 0, n, 3);
    valid &= check_variation("Position Y", data.means + 1, n, 3);
    valid &= check_variation("Position Z", data.means + 2, n, 3);
    valid &= check_variation("Scale X", data.scales + 0, n, 3);
    valid &= check_variation("SH0 R", data.sh0 + 0, n, 3);
    
    if (data.shN != nullptr && data.sh_num_coeffs > 0) {
        valid &= check_variation("ShN coeff[0]", data.shN + 0, n, data.sh_num_coeffs * 3);
    }
    
    // Test 8: Check shN range (if present)
    if (data.shN != nullptr && data.sh_num_coeffs > 0) {
        std::cout << "\n=== Test 8: ShN Range ===\n";
        float shN_min = 1e30f, shN_max = -1e30f;
        size_t total_shN = n * data.sh_num_coeffs * 3;
        for (size_t i = 0; i < total_shN; i++) {
            shN_min = std::min(shN_min, data.shN[i]);
            shN_max = std::max(shN_max, data.shN[i]);
        }
        std::cout << "  ShN range: [" << shN_min << ", " << shN_max << "]\n";
        if (shN_min < -10.0f || shN_max > 20.0f) {
            std::cout << "  ⚠ Unusual ShN range (expected roughly [-5, 15])\n";
        } else {
            std::cout << "  ✓ ShN range looks reasonable\n";
        }
    }

    tinysog::free_gaussian_data(data);
    
    std::cout << "\n==========\n";
    if (valid) {
        std::cout << "✓ All tests passed!\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed!\n";
        return 1;
    }
}

