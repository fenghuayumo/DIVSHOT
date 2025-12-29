/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <tinysog/tinysog.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

void print_stats(const char* name, const float* data, size_t n, size_t stride = 1) {
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    double sum = 0.0;
    
    for (size_t i = 0; i < n; i++) {
        float val = data[i * stride];
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
        sum += val;
    }
    
    double mean = sum / n;
    
    std::cout << name << ":\n";
    std::cout << "  Min:  " << min_val << "\n";
    std::cout << "  Max:  " << max_val << "\n";
    std::cout << "  Mean: " << mean << "\n";
}

void print_first_few(const char* name, const float* data, size_t count, size_t dim) {
    std::cout << "\nFirst 5 " << name << ":\n";
    for (size_t i = 0; i < std::min(count, size_t(5)); i++) {
        std::cout << "  [" << i << "]: ";
        for (size_t d = 0; d < dim; d++) {
            std::cout << std::setw(12) << data[i * dim + d];
            if (d < dim - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <sog_file>\n";
        return 1;
    }

    std::cout << "=== SOG File Verification Tool ===\n\n";
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
    
    std::cout << "Successfully loaded " << n << " Gaussians\n";
    std::cout << "SH degree: " << data.sh_degree << "\n\n";

    // Print statistics
    std::cout << "=== Position Statistics ===\n";
    print_stats("X", data.means + 0, n, 3);
    print_stats("Y", data.means + 1, n, 3);
    print_stats("Z", data.means + 2, n, 3);
    print_first_few("positions", data.means, n, 3);

    std::cout << "\n=== Scale Statistics ===\n";
    print_stats("Scale X", data.scales + 0, n, 3);
    print_stats("Scale Y", data.scales + 1, n, 3);
    print_stats("Scale Z", data.scales + 2, n, 3);
    print_first_few("scales", data.scales, n, 3);

    std::cout << "\n=== Rotation Statistics ===\n";
    print_stats("Quat W", data.rotations + 0, n, 4);
    print_stats("Quat X", data.rotations + 1, n, 4);
    print_stats("Quat Y", data.rotations + 2, n, 4);
    print_stats("Quat Z", data.rotations + 3, n, 4);
    print_first_few("rotations", data.rotations, n, 4);

    std::cout << "\n=== SH0 (Color) Statistics ===\n";
    print_stats("SH0 R", data.sh0 + 0, n, 3);
    print_stats("SH0 G", data.sh0 + 1, n, 3);
    print_stats("SH0 B", data.sh0 + 2, n, 3);
    print_first_few("sh0", data.sh0, n, 3);

    std::cout << "\n=== Opacity Statistics ===\n";
    print_stats("Opacity", data.opacity, n, 1);
    
    // Count how many are nearly transparent or opaque
    int nearly_transparent = 0;
    int nearly_opaque = 0;
    for (size_t i = 0; i < n; i++) {
        float op = data.opacity[i];
        if (op < 0.1f) nearly_transparent++;
        if (op > 0.9f) nearly_opaque++;
    }
    std::cout << "  Nearly transparent (<0.1): " << nearly_transparent << " (" 
              << (100.0 * nearly_transparent / n) << "%)\n";
    std::cout << "  Nearly opaque (>0.9): " << nearly_opaque << " ("
              << (100.0 * nearly_opaque / n) << "%)\n";
    
    std::cout << "\nFirst 10 opacity values:\n  ";
    for (size_t i = 0; i < std::min(n, size_t(10)); i++) {
        std::cout << data.opacity[i];
        if (i < 9) std::cout << ", ";
    }
    std::cout << "\n";

    // Check for NaN or inf
    std::cout << "\n=== Data Validity Check ===\n";
    auto check_validity = [](const char* name, const float* data, size_t count) {
        int nan_count = 0;
        int inf_count = 0;
        for (size_t i = 0; i < count; i++) {
            if (std::isnan(data[i])) nan_count++;
            if (std::isinf(data[i])) inf_count++;
        }
        if (nan_count > 0 || inf_count > 0) {
            std::cout << "  " << name << ": " << nan_count << " NaN, " 
                      << inf_count << " Inf\n";
        } else {
            std::cout << "  " << name << ": OK\n";
        }
    };
    
    check_validity("Positions", data.means, n * 3);
    check_validity("Scales", data.scales, n * 3);
    check_validity("Rotations", data.rotations, n * 4);
    check_validity("SH0", data.sh0, n * 3);
    check_validity("Opacity", data.opacity, n);

    tinysog::free_gaussian_data(data);
    
    std::cout << "\nVerification complete!\n";
    return 0;
}

