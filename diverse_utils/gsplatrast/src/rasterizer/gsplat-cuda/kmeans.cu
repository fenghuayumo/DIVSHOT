#include <ATen/Dispatch.h> // AT_DISPATCH_XXX
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAStream.h> // at::cuda::getCurrentCUDAStream

#ifdef USE_HIP
#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
#else
#include <cooperative_groups.h>
#include <cuda_runtime.h>
#endif
#include "null.h"

namespace cg = cooperative_groups;

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
