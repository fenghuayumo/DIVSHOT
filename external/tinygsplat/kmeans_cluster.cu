#include "tiny_gsplat.hpp"

#ifdef USE_HIP
#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
#else
#include <cooperative_groups.h>
#include <cuda_runtime.h>
#endif

namespace cg = cooperative_groups;

namespace tinygsplat
{
// https://alexminnaar.com/2019/03/05/cuda-kmeans.html
__device__ float distanceCUDA(const float x1, const float x2)
{
	return sqrt((x2 - x1) * (x2 - x1));
}

// This function finds the centroid value based on the points that are
// classified as belonding to the respective class
__global__ void updateCentersCUDA(
	const float *values,
	const int *ids,
	float *centers,
	int *center_sizes,
	const int n_values,
	const int n_centers)
{
	auto idx = cg::this_grid().thread_rank();
	auto block = cg::this_thread_block();
	if (idx >= n_values)
		return;
	__shared__ float collected_values[256];
	collected_values[block.thread_rank()] = values[idx];

	__shared__ int collected_ids[256];
	collected_ids[block.thread_rank()] = ids[idx];

	block.sync();

	// One thread per block take on the task to gather the values
	if (block.thread_rank() == 0)
	{
		float block_center_sums[256] = {0};
		int block_center_sizes[256] = {0};
		for (int i = 0; i < 256 && idx + i < n_values; ++i)
		{
			int clust_id = collected_ids[i];
			block_center_sums[clust_id] += collected_values[i];
			block_center_sizes[clust_id] += 1;
		}

		for (int i = 0; i < n_centers; ++i)
		{
			atomicAdd(&centers[i], block_center_sums[i]);
			atomicAdd(&center_sizes[i], block_center_sizes[i]);
		}
	}
}

void updateCenters(
	const float *values,
	const int *ids,
	float *centers,
	int *center_sizes,
	const int n_values,
	const int n_centers)
{
	updateCentersCUDA<<<(n_values + 255) / 256, 256>>>(
		values,
		ids,
		centers,
		center_sizes,
		n_values,
		n_centers);
}

// This function finds the closest centroid for each point
__global__ void updateIdsCUDA(
	const float *values,
	int *ids,
	const float *centers,
	const int n_values,
	const int n_centers)
{
	auto idx = cg::this_grid().thread_rank();
	auto block = cg::this_thread_block();

	if (idx >= n_values)
		return;

	float min_dist = INFINITY;
	int closest_centroid = 0;

	__shared__ float collected_centers[256];

	block.sync();
	collected_centers[block.thread_rank()] = centers[block.thread_rank()];
	block.sync();

	for (int i = 0; i < n_centers; ++i)
	{
		float dist = distanceCUDA(values[idx], collected_centers[i]);

		if (dist < min_dist)
		{
			min_dist = dist;
			closest_centroid = i;
		}
	}

	ids[idx] = closest_centroid;
}

void updateIds(
	const float *values,
	int *ids,
	const float *centers,
	const int n_values,
	const int n_centers)
{
	updateIdsCUDA<<<(n_values + 255) / 256, 256>>>(
		values,
		ids,
		centers,
		n_values,
		n_centers);
}

// Works with 256 centers 1 dimensional data only
std::tuple<std::vector<int>, std::vector<float>>  kmeans_cluster(
	const std::vector<float>& values,
	const std::vector<float>& centers,
	const float tol,
	const int max_iterations)
{
	const int n_values = values.size();
	const int n_centers = centers.size();
	std::vector<int> ids(n_values,0);
	std::vector<float> new_centers(n_centers, 0.0f);
	std::vector<float> old_centers(n_centers, 0.0f);
	std::vector<int> center_sizes(n_centers, 0);
	new_centers = centers;
	float* d_centers, *d_old_centers, *d_values;
	int* d_center_sizes,*d_ids;
	cudaMalloc(&d_centers,sizeof(float) * n_centers);
	cudaMalloc(&d_old_centers, sizeof(float) * n_centers);
	cudaMalloc(&d_center_sizes, sizeof(int) * n_values);
	cudaMemcpy(d_center_sizes, center_sizes.data(), sizeof(int) * n_centers, cudaMemcpyHostToDevice);

	cudaMalloc(&d_values, sizeof(float) * n_values);
	cudaMalloc(&d_ids, sizeof(int) * n_values);
	cudaMemcpy(d_values, values.data(), sizeof(float) * n_values, cudaMemcpyHostToDevice);
	cudaMemcpy(d_ids, ids.data(), sizeof(int) * n_values, cudaMemcpyHostToDevice);
	for (int i = 0; i < max_iterations; ++i)
	{
		cudaMemcpy(d_centers, new_centers.data(), sizeof(float) * n_centers, cudaMemcpyHostToDevice);
		cudaMemcpy(d_old_centers,old_centers.data(), sizeof(float) * n_centers, cudaMemcpyHostToDevice);

		updateIds(
			d_values,
			d_ids,
			d_centers,
			n_values,
			n_centers);
		
		cudaMemcpy(d_old_centers,d_centers, sizeof(float) * n_centers,cudaMemcpyDeviceToDevice);
		cudaMemset(d_centers,0, sizeof(float) * n_centers);
		cudaMemset(d_center_sizes, 0, sizeof(float) * n_centers);

		updateCenters(
			d_values,
			d_ids,
			d_centers,
			d_center_sizes,
			n_values,
			n_centers);

		cudaMemcpy(new_centers.data(), d_centers, sizeof(float) * n_centers, cudaMemcpyDeviceToHost);
		cudaMemcpy(old_centers.data(), d_old_centers, sizeof(float) * n_centers, cudaMemcpyDeviceToHost);
		cudaMemcpy(center_sizes.data(), d_center_sizes, sizeof(int) * n_centers, cudaMemcpyDeviceToHost);
		float center_shift = 0;
		for (auto j = 0; j < n_centers; j++)
		{
			if (center_sizes[j] != 0)
			{
				new_centers[j] /= center_sizes[j];
				center_shift += std::abs(old_centers[j] - new_centers[j]);
			}
			else 
				new_centers[j] = 0;
		}
		if (center_shift < tol)
			break;
	}
	cudaMemcpy(d_centers, new_centers.data(), sizeof(float) * n_centers, cudaMemcpyHostToDevice);
	updateIds(
		d_values,
		d_ids,
		d_centers,
		n_values,
		n_centers);
	cudaMemcpy(new_centers.data(), d_centers, sizeof(float) * n_centers, cudaMemcpyDeviceToHost);
	cudaMemcpy(ids.data(), d_ids, sizeof(int) * n_values, cudaMemcpyDeviceToHost);
	cudaFree(d_centers);
	cudaFree(d_old_centers);
	cudaFree(d_center_sizes);
	cudaFree(d_values);
	cudaFree(d_ids);
	return std::make_tuple(ids, new_centers);
}

} // namespace tinygsplat