/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "tinysog/tinysog.h"
#include "internal/archive_utils.h"
#include "internal/cuda_utils.cuh"
#include "internal/data_types.h"
#include "internal/decoder.h"
#include "internal/encoder.h"
#include "internal/webp_utils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
namespace tinysog {

    using namespace internal;

    tinysog::internal::expected<void, std::string> write_sog(
        const char* output_path,
        const GaussianData& data,
        const WriterOptions& options) {

        if (data.count == 0) {
            return tinysog::internal::unexpected(std::string("No gaussians to write"));
        }

        try {
        // Use either original data pointers or GPU-copied pointers
        const float* means_ptr = data.means;
        const float* rotations_ptr = data.rotations;
        const float* scales_ptr = data.scales;
        const float* sh0_ptr = data.sh0;
        const float* shN_ptr = data.shN;
        const float* opacity_ptr = data.opacity;
        
        // Optional GPU data holder
        internal::expected<GaussianData, std::string> data_gpu;
            
            if(data.is_cpu) {
                // Copy to GPU
                data_gpu = allocate_gaussian_data(data.count, data.sh_degree);
                if(!data_gpu) {
                    return tinysog::internal::unexpected(data_gpu.error());
                }
                cudaError_t err;
                err = cudaMemcpy(data_gpu->means, data.means, data.count * 3 * sizeof(float), cudaMemcpyHostToDevice);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy means to GPU: " << cudaGetErrorString(err) << std::endl;
                    return tinysog::internal::unexpected(std::string("Failed to copy means to GPU: ") + cudaGetErrorString(err));
                }
                
                err = cudaMemcpy(data_gpu->rotations, data.rotations, data.count * 4 * sizeof(float), cudaMemcpyHostToDevice);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy rotations to GPU: " << cudaGetErrorString(err) << std::endl;
                    return tinysog::internal::unexpected(std::string("Failed to copy rotations to GPU: ") + cudaGetErrorString(err));
                }
                
                err = cudaMemcpy(data_gpu->scales, data.scales, data.count * 3 * sizeof(float), cudaMemcpyHostToDevice);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy scales to GPU: " << cudaGetErrorString(err) << std::endl;
                    return tinysog::internal::unexpected(std::string("Failed to copy scales to GPU: ") + cudaGetErrorString(err));
                }
                
                err = cudaMemcpy(data_gpu->sh0, data.sh0, data.count * 3 * sizeof(float), cudaMemcpyHostToDevice);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy sh0 to GPU: " << cudaGetErrorString(err) << std::endl;
                    return tinysog::internal::unexpected(std::string("Failed to copy sh0 to GPU: ") + cudaGetErrorString(err));
                }
                
            err = cudaMemcpy(data_gpu->opacity, data.opacity, data.count * sizeof(float), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                std::cerr << "Failed to copy opacity to GPU: " << cudaGetErrorString(err) << std::endl;
                return tinysog::internal::unexpected(std::string("Failed to copy opacity to GPU: ") + cudaGetErrorString(err));
            }
            
            // Copy shN if present
            if (data.shN != nullptr && data.sh_num_coeffs > 0) {
                err = cudaMemcpy(data_gpu->shN, data.shN, data.count * data.sh_num_coeffs * 3 * sizeof(float), cudaMemcpyHostToDevice);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy shN to GPU: " << cudaGetErrorString(err) << std::endl;
                    return tinysog::internal::unexpected(std::string("Failed to copy shN to GPU: ") + cudaGetErrorString(err));
                }
            }
            
            // Use GPU pointers
            means_ptr = data_gpu->means;
            rotations_ptr = data_gpu->rotations;
            scales_ptr = data_gpu->scales;
            sh0_ptr = data_gpu->sh0;
            shN_ptr = data_gpu->shN;
            opacity_ptr = data_gpu->opacity;
            }
            
            // Wrap as internal Tensor format
            Tensor<float> means({data.count, 3}, true);
            Tensor<float> rotations({data.count, 4}, true);
            Tensor<float> scales({data.count, 3}, true);
            Tensor<float> sh0({data.count, 3}, true);
            Tensor<float> opacity({data.count}, true);

            means.data = const_cast<float*>(means_ptr);
            means.owns_data = false;
            rotations.data = const_cast<float*>(rotations_ptr);
            rotations.owns_data = false;
            scales.data = const_cast<float*>(scales_ptr);
            scales.owns_data = false;
            sh0.data = const_cast<float*>(sh0_ptr);
            sh0.owns_data = false;
            opacity.data = const_cast<float*>(opacity_ptr);
            opacity.owns_data = false;

        Tensor<float>* shN_tensor_ptr = nullptr;
        Tensor<float> shN_tensor;
        if (shN_ptr && data.sh_num_coeffs > 0) {
            // Shape should be [n * total_coeffs] where total_coeffs = sh_num_coeffs * 3
            shN_tensor = Tensor<float>({data.count * data.sh_num_coeffs * 3}, true);
            shN_tensor.data = const_cast<float*>(shN_ptr);
            shN_tensor.owns_data = false;
            shN_tensor_ptr = &shN_tensor;
        }

            // Encode
            EncoderConfig config;
            config.kmeans_iterations = options.iterations;
            config.webp_quality = options.webp_quality;
            config.use_morton_sort = true;  // Enable Morton sorting to match tensor version format

        auto encoded = encode_sog(means, rotations, scales, sh0, opacity,
                                  shN_tensor_ptr, data.sh_degree, config);

            if (!encoded) {
                return tinysog::internal::unexpected(encoded.error());
            }

            // Write to file
            std::filesystem::path path(output_path);
            bool is_bundle = path.extension() == ".sog" || options.bundle;

            if (is_bundle) {
                // Write ZIP archive
                ArchiveWriter writer(path);

                // Write all WebP images
                for (const auto& [name, img] : encoded->images) {
                    if (!writer.add_file(name, img.data.data(), img.data.size())) {
                        return tinysog::internal::unexpected("Failed to add " + name + " to archive");
                    }
                }

                // Write meta.json
                nlohmann::json meta;
                meta["version"] = encoded->metadata.version;
                meta["count"] = encoded->metadata.count;
                meta["width"] = encoded->metadata.width;
                meta["height"] = encoded->metadata.height;

                // Means (positions)
                meta["means"]["mins"] = {
                    encoded->metadata.means_mins[0],
                    encoded->metadata.means_mins[1],
                    encoded->metadata.means_mins[2]
                };
                meta["means"]["maxs"] = {
                    encoded->metadata.means_maxs[0],
                    encoded->metadata.means_maxs[1],
                    encoded->metadata.means_maxs[2]
                };
                meta["means"]["files"] = {"means_l.webp", "means_u.webp"};

                // Scales
                meta["scales"]["codebook"] = encoded->metadata.scales_codebook;
                meta["scales"]["files"] = {"scales.webp"};

                // Quaternions
                meta["quats"]["files"] = {"quats.webp"};

            // Colors (sh0)
            meta["sh0"]["codebook"] = encoded->metadata.sh0_codebook;
            meta["sh0"]["files"] = {"sh0.webp"};

            meta["sh_degree"] = data.sh_degree;

            // Spherical harmonics (optional)
            if (encoded->metadata.shN_data) {
                meta["shN"]["codebook"] = encoded->metadata.shN_data->codebook;
                meta["shN"]["palette_size"] = encoded->metadata.shN_data->palette_size;
                meta["shN"]["bands"] = encoded->metadata.shN_data->bands;
                meta["shN"]["coeffs"] = encoded->metadata.shN_data->coeffs;
                meta["shN"]["files"] = {"shN_centroids.webp", "shN_labels.webp"};
            }

            std::string meta_str = meta.dump(2);
                if (!writer.add_file("meta.json", meta_str.data(), meta_str.size())) {
                    return tinysog::internal::unexpected(std::string("Failed to add meta.json to archive"));
                }

            } else {
                // Write to directory
                std::filesystem::create_directories(path);

                // Write WebP files
                for (const auto& [name, img] : encoded->images) {
                    auto file_path = path / name;
                    std::ofstream file(file_path, std::ios::binary);
                    if (!file) {
                        return tinysog::internal::unexpected("Failed to open " + file_path.string());
                    }
                    file.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
                }

                // Write meta.json
                nlohmann::json meta;
                meta["version"] = encoded->metadata.version;
                meta["count"] = encoded->metadata.count;
                meta["width"] = encoded->metadata.width;
                meta["height"] = encoded->metadata.height;

                meta["means"]["mins"] = {
                    encoded->metadata.means_mins[0],
                    encoded->metadata.means_mins[1],
                    encoded->metadata.means_mins[2]
                };
                meta["means"]["maxs"] = {
                    encoded->metadata.means_maxs[0],
                    encoded->metadata.means_maxs[1],
                    encoded->metadata.means_maxs[2]
                };
                meta["means"]["files"] = {"means_l.webp", "means_u.webp"};

                meta["scales"]["codebook"] = encoded->metadata.scales_codebook;
                meta["scales"]["files"] = {"scales.webp"};

                meta["quats"]["files"] = {"quats.webp"};

                meta["sh0"]["codebook"] = encoded->metadata.sh0_codebook;
                meta["sh0"]["files"] = {"sh0.webp"};

                meta["sh_degree"] = data.sh_degree;

                if (encoded->metadata.shN_data) {
                    meta["shN"]["codebook"] = encoded->metadata.shN_data->codebook;
                    meta["shN"]["palette_size"] = encoded->metadata.shN_data->palette_size;
                    meta["shN"]["bands"] = encoded->metadata.shN_data->bands;
                    meta["shN"]["coeffs"] = encoded->metadata.shN_data->coeffs;
                    meta["shN"]["files"] = {"shN_centroids.webp", "shN_labels.webp"};
                }

                auto meta_path = path / "meta.json";
                std::ofstream meta_file(meta_path);
                if (!meta_file) {
                    return tinysog::internal::unexpected(std::string("Failed to open meta.json"));
                }
                meta_file << meta.dump(2);
            }

            // Cleanup GPU data if allocated
            if (data_gpu) {
                free_gaussian_data(*data_gpu);
            }

            return {};

        } catch (const std::exception& e) {
            return tinysog::internal::unexpected(std::string("Write failed: ") + e.what());
        }
    }

    tinysog::internal::expected<GaussianData, std::string> read_sog(
        const char* input_path,
        const ReaderOptions& options) {

        try {
            std::filesystem::path path(input_path);

            if (!std::filesystem::exists(path)) {
                return tinysog::internal::unexpected("Path does not exist: " + path.string());
            }

            // Read files
            std::map<std::string, std::vector<uint8_t>> files;

            if (path.extension() == ".sog") {
                // Read ZIP archive
                auto result = read_archive(path);
                if (!result) {
                    return tinysog::internal::unexpected(result.error());
                }
                files = std::move(result.value());

            } else if (std::filesystem::is_directory(path)) {
                // Read directory
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        std::ifstream file(entry.path(), std::ios::binary);
                        if (!file) {
                            return tinysog::internal::unexpected("Failed to read " + entry.path().string());
                        }
                        
                        std::vector<uint8_t> data(
                            (std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
                        
                        files[entry.path().filename().string()] = std::move(data);
                    }
                }

            } else {
                return tinysog::internal::unexpected(std::string("Unknown file format"));
            }

            // Parse meta.json
            auto it = files.find("meta.json");
            if (it == files.end()) {
                return tinysog::internal::unexpected(std::string("Missing meta.json"));
            }

            std::string meta_str(it->second.begin(), it->second.end());
            auto meta_json = nlohmann::json::parse(meta_str);

        SogMetadata metadata;
        metadata.version = meta_json["version"];
        metadata.count = meta_json["count"];
        metadata.width = meta_json["width"];
        metadata.height = meta_json["height"];
        metadata.sh_degree = meta_json.value("sh_degree", 0);

            // Parse means metadata
            if (meta_json.contains("means")) {
                auto& means = meta_json["means"];
                if (means.contains("mins") && means["mins"].size() >= 3) {
                    metadata.means_mins[0] = means["mins"][0];
                    metadata.means_mins[1] = means["mins"][1];
                    metadata.means_mins[2] = means["mins"][2];
                }
                if (means.contains("maxs") && means["maxs"].size() >= 3) {
                    metadata.means_maxs[0] = means["maxs"][0];
                    metadata.means_maxs[1] = means["maxs"][1];
                    metadata.means_maxs[2] = means["maxs"][2];
                }
            }

            // Parse scales codebook
            if (meta_json.contains("scales") && meta_json["scales"].contains("codebook")) {
                auto& codebook = meta_json["scales"]["codebook"];
                metadata.scales_codebook.resize(codebook.size());
                for (size_t i = 0; i < codebook.size(); ++i) {
                    metadata.scales_codebook[i] = codebook[i];
                }
            }

            // Parse sh0 codebook
            if (meta_json.contains("sh0") && meta_json["sh0"].contains("codebook")) {
                auto& codebook = meta_json["sh0"]["codebook"];
                metadata.sh0_codebook.resize(codebook.size());
                for (size_t i = 0; i < codebook.size(); ++i) {
                    metadata.sh0_codebook[i] = codebook[i];
                }
            }

            // Parse shN metadata (optional)
            if (meta_json.contains("shN")) {
                metadata.shN_data = std::make_unique<SogMetadata::ShNData>();
                auto& shN = meta_json["shN"];
                
                if (shN.contains("codebook")) {
                    auto& codebook = shN["codebook"];
                    metadata.shN_data->codebook.resize(codebook.size());
                    for (size_t i = 0; i < codebook.size(); ++i) {
                        metadata.shN_data->codebook[i] = codebook[i];
                    }
                }
                
                if (shN.contains("palette_size")) {
                    metadata.shN_data->palette_size = shN["palette_size"];
                }
                if (shN.contains("bands")) {
                    metadata.shN_data->bands = shN["bands"];
                }
                if (shN.contains("coeffs")) {
                    metadata.shN_data->coeffs = shN["coeffs"];
                }
            }

            // Decode WebP images
            std::map<std::string, WebPImage> images;
            for (const auto& [name, data] : files) {
                if (name.ends_with(".webp")) {
                    auto decoded = decode_webp(data.data(), data.size());
                    if (!decoded) {
                        return tinysog::internal::unexpected(decoded.error());
                    }
                    images[name] = std::move(decoded.value());
                }
            }

            // Decode gaussian data
            auto decoded_data = decode_sog(metadata, images, !options.cpu_only);
            if (!decoded_data) {
                return tinysog::internal::unexpected(decoded_data.error());
            }

            // Convert to public API format
            GaussianData result;
            result.count = decoded_data->means.shape[0];
            result.means = decoded_data->means.data;
            result.rotations = decoded_data->rotations.data;
            result.scales = decoded_data->scales.data;
            result.sh0 = decoded_data->sh0.data;
            result.opacity = decoded_data->opacity.data;
            result.shN = (decoded_data->shN.data != nullptr) ? decoded_data->shN.data : nullptr;
            result.sh_degree = decoded_data->sh_degree;
            // sh_num_coeffs is coefficients per channel (shN shape is [n, coeffs_per_channel * 3])
            result.sh_num_coeffs = (decoded_data->shN.data != nullptr) ? (decoded_data->shN.shape[1] / 3) : 0;
            result.is_cpu = options.cpu_only;  // Set CPU/GPU flag

            // Mark as not owning data (managed by decoded_data)
            decoded_data->means.owns_data = false;
            decoded_data->rotations.owns_data = false;
            decoded_data->scales.owns_data = false;
            decoded_data->sh0.owns_data = false;
            decoded_data->opacity.owns_data = false;
            if (decoded_data->shN.data != nullptr) {
                decoded_data->shN.owns_data = false;
            }

            return result;

        } catch (const std::exception& e) {
            return tinysog::internal::unexpected(std::string("Read failed: ") + e.what());
        }
    }

    tinysog::internal::expected<GaussianData, std::string> allocate_gaussian_data(
        size_t count,
        int sh_degree) {

        try {
            GaussianData data;
            data.count = count;
            data.sh_degree = sh_degree;
            data.is_cpu = false;  // Allocating CUDA device memory

            data.means = cuda_malloc<float>(count * 3);
            data.rotations = cuda_malloc<float>(count * 4);
            data.scales = cuda_malloc<float>(count * 3);
            data.sh0 = cuda_malloc<float>(count * 3);
            data.opacity = cuda_malloc<float>(count);

            if (sh_degree > 0) {
                // 计算球谐系数数量
                const int coeffs[] = {0, 3, 8, 15};
                data.sh_num_coeffs = coeffs[sh_degree];
                data.shN = cuda_malloc<float>(count * data.sh_num_coeffs * 3);
            }

            return data;

        } catch (const std::exception& e) {
            return tinysog::internal::unexpected(std::string("Allocation failed: ") + e.what());
        }
    }

    void free_gaussian_data(GaussianData& data) {
        if (data.is_cpu) {
            // Free CPU memory
            if (data.means) {
                std::free(data.means);
                data.means = nullptr;
            }
            if (data.rotations) {
                std::free(data.rotations);
                data.rotations = nullptr;
            }
            if (data.scales) {
                std::free(data.scales);
                data.scales = nullptr;
            }
            if (data.sh0) {
                std::free(data.sh0);
                data.sh0 = nullptr;
            }
            if (data.opacity) {
                std::free(data.opacity);
                data.opacity = nullptr;
            }
            if (data.shN) {
                std::free(data.shN);
                data.shN = nullptr;
            }
        } else {
            // Free CUDA device memory
            if (data.means) {
                cuda_free(data.means);
                data.means = nullptr;
            }
            if (data.rotations) {
                cuda_free(data.rotations);
                data.rotations = nullptr;
            }
            if (data.scales) {
                cuda_free(data.scales);
                data.scales = nullptr;
            }
            if (data.sh0) {
                cuda_free(data.sh0);
                data.sh0 = nullptr;
            }
            if (data.opacity) {
                cuda_free(data.opacity);
                data.opacity = nullptr;
            }
            if (data.shN) {
                cuda_free(data.shN);
                data.shN = nullptr;
            }
        }
        data.count = 0;
    }

    const char* get_version() {
        return "TinySOG v0.1.0";
    }

} // namespace tinysog

