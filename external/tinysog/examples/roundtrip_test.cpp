/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file roundtrip_test.cpp
 * @brief 完整的编码-解码往返测试
 * 
 * 此示例演示如何：
 * 1. 创建测试高斯数据
 * 2. 写入SOG文件
 * 3. 读取SOG文件
 * 4. 验证数据完整性
 */

#include <tinysog/tinysog.h>
#include <iostream>
#include <cmath>
#include <random>
#include <cuda_runtime.h>

// 辅助函数：打印CUDA错误
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl; \
            exit(1); \
        } \
    } while(0)

// 生成随机高斯数据
void generate_random_gaussians(tinysog::GaussianData& data, size_t count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(-10.0f, 10.0f);
    std::normal_distribution<float> scale_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> color_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> rot_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> opacity_dist(-5.0f, 5.0f);

    // 在CPU上生成数据
    std::vector<float> means_host(count * 3);
    std::vector<float> rotations_host(count * 4);
    std::vector<float> scales_host(count * 3);
    std::vector<float> sh0_host(count * 3);
    std::vector<float> opacity_host(count);

    for (size_t i = 0; i < count; ++i) {
        // 位置
        means_host[i * 3 + 0] = pos_dist(gen);
        means_host[i * 3 + 1] = pos_dist(gen);
        means_host[i * 3 + 2] = pos_dist(gen);

        // 四元数（归一化）
        float w = rot_dist(gen);
        float x = rot_dist(gen);
        float y = rot_dist(gen);
        float z = rot_dist(gen);
        float norm = std::sqrt(w*w + x*x + y*y + z*z);
        rotations_host[i * 4 + 0] = w / norm;
        rotations_host[i * 4 + 1] = x / norm;
        rotations_host[i * 4 + 2] = y / norm;
        rotations_host[i * 4 + 3] = z / norm;

        // 缩放
        scales_host[i * 3 + 0] = std::exp(scale_dist(gen));
        scales_host[i * 3 + 1] = std::exp(scale_dist(gen));
        scales_host[i * 3 + 2] = std::exp(scale_dist(gen));

        // 颜色
        sh0_host[i * 3 + 0] = color_dist(gen);
        sh0_host[i * 3 + 1] = color_dist(gen);
        sh0_host[i * 3 + 2] = color_dist(gen);

        // 不透明度（logit空间）
        opacity_host[i] = opacity_dist(gen);
    }

    // 拷贝到GPU
    cudaError_t err;
    err = cudaMemcpy(data.means, means_host.data(), count * 3 * sizeof(float), cudaMemcpyHostToDevice);
    CUDA_CHECK(err);
    err = cudaMemcpy(data.rotations, rotations_host.data(), count * 4 * sizeof(float), cudaMemcpyHostToDevice);
    CUDA_CHECK(err);
    err = cudaMemcpy(data.scales, scales_host.data(), count * 3 * sizeof(float), cudaMemcpyHostToDevice);
    CUDA_CHECK(err);
    err = cudaMemcpy(data.sh0, sh0_host.data(), count * 3 * sizeof(float), cudaMemcpyHostToDevice);
    CUDA_CHECK(err);
    err = cudaMemcpy(data.opacity, opacity_host.data(), count * sizeof(float), cudaMemcpyHostToDevice);
    CUDA_CHECK(err);
}

// 计算平均误差
float compute_error(const float* a, const float* b, size_t count) {
    std::vector<float> a_host(count);
    std::vector<float> b_host(count);
    
    cudaError_t err;
    err = cudaMemcpy(a_host.data(), a, count * sizeof(float), cudaMemcpyDeviceToHost);
    CUDA_CHECK(err);
    err = cudaMemcpy(b_host.data(), b, count * sizeof(float), cudaMemcpyDeviceToHost);
    CUDA_CHECK(err);
    
    double sum_error = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum_error += std::abs(a_host[i] - b_host[i]);
    }
    return static_cast<float>(sum_error / count);
}

int main(int argc, char** argv) {
    std::cout << "=== TinySOG 往返测试 ===" << std::endl;
    std::cout << "TinySOG版本: " << tinysog::get_version() << std::endl << std::endl;

    // 测试参数
    const size_t count = 10000;
    const char* output_path = "test_output.sog";

    // Step 1: 分配和生成测试数据
    std::cout << "步骤 1: 生成 " << count << " 个高斯点..." << std::endl;
    auto alloc_result = tinysog::allocate_gaussian_data(count, 0);
    if (!alloc_result) {
        std::cerr << "分配失败: " << alloc_result.error() << std::endl;
        return 1;
    }
    auto data = alloc_result.value();
    generate_random_gaussians(data, count);
    std::cout << "[OK] Data generation completed" << std::endl << std::endl;

    // Step 2: 写入SOG文件
    std::cout << "步骤 2: 写入SOG文件..." << std::endl;
    tinysog::WriterOptions write_opts;
    write_opts.iterations = 20;
    write_opts.webp_quality = 90.0f;
    write_opts.bundle = true;

    auto write_result = tinysog::write_sog(output_path, data, write_opts);
    if (!write_result) {
        std::cerr << "写入失败: " << write_result.error() << std::endl;
        tinysog::free_gaussian_data(data);
        return 1;
    }
    std::cout << "[OK] Successfully written to: " << output_path << std::endl << std::endl;

    // Step 3: 读取SOG文件
    std::cout << "步骤 3: 读取SOG文件..." << std::endl;
    tinysog::ReaderOptions read_opts;
    read_opts.cpu_only = false;

    auto read_result = tinysog::read_sog(output_path, read_opts);
    if (!read_result) {
        std::cerr << "读取失败: " << read_result.error() << std::endl;
        tinysog::free_gaussian_data(data);
        return 1;
    }
    auto loaded_data = read_result.value();
    std::cout << "[OK] Successfully read " << loaded_data.count << " Gaussian points" << std::endl << std::endl;

    // Step 4: 验证数据
    std::cout << "步骤 4: 验证数据完整性..." << std::endl;
    
    if (loaded_data.count != count) {
        std::cerr << "[ERROR] Count mismatch: " << loaded_data.count << " vs " << count << std::endl;
        tinysog::free_gaussian_data(data);
        tinysog::free_gaussian_data(loaded_data);
        return 1;
    }

    // 计算重建误差
    float means_error = compute_error(data.means, loaded_data.means, count * 3);
    float scales_error = compute_error(data.scales, loaded_data.scales, count * 3);
    float sh0_error = compute_error(data.sh0, loaded_data.sh0, count * 3);
    float opacity_error = compute_error(data.opacity, loaded_data.opacity, count);

    std::cout << "平均重建误差:" << std::endl;
    std::cout << "  位置:   " << means_error << std::endl;
    std::cout << "  缩放:   " << scales_error << std::endl;
    std::cout << "  颜色:   " << sh0_error << std::endl;
    std::cout << "  不透明: " << opacity_error << std::endl << std::endl;

    // 判断测试是否通过
    const float tolerance = 0.1f;
    bool passed = (means_error < tolerance) && 
                  (scales_error < tolerance) &&
                  (sh0_error < tolerance) &&
                  (opacity_error < tolerance);

    if (passed) {
        std::cout << "[PASS] Roundtrip test succeeded!" << std::endl;
    } else {
        std::cout << "[FAIL] Roundtrip test failed! Error exceeds tolerance " << tolerance << std::endl;
    }

    // 清理
    tinysog::free_gaussian_data(data);
    tinysog::free_gaussian_data(loaded_data);

    return passed ? 0 : 1;
}




