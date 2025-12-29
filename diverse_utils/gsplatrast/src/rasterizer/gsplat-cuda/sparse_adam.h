#pragma once
#include <ATen/core/Tensor.h>

namespace gsplat {

// Sparse Adam kernel: only update parameters at sparse gradient locations
// Args:
//   param: [N, D] parameter tensor
//   exp_avg: [N, D] first moment buffer
//   exp_avg_sq: [N, D] second moment buffer
//   grad_indices: [nnz] sparse gradient indices (which parameters to update)
//   grad_values: [nnz, D] sparse gradient values
//   lr: learning rate
//   beta1: first moment decay rate
//   beta2: second moment decay rate
//   eps: epsilon for numerical stability
//   bias_correction1_rcp: 1 / (1 - beta1^t)
//   bias_correction2_sqrt_rcp: 1 / sqrt(1 - beta2^t)
void launch_sparse_adam_kernel(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& grad_indices,
    const at::Tensor& grad_values,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1_rcp,
    const float bias_correction2_sqrt_rcp
);

} // namespace gsplat

