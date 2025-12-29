/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file ply_to_sog.cpp
 * @brief PLY to SOG converter
 * 
 * Converts 3D Gaussian Splatting PLY files to SOG format
 */

#include <tinysog/tinysog.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>
#include <filesystem>

// PLY property layout
struct PLYLayout {
    size_t vertex_count = 0;
    size_t vertex_stride = 0;
    
    // Property offsets (SIZE_MAX means not present)
    size_t pos_offsets[3] = {SIZE_MAX, SIZE_MAX, SIZE_MAX};
    size_t normal_offsets[3] = {SIZE_MAX, SIZE_MAX, SIZE_MAX};
    size_t opacity_offset = SIZE_MAX;
    size_t scale_offsets[3] = {SIZE_MAX, SIZE_MAX, SIZE_MAX};
    size_t rot_offsets[4] = {SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX};
    size_t dc_start_offset = SIZE_MAX;
    int dc_count = 0;
    size_t rest_start_offset = SIZE_MAX;
    int rest_count = 0;
    
    [[nodiscard]] bool has_positions() const { return pos_offsets[0] != SIZE_MAX; }
    [[nodiscard]] bool has_opacity() const { return opacity_offset != SIZE_MAX; }
    [[nodiscard]] bool has_scaling() const { return scale_offsets[0] != SIZE_MAX; }
    [[nodiscard]] bool has_rotation() const { return rot_offsets[0] != SIZE_MAX; }
    [[nodiscard]] int get_sh_degree() const {
        // f_rest contains (sh_degree+1)^2 - 1 coefficients per channel
        // For sh_degree=3: (3+1)^2 - 1 = 15 coefficients per channel = 45 total
        if (rest_count >= 45) return 3;
        if (rest_count >= 24) return 2;
        if (rest_count >= 9) return 1;
        return 0;
    }
};

PLYLayout parse_ply_header(std::ifstream& file) {
    PLYLayout layout;
    std::string line;
    bool in_vertex_element = false;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.find("element vertex") == 0) {
            sscanf(line.c_str(), "element vertex %zu", &layout.vertex_count);
            in_vertex_element = true;
        }
        else if (line.find("element") == 0) {
            in_vertex_element = false;
        }
        else if (in_vertex_element && line.find("property float") == 0) {
            // Extract property name
            size_t name_start = line.rfind(' ') + 1;
            std::string prop_name = line.substr(name_start);
            
            // Assign offset based on property name
            if (prop_name == "x") layout.pos_offsets[0] = layout.vertex_stride;
            else if (prop_name == "y") layout.pos_offsets[1] = layout.vertex_stride;
            else if (prop_name == "z") layout.pos_offsets[2] = layout.vertex_stride;
            else if (prop_name == "nx") layout.normal_offsets[0] = layout.vertex_stride;
            else if (prop_name == "ny") layout.normal_offsets[1] = layout.vertex_stride;
            else if (prop_name == "nz") layout.normal_offsets[2] = layout.vertex_stride;
            else if (prop_name == "opacity") layout.opacity_offset = layout.vertex_stride;
            else if (prop_name == "scale_0") layout.scale_offsets[0] = layout.vertex_stride;
            else if (prop_name == "scale_1") layout.scale_offsets[1] = layout.vertex_stride;
            else if (prop_name == "scale_2") layout.scale_offsets[2] = layout.vertex_stride;
            else if (prop_name == "rot_0") layout.rot_offsets[0] = layout.vertex_stride;
            else if (prop_name == "rot_1") layout.rot_offsets[1] = layout.vertex_stride;
            else if (prop_name == "rot_2") layout.rot_offsets[2] = layout.vertex_stride;
            else if (prop_name == "rot_3") layout.rot_offsets[3] = layout.vertex_stride;
            else if (prop_name.find("f_dc_") == 0) {
                int idx = std::atoi(prop_name.c_str() + 5);
                if (idx == 0) layout.dc_start_offset = layout.vertex_stride;
                if (idx >= layout.dc_count) layout.dc_count = idx + 1;
            }
            else if (prop_name.find("f_rest_") == 0) {
                int idx = std::atoi(prop_name.c_str() + 7);
                if (idx == 0) layout.rest_start_offset = layout.vertex_stride;
                if (idx >= layout.rest_count) layout.rest_count = idx + 1;
            }
            
            layout.vertex_stride += sizeof(float);
        }
        else if (line.find("end_header") == 0) {
            break;
        }
    }
    
    return layout;
}

bool load_ply_binary(const std::string& filename, tinysog::GaussianData& data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open PLY file: " << filename << std::endl;
        return false;
    }
    
    // Parse header to get layout
    auto layout = parse_ply_header(file);
    std::cout << "PLY file contains " << layout.vertex_count << " vertices" << std::endl;
    std::cout << "Vertex stride: " << layout.vertex_stride << " bytes" << std::endl;
    
    if (layout.vertex_count == 0) {
        std::cerr << "No vertices found in PLY file" << std::endl;
        return false;
    }
    
    if (!layout.has_positions()) {
        std::cerr << "PLY file missing position data" << std::endl;
        return false;
    }
    
    // Determine SH degree from PLY data
    int sh_degree = layout.get_sh_degree();
    
    // Allocate data with correct sh_degree
    auto alloc_result = tinysog::allocate_gaussian_data(layout.vertex_count, sh_degree);
    if (!alloc_result) {
        std::cerr << "Failed to allocate data: " << alloc_result.error() << std::endl;
        return false;
    }
    data = alloc_result.value();
    
    const size_t n = layout.vertex_count;
    
    // Allocate temporary buffers
    std::vector<float> temp_positions(n * 3, 0.0f);
    std::vector<float> temp_sh_dc(n * 3, 0.0f);
    std::vector<float> temp_sh_rest;
    if (layout.rest_count > 0) {
        temp_sh_rest.resize(n * layout.rest_count, 0.0f);
    }
    std::vector<float> temp_opacity(n, 0.0f);
    std::vector<float> temp_scales(n * 3, 0.0f);
    std::vector<float> temp_rotation(n * 4);
    
    // Initialize rotation to identity quaternion [1, 0, 0, 0]
    for (size_t i = 0; i < n; i++) {
        temp_rotation[i * 4] = 1.0f;
        temp_rotation[i * 4 + 1] = 0.0f;
        temp_rotation[i * 4 + 2] = 0.0f;
        temp_rotation[i * 4 + 3] = 0.0f;
    }
    
    // Read binary data using computed offsets
    std::vector<char> vertex_buffer(layout.vertex_stride);
    
    for (size_t i = 0; i < n; i++) {
        // Read entire vertex
        file.read(vertex_buffer.data(), layout.vertex_stride);
        if (!file) {
            std::cerr << "Failed to read vertex " << i << std::endl;
            return false;
        }
        
        // Extract position
        if (layout.has_positions()) {
            temp_positions[i * 3 + 0] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.pos_offsets[0]);
            temp_positions[i * 3 + 1] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.pos_offsets[1]);
            temp_positions[i * 3 + 2] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.pos_offsets[2]);
        }
        
        // Extract SH DC coefficients (f_dc_0, f_dc_1, f_dc_2)
        if (layout.dc_start_offset != SIZE_MAX && layout.dc_count >= 3) {
            for (int j = 0; j < 3; j++) {
                temp_sh_dc[i * 3 + j] = *reinterpret_cast<const float*>(
                    vertex_buffer.data() + layout.dc_start_offset + j * sizeof(float));
            }
        }
        
        // Extract SH rest coefficients (f_rest_0, f_rest_1, ...)
        if (layout.rest_start_offset != SIZE_MAX && layout.rest_count > 0) {
            for (int j = 0; j < layout.rest_count; j++) {
                temp_sh_rest[i * layout.rest_count + j] = *reinterpret_cast<const float*>(
                    vertex_buffer.data() + layout.rest_start_offset + j * sizeof(float));
            }
        }
        
        // Extract opacity
        if (layout.has_opacity()) {
            temp_opacity[i] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.opacity_offset);
        }
        
        // Extract scales
        if (layout.has_scaling()) {
            temp_scales[i * 3 + 0] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.scale_offsets[0]);
            temp_scales[i * 3 + 1] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.scale_offsets[1]);
            temp_scales[i * 3 + 2] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.scale_offsets[2]);
        }
        
        // Extract rotation quaternion
        if (layout.has_rotation()) {
            temp_rotation[i * 4 + 0] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.rot_offsets[0]);
            temp_rotation[i * 4 + 1] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.rot_offsets[1]);
            temp_rotation[i * 4 + 2] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.rot_offsets[2]);
            temp_rotation[i * 4 + 3] = *reinterpret_cast<const float*>(vertex_buffer.data() + layout.rot_offsets[3]);
        }
    }
    
    // Convert opacity from logit space to [0,1] space
    // PLY stores opacity in logit space, but tinysog encoder expects [0,1]
    for (size_t i = 0; i < n; i++) {
        temp_opacity[i] = 1.0f / (1.0f + std::exp(-temp_opacity[i]));  // sigmoid
    }
    
    // Copy to GaussianData (use cudaMemcpy for CPU-to-GPU transfer)
    cudaError_t err;
    err = cudaMemcpy(data.means, temp_positions.data(), n * 3 * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        std::cerr << "Failed to copy means to GPU: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    
    err = cudaMemcpy(data.rotations, temp_rotation.data(), n * 4 * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        std::cerr << "Failed to copy rotations to GPU: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    
    err = cudaMemcpy(data.scales, temp_scales.data(), n * 3 * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        std::cerr << "Failed to copy scales to GPU: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    
    err = cudaMemcpy(data.sh0, temp_sh_dc.data(), n * 3 * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        std::cerr << "Failed to copy sh0 to GPU: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    
    // Copy shN if present
    if (data.shN != nullptr && !temp_sh_rest.empty()) {
        err = cudaMemcpy(data.shN, temp_sh_rest.data(), temp_sh_rest.size() * sizeof(float), cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            std::cerr << "Failed to copy shN to GPU: " << cudaGetErrorString(err) << std::endl;
            return false;
        }
    }
    
    err = cudaMemcpy(data.opacity, temp_opacity.data(), n * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        std::cerr << "Failed to copy opacity to GPU: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    
    std::cout << "Successfully loaded PLY data" << std::endl;
    return true;
}

int main(int argc, char** argv) {
    std::cout << "=== TinySOG PLY to SOG Converter ===" << std::endl;
    std::cout << "TinySOG version: " << tinysog::get_version() << std::endl << std::endl;
    
    // Parse arguments
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.ply> [output.sog]" << std::endl;
        std::cerr << "Example: " << argv[0] << " point_cloud.ply output.sog" << std::endl;
        std::cerr << "If output is not specified, will use input filename with .sog extension" << std::endl;
        return 1;
    }
    
    std::string input_path = argv[1];
    std::string output_path;
    
    if (argc == 3) {
        output_path = argv[2];
    } else {
        // Auto-generate output path
        std::filesystem::path input_fs(input_path);
        output_path = input_fs.parent_path().string() + "/" + input_fs.stem().string() + ".sog";
    }
    
    std::cout << "Input PLY:  " << input_path << std::endl;
    std::cout << "Output SOG: " << output_path << std::endl << std::endl;
    
    // Check input file exists
    if (!std::filesystem::exists(input_path)) {
        std::cerr << "Error: Input file does not exist: " << input_path << std::endl;
        return 1;
    }
    
    // Load PLY file
    std::cout << "Loading PLY file..." << std::endl;
    tinysog::GaussianData data;
    if (!load_ply_binary(input_path, data)) {
        std::cerr << "Failed to load PLY file" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Statistics:" << std::endl;
    std::cout << "  Point count: " << data.count << std::endl;
    std::cout << "  SH degree:   " << data.sh_degree << std::endl;
    std::cout << std::endl;
    
    // Write SOG file
    std::cout << "Writing SOG file..." << std::endl;
    std::cout << "  data.sh_degree before write: " << data.sh_degree << std::endl;
    std::cout << "  data.sh_num_coeffs before write: " << data.sh_num_coeffs << std::endl;
    std::cout << "  data.shN pointer: " << (data.shN ? "not null" : "null") << std::endl;
    std::cout << "  data.is_cpu: " << (data.is_cpu ? "true" : "false") << std::endl;
    tinysog::WriterOptions write_opts;
    write_opts.iterations = 20;
    write_opts.webp_quality = 100;  // Must be 100 (lossless) to preserve index values
    write_opts.bundle = true;
    
    auto write_result = tinysog::write_sog(output_path.c_str(), data, write_opts);
    if (!write_result) {
        std::cerr << "Failed to write SOG: " << write_result.error() << std::endl;
        tinysog::free_gaussian_data(data);
        return 1;
    }
    
    std::cout << "Successfully wrote SOG file!" << std::endl;
    
    // Get file sizes
    auto input_size = std::filesystem::file_size(input_path);
    auto output_size = std::filesystem::file_size(output_path);
    
    std::cout << std::endl;
    std::cout << "File sizes:" << std::endl;
    std::cout << "  Input PLY:  " << (input_size / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  Output SOG: " << (output_size / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  Compression ratio: " << (100.0 * output_size / input_size) << "%" << std::endl;
    std::cout << std::endl;
    
    // Verify by reading back
    std::cout << "Verifying by reading back..." << std::endl;
    tinysog::ReaderOptions read_opts;
    read_opts.cpu_only = true;  // Read to CPU for verification
    
    auto read_result = tinysog::read_sog(output_path.c_str(), read_opts);
    if (!read_result) {
        std::cerr << "Verification failed: " << read_result.error() << std::endl;
        tinysog::free_gaussian_data(data);
        return 1;
    }
    
    auto loaded = read_result.value();
    std::cout << "Verification successful! Loaded " << loaded.count << " points" << std::endl;
    
    // Cleanup
    tinysog::free_gaussian_data(data);
    tinysog::free_gaussian_data(loaded);
    
    std::cout << std::endl;
    std::cout << "Conversion completed successfully!" << std::endl;
    
    return 0;
}

