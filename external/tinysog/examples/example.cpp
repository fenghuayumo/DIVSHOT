/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <tinysog/tinysog.h>
#include <cuda_runtime.h>
#include <iostream>
#include <random>

/**
 * @brief 生成随机测试数据
 */
tinysog::GaussianData generate_test_data(size_t count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    auto data_result = tinysog::allocate_gaussian_data(count, 0);
    if (!data_result) {
        std::cerr << "Failed to allocate: " << data_result.error() << std::endl;
        exit(1);
    }

    auto data = data_result.value();

    // 生成CPU端随机数据
    std::vector<float> cpu_means(count * 3);
    std::vector<float> cpu_rotations(count * 4);
    std::vector<float> cpu_scales(count * 3);
    std::vector<float> cpu_sh0(count * 3);
    std::vector<float> cpu_opacity(count);

    for (size_t i = 0; i < count; ++i) {
        // 位置
        cpu_means[i * 3 + 0] = dist(gen) * 10.0f;
        cpu_means[i * 3 + 1] = dist(gen) * 10.0f;
        cpu_means[i * 3 + 2] = dist(gen) * 10.0f;

        // 旋转（归一化四元数）
        float qw = dist(gen);
        float qx = dist(gen);
        float qy = dist(gen);
        float qz = dist(gen);
        float qlen = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
        cpu_rotations[i * 4 + 0] = qw / qlen;
        cpu_rotations[i * 4 + 1] = qx / qlen;
        cpu_rotations[i * 4 + 2] = qy / qlen;
        cpu_rotations[i * 4 + 3] = qz / qlen;

        // 缩放（对数空间）
        cpu_scales[i * 3 + 0] = std::log(0.01f + std::abs(dist(gen)) * 0.1f);
        cpu_scales[i * 3 + 1] = std::log(0.01f + std::abs(dist(gen)) * 0.1f);
        cpu_scales[i * 3 + 2] = std::log(0.01f + std::abs(dist(gen)) * 0.1f);

        // 颜色
        cpu_sh0[i * 3 + 0] = std::abs(dist(gen));
        cpu_sh0[i * 3 + 1] = std::abs(dist(gen));
        cpu_sh0[i * 3 + 2] = std::abs(dist(gen));

        // 不透明度（logit空间）
        float opacity = 0.5f + 0.5f * dist(gen);
        cpu_opacity[i] = std::log(opacity / (1.0f - opacity));
    }

    // 拷贝到GPU
    cudaMemcpy(data.means, cpu_means.data(), count * 3 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(data.rotations, cpu_rotations.data(), count * 4 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(data.scales, cpu_scales.data(), count * 3 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(data.sh0, cpu_sh0.data(), count * 3 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(data.opacity, cpu_opacity.data(), count * sizeof(float), cudaMemcpyHostToDevice);

    return data;
}

int main(int argc, char** argv) {
    std::cout << "=== TinySOG Example ===" << std::endl;
    std::cout << tinysog::get_version() << std::endl << std::endl;

    const size_t num_gaussians = 10000;
    const char* output_file = "test_output.sog";

    // 生成测试数据
    std::cout << "Generating " << num_gaussians << " random gaussians..." << std::endl;
    auto data = generate_test_data(num_gaussians);

    // 写入SOG文件
    std::cout << "Writing SOG file: " << output_file << std::endl;
    tinysog::WriterOptions write_opts;
    write_opts.iterations = 5;
    write_opts.bundle = true;

    auto write_result = tinysog::write_sog(output_file, data, write_opts);
    if (!write_result) {
        std::cerr << "Write failed: " << write_result.error() << std::endl;
        tinysog::free_gaussian_data(data);
        return 1;
    }

    std::cout << "Write successful!" << std::endl;

    // 释放原始数据
    tinysog::free_gaussian_data(data);

    // 读取SOG文件
    std::cout << "\nReading SOG file: " << output_file << std::endl;
    tinysog::ReaderOptions read_opts;

    auto read_result = tinysog::read_sog(output_file, read_opts);
    if (!read_result) {
        std::cerr << "Read failed: " << read_result.error() << std::endl;
        return 1;
    }

    auto loaded_data = read_result.value();
    std::cout << "Read successful!" << std::endl;
    std::cout << "Loaded " << loaded_data.count << " gaussians" << std::endl;
    std::cout << "SH degree: " << loaded_data.sh_degree << std::endl;

    // 清理
    tinysog::free_gaussian_data(loaded_data);

    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}







