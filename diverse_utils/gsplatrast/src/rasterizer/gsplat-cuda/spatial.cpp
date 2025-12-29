#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "common.h"     // where all the macros are defined
#include "ops.h"        // a collection of all gsplat operators
#include "simple_knn.h"
namespace gsplat{
  
at::Tensor distKnn2(const at::Tensor& points)
{
	const int P = points.size(0);

	auto float_opts = points.options().dtype(at::kFloat);
	at::Tensor means = at::full({ P }, 0.0, float_opts);

	SimpleKNN::knn(P, (float3*)points.contiguous().data<float>(), means.contiguous().data<float>());

	return means.unsqueeze(-1);
}


std::tuple<at::Tensor,at::Tensor>
distIndices2(const at::Tensor& points, int K)
{
  const int P = points.size(0);

  auto float_opts = points.options().dtype(at::kFloat);
  auto int_opts = points.options().dtype(at::kInt);
  at::Tensor dists = at::full({P * K}, 0.0, float_opts);
  at::Tensor indices = at::full({P * K}, -1, int_opts);
  
  SimpleKNN::knn_index2(K, P, (float3*)points.contiguous().data<float>(), dists.contiguous().data<float>(), indices.contiguous().data<int>());

  return {dists, indices};
}

std::tuple<at::Tensor,at::Tensor>
distIndicesQ(const at::Tensor& points, const at::Tensor& q_indices, const at::Tensor& n_indices, int K)
{
  const int P = points.size(0);
  const int Q = q_indices.size(0);
  const int N = n_indices.size(0);

  auto float_opts = points.options().dtype(at::kFloat);
  auto int_opts = points.options().dtype(at::kInt);
  at::Tensor dists = at::full({Q * K}, 0.0, float_opts);
  at::Tensor indices = at::full({Q * K}, -1, int_opts);
  
  SimpleKNN::knn_indexQ(K, P, (float3*)points.contiguous().data<float>(), Q, q_indices.contiguous().data<int>(), N, n_indices.contiguous().data<int>(), dists.contiguous().data<float>(), indices.contiguous().data<int>());

  return {dists, indices};
}
}
