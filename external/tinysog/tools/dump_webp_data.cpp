/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <archive.h>
#include <archive_entry.h>
#include <webp/decode.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <sog_file>\n";
        return 1;
    }

    std::cout << "=== WebP Data Dumper ===\n\n";
    
    // Read SOG archive
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    
    if (archive_read_open_filename(a, argv[1], 10240) != ARCHIVE_OK) {
        std::cerr << "Failed to open " << argv[1] << "\n";
        return 1;
    }

    // Find means_l.webp
    struct archive_entry* entry;
    std::vector<uint8_t> webp_data;
    
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name = archive_entry_pathname(entry);
        if (name == "means_l.webp") {
            size_t size = archive_entry_size(entry);
            webp_data.resize(size);
            archive_read_data(a, webp_data.data(), size);
            std::cout << "Found means_l.webp (" << size << " bytes)\n";
            break;
        }
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    
    if (webp_data.empty()) {
        std::cerr << "means_l.webp not found in archive\n";
        return 1;
    }

    // Decode WebP
    int width, height;
    uint8_t* rgba = WebPDecodeRGBA(webp_data.data(), webp_data.size(), &width, &height);
    
    if (!rgba) {
        std::cerr << "Failed to decode WebP\n";
        return 1;
    }

    std::cout << "Decoded WebP: " << width << "x" << height << "\n\n";
    
    // Print first 10 pixels
    std::cout << "First 10 pixels (RGBA):\n";
    int max_pixels = (width * height < 10) ? width * height : 10;
    for (int i = 0; i < max_pixels; i++) {
        int idx = i * 4;
        printf("  [%d]: R=%3d G=%3d B=%3d A=%3d\n", i,
               rgba[idx+0], rgba[idx+1], rgba[idx+2], rgba[idx+3]);
    }
    
    // Check for all-zero or all-255
    int zero_count = 0;
    int ff_count = 0;
    int total_bytes = width * height * 4;
    for (int i = 0; i < total_bytes; i++) {
        if (rgba[i] == 0) zero_count++;
        if (rgba[i] == 255) ff_count++;
    }
    
    std::cout << "\nStatistics:\n";
    std::cout << "  Total bytes: " << (width * height * 4) << "\n";
    std::cout << "  Zero bytes: " << zero_count << " (" << (100.0 * zero_count / (width * height * 4)) << "%)\n";
    std::cout << "  0xFF bytes: " << ff_count << " (" << (100.0 * ff_count / (width * height * 4)) << "%)\n";
    
    WebPFree(rgba);
    
    return 0;
}

