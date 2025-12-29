/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "kmeans.cuh"
#include "cuda_utils.cuh"
#include <algorithm>
#include <cmath>
#include <random>
#include <thrust/device_ptr.h>
#include <thrust/gather.h>
#include <thrust/scatter.h>
#include <thrust/sequence.h>
#include <thrust/sort.h>

#include "tinysog/expected.h"
namespace tinysog {
    namespace internal {

        namespace {

            /**
             * @brief 分配聚类标签的CUDA kernel
             */
            __global__ void assign_clusters_kernel(
                const float* __restrict__ data,
                const float* __restrict__ centroids,
                int32_t* __restrict__ labels,
                const int n_points,
                const int n_clusters,
                const int n_dims) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n_points)
                    return;

                float min_dist = INFINITY;
                int32_t best_cluster = 0;

                // 对每个质心计算距离
                for (int c = 0; c < n_clusters; ++c) {
                    float dist = 0.0f;

                    // 计算欧氏距离平方
                    for (int d = 0; d < n_dims; ++d) {
                        float diff = data[idx * n_dims + d] - centroids[c * n_dims + d];
                        dist += diff * diff;
                    }

                    if (dist < min_dist) {
                        min_dist = dist;
                        best_cluster = c;
                    }
                }

                labels[idx] = best_cluster;
            }

            /**
             * @brief 1D聚类的优化kernel
             */
            __global__ void assign_clusters_1d_kernel(
                const float* __restrict__ data,
                const float* __restrict__ centroids,
                int32_t* __restrict__ labels,
                const int n_points,
                const int n_clusters) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n_points)
                    return;

                const float point = data[idx];
                float min_dist = INFINITY;
                int32_t best = 0;

                // 对于已排序的质心，可以使用线性搜索
                for (int c = 0; c < n_clusters; ++c) {
                    float dist = fabsf(point - centroids[c]);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best = c;
                    }
                }

                labels[idx] = best;
            }

            /**
             * @brief 更新质心的CUDA kernel
             */
            __global__ void update_centroids_kernel(
                const float* __restrict__ data,
                const int32_t* __restrict__ labels,
                float* __restrict__ new_centroids,
                int32_t* __restrict__ counts,
                const int n_points,
                const int n_clusters,
                const int n_dims) {

                const int cluster_id = blockIdx.x;
                const int dim = threadIdx.x;

                if (cluster_id >= n_clusters || dim >= n_dims)
                    return;

                float sum = 0.0f;
                int32_t count = 0;

                // 累加属于此聚类的所有点
                for (int i = 0; i < n_points; ++i) {
                    if (labels[i] == cluster_id) {
                        sum += data[i * n_dims + dim];
                        if (dim == 0)
                            count++;
                    }
                }

                // 保存计数
                if (dim == 0) {
                    counts[cluster_id] = count;
                }

                __syncthreads();

                // 计算平均值
                if (counts[cluster_id] > 0) {
                    new_centroids[cluster_id * n_dims + dim] = sum / counts[cluster_id];
                }
            }

            /**
             * @brief 计算质心移动距离
             */
            __global__ void compute_centroid_movement_kernel(
                const float* __restrict__ old_centroids,
                const float* __restrict__ new_centroids,
                float* __restrict__ movements,
                const int n_clusters,
                const int n_dims) {

                const int idx = blockIdx.x * blockDim.x + threadIdx.x;
                if (idx >= n_clusters)
                    return;

                float dist = 0.0f;
                for (int d = 0; d < n_dims; ++d) {
                    float diff = new_centroids[idx * n_dims + d] - old_centroids[idx * n_dims + d];
                    dist += diff * diff;
                }

                movements[idx] = sqrtf(dist);
            }

            /**
             * @brief 最大值归约kernel
             */
            __global__ void max_reduce_kernel(
                const float* __restrict__ input,
                float* __restrict__ output,
                const int n) {

                __shared__ float s_max[256];

                const int tid = threadIdx.x;
                const int idx = blockIdx.x * blockDim.x + tid;

                // 加载数据到共享内存
                s_max[tid] = (idx < n) ? input[idx] : -INFINITY;
                __syncthreads();

                // 归约求最大值
                for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
                    if (tid < stride) {
                        s_max[tid] = fmaxf(s_max[tid], s_max[tid + stride]);
                    }
                    __syncthreads();
                }

                // 写回结果
                if (tid == 0) {
                    output[blockIdx.x] = s_max[0];
                }
            }

            /**
             * @brief 使用k-means++初始化质心
             */
            void initialize_centroids_plusplus(
                const Tensor<float>& data,
                Tensor<float>& centroids,
                int k) {

                const int n = data.shape[0];
                const int d = data.shape[1];

                // 随机选择第一个质心
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, n - 1);

                int first_idx = dis(gen);

                // 拷贝第一个质心
                cuda_copy_d2d(centroids.data,
                              data.data + first_idx * d,
                              d);

                // TODO: 完整的k-means++实现
                // 为简化起见，这里使用随机初始化
                for (int c = 1; c < k; ++c) {
                    int idx = dis(gen);
                    cuda_copy_d2d(centroids.data + c * d,
                                  data.data + idx * d,
                                  d);
                }
            }

        } // anonymous namespace

        tinysog::internal::expected<KMeansResult, std::string> kmeans(
            const Tensor<float>& data,
            int k,
            int iterations,
            float tolerance) {

            if (data.shape.size() != 2) {
                return tinysog::internal::unexpected(std::string("Data must be 2D tensor [N, D]"));
            }

            if (!data.is_cuda) {
                return tinysog::internal::unexpected(std::string("Data must be on CUDA device"));
            }

            const int n = data.shape[0];
            const int d = data.shape[1];

            if (n <= k) {
                // 点数少于聚类数，直接返回所有点作为质心
                KMeansResult result;
                result.centroids = Tensor<float>({static_cast<size_t>(n), static_cast<size_t>(d)}, true);
                result.labels = Tensor<int32_t>({static_cast<size_t>(n)}, true);

                cuda_copy_d2d(result.centroids.data, data.data, n * d);

                thrust::device_ptr<int32_t> d_labels(result.labels.data);
                thrust::sequence(d_labels, d_labels + n, 0);

                return result;
            }

            try {
                // 初始化结果
                KMeansResult result;
                result.centroids = Tensor<float>({static_cast<size_t>(k), static_cast<size_t>(d)}, true);
                result.labels = Tensor<int32_t>({static_cast<size_t>(n)}, true);

                // 初始化质心
                initialize_centroids_plusplus(data, result.centroids, k);

                // 分配工作空间
                Tensor<float> old_centroids({static_cast<size_t>(k), static_cast<size_t>(d)}, true);
                Tensor<int32_t> counts({static_cast<size_t>(k)}, true);

                int grid_size, block_size;
                get_grid_config(n, grid_size, block_size);

                // 迭代
                for (int iter = 0; iter < iterations; ++iter) {
                    // 保存旧质心
                    cuda_copy_d2d(old_centroids.data, result.centroids.data, k * d);

                    // 分配聚类
                    assign_clusters_kernel<<<grid_size, block_size>>>(
                        data.data,
                        result.centroids.data,
                        result.labels.data,
                        n, k, d);

                    CUDA_CHECK(cudaGetLastError());

                    // 更新质心
                    cuda_memset(counts.data, 0, k);
                    dim3 update_block(d, 1);
                    dim3 update_grid(k, 1);

                    update_centroids_kernel<<<update_grid, update_block>>>(
                        data.data,
                        result.labels.data,
                        result.centroids.data,
                        counts.data,
                        n, k, d);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();

                    // 检查收敛（简化版本）
                    if (iter > 0 && tolerance > 0) {
                        // TODO: 实现完整的收敛检查
                    }
                }

                return result;

            } catch (const std::exception& e) {
                return tinysog::internal::unexpected(std::string("K-means failed: ") + e.what());
            }
        }

        tinysog::internal::expected<KMeansResult, std::string> kmeans_1d(
            const Tensor<float>& data,
            int k,
            int iterations) {

            if (data.shape.size() != 1) {
                return tinysog::internal::unexpected(std::string("Data must be 1D tensor [N]"));
            }

            if (!data.is_cuda) {
                return tinysog::internal::unexpected(std::string("Data must be on CUDA device"));
            }

            const int n = data.shape[0];

            if (n <= k) {
                // 点数少于聚类数
                KMeansResult result;
                result.centroids = Tensor<float>({static_cast<size_t>(n), 1}, true);
                result.labels = Tensor<int32_t>({static_cast<size_t>(n)}, true);

                cuda_copy_d2d(result.centroids.data, data.data, n);

                thrust::device_ptr<int32_t> d_labels(result.labels.data);
                thrust::sequence(d_labels, d_labels + n, 0);

                // 排序
                thrust::device_ptr<float> d_centroids(result.centroids.data);
                thrust::sort_by_key(d_centroids, d_centroids + n, d_labels);

                return result;
            }

            try {
                // 计算数据范围
                Tensor<float> data_copy({static_cast<size_t>(n)}, true);
                cuda_copy_d2d(data_copy.data, data.data, n);

                thrust::device_ptr<float> d_data(data_copy.data);
                auto minmax = thrust::minmax_element(d_data, d_data + n);

                float min_val, max_val;
                cuda_copy_d2h(&min_val, thrust::raw_pointer_cast(minmax.first), 1);
                cuda_copy_d2h(&max_val, thrust::raw_pointer_cast(minmax.second), 1);

                // 初始化结果
                KMeansResult result;
                result.centroids = Tensor<float>({static_cast<size_t>(k), 1}, true);
                result.labels = Tensor<int32_t>({static_cast<size_t>(n)}, true);

                // 均匀初始化质心
                std::vector<float> init_centroids(k);
                for (int i = 0; i < k; ++i) {
                    init_centroids[i] = min_val + (max_val - min_val) * i / (k - 1);
                }
                cuda_copy_h2d(result.centroids.data, init_centroids.data(), k);

                int grid_size, block_size;
                get_grid_config(n, grid_size, block_size);

                // 迭代
                for (int iter = 0; iter < iterations; ++iter) {
                    // 排序质心
                    thrust::device_ptr<float> d_centroids(result.centroids.data);
                    thrust::sort(d_centroids, d_centroids + k);

                    // 分配聚类（1D优化）
                    assign_clusters_1d_kernel<<<grid_size, block_size>>>(
                        data.data,
                        result.centroids.data,
                        result.labels.data,
                        n, k);

                    CUDA_CHECK(cudaGetLastError());

                    // 更新质心
                    Tensor<int32_t> counts({static_cast<size_t>(k)}, true);
                    cuda_memset(counts.data, 0, k);

                    dim3 update_block(1, 1);
                    dim3 update_grid(k, 1);

                    update_centroids_kernel<<<update_grid, update_block>>>(
                        data.data,
                        result.labels.data,
                        result.centroids.data,
                        counts.data,
                        n, k, 1);

                    CUDA_CHECK(cudaGetLastError());
                    cuda_synchronize();
                }

                // 最终排序
                Tensor<int64_t> sort_indices({static_cast<size_t>(k)}, true);
                thrust::device_ptr<int64_t> d_indices(sort_indices.data);
                thrust::sequence(d_indices, d_indices + k, 0LL);

                thrust::device_ptr<float> d_centroids(result.centroids.data);
                thrust::sort_by_key(d_centroids, d_centroids + k, d_indices);

                // 重映射标签
                Tensor<int32_t> inv_map({static_cast<size_t>(k)}, true);
                Tensor<int32_t> sort_idx_int32({static_cast<size_t>(k)}, true);

                // 转换int64到int32
                thrust::copy(d_indices, d_indices + k, 
                            thrust::device_ptr<int32_t>(sort_idx_int32.data));

                // 创建逆映射
                Tensor<int32_t> seq({static_cast<size_t>(k)}, true);
                thrust::sequence(thrust::device_ptr<int32_t>(seq.data), 
                                thrust::device_ptr<int32_t>(seq.data) + k, 0);

                thrust::scatter(thrust::device_ptr<int32_t>(seq.data), 
                                thrust::device_ptr<int32_t>(seq.data) + k,
                                thrust::device_ptr<int32_t>(sort_idx_int32.data),
                                thrust::device_ptr<int32_t>(inv_map.data));

                // 应用逆映射到标签
                Tensor<int32_t> new_labels({static_cast<size_t>(n)}, true);
                thrust::gather(thrust::device_ptr<int32_t>(result.labels.data),
                               thrust::device_ptr<int32_t>(result.labels.data) + n,
                               thrust::device_ptr<int32_t>(inv_map.data),
                               thrust::device_ptr<int32_t>(new_labels.data));

                result.labels = std::move(new_labels);

                cuda_synchronize();

                return result;

            } catch (const std::exception& e) {
                return tinysog::internal::unexpected(std::string("K-means 1D failed: ") + e.what());
            }
        }

    } // namespace internal
} // namespace tinysog







